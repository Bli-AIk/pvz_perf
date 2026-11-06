let cached;
async function fetchResource(input,options){
 const base=new URL('./',document.baseURI).href;
 const absolute=typeof input==='string'?new URL(input,document.baseURI).href:input;
 const resource=typeof absolute==='string'&&absolute.startsWith(base)?'./'+absolute.slice(base.length):absolute;
 const label=typeof resource==='string'?resource.split('/').at(-1).split('?')[0]:'资源';
 let response;
 try{response=await fetch(resource,options)}catch(error){throw Error('资源请求失败：'+label+'（'+(error.message||error)+'；可能是网络或宿主安全策略拦截）')}
 if(!response.ok)throw Error('资源读取失败：'+label+'（HTTP '+response.status+'）');
 return response;
}
export async function loadSaveBundle(){
 return cached ||= (async()=>{
  const response=await fetchResource('./save-bundle.json');
  const bundle=await response.json(),files={};
  for(const name of ['users.dat','user1.dat64','game1_58.dat64']){
   if(typeof bundle[name]!=='string')throw Error('预置存档包缺少 '+name);
   files[name]=Uint8Array.from(atob(bundle[name]),c=>c.charCodeAt(0));
  }
  if(new TextDecoder().decode(files['user1.dat64'].slice(0,8))!=='PVZUSR64'||new TextDecoder().decode(files['game1_58.dat64'].slice(0,8))!=='PVZDAT64')throw Error('预置存档包内容无效');
  return files;
 })().catch(error=>{cached=null;throw error});
}
async function readMusicStart(data,order){
 const {MusicMixer}=await import('./audio/music-mixer.js');
 const compiled=window.Module?.pvzMusic?.compiled;
 if(!compiled)throw Error('音乐解码器尚未准备就绪');
 const mixer=await MusicMixer.create(compiled,48000);
 try{
  mixer.command({op:'load',id:1,data});
  mixer.command({op:'seek',id:1,order,row:0,revision:0});
  mixer.command({op:'play',id:1}); // Process destination row commands once, without audio output.
  const state=mixer.positions()[0];
  if(!(state.tempo>0&&state.speed>0))throw Error('音乐初始速度无效');
  return state;
 }finally{mixer.close()}
}
export async function runSaveTool(request,input){
 if(window.PVZSaveToolJob)throw Error('离线存档工具正在处理上一项任务');
 const bundle=await loadSaveBundle(),iframe=document.createElement('iframe');
 iframe.setAttribute('aria-hidden','true');iframe.style.cssText='position:fixed;left:-10000px;top:0;width:800px;height:600px;pointer-events:none';
 let timer;
 try{return await new Promise((resolve,reject)=>{
  window.PVZSaveToolJob={request,fetchResource,readMusicStart,runtimeStem:window.Module?.pvzRuntimeStem||'pvz-perf',baseURL:new URL('./',document.baseURI).href,files:{'users.dat':bundle['users.dat'],'user1.dat64':bundle['user1.dat64'],...(input?{'input.dat64':input}:{})},resolve,reject};
  timer=setTimeout(()=>reject(Error('离线存档工具处理超时')),180000);
  // Toy serves ordinary HTML routes through its own page shell. Do not
  // navigate to a second HTML URL for an implementation detail: keep an
  // inherited-origin srcdoc, and forbid it from navigating the top window.
  const base=new URL('./',document.baseURI).href;
  iframe.setAttribute('sandbox','allow-scripts allow-same-origin');
  const escaped=base.replaceAll('&','&amp;').replaceAll('"','&quot;').replaceAll('<','&lt;');
  iframe.srcdoc='<!doctype html><meta charset="utf-8"><base href="'+escaped+'"><canvas id="canvas" width="800" height="600"></canvas><script type="module" src="./save-tool-runtime.js"></script>';
  document.body.appendChild(iframe);
 })}finally{clearTimeout(timer);try{iframe.contentDocument?.getElementById('canvas')?.getContext('webgl2')?.getExtension('WEBGL_lose_context')?.loseContext()}catch{}iframe.remove();delete window.PVZSaveToolJob}
}
export function downloadSave(bytes,name='game1_58.dat64'){
 const url=URL.createObjectURL(new Blob([bytes],{type:'application/octet-stream'}));
 const link=document.createElement('a');link.href=url;link.download=name;link.click();setTimeout(()=>URL.revokeObjectURL(url),60000);
}
