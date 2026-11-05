import assert from 'node:assert/strict';
import {authorizeReward} from './game/rewards.js';

let calls=0;
const forbidden=async()=>{++calls;throw Error('Offline path contacted SDK')};
for(const kind of ['perfect','endless'])await authorizeReward(kind,{bypassBilibiliChecks:true},forbidden);
assert.equal(calls,0);
await assert.rejects(authorizeReward('perfect',{bypassBilibiliChecks:false,perfectSaveRequirement:'coin',rewardVideoBvids:[]},forbidden),/BV/);
assert.equal(calls,0);
const config={bypassBilibiliChecks:false,perfectSaveRequirement:'coin',rewardVideoBvids:['BVtestA','BVtestB']};
await authorizeReward('perfect',config,async()=>({getVideoUserActions:async req=>{
 assert.deepEqual(req.videos,[{bvid:'BVtestA'},{bvid:'BVtestB'}]);
 return {status:'partial',items:[{status:'denied'},{status:'ok',coinCount:1}]};
}}));
for(const status of ['unsupported','denied','not_login'])
 await assert.rejects(authorizeReward('perfect',config,async()=>({getVideoUserActions:async()=>({status,items:[]})})));
await assert.rejects(authorizeReward('perfect',config,async()=>({getVideoUserActions:async()=>({status:'ok',items:[{status:'ok',coinCount:0}]})})));
await assert.rejects(authorizeReward('endless',config,async()=>({getAuthorRelation:async()=>({status:'ok',data:{isFollowing:false}})})));
await authorizeReward('endless',config,async()=>({getAuthorRelation:async()=>({status:'ok',data:{isFollowing:true}})}));
for(const kind of ['perfect','endless']){
 const following={bypassBilibiliChecks:false,perfectSaveRequirement:'following',rewardVideoBvids:[]};
 await authorizeReward(kind,following,async()=>({getAuthorRelation:async()=>({status:'ok',data:{isFollowing:false,isAuthor:true}})}));
 await authorizeReward(kind,following,async()=>({getAuthorRelation:async()=>({status:'ok',data:{isFollowing:true,isAuthor:false}})}));
 await assert.rejects(authorizeReward(kind,following,async()=>({getAuthorRelation:async()=>({status:'ok',data:{isFollowing:false,isAuthor:false}})})));
 await assert.rejects(authorizeReward(kind,following,async()=>({getAuthorRelation:async()=>({status:'unsupported',data:{isAuthor:true}})})));
}
console.log('PASS reward authorization: offline makes zero SDK calls; empty BV, unsupported, denied and unmet actions never grant saves');
