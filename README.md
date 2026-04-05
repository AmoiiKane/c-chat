# c-chat 💬

A lightweight terminal chat application written in C — TCP sockets, POSIX threads, real-time messaging.
Comes with a **gothic-themed desktop GUI** built in Electron, usable on Linux and Windows.

[![C](https://img.shields.io/badge/C-00599C?style=for-the-badge&logo=c&logoColor=white)](https://github.com/AmoiiKane/c-chat)
[![License](https://img.shields.io/badge/License-MIT-green?style=for-the-badge)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Windows-lightgrey?style=for-the-badge)](https://github.com/AmoiiKane/c-chat)
[![Electron](https://img.shields.io/badge/GUI-Electron-47848F?style=for-the-badge&logo=electron&logoColor=white)](https://github.com/AmoiiKane/c-chat)

---

## Features

- 🔌 **TCP socket server** — handles up to 32 simultaneous clients
- 🧵 **Multithreaded** — one `pthread` per client, non-blocking broadcasts
- 📡 **Real-time messaging** — messages instantly broadcast to all connected users
- 👤 **Usernames** — clients pick a callsign on connect
- 🖼️ **Profile pictures** — GUI clients can set a custom avatar image
- 🚪 **Join/leave notifications** — the room announces when users connect or disconnect
- 🛑 **Graceful shutdown** — `Ctrl+C` cleans up all connections
- 🎨 **Gothic GUI** — dystopian desktop client, frameless window, ANSI-inspired design
- 🌐 **Cross-network** — connect over LAN or Tailscale from anywhere in the world

---

## Architecture

```
GUI Client (Electron)      Terminal Client (C)
       │                          │
       └──────────┬───────────────┘
                  ▼
        Server (C · TCP :8080)
                  │
        broadcast to all clients
```

The **server is a compiled C binary** — it runs on Linux only (or WSL on Windows).
The **GUI client is Electron** — it runs natively on Linux and Windows.
The **terminal client** is a compiled C binary for Linux/macOS.

---

## Project Structure

```
c-chat/
├── src/
│   ├── server.c      # TCP server — pthread per client, broadcast, mutex
│   ├── client.c      # Terminal TCP client — threaded send/receive
│   └── common.h      # Shared constants and buffer sizes
├── gui/
│   ├── main.js       # Electron main process — TCP socket bridge
│   ├── index.html    # Login screen
│   ├── chat.html     # Chat screen
│   ├── style.css     # Gothic theme
│   ├── login.js      # Login logic
│   ├── chat.js       # Chat logic
│   ├── package.json  # Electron dependencies
│   └── assets/
│       └── bg.jpg    # Background wallpaper
├── Makefile
└── README.md
```

---

## Running the Server (Linux)

The server must run on a Linux machine. It does **not** run natively on Windows — use WSL if you're on Windows and want to self-host.

### Build

```bash
git clone https://github.com/AmoiiKane/c-chat.git
cd c-chat
make
```

Requires GCC. Binaries go to `bin/`:

```
bin/server
bin/client
```

### Start

```bash
./bin/server
```

The server listens on `TCP :8080`. Keep this terminal open — it's the hub all clients connect to.

---

## Connecting from Another Machine

### Option A — Same local network (LAN)

Find the server machine's local IP:

```bash
ip a | grep inet
```

Look for something like `192.168.x.x`. That's the address clients use.

### Option B — Over the internet with Tailscale (recommended)

Tailscale creates a private encrypted network between machines. No port forwarding, no public IP needed.

**On the server machine (Linux):**

```bash
# Install Tailscale
curl -fsSL https://tailscale.com/install.sh | sh
sudo tailscale up

# Get your Tailscale IP — share this with your friends
tailscale ip -4
```

It will look like `100.x.x.x`. That's the address everyone uses to connect.

**On each client machine (Windows or Linux):**

1. Download and install Tailscale from [tailscale.com/download](https://tailscale.com/download)
2. Sign in with the **same account** as the server, or get invited via the admin console at [login.tailscale.com](https://login.tailscale.com)
3. Open the GUI client, enter the server's Tailscale IP and port `8080`

> **Inviting someone to your Tailscale network:**
> Go to [login.tailscale.com](https://login.tailscale.com) → Settings → Users → Invite user.
> They install Tailscale, log in, and they're instantly on your private network.

---

## GUI Client Setup

The GUI client works on **Linux and Windows**. It connects to the C server over TCP.

### Prerequisites

- [Node.js](https://nodejs.org) (v18 or later) — [download for Windows](https://nodejs.org/en/download)
- Git — [download for Windows](https://git-scm.com/download/win)

### Linux

```bash
git clone https://github.com/AmoiiKane/c-chat.git
cd c-chat/gui
npm install
npm start
```

### Windows

Open **PowerShell** or **Command Prompt**:

```powershell
git clone https://github.com/AmoiiKane/c-chat.git
cd c-chat\gui
npm install
npm start
```

> If `npm install` fails with a script execution error, run this first in PowerShell (as Administrator):
> ```powershell
> Set-ExecutionPolicy RemoteSigned -Scope CurrentUser
> ```

A gothic desktop window will open. Fill in:

| Field | What to enter |
|-------|--------------|
| **Callsign** | Your username |
| **Server Address** | The server's IP (LAN or Tailscale) |
| **Port** | `8080` (default) |
| **Avatar** | Any image file from your machine |

Click **ENTER THE SECTOR** to connect.

---

## Terminal Client (Linux / macOS only)

```bash
# Connect to localhost
./bin/client

# Connect to a remote server
./bin/client 192.168.1.10

# Custom host and port
./bin/client 192.168.1.10 9090
```

**Commands inside the chat:**

```
/quit    — disconnect from the server
```

---

## Troubleshooting

**"Connection refused" on Windows GUI:**
- Make sure the server is running (`./bin/server` on the Linux machine)
- Double-check the IP address — use the Tailscale IP (`100.x.x.x`), not localhost
- Make sure both machines are on the same Tailscale network

**`npm install` fails on Windows:**
- Make sure Node.js is installed: `node -v` should return a version number
- Run PowerShell as Administrator and set the execution policy (see above)

**`make` not found on Linux:**
- Arch: `sudo pacman -S base-devel`
- Ubuntu/Debian: `sudo apt install build-essential`

**Port already in use:**
```bash
ss -tlnp | grep 8080
kill <PID>
```

---

## How It Works

1. Server listens on `TCP :8080`
2. Each new connection spawns a dedicated `pthread`
3. Incoming messages are broadcast to all other connected clients under a mutex lock
4. `SIGPIPE` is ignored — the server stays alive if a client disconnects abruptly
5. The Electron GUI bridges TCP via Node.js `net` module — no browser, native window

---

## License

MIT — free to use, modify, and distribute.

---

*Built by [AmoiiKane](https://github.com/AmoiiKane)*
