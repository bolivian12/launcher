(() => {
  const status = document.querySelector('#release-status');
  const buttons = [...document.querySelectorAll('[data-platform]')];
  const dialog = document.querySelector('#download-dialog');
  const file = document.querySelector('#download-file');
  const retry = document.querySelector('#download-retry');
  const close = document.querySelector('#download-close');
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
    status.textContent = t(state, { count: assets.size, version });
    buttons.forEach(button => { button.textContent = t('availability'); });
    close.setAttribute('aria-label', t('close'));
    retry.textContent = t(busy ? 'checking' : 'retry'); retry.disabled = busy;
    const notes = document.querySelector('[data-release-notes]');
    if(current) notes.href=current.url;
    if (!selected) return;
    const asset = assets.get(selected);
    window.renderLinuxGuide?.(selected, version, current?.tag || 'v1.1.0');
    document.querySelector('#download-title').textContent = names[selected];
    document.querySelector('#download-message').textContent = t(asset ? 'panelReady' : busy ? 'checking' : state === 'unavailable' ? 'panelError' : 'panelPending');
    file.hidden = !asset;
    if(asset) {
      file.href=asset.url;file.setAttribute('download',asset.name);
      file.textContent=asset.size ? t('download',{size:number(asset.size/1048576)}) : `${t('navDownload')} ↓`;
    } else file.removeAttribute('href');
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
        for(const platform of Object.keys(names)) {
          if(next.has(platform))continue;
          const candidates=release.files.filter(a=>typeof a.name==='string'&&a.name.toLowerCase().includes(platform)&&/\.(exe|zip|dmg|tar\.gz|appimage)$/i.test(a.name));
          const asset=(platform==='windows'?candidates.find(a=>/\.exe$/i.test(a.name)):null)||candidates[0];
          if(!asset)continue;
          const url=asset.browser_download_url||asset.direct_asset_url||asset.url;
          // GitGud can mirror a GitHub package, but arbitrary external destinations are rejected.
          if(!trusted(url,release.source,true)&&!trusted(url,'github',true))continue;
          next.set(platform,{name:asset.name,url,size:Number.isFinite(asset.size)&&asset.size>0?asset.size:0});
        }
      }
      current=latest;version=latest.version.join('.');assets=next;state=assets.size?'ready':'preparing';
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
