(() => {
  const status = document.querySelector('#release-status');
  const links = [...document.querySelectorAll('[data-platform]')];
  let state = 'checking';
  let count = 0;
  const assets = new Map();
  function render() {
    const { t, number } = window.ebaliaI18n;
    status.textContent = t(state, { count });
    for (const link of links) {
      const asset = assets.get(link.dataset.platform);
      link.textContent = asset ? t('download', { size: number(asset.size / 1048576) }) : t(state === 'ready' || state === 'preparing' ? 'pending' : 'availability');
      if (asset) link.href = asset.browser_download_url;
    }
  }
  window.addEventListener('ebalia-language-change', render);
  render();
  (async () => {
    try {
      const response = await fetch('https://api.github.com/repos/bolivian12/launcher/releases/tags/v1.0.0', { signal: AbortSignal.timeout(12000) });
      if (!response.ok) throw new Error('Release unavailable');
      const release = await response.json();
      for (const link of links) {
        const asset = (release.assets || []).find(a => a.name.toLowerCase().includes(link.dataset.platform) && /\.(zip|dmg|tar\.gz|appimage)$/i.test(a.name));
        if (!asset || !Number.isFinite(asset.size) || asset.size < 0) continue;
        const url = new URL(asset.browser_download_url);
        if (url.protocol !== 'https:' || url.hostname !== 'github.com' || !url.pathname.startsWith('/bolivian12/launcher/releases/download/')) continue;
        assets.set(link.dataset.platform, asset);
        count++;
      }
      state = count ? 'ready' : 'preparing';
    } catch { state = 'unavailable'; }
    render();
  })();
})();
