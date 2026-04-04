# c-chat 💬

A lightweight chat application written in C — TCP sockets, real-time messaging, and full cross-platform support. Available as both a terminal client and a native GUI client (no browser, no Electron).

![C](https://img.shields.io/badge/C-00599C?style=for-the-badge&logo=c&logoColor=white)
![License](https://img.shields.io/badge/License-MIT-green?style=for-the-badge)
![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey?style=for-the-badge)

---

## Features

- 🔌 **TCP socket server** — handles up to 32 simultaneous clients
- 🧵 **Multithreaded** — one thread per client, non-blocking broadcasts
- 📡 **Real-time messaging** — messages instantly broadcast to all connected users
- 👤 **Usernames** — clients pick a username on connect
- 🚪 **Join/leave notifications** — room announces when users connect or disconnect
- 🛑 **Graceful shutdown** — `Ctrl+C` cleans up all connections
- 🖥️ **GUI client** — native desktop window, no terminal required
- 🪟 **Cross-platform** — runs on Linux, macOS, and Windows natively

---

## Quick Setup (recommended)

These scripts check for all dependencies, fix the project structure, and build everything in one shot.

### Linux / macOS
```bash
chmod +x setup.sh
./setup.sh
```

### Windows
Right-click `setup.bat` → **Run as administrator**

The scripts auto-detect your OS/distro and install any missing packages, then build all binaries. Jump straight to [Usage](#usage) after running them.

---

## Manual Setup / Dependencies

### Terminal client (`server` + `client`)
No extra dependencies — just a C compiler.

| Platform | Compiler  | Notes                         |
|----------|-----------|-------------------------------|
| Linux    | GCC       | `pthread` included in glibc   |
| macOS    | GCC/Clang | `pthread` included            |
| Windows  | MinGW-w64 | Winsock2 linked automatically |

### GUI client (`gui_client`)
Requires GTK3 and WebKitGTK. Only needed if you want the graphical interface.

| Platform | Install command |
|----------|----------------|
| Arch Linux | `sudo pacman -S gtk3 webkit2gtk-4.1` |
| Ubuntu / Debian | `sudo apt install libgtk-3-dev libwebkit2gtk-4.1-dev` |
| Fedora | `sudo dnf install gtk3-devel webkit2gtk4.1-devel` |
| macOS | `brew install gtk+3 webkit2gtk` |
| Windows | WebView2 is bundled with Windows 11 — no extra install needed |

---

## Build

### Linux / macOS
```bash
git clone https://github.com/AmoiiKane/c-chat.git
cd c-chat
make
```

### Arch Linux
```bash
sudo pacman -S gtk3 webkit2gtk-4.1
git clone https://github.com/AmoiiKane/c-chat.git
cd c-chat
make
```

### Windows
Install [MinGW-w64](https://winlibs.com) first, then open Command Prompt:
```cmd
git clone https://github.com/AmoiiKane/c-chat.git
cd c-chat
make
```

For a release build (optimized, no debug symbols):
```bash
make release
```

Binaries are placed in `bin/`:
```
bin/server        (Linux/macOS)
bin/server.exe    (Windows)
bin/client        (Linux/macOS)
bin/client.exe    (Windows)
bin/gui_client    (Linux/macOS — requires GTK + WebKitGTK)
bin/gui_client.exe (Windows — requires WebView2)
```

---

## Usage

### Server

Start the server on any machine (terminal or background):

```bash
# Linux / macOS
./bin/server

# Windows
bin\server.exe
```

The server listens on port `8080` by default. Keep it running — all clients connect to it.

---

### Terminal client

```bash
# Connect to localhost
./bin/client

# Connect to a remote server
./bin/client 192.168.1.10

# Custom host and port
./bin/client 192.168.1.10 9090

# Windows
bin\client.exe 192.168.1.10
bin\client.exe 192.168.1.10 9090
```

**Commands inside the chat:**
```
/quit    — disconnect from the server
```

---

### GUI client

Launch the graphical client — no terminal required:

```bash
# Linux / macOS
./bin/gui_client

# Windows
bin\gui_client.exe
```

A native desktop window opens with a connection screen. Fill in your username, the server's IP address, and port (default `8080`), then click **Connect**. The chat interface appears automatically once connected.

**Using with Tailscale:** If the server is on a Tailscale network, enter the server's Tailscale IP (e.g. `100.x.x.x`) or its MagicDNS hostname in the host field. No other configuration needed.

---

## How it works

```
GUI/Terminal     GUI/Terminal     GUI/Terminal
  Client A ──────┐
  Client B ───────┼──► Server (TCP :8080) ──► broadcast to all other clients
  Client C ───────┘
```

1. Server listens on `TCP :8080`
2. Each new connection spawns a dedicated thread
3. Incoming messages are broadcast to all other connected clients under a mutex lock
4. Server stays alive if a client disconnects abruptly
5. GUI client embeds a WebKitGTK webview — the chat UI is HTML/CSS/JS, the networking is C

---

## Project Structure

```
c-chat/
├── src/
│   ├── server.c       # TCP server, one thread per client, broadcast
│   ├── client.c       # TCP client, threaded send/receive
│   ├── gui_client.c   # Native GUI client (GTK + WebKitGTK)
│   └── common.h       # Cross-platform socket/thread abstraction
├── Makefile           # Auto-detects OS and available GUI deps
├── setup.sh           # Linux/macOS one-shot setup + build script
├── setup.bat          # Windows one-shot setup + build script
└── README.md
```

---

## License

MIT — free to use, modify, and distribute.

---

*Built by [AmoiiKane](https://github.com/AmoiiKane)*
