// Startup-only watchdog. Dispose its timer when resources and a later game
// frame are ready; there is no polling in normal play.
export function createStartupMonitor({readState,onProgress,onReady,onError,
 now=()=>performance.now(),hidden=()=>document.hidden,
 schedule=callback=>setInterval(callback,250),cancel=timer=>clearInterval(timer),timeoutMs=120000}){
 let timer=null,previous=0,elapsed=0,phase='启动游戏',readyDraws=null;
 const stop=()=>{if(timer!==null)cancel(timer);timer=null};
 const setPhase=value=>{phase=String(value);if(timer!==null)onProgress(phase,elapsed)};
 const tick=()=>{
  const time=now(),delta=Math.max(0,time-previous);previous=time;if(hidden())return;
  elapsed+=delta;
  try{
   const state=readState();
   if(state.loaded){
    if(readyDraws===null)readyDraws=state.draws;
    else if(state.draws>readyDraws&&state.updates>0){stop();onReady();return}
   }
   onProgress(phase,elapsed);
   if(elapsed>=timeoutMs){stop();onError('游戏启动超时：'+phase+'。资源完成='+!!state.loaded+'，更新='+state.updates+'，绘制='+state.draws+'。请复制错误信息反馈。')}
  }catch(error){stop();onError(error.stack||String(error))}
 };
 return {start(){stop();previous=now();elapsed=0;readyDraws=null;timer=schedule(tick);onProgress(phase,0)},stop,setPhase};
}
