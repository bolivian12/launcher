import {test} from 'node:test';
import assert from 'node:assert/strict';
import {createHmac} from 'node:crypto';
import {membership, visiblePosts, validWebhook} from './access.mjs';
const identity=(amount=500,campaign='42',status='active_patron')=>({data:{type:'user',relationships:{memberships:{data:[{id:'own'}]}}},included:[{type:'member',id:'own',attributes:{currently_entitled_amount_cents:amount,patron_status:status},relationships:{campaign:{data:{id:campaign}},currently_entitled_tiers:{data:[{id:'gold'}]}}}]});
const post=(id,attributes)=>({type:'post',id,attributes:{title:id,content:'secret',url:'/posts/'+id,published_at:'2026-01-01T00:00:00Z',...attributes}});
test('paid identity requires current paid membership in the correct campaign',()=>{
 assert.equal(membership(identity(),'42').active,true);
 for(const doc of [identity(0),identity(500,'99'),identity(500,'42','former_patron')]) assert.equal(membership(doc,'42').active,false);
 const doc=identity();doc.data.relationships.memberships.data=[];assert.equal(membership(doc,'42').active,false);
});
test('private posts fail closed; billing flag is not an access grant',()=>{
 const data=[post('public',{is_public:true}),post('gold',{is_public:false,tiers:['gold']}),post('silver',{is_public:false,tiers:['silver']}),post('billing',{is_public:false,is_paid:true,tiers:[]}),post('missing',{tiers:['gold']})];
 assert.deepEqual(visiblePosts({data},{active:false}).map(p=>p.id),['public']);
 assert.deepEqual(visiblePosts({data},{active:true,tiers:['gold']}).map(p=>p.id),['public','gold']);
});
test('future, malformed and external posts are rejected',()=>{
 for(const extra of [{published_at:'2999-01-01T00:00:00Z'},{published_at:'invalid'},{published_at:'2026-01-01'},{url:'https://evil.test/post'},{url:'https://user:password@patreon.com/post'}]) assert.deepEqual(visiblePosts({data:[post('bad',{is_public:true,...extra})]},{}),[]);
});
test('webhook verification binds signature to secret and exact body',()=>{
 const body=Buffer.from('{"data":{}}'),sig=createHmac('md5','test').update(body).digest('hex');
 assert.equal(validWebhook('test',body,sig),true);
 for(const [secret,payload,signature] of [['',body,sig],['wrong',body,sig],['test',Buffer.from('{}'),sig],['test',body,'oops']]) assert.equal(validWebhook(secret,payload,signature),false);
});

const ebaliaTiers=[['22876765','Exclusive Chad',250],['22870809','Explicit Chad',550],['23037742','SENIOR CHAD',750]];
function ebaliaIdentity(tier,status='active_patron',amount=1){
 const doc=identity(amount,'12138678',status);
 doc.included[0].relationships.currently_entitled_tiers.data=[{id:tier[0]}];
 doc.included.push(...ebaliaTiers.map(([id,title,amount_cents])=>({type:'tier',id,attributes:{title,amount_cents}})));
 return doc;
}
test('each real EBALIA tier is identified by ID, never by payment amount',()=>{
 const posts={data:ebaliaTiers.map(([id,title])=>post(id,{is_public:false,tiers:[id],title}))};
 for(const tier of ebaliaTiers){
  const who=membership(ebaliaIdentity(tier),'12138678');
  assert.equal(who.active,true);assert.equal(who.tier,tier[1]);assert.deepEqual(who.tiers,[tier[0]]);
  assert.deepEqual(who.tier_details,[{id:tier[0],title:tier[1],amount_cents:tier[2]}]);
  assert.deepEqual(visiblePosts(posts,who).map(p=>p.id),[tier[0]]);
 }
});
test('upgrade, downgrade, expiry and free membership replace previous entitlements',()=>{
 for(const tier of [ebaliaTiers[0],ebaliaTiers[2],ebaliaTiers[1]]){
  assert.deepEqual(membership(ebaliaIdentity(tier),'12138678').tiers,[tier[0]]);
 }
 for(const doc of [ebaliaIdentity(ebaliaTiers[2],'former_patron'),ebaliaIdentity(ebaliaTiers[2],'declined_patron'),ebaliaIdentity(ebaliaTiers[0],'active_patron',0)]){
  const who=membership(doc,'12138678');assert.equal(who.active,false);assert.deepEqual(who.tiers,[]);assert.equal(who.tier,'');
 }
});
test('renaming a tier preserves ID based access; multiple entitlements preserve each ID',()=>{
 const doc=ebaliaIdentity(ebaliaTiers[0]);doc.included[1].attributes.title='Renamed tier';
 doc.included[0].relationships.currently_entitled_tiers.data.push({id:ebaliaTiers[2][0]});
 const who=membership(doc,'12138678');assert.deepEqual(who.tiers,[ebaliaTiers[0][0],ebaliaTiers[2][0]]);
 assert.equal(who.tier,'SENIOR CHAD · Renamed tier');
});
test('a newly created tier requires no code or price mapping changes',()=>{
 const doc=ebaliaIdentity(ebaliaTiers[0]);
 doc.included[0].relationships.currently_entitled_tiers.data=[{id:'future-tier-987'}];
 doc.included.push({type:'tier',id:'future-tier-987',attributes:{title:'New future membership',amount_cents:1200}});
 const who=membership(doc,'12138678');
 assert.equal(who.tier,'New future membership');assert.deepEqual(who.tiers,['future-tier-987']);
 assert.equal(visiblePosts({data:[post('new-exclusive',{is_public:false,tiers:['future-tier-987']})]},who).length,1);
});
