(() => {
  const status = document.querySelector('#release-status');
  const buttons = [...document.querySelectorAll('[data-platform]')];
  const dialog = document.querySelector('#download-dialog');
  const file = document.querySelector('#download-file');
  const retry = document.querySelector('#download-retry');
  const close = document.querySelector('#download-close');
  const names = { windows: 'Windows', macos: 'macOS', linux: 'Linux' };
  let state = 'checking', selected = null, busy = false;
  const assets = new Map();
  function render() {
    const { t, number } = window.ebaliaI18n;
    status.textContent = t(state, { count: assets.size });
    buttons.forEach(button => { button.textContent = t('availability'); });
    close.setAttribute('aria-label', t('close'));
    retry.textContent = t(busy ? 'checking' : 'retry');
    retry.disabled = busy;
    if (!selected) return;
    const asset = assets.get(selected);
    document.querySelector('#download-title').textContent = names[selected];
    document.querySelector('#download-message').textContent = t(asset ? 'panelReady' : busy ? 'checking' : state === 'unavailable' ? 'panelError' : 'panelPending');
    file.hidden = !asset;
    if (asset) {
      file.href = asset.browser_download_url;
      file.setAttribute('download', asset.name);
      file.textContent = t('download', { size: number(asset.size / 1048576) });
    } else file.removeAttribute('href');
  }
  async function check() {
    if (busy) return;
    busy = true; state = 'checking'; render();
    try {
      const response = await fetch('https://api.github.com/repos/bolivian12/launcher/releases/tags/v1.0.0', { signal: AbortSignal.timeout(12000) });
      if (response.status === 404) { state = 'preparing'; return; }
      if (!response.ok) throw new Error('Release unavailable');
      const release = await response.json();
      assets.clear();
      for (const platform of Object.keys(names)) {
        const asset = (release.assets || []).find(a => a.name.toLowerCase().includes(platform) && /\.(zip|dmg|tar\.gz|appimage)$/i.test(a.name));
        if (!asset || !Number.isFinite(asset.size) || asset.size < 0) continue;
        const url = new URL(asset.browser_download_url);
        if (url.protocol !== 'https:' || url.hostname !== 'github.com' || !url.pathname.startsWith('/bolivian12/launcher/releases/download/')) continue;
        assets.set(platform, asset);
      }
      state = assets.size ? 'ready' : 'preparing';
    } catch { state = 'unavailable'; }
    finally { busy = false; render(); }
  }
  buttons.forEach(button => button.addEventListener('click', () => {
    selected = button.dataset.platform; render(); dialog.showModal();
  }));
  close.addEventListener('click', () => dialog.close());
  dialog.addEventListener('click', event => {
    const box = dialog.getBoundingClientRect();
    if (event.target === dialog && (event.clientX < box.left || event.clientX > box.right || event.clientY < box.top || event.clientY > box.bottom)) dialog.close();
  });
  retry.addEventListener('click', check);
  window.addEventListener('ebalia-language-change', render);
  check();
})();
