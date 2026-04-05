const { ipcRenderer } = require('electron');
const path = require('path');

// ── Session ──────────────────────────────────────────────────────────
const username   = sessionStorage.getItem('username') || 'Unknown';
const host       = sessionStorage.getItem('host') || '127.0.0.1';
const port       = sessionStorage.getItem('port') || '8080';
const avatarPath = sessionStorage.getItem('avatar') || '';

// ── DOM refs ─────────────────────────────────────────────────────────
const messagesEl  = document.getElementById('messages');
const msgInput    = document.getElementById('msg-input');
const sendBtn     = document.getElementById('send-btn');
const userListEl  = document.getElementById('user-list');
const myAvatar    = document.getElementById('my-avatar');
const myUsername  = document.getElementById('my-username');
const headerServer = document.getElementById('header-server');

// ── Init UI ───────────────────────────────────────────────────────────
myUsername.textContent  = username;
headerServer.textContent = `${host}:${port}`;
if (avatarPath) {
  myAvatar.src = `file://${avatarPath}`;
} else {
  myAvatar.src = '';
  myAvatar.style.display = 'none';
  const wrap = document.querySelector('.my-avatar-wrap');
  const placeholder = document.createElement('div');
  placeholder.style.cssText = `width:40px;height:40px;border:1px solid rgba(180,140,80,0.25);background:rgba(0,0,0,0.4);display:flex;align-items:center;justify-content:center;font-family:'Cinzel Decorative',serif;font-size:14px;color:#7a6025;`;
  placeholder.textContent = username[0].toUpperCase();
  wrap.insertBefore(placeholder, myAvatar);
}

// ── Online users (simple local tracking) ─────────────────────────────
const onlineUsers = new Set([username]);

function renderUserList() {
  userListEl.innerHTML = '';
  onlineUsers.forEach(u => {
    const div = document.createElement('div');
    div.className = 'user-item';
    div.innerHTML = `<div class="user-dot"></div><span>${u}</span>`;
    userListEl.appendChild(div);
  });
}
renderUserList();

// ── Message rendering ─────────────────────────────────────────────────
function addSystemMessage(text) {
  const div = document.createElement('div');
  div.className = 'msg-system';
  div.textContent = text;
  messagesEl.appendChild(div);
  scrollBottom();
}

function addMessage(author, text, isOwn = false) {
  const time = new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });

  const div = document.createElement('div');
  div.className = `msg${isOwn ? ' msg-own' : ''}`;

  // Avatar
  let avatarEl;
  if (isOwn && avatarPath) {
    avatarEl = document.createElement('img');
    avatarEl.className = 'msg-avatar';
    avatarEl.src = `file://${avatarPath}`;
    avatarEl.alt = author;
  } else {
    avatarEl = document.createElement('div');
    avatarEl.className = 'msg-avatar-placeholder';
    avatarEl.textContent = author[0]?.toUpperCase() || '?';
  }

  const body = document.createElement('div');
  body.className = 'msg-body';
  body.innerHTML = `
    <div class="msg-meta">
      <span class="msg-author">${escapeHtml(author)}</span>
      <span class="msg-time">${time}</span>
    </div>
    <div class="msg-text">${escapeHtml(text)}</div>
  `;

  div.appendChild(avatarEl);
  div.appendChild(body);
  messagesEl.appendChild(div);
  scrollBottom();
}

function escapeHtml(str) {
  return str
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}

function scrollBottom() {
  messagesEl.scrollTop = messagesEl.scrollHeight;
}

// ── Incoming TCP data ─────────────────────────────────────────────────
ipcRenderer.on('tcp-message', (event, raw) => {
  // Server sends lines — could be multiple in one chunk
  const lines = raw.split('\n').filter(l => l.trim());
  lines.forEach(line => parseLine(line.trim()));
});

function parseLine(line) {
  // System messages: "[username] has joined" / "[username] has left"
  const joinMatch = line.match(/^\[(.+?)\] has joined/);
  const leftMatch = line.match(/^\[(.+?)\] has left/);

  if (joinMatch) {
    onlineUsers.add(joinMatch[1]);
    renderUserList();
    addSystemMessage(`⛧ ${line}`);
    return;
  }
  if (leftMatch) {
    onlineUsers.delete(leftMatch[1]);
    renderUserList();
    addSystemMessage(`⛧ ${line}`);
    return;
  }

  // Regular chat: "[username]: message"
  const msgMatch = line.match(/^\[(.+?)\]: (.+)$/);
  if (msgMatch) {
    const [, author, text] = msgMatch;
    const isOwn = author === username;
    addMessage(author, text, isOwn);
    return;
  }

  // Fallback — show as system
  if (line) addSystemMessage(line);
}

ipcRenderer.on('tcp-disconnected', () => {
  addSystemMessage('⛧ Disconnected from server.');
  setTimeout(() => { window.location.href = 'index.html'; }, 2000);
});

// ── Send message ──────────────────────────────────────────────────────
function sendMessage() {
  const text = msgInput.value.trim();
  if (!text) return;

  if (text === '/quit') {
    ipcRenderer.send('tcp-disconnect');
    window.location.href = 'index.html';
    return;
  }

  ipcRenderer.send('tcp-send', text);
  // Show own message immediately
  addMessage(username, text, true);
  msgInput.value = '';
}

sendBtn.addEventListener('click', sendMessage);
msgInput.addEventListener('keydown', (e) => {
  if (e.key === 'Enter') sendMessage();
});

// ── Disconnect button ─────────────────────────────────────────────────
document.getElementById('disconnect-btn').addEventListener('click', () => {
  ipcRenderer.send('tcp-disconnect');
  window.location.href = 'index.html';
});

// ── Welcome message ───────────────────────────────────────────────────
addSystemMessage(`⛧ Connected to ${host}:${port} — Welcome, ${username}`);
msgInput.focus();
