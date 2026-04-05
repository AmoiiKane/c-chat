const { ipcRenderer } = require('electron');
const path = require('path');

let avatarPath = null;

// Avatar picker
document.getElementById('pick-avatar-btn').addEventListener('click', async () => {
  const filePath = await ipcRenderer.invoke('pick-avatar');
  if (!filePath) return;
  avatarPath = filePath;
  const preview = document.getElementById('avatar-preview');
  preview.innerHTML = `<img src="file://${filePath}" alt="avatar" />`;
  document.getElementById('avatar-name').textContent = path.basename(filePath);
});

// Connect
document.getElementById('connect-btn').addEventListener('click', connect);
document.addEventListener('keydown', (e) => {
  if (e.key === 'Enter') connect();
});

async function connect() {
  const username = document.getElementById('username').value.trim();
  const host     = document.getElementById('host').value.trim() || '127.0.0.1';
  const port     = parseInt(document.getElementById('port').value.trim()) || 8080;
  const errEl    = document.getElementById('error-msg');

  if (!username) { errEl.textContent = '⚠ A callsign is required.'; return; }

  errEl.textContent = '';
  document.querySelector('.btn-text').style.display = 'none';
  document.querySelector('.btn-loader').style.display = 'inline';
  document.getElementById('connect-btn').disabled = true;

  try {
    const result = await ipcRenderer.invoke('tcp-connect', { host, port, username });
    if (result.ok) {
      // Store session data and navigate to chat
      sessionStorage.setItem('username', username);
      sessionStorage.setItem('host', host);
      sessionStorage.setItem('port', port);
      sessionStorage.setItem('avatar', avatarPath || '');
      window.location.href = 'chat.html';
    }
  } catch (err) {
    errEl.textContent = `⚠ ${err.error || 'Connection failed.'}`;
    document.querySelector('.btn-text').style.display = 'inline';
    document.querySelector('.btn-loader').style.display = 'none';
    document.getElementById('connect-btn').disabled = false;
  }
}
