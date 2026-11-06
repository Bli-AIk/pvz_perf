export async function authorizeReward(config,provideToy){
 if(config.bypassBilibiliChecks===true)return;
 const toy=await provideToy();
 if(typeof toy?.getAuthorRelation==='function'){
  // The two alternatives are independent: a failed author lookup must not
  // prevent a successful coin lookup. Following alone never grants a reward.
  try{
   const relation=await toy.getAuthorRelation();
   if(relation.status==='ok'&&relation.data?.isAuthor===true)return;
  }catch{}
 }
 const bvid=config.rewardVideoBvid;
 if(typeof bvid!=='string'||!/^BV[0-9A-Za-z]{10}$/.test(bvid))throw Error('尚未配置有效的投币视频 BV 号。');
 if(typeof toy?.getVideoUserActions!=='function')throw Error('当前 B 站环境不支持投币状态查询，请使用支持此接口的 Toy 宿主。');
 let result;
 try{result=await toy.getVideoUserActions({videos:[{bvid}]})}catch(error){throw Error('投币状态查询失败：'+(error.message||error))}
 if(result.status!=='ok'&&result.status!=='partial')throw Error('无法验证投币状态：'+result.status+'。请登录 B 站或在 B 站 App 内打开。');
 if(!result.items?.some(item=>item.bvid===bvid&&item.status==='ok'&&Number.isInteger(item.coinCount)&&item.coinCount>=2))
  throw Error('尚未检测到给 '+bvid+' 投满两个币。投币后可再次点击领取。');
}

let sdk;
export function provideBilibiliToy(){
 if(globalThis.toy)return Promise.resolve(globalThis.toy);
 return sdk ||= new Promise((resolve,reject)=>{
  const script=document.createElement('script');
  script.src='https://s1.hdslb.com/bfs/seed/toy/app/sdk/toy-sdk.js';
  script.onload=()=>resolve(globalThis.toy);
  script.onerror=()=>{sdk=null;script.remove();reject(Error('B 站接口加载失败，请检查网络。'))};
  document.head.appendChild(script);
 });
}
