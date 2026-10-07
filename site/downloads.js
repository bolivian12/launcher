(() => {
  const status = document.querySelector('#release-status');
  const buttons = [...document.querySelectorAll('[data-platform]')];
  const dialog = document.querySelector('#download-dialog');
  const options = document.querySelector('#download-options');
  const retry = document.querySelector('#download-retry');
  const close = document.querySelector('#download-close');
  const hash = document.querySelector('#download-hash');
  // Packages offered per system, in order. Older releases fall back to their single package.
  const kinds = {
    windows: [['Installer', /-setup\.exe$/i, 'Installer'], ['Portable', /-portable\.zip$/i, 'Portable']],
    macos: [['MacArm', /-arm64\.dmg$/i, 'MacArm'], ['MacIntel', /-x64\.dmg$/i, 'MacIntel'], ['MacPortableArm', /-arm64-portable\.zip$/i, 'MacPortable'], ['MacPortableIntel', /-x64-portable\.zip$/i, 'MacPortable']],
    linux: [['Appimage', /\.appimage$/i, 'Appimage'], ['LinuxPortable', /-portable\.tar\.gz$/i, 'LinuxPortable']]
  };
  let copied = null;
  const names = { windows: 'Windows', macos: 'macOS', linux: 'Linux' };
  let state = 'checking', selected = null, busy = false, version = '1.1.0', current = null;
  let assets = new Map();
  const parse = tag => /^v?(\d+)\.(\d+)\.(\d+)$/.exec(tag || '')?.slice(1).map(Number);
  const compare = (a,b) => { for(let i=0;i<3;i++) if(a[i]!==b[i]) return a[i]-b[i]; return 0; };
  function trusted(raw, source, download) {
    try {
      const u = new URL(raw);
      if(u.protocol !== 'https:' || u.username || u.password) return false;
      if(source === 'github') return u.hostname === 'github.com' && ['/ebalia-real/launcher/releases/', '/bolivian12/launcher/releases/'].some(prefix => u.pathname.startsWith(prefix + (download ? 'download/' : 'tag/')));
      return u.hostname === 'gitgud.io' && (u.pathname.startsWith('/castigarse/launcher/-/') || (download && u.pathname.startsWith('/api/v4/projects/51367/packages/')));
    } catch { return false; }
  }
  function render() {
    const { t, number } = window.ebaliaI18n;
    document.querySelectorAll('[data-release-version]').forEach(el => {el.textContent=version;});
    status.textContent = t(state, { count: [...assets.values()].reduce((n, list) => n + list.length, 0), version });
    buttons.forEach(button => { button.textContent = t('availability'); });
    close.setAttribute('aria-label', t('close'));
    retry.textContent = t(busy ? 'checking' : 'retry'); retry.disabled = busy;
    const notes = document.querySelector('[data-release-notes]');
    if(current) notes.href=current.url;
    if (!selected) return;
    const list = assets.get(selected) || [];
    window.renderLinuxGuide?.(selected, version, current?.tag || 'v1.1.0');
    document.querySelector('#download-title').textContent = names[selected];
    document.querySelector('#download-message').textContent = t(list.length ? 'panelReady' : busy ? 'checking' : state === 'unavailable' ? 'panelError' : 'panelPending');
    options.replaceChildren(...list.map(asset => {
      const card = document.createElement('article'); card.className = 'download-option';
      const title = document.createElement('h3'); title.textContent = `${t('kind' + asset.kind)} · ${asset.name.slice(asset.name.indexOf('.'))}`;
      const help = document.createElement('p'); help.textContent = asset.help ? t('kind' + asset.help + 'Help') : '';
      const link = document.createElement('a'); link.className = 'primary'; link.href = asset.url; link.setAttribute('download', asset.name);
      link.textContent = asset.size ? t('download', {size: number(asset.size / 1048576)}) : `${t('navDownload')} ↓`;
      const head = document.createElement('div'); head.className = 'hash-head';
      const label = document.createElement('span'); label.className = 'eyebrow'; label.textContent = 'SHA-256';
      const copy = document.createElement('button'); copy.type = 'button'; copy.hidden = !asset.sha;
      copy.textContent = t(copied === asset.name ? 'hashCopied' : 'hashCopy');
      copy.addEventListener('click', async () => {
        try { await navigator.clipboard.writeText(asset.sha); copied = asset.name; render(); setTimeout(() => { copied = null; render(); }, 1600); } catch {}
      });
      head.append(label, copy);
      const code = document.createElement('code'); code.textContent = asset.sha || t('hashPending');
      card.append(title, help, link, head, code);
      return card;
    }));
    hash.hidden = !list.length;
    if(list.length) {
      document.querySelector('#download-hash-help').textContent = t('hashHelp');
      const name = list[0].name;
      document.querySelector('#download-hash-command').textContent = {windows: `certutil -hashfile ${name} SHA256`, macos: `shasum -a 256 ${name}`, linux: `sha256sum ${name}`}[selected];
      const all = document.querySelector('#download-hash-all'); all.hidden = !list[0].sums; if(list[0].sums) all.href = list[0].sums;
      all.textContent = `${t('hashAll')} · SHA256SUMS.txt ↗`;
    }
  }
  // GitHub reports a SHA-256 per asset; otherwise read the release's SHA256SUMS.txt.
  async function fillHashes(map) {
    const missing = [...map.values()].flat().filter(a => !a.sha);
    const source = missing.find(a => a.sumsApi || a.sums);
    if(!source) return;
    for(const [url, headers] of [[source.sumsApi, {Accept: 'application/octet-stream'}], [source.sums, {}]]) {
      if(!url) continue;
      try {
        const response = await fetch(url, {headers, cache: 'no-store', signal: AbortSignal.timeout(12000)});
        if(!response.ok) continue;
        const text = await response.text();
        const table = new Map(text.split('\n').map(l => /^([0-9a-f]{64})\s+\*?(.+)$/i.exec(l.trim())).filter(Boolean).map(m => [m[2], m[1].toLowerCase()]));
        for(const asset of missing) if(table.has(asset.name)) asset.sha = table.get(asset.name);
        return;
      } catch {}
    }
  }
  async function releases(source) {
    const endpoint = source === 'github' ? 'https://api.github.com/repos/ebalia-real/launcher/releases?per_page=30' : 'https://gitgud.io/api/v4/projects/51367/releases?per_page=30';
    const response=await fetch(endpoint,{cache:'no-store',signal:AbortSignal.timeout(12000)});
    if(!response.ok) throw new Error('Release service unavailable');
    const data=await response.json();if(!Array.isArray(data))throw new Error('Invalid release response');
    return data.filter(r=>parse(r.tag_name)&&!r.draft&&!r.prerelease&&!r.upcoming_release&&(!r.released_at||Date.parse(r.released_at)<=Date.now())).map(r=>({
      version:parse(r.tag_name),tag:r.tag_name,source,
      url:source==='github'?r.html_url:r._links?.self,
      files:source==='github'?r.assets||[]:r.assets?.links||[]
    })).filter(r=>trusted(r.url,source,false));
  }
  async function check() {
    if(busy)return;busy=true;if(!current)state='checking';render();
    try {
      const results=await Promise.allSettled([releases('github'),releases('gitgud')]);
      const candidates=results.flatMap(r=>r.status==='fulfilled'?r.value:[]).sort((a,b)=>compare(b.version,a.version));
      const latest=candidates[0];
      if(!latest){state=current?(assets.size?'ready':'preparing'):results.every(r=>r.status==='rejected')?'unavailable':'preparing';return;}
      if(current&&compare(latest.version,current.version)<0)return;
      const next = new Map();
      for(const release of candidates.filter(r=>compare(r.version,latest.version)===0)) {
        const sumsAsset = release.files.find(a=>a.name==='SHA256SUMS.txt');
        const sumsUrl = sumsAsset && (sumsAsset.browser_download_url||sumsAsset.direct_asset_url||sumsAsset.url);
        const sums = sumsUrl && (trusted(sumsUrl,release.source,true)||trusted(sumsUrl,'github',true)) ? sumsUrl : null;
        const sumsApi = release.source==='github' && /^https:\/\/api\.github\.com\/repos\/(ebalia-real|bolivian12)\/launcher\/releases\/assets\/\d+$/.test(sumsAsset?.url||'') ? sumsAsset.url : null;
        for(const platform of Object.keys(names)) {
          if(next.has(platform))continue;
          const files=release.files.filter(a=>typeof a.name==='string'&&a.name.toLowerCase().includes(platform)&&!a.name.endsWith('-update.zip')&&/\.(exe|zip|dmg|tar\.gz|appimage)$/i.test(a.name));
          const pick=[];
          for(const [kind,pattern,help] of kinds[platform]){const asset=files.find(a=>pattern.test(a.name));if(asset)pick.push([asset,kind,help]);}
          if(!pick.length&&files.length)pick.push([files.find(a=>/\.exe$/i.test(a.name))||files[0],'Package','']);
          const list=[];
          for(const [asset,kind,help] of pick) {
            const url=asset.browser_download_url||asset.direct_asset_url||asset.url;
            // GitGud can mirror a GitHub package, but arbitrary external destinations are rejected.
            if(!trusted(url,release.source,true)&&!trusted(url,'github',true))continue;
            const digest=/^sha256:[0-9a-f]{64}$/i.test(asset.digest||'')?asset.digest.slice(7).toLowerCase():'';
            list.push({kind,help,name:asset.name,url,size:Number.isFinite(asset.size)&&asset.size>0?asset.size:0,sha:digest,sums,sumsApi});
          }
          if(list.length)next.set(platform,list);
        }
      }
      current=latest;version=latest.version.join('.');assets=next;state=assets.size?'ready':'preparing';
      render();await fillHashes(next);
    } catch {state='unavailable';}
    finally {busy=false;render();}
  }
  buttons.forEach(button=>button.addEventListener('click',()=>{selected=button.dataset.platform;render();dialog.showModal();}));
  close.addEventListener('click',()=>dialog.close());
  dialog.addEventListener('click',event=>{const b=dialog.getBoundingClientRect();if(event.target===dialog&&(event.clientX<b.left||event.clientX>b.right||event.clientY<b.top||event.clientY>b.bottom))dialog.close();});
  retry.addEventListener('click',check);
  window.addEventListener('ebalia-language-change',render);
  document.addEventListener('visibilitychange',()=>{if(!document.hidden)check();});
  window.addEventListener('online',check);
  setInterval(()=>{if(!document.hidden)check();},60000);
  check();
})();
