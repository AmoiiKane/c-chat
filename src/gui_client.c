/*
 * c-chat GUI client
 * WebKit2GTK on Linux | WebView2 on Windows | WKWebView on macOS
 *
 * Architecture:
 *   - GTK window hosts a WebKitWebView (full viewport)
 *   - HTML/CSS/JS chat UI runs inside the WebView
 *   - JS calls window.cchat.send(msg) → C receives it via script message handler
 *   - C socket thread reads from server → calls webkit_web_view_evaluate_javascript()
 *     to push messages into the JS UI
 *   - Connection dialog is part of the HTML — no terminal ever opens
 */

#include "common.h"

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#include <pthread.h>
#include <stdatomic.h>

/* ── State ──────────────────────────────────────────────────── */
static WebKitWebView  *g_webview  = NULL;
static sock_t          g_sock     = SOCK_INVALID;
static atomic_int      g_running  = 0;
static pthread_t       g_recv_tid;

/* ── Escape a C string for safe embedding in a JS string literal ── */
static void js_escape(const char *src, char *dst, size_t dstlen)
{
    size_t j = 0;
    for (size_t i = 0; src[i] && j + 4 < dstlen; i++) {
        unsigned char c = (unsigned char)src[i];
        if      (c == '\\') { dst[j++] = '\\'; dst[j++] = '\\'; }
        else if (c == '\'') { dst[j++] = '\\'; dst[j++] = '\''; }
        else if (c == '\n') { dst[j++] = '\\'; dst[j++] = 'n';  }
        else if (c == '\r') { /* skip */ }
        else                { dst[j++] = (char)c; }
    }
    dst[j] = '\0';
}

/* ── Push a message into the JS UI (called from any thread via g_idle) ── */
typedef struct { char *js; } IdleJs;

static gboolean run_js_idle(gpointer data)
{
    IdleJs *p = (IdleJs *)data;
    if (g_webview)
        webkit_web_view_evaluate_javascript(g_webview, p->js, -1,
                                            NULL, NULL, NULL, NULL, NULL);
    free(p->js);
    free(p);
    return G_SOURCE_REMOVE;
}

static void push_js(const char *fmt, const char *arg)
{
    char escaped[BUFFER_SIZE * 4];
    if (arg) js_escape(arg, escaped, sizeof(escaped));

    char js[BUFFER_SIZE * 5];
    if (arg) snprintf(js, sizeof(js), fmt, escaped);
    else     snprintf(js, sizeof(js), "%s", fmt);

    IdleJs *p = malloc(sizeof(IdleJs));
    p->js = strdup(js);
    g_idle_add(run_js_idle, p);
}

/* ── Receive thread — reads from server, pushes to UI ───────── */
static void *recv_thread(void *arg)
{
    (void)arg;
    char buf[BUFFER_SIZE];
    int  n;

    while (atomic_load(&g_running) &&
           (n = (int)sock_read(g_sock, buf, BUFFER_SIZE - 1)) > 0) {
        buf[n] = '\0';
        /* strip trailing newline */
        size_t len = strlen(buf);
        while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r'))
            buf[--len] = '\0';
        if (len == 0) continue;

        /* Check if it's the username prompt from the server */
        if (strstr(buf, "Enter username")) {
            push_js("UI.promptUsername()", NULL);
        } else {
            push_js("UI.recv('%s')", buf);
        }
    }

    if (atomic_load(&g_running)) {
        atomic_store(&g_running, 0);
        push_js("UI.disconnected()", NULL);
    }
    return NULL;
}

/* ── JS → C message handler ─────────────────────────────────── */
static void on_script_message(WebKitUserContentManager *mgr,
                              WebKitJavascriptResult   *result,
                              gpointer                  user_data)
{
    (void)mgr; (void)user_data;

    JSCValue   *val = webkit_javascript_result_get_js_value(result);
    char       *str = jsc_value_to_string(val);
    if (!str) return;

    /* Protocol: "connect:<host>:<port>:<username>"  or  "msg:<text>" */
    if (strncmp(str, "connect:", 8) == 0) {
        char *p    = str + 8;
        char *host = strsep(&p, ":");
        char *port_s = strsep(&p, ":");
        char *uname  = p;   /* rest is username */

        if (!host || !port_s || !uname) { free(str); return; }

        int port = atoi(port_s);
        if (port <= 0 || port > 65535) { free(str); return; }

        /* Close existing connection */
        if (g_sock != SOCK_INVALID) {
            atomic_store(&g_running, 0);
            sock_close(g_sock);
            pthread_join(g_recv_tid, NULL);
            g_sock = SOCK_INVALID;
        }

        /* Connect */
        struct sockaddr_in sa;
        memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;
        sa.sin_port   = htons(port);
        if (inet_pton(AF_INET, host, &sa.sin_addr) <= 0) {
            push_js("UI.error('Invalid host address')", NULL);
            free(str); return;
        }

        g_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (g_sock == SOCK_INVALID ||
            connect(g_sock, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
            push_js("UI.error('Could not connect to server')", NULL);
            if (g_sock != SOCK_INVALID) { sock_close(g_sock); g_sock = SOCK_INVALID; }
            free(str); return;
        }

        atomic_store(&g_running, 1);
        pthread_create(&g_recv_tid, NULL, recv_thread, NULL);
        pthread_detach(g_recv_tid);

        /* Send username to server (server will prompt — recv_thread handles it) */
        /* We store it temporarily; recv_thread will send it when prompted */
        /* Actually send it immediately — server reads it after the prompt */
        char uname_nl[USERNAME_SIZE + 2];
        snprintf(uname_nl, sizeof(uname_nl), "%s\n", uname);
        sock_write(g_sock, uname_nl, (int)strlen(uname_nl));

        push_js("UI.connected('%s')", uname);

    } else if (strncmp(str, "msg:", 4) == 0) {
        const char *msg = str + 4;
        if (g_sock != SOCK_INVALID && atomic_load(&g_running)) {
            char out[BUFFER_SIZE];
            snprintf(out, sizeof(out), "%s\n", msg);
            sock_write(g_sock, out, (int)strlen(out));
            if (strcmp(msg, "/quit") == 0) {
                atomic_store(&g_running, 0);
                sock_close(g_sock);
                g_sock = SOCK_INVALID;
                push_js("UI.disconnected()", NULL);
            }
        }
    }

    free(str);
}

/* ── HTML/CSS/JS UI ─────────────────────────────────────────── */
static const char *HTML = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>c-chat</title>
<style>
  @import url('https://fonts.googleapis.com/css2?family=DM+Mono:ital,wght@0,400;0,500;1,400&family=Syne:wght@700;800&display=swap');

  *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

  :root {
    --bg:        #0d0f14;
    --surface:   #13161e;
    --border:    #1e2330;
    --accent:    #5afa9e;
    --accent2:   #3de8ff;
    --muted:     #3a4058;
    --text:      #d4daf0;
    --text-dim:  #5a6380;
    --sys:       #ffd166;
    --danger:    #ff5f7e;
    --radius:    10px;
    --font-mono: 'DM Mono', monospace;
    --font-head: 'Syne', sans-serif;
  }

  html, body {
    height: 100%;
    background: var(--bg);
    color: var(--text);
    font-family: var(--font-mono);
    font-size: 13px;
    overflow: hidden;
    -webkit-font-smoothing: antialiased;
  }

  /* ── CONNECT SCREEN ── */
  #connect-screen {
    position: fixed; inset: 0;
    display: flex; align-items: center; justify-content: center;
    background: var(--bg);
    z-index: 100;
    transition: opacity 0.4s ease, visibility 0.4s ease;
  }
  #connect-screen.hidden { opacity: 0; visibility: hidden; pointer-events: none; }

  .connect-card {
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: 16px;
    padding: 40px 44px;
    width: 420px;
    box-shadow: 0 0 60px rgba(94,250,158,0.06), 0 24px 60px rgba(0,0,0,0.5);
  }

  .logo {
    font-family: var(--font-head);
    font-size: 28px;
    font-weight: 800;
    letter-spacing: -0.5px;
    color: #fff;
    margin-bottom: 6px;
    display: flex; align-items: center; gap: 10px;
  }
  .logo span { color: var(--accent); }
  .logo-dot {
    width: 8px; height: 8px;
    background: var(--accent);
    border-radius: 50%;
    box-shadow: 0 0 12px var(--accent);
    animation: pulse 2s ease-in-out infinite;
  }
  @keyframes pulse {
    0%,100% { opacity: 1; box-shadow: 0 0 10px var(--accent); }
    50%      { opacity: 0.4; box-shadow: 0 0 4px var(--accent); }
  }

  .subtitle {
    color: var(--text-dim);
    font-size: 12px;
    margin-bottom: 32px;
    letter-spacing: 0.3px;
  }

  .field { margin-bottom: 16px; }
  .field label {
    display: block;
    font-size: 10px;
    letter-spacing: 1.5px;
    text-transform: uppercase;
    color: var(--text-dim);
    margin-bottom: 7px;
  }
  .field input {
    width: 100%;
    background: var(--bg);
    border: 1px solid var(--border);
    border-radius: var(--radius);
    color: var(--text);
    font-family: var(--font-mono);
    font-size: 13px;
    padding: 10px 14px;
    outline: none;
    transition: border-color 0.2s, box-shadow 0.2s;
  }
  .field input:focus {
    border-color: var(--accent);
    box-shadow: 0 0 0 3px rgba(94,250,158,0.1);
  }
  .field input::placeholder { color: var(--muted); }

  .row { display: flex; gap: 12px; }
  .row .field { flex: 1; }
  .row .field:last-child { flex: 0 0 100px; }

  .btn-connect {
    width: 100%;
    margin-top: 8px;
    padding: 12px;
    background: var(--accent);
    color: #0d0f14;
    border: none;
    border-radius: var(--radius);
    font-family: var(--font-head);
    font-size: 14px;
    font-weight: 700;
    letter-spacing: 0.5px;
    cursor: pointer;
    transition: transform 0.15s, box-shadow 0.15s, background 0.15s;
  }
  .btn-connect:hover {
    background: #7dffc0;
    box-shadow: 0 0 20px rgba(94,250,158,0.35);
    transform: translateY(-1px);
  }
  .btn-connect:active { transform: translateY(0); }
  .btn-connect:disabled { opacity: 0.4; cursor: not-allowed; transform: none; }

  #connect-error {
    margin-top: 12px;
    font-size: 12px;
    color: var(--danger);
    min-height: 16px;
    text-align: center;
  }

  /* ── CHAT SCREEN ── */
  #chat-screen {
    position: fixed; inset: 0;
    display: flex; flex-direction: column;
    opacity: 0; visibility: hidden;
    transition: opacity 0.4s ease, visibility 0.4s ease;
  }
  #chat-screen.visible { opacity: 1; visibility: visible; }

  /* Header */
  #header {
    display: flex; align-items: center; justify-content: space-between;
    padding: 14px 20px;
    border-bottom: 1px solid var(--border);
    background: var(--surface);
    flex-shrink: 0;
  }
  #header-left { display: flex; align-items: center; gap: 10px; }
  #header-logo {
    font-family: var(--font-head);
    font-size: 16px; font-weight: 800;
    color: #fff;
  }
  #header-logo span { color: var(--accent); }
  #status-dot {
    width: 7px; height: 7px;
    border-radius: 50%;
    background: var(--accent);
    box-shadow: 0 0 8px var(--accent);
  }
  #status-dot.off { background: var(--danger); box-shadow: 0 0 8px var(--danger); }
  #status-text { color: var(--text-dim); font-size: 12px; }
  #btn-disconnect {
    background: none; border: 1px solid var(--border);
    border-radius: 7px; color: var(--text-dim);
    font-family: var(--font-mono); font-size: 11px;
    padding: 5px 12px; cursor: pointer;
    transition: border-color 0.2s, color 0.2s;
  }
  #btn-disconnect:hover { border-color: var(--danger); color: var(--danger); }

  /* Messages */
  #messages {
    flex: 1;
    overflow-y: auto;
    padding: 20px 20px 8px;
    display: flex; flex-direction: column; gap: 4px;
    scroll-behavior: smooth;
  }
  #messages::-webkit-scrollbar { width: 4px; }
  #messages::-webkit-scrollbar-track { background: transparent; }
  #messages::-webkit-scrollbar-thumb { background: var(--border); border-radius: 4px; }

  .msg {
    display: flex; align-items: baseline; gap: 10px;
    padding: 3px 0;
    animation: fadeIn 0.15s ease;
  }
  @keyframes fadeIn { from { opacity: 0; transform: translateY(4px); } to { opacity: 1; } }

  .msg-time { color: var(--muted); font-size: 10px; flex-shrink: 0; letter-spacing: 0.3px; }
  .msg-user { color: var(--accent2); font-size: 12px; font-weight: 500; flex-shrink: 0; min-width: 80px; }
  .msg-text { color: var(--text); line-height: 1.5; word-break: break-word; }

  .msg.system .msg-user { color: var(--sys); }
  .msg.system .msg-text { color: var(--sys); font-style: italic; }
  .msg.own    .msg-user { color: var(--accent); }
  .msg.error  .msg-user { color: var(--danger); }
  .msg.error  .msg-text { color: var(--danger); }

  /* Input bar */
  #input-bar {
    display: flex; align-items: center; gap: 10px;
    padding: 12px 16px;
    border-top: 1px solid var(--border);
    background: var(--surface);
    flex-shrink: 0;
  }
  #prompt-label {
    color: var(--accent);
    font-size: 13px;
    flex-shrink: 0;
    user-select: none;
  }
  #msg-input {
    flex: 1;
    background: none;
    border: none;
    outline: none;
    color: var(--text);
    font-family: var(--font-mono);
    font-size: 13px;
    caret-color: var(--accent);
  }
  #msg-input::placeholder { color: var(--muted); }
  #btn-send {
    background: var(--accent);
    border: none; border-radius: 7px;
    color: #0d0f14;
    font-family: var(--font-head);
    font-size: 12px; font-weight: 700;
    padding: 7px 16px;
    cursor: pointer;
    transition: background 0.15s, box-shadow 0.15s;
    flex-shrink: 0;
  }
  #btn-send:hover { background: #7dffc0; box-shadow: 0 0 14px rgba(94,250,158,0.3); }
  #btn-send:disabled { opacity: 0.3; cursor: not-allowed; }
</style>
</head>
<body>

<!-- ── CONNECT SCREEN ── -->
<div id="connect-screen">
  <div class="connect-card">
    <div class="logo">
      <div class="logo-dot"></div>
      c-<span>chat</span>
    </div>
    <div class="subtitle">lightweight terminal chat — now with a face</div>

    <div class="field">
      <label>Username</label>
      <input id="inp-username" type="text" placeholder="yourname" maxlength="31" autocomplete="off" spellcheck="false">
    </div>
    <div class="row">
      <div class="field">
        <label>Server Host</label>
        <input id="inp-host" type="text" placeholder="127.0.0.1" autocomplete="off" spellcheck="false">
      </div>
      <div class="field">
        <label>Port</label>
        <input id="inp-port" type="text" placeholder="8080" autocomplete="off" spellcheck="false">
      </div>
    </div>

    <button class="btn-connect" id="btn-connect" onclick="UI.connect()">Connect</button>
    <div id="connect-error"></div>
  </div>
</div>

<!-- ── CHAT SCREEN ── -->
<div id="chat-screen">
  <div id="header">
    <div id="header-left">
      <div id="header-logo">c-<span>chat</span></div>
      <div id="status-dot"></div>
      <div id="status-text">connected</div>
    </div>
    <button id="btn-disconnect" onclick="UI.quit()">disconnect</button>
  </div>

  <div id="messages"></div>

  <div id="input-bar">
    <span id="prompt-label">❯</span>
    <input id="msg-input" type="text" placeholder="type a message…" autocomplete="off" spellcheck="false">
    <button id="btn-send" onclick="UI.send()">Send</button>
  </div>
</div>

<script>
const UI = (() => {
  let myName = '';

  function now() {
    const d = new Date();
    return d.getHours().toString().padStart(2,'0') + ':' +
           d.getMinutes().toString().padStart(2,'0');
  }

  function addMsg(user, text, cls) {
    const box = document.getElementById('messages');
    const div = document.createElement('div');
    div.className = 'msg' + (cls ? ' ' + cls : '');
    div.innerHTML =
      `<span class="msg-time">${now()}</span>` +
      `<span class="msg-user">${esc(user)}</span>` +
      `<span class="msg-text">${esc(text)}</span>`;
    box.appendChild(div);
    box.scrollTop = box.scrollHeight;
  }

  function esc(s) {
    return String(s)
      .replace(/&/g,'&amp;').replace(/</g,'&lt;')
      .replace(/>/g,'&gt;').replace(/"/g,'&quot;');
  }

  function setError(msg) {
    document.getElementById('connect-error').textContent = msg;
  }

  return {
    connect() {
      const uname = document.getElementById('inp-username').value.trim();
      const host  = document.getElementById('inp-host').value.trim() || '127.0.0.1';
      const port  = document.getElementById('inp-port').value.trim() || '8080';

      if (!uname) { setError('Username is required'); return; }
      if (!/^\d+$/.test(port) || +port < 1 || +port > 65535)
        { setError('Port must be 1–65535'); return; }

      setError('');
      document.getElementById('btn-connect').disabled = true;
      document.getElementById('btn-connect').textContent = 'Connecting…';

      myName = uname;
      window.webkit.messageHandlers.cchat.postMessage(`connect:${host}:${port}:${uname}`);
    },

    connected(name) {
      myName = name;
      document.getElementById('connect-screen').classList.add('hidden');
      document.getElementById('chat-screen').classList.add('visible');
      document.getElementById('status-dot').classList.remove('off');
      document.getElementById('status-text').textContent = `connected as ${name}`;
      document.getElementById('msg-input').focus();
      addMsg('system', `Connected to server as ${name}`, 'system');
    },

    disconnected() {
      document.getElementById('status-dot').classList.add('off');
      document.getElementById('status-text').textContent = 'disconnected';
      document.getElementById('btn-send').disabled = true;
      document.getElementById('msg-input').disabled = true;
      addMsg('system', 'Disconnected from server', 'system');
    },

    promptUsername() {
      /* Server sent "Enter username" — we already sent it at connect time, ignore */
    },

    recv(raw) {
      /* Parse "[user]: text"  or  "[user joined/left…]" */
      const joined = raw.match(/^\[(.+) joined the chat\]$/);
      const left   = raw.match(/^\[(.+) left the chat\]$/);
      const chat   = raw.match(/^\[(.+)\]: (.+)$/);

      if (joined)     addMsg('→', `${joined[1]} joined`, 'system');
      else if (left)  addMsg('←', `${left[1]} left`, 'system');
      else if (chat)  addMsg(chat[1], chat[2], chat[1] === myName ? 'own' : '');
      else            addMsg('server', raw, 'system');
    },

    error(msg) {
      /* Could be called from connect screen or chat screen */
      if (document.getElementById('connect-screen').classList.contains('hidden')) {
        addMsg('error', msg, 'error');
      } else {
        setError(msg);
        document.getElementById('btn-connect').disabled = false;
        document.getElementById('btn-connect').textContent = 'Connect';
      }
    },

    send() {
      const inp = document.getElementById('msg-input');
      const txt = inp.value.trim();
      if (!txt) return;
      addMsg(myName, txt, 'own');
      window.webkit.messageHandlers.cchat.postMessage(`msg:${txt}`);
      inp.value = '';
      inp.focus();
    },

    quit() {
      window.webkit.messageHandlers.cchat.postMessage('msg:/quit');
    }
  };
})();

/* Enter key to send */
document.getElementById('msg-input').addEventListener('keydown', e => {
  if (e.key === 'Enter') UI.send();
});
document.getElementById('inp-username').addEventListener('keydown', e => {
  if (e.key === 'Enter') document.getElementById('inp-host').focus();
});
document.getElementById('inp-host').addEventListener('keydown', e => {
  if (e.key === 'Enter') document.getElementById('inp-port').focus();
});
document.getElementById('inp-port').addEventListener('keydown', e => {
  if (e.key === 'Enter') UI.connect();
});

/* Focus username on load */
window.addEventListener('load', () => {
  document.getElementById('inp-username').focus();
});
</script>
</body>
</html>
)HTML";

/* ── GTK app activate ────────────────────────────────────────── */
static void activate(GtkApplication *app, gpointer user_data)
{
    (void)user_data;

    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "c-chat");
    gtk_window_set_default_size(GTK_WINDOW(window), 860, 600);
    gtk_window_set_resizable(GTK_WINDOW(window), TRUE);

    /* User content manager for JS→C messages */
    WebKitUserContentManager *ucm = webkit_user_content_manager_new();
    g_signal_connect(ucm, "script-message-received::cchat",
                     G_CALLBACK(on_script_message), NULL);
    webkit_user_content_manager_register_script_message_handler(ucm, "cchat");

    g_webview = WEBKIT_WEB_VIEW(
        webkit_web_view_new_with_user_content_manager(ucm));

    /* Settings */
    WebKitSettings *settings = webkit_web_view_get_settings(g_webview);
    webkit_settings_set_enable_developer_extras(settings, FALSE);
    webkit_settings_set_enable_write_console_messages_to_stdout(settings, TRUE);

    gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(g_webview));

    webkit_web_view_load_html(g_webview, HTML, "file:///");
    gtk_widget_show_all(window);
}

/* ── Cleanup on quit ─────────────────────────────────────────── */
static void on_shutdown(GtkApplication *app, gpointer user_data)
{
    (void)app; (void)user_data;
    atomic_store(&g_running, 0);
    if (g_sock != SOCK_INVALID) {
        sock_close(g_sock);
        g_sock = SOCK_INVALID;
    }
    net_cleanup();
}

/* ── main ────────────────────────────────────────────────────── */
int main(int argc, char *argv[])
{
    net_init();

    GtkApplication *app = gtk_application_new("com.cchat.gui",
                                              G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate",  G_CALLBACK(activate),    NULL);
    g_signal_connect(app, "shutdown",  G_CALLBACK(on_shutdown),  NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
