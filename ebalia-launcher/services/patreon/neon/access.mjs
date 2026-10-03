import {createHash, createHmac, timingSafeEqual} from 'node:crypto';
export const digest = value => createHash('sha256').update(value).digest('hex');
export const guest = () => ({active:false, tiers:[], tier_details:[], name:'', tier:''});
export function membership(doc, campaign) {
  if (doc.data?.type !== 'user') throw new Error('Invalid identity');
  const result = {...guest(), name:doc.data.attributes?.full_name || ''};
  const owned = new Set((doc.data.relationships?.memberships?.data || []).map(m=>m.id));
  const included = doc.included || [];
  for (const m of included) {
    if (m.type !== 'member' || !owned.has(m.id) || m.relationships?.campaign?.data?.id !== campaign ||
        m.attributes?.patron_status !== 'active_patron' || !(m.attributes?.currently_entitled_amount_cents > 0)) continue;
    result.active = true;
    result.tiers.push(...(m.relationships?.currently_entitled_tiers?.data || []).map(t=>t.id));
  }
  result.tiers = [...new Set(result.tiers.map(String))];
  result.tier_details = included.filter(t=>t.type==='tier' && result.tiers.includes(String(t.id)))
    .map(t=>({id:String(t.id),title:t.attributes?.title || '',amount_cents:Number(t.attributes?.amount_cents)||0}))
    .sort((a,b)=>b.amount_cents-a.amount_cents || a.id.localeCompare(b.id));
  result.tier = result.tier_details.map(t=>t.title).filter(Boolean).join(' · ');
  // The amount paid is never used to guess a tier: discounts, taxes and
  // custom pledges do not change the entitled tier IDs returned by Patreon.
  return result;
}
export function visiblePosts(doc, identity, now = Date.now()) {
  return (doc.data || []).filter(item=> {
    const a = item.attributes || {};
    if (item.type !== 'post' || !a.published_at || !/([zZ]|[+-]\d\d:\d\d)$/.test(a.published_at) || !Number.isFinite(Date.parse(a.published_at)) || Date.parse(a.published_at)>now) return false;
    let url; try { url=new URL(a.url, 'https://www.patreon.com'); } catch { return false; }
    if (!a.url || url.protocol!=='https:' || !['patreon.com','www.patreon.com'].includes(url.hostname) || url.username || url.password) return false;
    // is_paid describes per-post billing, not audience permissions. Missing or
    // empty tier metadata cannot safely authorize a private post.
    return a.is_public===true || (a.is_public===false && identity.active && Array.isArray(a.tiers) &&
      a.tiers.some(t=>identity.tiers?.includes(String(typeof t==='object' ? t?.id : t))));
  }).map(({id,attributes:a})=>({id,title:a.title||'',content:a.content||'',url:new URL(a.url,'https://www.patreon.com').href,date:a.published_at,is_public:a.is_public===true}))
    .sort((a,b)=>Date.parse(b.date)-Date.parse(a.date));
}
export function validWebhook(secret, body, signature) {
  if (!secret || !/^[a-f0-9]{32}$/i.test(signature)) return false;
  return timingSafeEqual(createHmac('md5',secret).update(body).digest(),Buffer.from(signature,'hex'));
}
export const events = ['posts:publish','posts:update','posts:delete','members:create','members:update','members:delete','members:pledge:create','members:pledge:update','members:pledge:delete'];
