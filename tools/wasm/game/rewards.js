export async function authorizeReward(kind,config,provideToy){
 if(config.bypassBilibiliChecks===true)return;
 const coin=kind==='perfect'&&config.perfectSaveRequirement==='coin';
 if(coin&&(!Array.isArray(config.rewardVideoBvids)||!config.rewardVideoBvids.length))
  throw Error('尚未配置投币视频 BV 号，请稍后再试。');
 const toy=await provideToy();
 if(coin){
  if(typeof toy?.getVideoUserActions!=='function')throw Error('当前 B 站环境不支持投币状态查询，请使用支持此接口的 Toy 宿主。');
  const result=await toy.getVideoUserActions({videos:config.rewardVideoBvids.map(bvid=>({bvid}))});
  if(result.status!=='ok'&&result.status!=='partial')throw Error('无法验证投币状态：'+result.status+'。请登录 B 站或在 B 站 App 内打开。');
  if(!result.items?.some(item=>item.status==='ok'&&Number(item.coinCount)>0))throw Error('尚未检测到对指定视频的投币。投币后可再次点击领取。');
 }else{
  if(typeof toy?.getAuthorRelation!=='function')throw Error('当前 B 站环境不支持关注状态查询。');
  const result=await toy.getAuthorRelation();
  if(result.status!=='ok')throw Error('无法验证关注状态：'+result.status+'。请登录 B 站或在 B 站 App 内打开。');
  if(!result.data?.isFollowing&&!result.data?.isAuthor)throw Error('尚未检测到关注作者。关注后可再次点击领取。');
 }
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
