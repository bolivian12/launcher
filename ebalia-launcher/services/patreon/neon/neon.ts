import {defineConfig} from '@neon/config/v1';
const keys=['CLIENT_ID','CLIENT_SECRET','CAMPAIGN_ID','CREATOR_TOKEN','CREATOR_REFRESH','WEBHOOK_SECRET','PUBLIC_URL'];
export default defineConfig({functions:{patreon:{name:'EBALIA Patreon',source:'./index.mjs',env:Object.fromEntries(keys.map(k=>['EBALIA_PATREON_'+k,process.env['EBALIA_PATREON_'+k]||'']))}}});
