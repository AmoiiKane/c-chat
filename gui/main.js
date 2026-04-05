const { app, BrowserWindow, ipcMain, dialog } = require('electron');
const net = require('net');
const path = require('path');

let mainWindow;
let tcpSocket = null;

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1100,
    height: 700,
    minWidth: 800,
    minHeight: 550,
    frame: false,
    transparent: false,
    backgroundColor: '#0a0a0a',
    webPreferences: {
      nodeIntegration: true,
      contextIsolation: false,
    },
    icon: path.join(__dirname, 'assets', 'bg.jpg'),
  });

  mainWindow.loadFile('index.html');
}

app.whenReady().then(createWindow);

app.on('window-all-closed', () => {
  if (tcpSocket) tcpSocket.destroy();
  app.quit();
});

// Window controls
ipcMain.on('window-close', () => mainWindow.close());
ipcMain.on('window-minimize', () => mainWindow.minimize());
ipcMain.on('window-maximize', () => {
  mainWindow.isMaximized() ? mainWindow.unmaximize() : mainWindow.maximize();
});

// File picker for avatar
ipcMain.handle('pick-avatar', async () => {
  const result = await dialog.showOpenDialog(mainWindow, {
    title: 'Choose your avatar',
    filters: [{ name: 'Images', extensions: ['jpg', 'jpeg', 'png', 'gif', 'webp'] }],
    properties: ['openFile'],
  });
  if (result.canceled || result.filePaths.length === 0) return null;
  return result.filePaths[0];
});

// TCP connect
ipcMain.handle('tcp-connect', (event, { host, port, username }) => {
  return new Promise((resolve, reject) => {
    if (tcpSocket) {
      tcpSocket.destroy();
      tcpSocket = null;
    }

    tcpSocket = new net.Socket();
    tcpSocket.setEncoding('utf8');

    tcpSocket.connect(port, host, () => {
      // Send username as first message (matches server protocol)
      tcpSocket.write(username + '\n');
      resolve({ ok: true });
    });

    tcpSocket.on('data', (data) => {
      mainWindow.webContents.send('tcp-message', data);
    });

    tcpSocket.on('close', () => {
      mainWindow.webContents.send('tcp-disconnected');
    });

    tcpSocket.on('error', (err) => {
      reject({ ok: false, error: err.message });
    });
  });
});

// Send message
ipcMain.on('tcp-send', (event, message) => {
  if (tcpSocket && !tcpSocket.destroyed) {
    tcpSocket.write(message + '\n');
  }
});

// Disconnect
ipcMain.on('tcp-disconnect', () => {
  if (tcpSocket) {
    tcpSocket.write('/quit\n');
    tcpSocket.destroy();
    tcpSocket = null;
  }
});
