import pg from 'pg';
import {attachDatabasePool} from '@neon/functions';
import {randomBytes} from 'node:crypto';
import {setTimeout as delay} from 'node:timers/promises';
import {digest, guest, membership, visiblePosts, validWebhook, events} from './access.mjs';
const pool = new pg.Pool({connectionString:process.env.DATABASE_URL,max:5,connectionTimeoutMillis:10000,idleTimeoutMillis:10000});
attachDatabasePool(pool);
const env = key => process.env['EBALIA_PATREON_'+key] || '';
const headers = {'Cache-Control':'no-store','Referrer-Policy':'no-referrer','X-Content-Type-Options':'nosniff'};
const json = (value,status=200)=>Response.json(value,{status,headers});
class Failure extends Error {constructor(status){super('Request failed');this.status=status;}}
let initialized;
async function initialize() {
  if (!initialized) initialized=(async()=>{
    const c=await pool.connect();
    try {
      await c.query('BEGIN');
      await c.query('SELECT pg_advisory_xact_lock(172834092)');
      await c.query('CREATE TABLE IF NOT EXISTS sessions (token TEXT PRIMARY KEY, state TEXT UNIQUE, expires DOUBLE PRECISION, access TEXT, refresh TEXT)');
      await c.query('CREATE TABLE IF NOT EXISTS creator (id INTEGER PRIMARY KEY, access TEXT, refresh TEXT)');
      await c.query('CREATE TABLE IF NOT EXISTS ebalia_revision (id INTEGER PRIMARY KEY, revision BIGINT NOT NULL)');
      await c.query('INSERT INTO ebalia_revision VALUES (1,0) ON CONFLICT DO NOTHING');
      await c.query('COMMIT');
    } catch(e) {await c.query('ROLLBACK');throw e;} finally {c.release();}
  })().catch(e=>{initialized=null;throw e;});
  return initialized;
}
const version=async()=>String((await pool.query('SELECT revision FROM ebalia_revision WHERE id=1')).rows[0].revision);
async function remote(url,options={}) {
  const response=await fetch(url,{...options,signal:AbortSignal.timeout(20000),redirect:'error'});
  if (!response.ok) throw new Failure(response.status===401?401:502);
  return response.json();
}
const api=(path,token)=>remote('https://www.patreon.com/api/oauth2/v2'+path,{headers:{Authorization:'Bearer '+token}});
const oauth=form=>remote('https://www.patreon.com/api/oauth2/token',{method:'POST',body:new URLSearchParams({...form,client_id:env('CLIENT_ID'),client_secret:env('CLIENT_SECRET')})});
function requireConfig() {
  if (!env('CLIENT_ID') || !env('CLIENT_SECRET') || !/^\d+$/.test(env('CAMPAIGN_ID')) || !env('PUBLIC_URL').startsWith('https://')) throw new Failure(503);
}
async function login() {
  requireConfig();
  const token=randomBytes(32).toString('base64url'),state=randomBytes(32).toString('base64url');
  await pool.query('DELETE FROM sessions WHERE expires < $1',[Date.now()/1000]);
  await pool.query('INSERT INTO sessions VALUES ($1,$2,$3,$4,$5)',[digest(token),digest(state),Date.now()/1000+300,'','']);
  return {session:token,auth_url:'https://www.patreon.com/oauth2/authorize?'+new URLSearchParams({response_type:'code',client_id:env('CLIENT_ID'),redirect_uri:env('PUBLIC_URL')+'/callback',scope:'identity identity.memberships',state})};
}
async function callback(query) {
  requireConfig();
  const state=query.get('state'),code=query.get('code');
  if (!state || !code) throw new Failure(401);
  // Atomic consumption prevents replay across concurrent function instances.
  const row=(await pool.query('UPDATE sessions SET state=NULL WHERE state=$1 AND expires>$2 RETURNING token',[digest(state),Date.now()/1000])).rows[0];
  if (!row) throw new Failure(401);
  const tokens=await oauth({grant_type:'authorization_code',code,redirect_uri:env('PUBLIC_URL')+'/callback'});
  if (!tokens.access_token) throw new Failure(502);
  await pool.query('UPDATE sessions SET access=$1,refresh=$2,expires=$3 WHERE token=$4',[tokens.access_token,tokens.refresh_token||'',Date.now()/1000+30*86400,row.token]);
  return {message:'Volvé a EBALIA Launcher para completar la conexión.'};
}
async function withTokens(token,read) {
  const c=await pool.connect();
  try {
    await c.query('BEGIN');
    // Serialize refresh-token rotation across all isolates for this identity.
    await c.query('SELECT pg_advisory_xact_lock(hashtextextended($1,0))',[token?'session:'+digest(token):'creator']);
    const row=token ? (await c.query('SELECT access,refresh FROM sessions WHERE token=$1 AND expires>$2',[digest(token),Date.now()/1000])).rows[0] :
      (await c.query('SELECT access,refresh FROM creator WHERE id=1')).rows[0] || {access:env('CREATOR_TOKEN'),refresh:env('CREATOR_REFRESH')};
    if (!row) throw new Failure(401);
    if (!row.access) { if (token) return {pending:true}; throw new Failure(503); }
    let result;
    try {result=await read(row.access);} catch(e) {
      if (e.status!==401 || !row.refresh) throw e;
      const tokens=await oauth({grant_type:'refresh_token',refresh_token:row.refresh});
      if (!tokens.access_token) throw new Failure(502);
      if (token) await c.query('UPDATE sessions SET access=$1,refresh=$2 WHERE token=$3',[tokens.access_token,tokens.refresh_token||row.refresh,digest(token)]);
      else await c.query('INSERT INTO creator VALUES (1,$1,$2) ON CONFLICT (id) DO UPDATE SET access=excluded.access,refresh=excluded.refresh',[tokens.access_token,tokens.refresh_token||row.refresh]);
      // Persist rotated credentials even if the following Patreon request fails.
      await c.query('COMMIT');
      return await read(tokens.access_token);
    }
    await c.query('COMMIT');
    return result;
  } finally {await c.query('ROLLBACK').catch(()=>{});c.release();}
}
let cached;
async function posts(revision) {
  if (cached && cached.revision===revision && Date.now()-cached.at<30000) return cached.doc;
  const query=new URLSearchParams({'fields[post]':'title,content,url,published_at,is_public,is_paid,tiers','sort':'-published_at','page[count]':'100'});
  const doc=await withTokens('',token=>api('/campaigns/'+env('CAMPAIGN_ID')+'/posts?'+query,token));
  if (!Array.isArray(doc.data)) throw new Failure(502);
  cached={doc,revision,at:Date.now()};
  return doc;
}
let tierCache;
async function campaignTiers(revision) {
  if(tierCache && tierCache.revision===revision && Date.now()-tierCache.at<60000) return tierCache.tiers;
  const query=new URLSearchParams({include:'tiers','fields[tier]':'title,amount_cents,published'});
  const doc=await withTokens('',token=>api('/campaigns/'+env('CAMPAIGN_ID')+'?'+query,token));
  const tiers=(doc.included||[]).filter(t=>t.type==='tier' && t.attributes?.published===true)
    .map(t=>({id:String(t.id),title:t.attributes?.title||'',amount_cents:Number(t.attributes?.amount_cents)||0}))
    .sort((a,b)=>a.amount_cents-b.amount_cents || a.id.localeCompare(b.id));
  tierCache={revision,at:Date.now(),tiers};return tiers;
}
async function feed(token) {
  requireConfig();
  const revision=await version();
  const query=new URLSearchParams({include:'memberships.currently_entitled_tiers,memberships.campaign','fields[user]':'full_name','fields[member]':'patron_status,currently_entitled_amount_cents','fields[tier]':'title,amount_cents'});
  const doc=token?await withTokens(token,t=>api('/identity?'+query,t)):null;
  if (doc?.pending) return {pending:true,posts:[]};
  const identity=doc?membership(doc,env('CAMPAIGN_ID')):guest();
  return {identity,tiers:await campaignTiers(revision),posts:visiblePosts(await posts(revision),identity),revision,updated_at:new Date().toISOString()};
}
async function webhook(request) {
  const reader=request.body?.getReader();
  if (!reader) throw new Failure(413);
  let size=0;const chunks=[];
  while(true) {const {done,value}=await reader.read();if(done)break;size+=value.length;if(size>1048576){await reader.cancel();throw new Failure(413);}chunks.push(value);}
  const body=Buffer.concat(chunks);
  if (!validWebhook(env('WEBHOOK_SECRET'),body,request.headers.get('x-patreon-signature')||'')) throw new Failure(401);
  if (!events.includes(request.headers.get('x-patreon-event'))) throw new Failure(400);
  const campaign=JSON.parse(body.toString()).data?.relationships?.campaign?.data?.id;
  if (campaign && campaign!==env('CAMPAIGN_ID')) throw new Failure(401);
  await pool.query('UPDATE ebalia_revision SET revision=revision+1 WHERE id=1');
  return {ok:true};
}
export default {async fetch(request) {
  try {
    await initialize();
    const url=new URL(request.url), route=request.method+' '+url.pathname;
    const auth=request.headers.get('authorization')||'',token=auth.startsWith('Bearer ')?auth.slice(7):'';
    if (route==='GET /health') return json({ok:true,patreon_configured:Boolean(env('CAMPAIGN_ID') && env('CREATOR_TOKEN') && env('PUBLIC_URL'))});
    if (route==='POST /v1/login') return json(await login());
    if (route==='GET /callback') return json(await callback(url.searchParams));
    if (route==='GET /v1/feed') return json(await feed(token));
    if (route==='POST /v1/logout') {await pool.query('DELETE FROM sessions WHERE token=$1',[digest(token)]);return json({ok:true});}
    if (route==='POST /v1/webhook') return json(await webhook(request));
    if (route==='GET /v1/events') {
      const deadline=Date.now()+25000,after=url.searchParams.get('after');let revision=await version();
      while(after===revision && Date.now()<deadline && !request.signal.aborted) {
        await delay(1000,undefined,{signal:request.signal});revision=await version();
      }
      return json({revision});
    }
    return json({error:'Not found'},404);
  } catch(e) {
    // Never expose provider diagnostics, connection strings, codes or tokens.
    const status=e instanceof Failure?e.status:503;
    return json({error:status===401?'Session expired or callback invalid':'Patreon unavailable or configuration incomplete'},status);
  }
}};
