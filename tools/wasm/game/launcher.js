const statusText=document.getElementById('status'),start=document.getElementById('start'),stage=document.getElementById('stage');
const form=document.getElementById('launch-form'),fps=document.getElementById('target-fps');
const autoFps=document.getElementById('auto-fps'),aspect=document.getElementById('aspect');
const showFps=document.getElementById('show-fps');
const landscape=document.getElementById('landscape-ui'),volume=document.getElementById('sfx-volume');
const canvas=document.getElementById('canvas');
const musicVolume=document.getElementById('music-volume'),launchFullscreen=document.getElementById('launch-fullscreen');
const saveToolsReady=import('./save-tools.js'),lineupToolsReady=import('./lineups.js');
const progressToolsReady=import('./progress-storage.js');
let progressStorage;
const mobileUA=/Android|iPhone|iPad|iPod/i.test(navigator.userAgent);
const mobileInput=mobileUA||
 (navigator.platform==='MacIntel'&&navigator.maxTouchPoints>1)||matchMedia('(pointer:coarse)').matches;
const saveRoot='/persist/development-v331';
const storageKey='pvz_perf_web_launcher_v1';
// Distribution policy is read once, outside the game loop and user preferences.
const webConfig=window.PVZWebConfig;
if(typeof webConfig?.bypassBilibiliChecks!=='boolean')throw Error('缺少有效的 web-config.js 配置');
const fixedMobileRate=mobileUA&&!webConfig.bypassBilibiliChecks;
let fpsTimer=0;
if(!fixedMobileRate)import('./frame-rate.js').then(({createFrameRateObserver})=>{
 Module.pvzObserveFrame=createFrameRateObserver(rate=>{
  if(refreshRate===rate)return;
  refreshRate=rate;Module.pvzRefreshRate=rate;
  if(running)Module._pvz_web_refresh_rate?.(rate);
  updateControls();
 });
});
function startFps(){const label=document.getElementById("game-fps");clearInterval(fpsTimer);fpsTimer=0;label.hidden=!showFps.checked;if(label.hidden)return;let time=performance.now(),draws=Module._pvz_web_draws();fpsTimer=setInterval(()=>{const now=performance.now(),next=Module._pvz_web_draws();label.textContent="FPS "+((next-draws)*1000/(now-time)).toFixed(1);time=now;draws=next},1000)}
let runtimeReady=false,running=false,formEdited=false,refreshRate=60,resizeScheduled=false,lastWidth=0;
function readPreferences(){try{return JSON.parse(localStorage.getItem(storageKey)||'{}')||{}}catch{return {}}}
function normalizePreferences(value){
 const finite=(x,fallback)=>Number.isFinite(Number(x))?Number(x):fallback;
 return {targetFps:Math.max(30,Math.min(1000000000,Math.round(finite(value.targetFps,mobileUA?60:100)))),
  autoDetectFrameRate:!fixedMobileRate&&(typeof value.autoDetectFrameRate==='boolean'?value.autoDetectFrameRate:!mobileUA),landscapeUI:value.landscapeUI===true,showFps:value.showFps===true,
  sfxVolume:Math.max(0,Math.min(100,Math.round(finite(value.sfxVolume,100)))),
  musicVolume:Math.max(0,Math.min(100,Math.round(finite(value.musicVolume,100)))),
  fullscreen:typeof value.screenMode==='string'?value.screenMode==='fullscreen':value.fullscreen===true,
  aspect:['auto','4:3','16:9','21:9'].includes(value.aspect)?value.aspect:'auto'};
}
function fillPreferences(value){const p=normalizePreferences(value);fps.value=p.targetFps;autoFps.checked=p.autoDetectFrameRate;showFps.checked=p.showFps;landscape.checked=p.landscapeUI;volume.value=p.sfxVolume;musicVolume.value=p.musicVolume;launchFullscreen.checked=p.fullscreen;aspect.value=p.aspect;updateControls()}
function preferences(){return normalizePreferences({targetFps:fps.value,autoDetectFrameRate:autoFps.checked,showFps:showFps.checked,landscapeUI:landscape.checked,sfxVolume:volume.value,musicVolume:musicVolume.value,fullscreen:launchFullscreen.checked,aspect:aspect.value})}
function showError(message){
 clearInterval(startupTimer);
 clearInterval(fpsTimer);fpsTimer=0;
 const box=document.getElementById('error');box.hidden=false;box.replaceChildren();
 const heading=document.createElement('h2');heading.textContent='游戏未能正常启动或运行';
 const details=document.createElement('pre');
 details.textContent=String(message)+'\n\n图形接口：'+(Module?.pvzSelectedBackend||'检测中')+
  '\n'+(document.getElementById('graphics-capabilities').textContent||'')+'\n\n启动阶段：'+startupPhase+'\n浏览器：'+navigator.userAgent;
 const actions=document.createElement('div');actions.className='error-actions';
 const retry=document.createElement('button');retry.type='button';retry.textContent='重新加载';retry.onclick=()=>location.reload();actions.append(retry);
 if(Module?.pvzSelectedBackend==='webgpu'&&window.PVZGraphicsConfig?.fallback&&Module.pvzGraphicsCapabilities?.webgl){
  const fallback=document.createElement('button');fallback.id='graphics-retry-webgl';fallback.type='button';fallback.textContent='改用 WebGL 2';
  fallback.onclick=()=>{try{localStorage.setItem('pvz_graphics_backend','webgl');location.reload()}catch(error){details.textContent+='\n无法保存图形接口选择：'+error.message}};
  actions.prepend(fallback);
 }
 const copy=document.createElement('button');copy.type='button';copy.className='secondary';copy.textContent='复制错误信息';
 copy.onclick=async()=>{try{await navigator.clipboard.writeText(details.textContent);copy.textContent='已复制'}catch{copy.textContent='请选中上方文字复制'}};actions.append(copy);
 box.append(heading,details,actions);console.error(message);
}
function updateControls(){
 autoFps.disabled=fixedMobileRate;fps.disabled=autoFps.checked;
 document.getElementById('volume-value').value=volume.value;document.getElementById('music-value').value=musicVolume.value;
 document.getElementById('target-fps-info').textContent=mobileUA?'手机建议值：60 FPS，可手动调整；实际绘制受浏览器与屏幕限制。':'最低 30 FPS；实际绘制受浏览器与屏幕限制。';
 document.getElementById('refresh-info').textContent=fixedMobileRate?'手机 B 站环境不检测刷新率；默认目标为 60 FPS。':`浏览器绘制节奏约 ${refreshRate} Hz（估算），自动跟随开启时在运行中更新。`;
 scheduleResize();
}
function viewportFor(width,height,mode,minimumWidth=800){
 const requested=mode==='auto'?width/height:Number(mode.split(':')[0])/Number(mode.split(':')[1]);
 const logicalWidth=Math.round(Math.max(minimumWidth,Math.min(1400,requested*600)));
 const ratio=logicalWidth/600,cssWidth=Math.min(width,height*ratio);
 return {width:logicalWidth,height:600,cssWidth,cssHeight:cssWidth/ratio};
}
function fitCanvas(){
 resizeScheduled=false;
 if(running&&Module.pvzTextInput?.active)return; // The soft keyboard must not rotate/resize the game underneath an editor.
 const mobile=mobileInput;
 const rotate=mobile&&document.body.classList.contains('playing')&&innerHeight>innerWidth;
 document.body.classList.toggle('portrait-game',rotate);
 if(rotate){stage.style.width=`${innerHeight}px`;stage.style.height=`${innerWidth}px`}
 else{stage.style.width='';stage.style.height=''}
 const bounds=stage.getBoundingClientRect();if(bounds.width<1||bounds.height<1)return;
 const p=preferences(),w=stage.clientWidth,h=stage.clientHeight;
 const view=viewportFor(mobile?Math.max(w,h):w,mobile?Math.min(w,h):h,p.aspect,running&&Module.pvzLandscapeActive?890:800);
 canvas.style.width=`${view.cssWidth}px`;canvas.style.height=`${view.cssHeight}px`;
 Module.pvzViewport={width:view.width,height:view.height};
 document.getElementById('viewport-info').textContent=`${view.width} × 600 · ${(view.width/600).toFixed(2)}:1`;
 if(running&&runtimeReady&&lastWidth!==view.width){Module._pvz_web_resize(view.width,view.height);lastWidth=view.width}
 return view;
}
function scheduleResize(){if(!resizeScheduled){resizeScheduled=true;requestAnimationFrame(fitCanvas)}}
function pointerPosition(x,y){
 const b=canvas.getBoundingClientRect();
 return document.body.classList.contains('portrait-game')?
  {x:(y-b.top)/b.height,y:1-(x-b.left)/b.width}:{x:(x-b.left)/b.width,y:(y-b.top)/b.height};
}
// SDL's DOM coordinate conversion does not invert CSS rotation. Feed already
// mapped pointer events into its queue; keep keys, text and wheel on SDL.
const activePointers=new Map();
function cancelPointer(id){
 const pointer=activePointers.get(id);if(!pointer)return;
 activePointers.delete(id);
 Module._pvz_web_pointer?.(pointer.touch?6:7,id,0,0,pointer.button,performance.now());
 if(canvas.hasPointerCapture(id))canvas.releasePointerCapture(id);
}
function cancelPointers(){for(const id of activePointers.keys())cancelPointer(id)}
for(const type of ['pointerdown','pointermove','pointerup','pointercancel'])canvas.addEventListener(type,e=>{
 if(!running||!Module._pvz_web_pointer)return;
 if(type==='pointercancel'){cancelPointer(e.pointerId);return}
 if(type==='pointerdown'){
  // preventDefault below suppresses the browser's normal focus transfer. A Toy
  // sidebar belongs to the parent document, so explicitly refocus our iframe.
  window.focus();
  if(Module.pvzTextInput?.active)Module.pvzTextInput.focus();else canvas.focus({preventScroll:true});
  // A genuine canvas press can restore application focus even when a mobile
  // Toy host ignores window.focus(). This queues focus before the touch itself.
  Module._pvz_web_focus?.(1);
  activePointers.set(e.pointerId,{touch:e.pointerType!=='mouse',button:e.button+1});
 }
 const p=pointerPosition(e.clientX,e.clientY),touch=e.pointerType!=='mouse';
 const kind=touch?(type==='pointerdown'?3:type==='pointermove'?4:type==='pointerup'?5:6):
  type==='pointerdown'?1:type==='pointerup'?2:0;
 if(type==='pointerdown')canvas.setPointerCapture(e.pointerId);
 Module._pvz_web_pointer(kind,e.pointerId,p.x,p.y,e.button+1,e.timeStamp);
 if(type==='pointerup'){
  activePointers.delete(e.pointerId);
  if(canvas.hasPointerCapture(e.pointerId))canvas.releasePointerCapture(e.pointerId);
 }
 e.preventDefault();
});
canvas.addEventListener('lostpointercapture',e=>cancelPointer(e.pointerId));
window.addEventListener('blur',()=>{cancelPointers();Module._pvz_web_focus?.(0)});
window.addEventListener('focus',()=>{if(!document.hidden)Module._pvz_web_focus?.(1)});
document.addEventListener('visibilitychange',()=>{if(document.hidden)cancelPointers();Module._pvz_web_focus?.(document.hidden?0:1)});
for(const type of ['mousedown','mousemove','mouseup','touchstart','touchmove','touchend','touchcancel'])
 canvas.addEventListener(type,e=>{if(running){e.preventDefault();e.stopImmediatePropagation()}},{capture:true,passive:false});
window.addEventListener('resize',scheduleResize);
async function measureRefreshRate(){
 const samples=[];let previous;for(let i=0;i<25;i++)await new Promise(resolve=>requestAnimationFrame(time=>{if(previous!==undefined){const dt=time-previous;if(dt>=1&&dt<=100)samples.push(dt)}previous=time;resolve()}));
 if(samples.length>=12){samples.sort((a,b)=>a-b);const raw=1000/samples[Math.floor(samples.length/2)];const rates=[30,48,50,60,72,75,90,100,120,144,165,180,200,240,360,480];const nearest=rates.reduce((a,b)=>Math.abs(b-raw)<Math.abs(a-raw)?b:a);refreshRate=Math.max(30,Math.round(Math.abs(nearest-raw)/raw<.035?nearest:raw))}
 Module.pvzRefreshRate=refreshRate;updateControls();return refreshRate;
}
function loadNativeSettings(){try{return JSON.parse(Module.FS.readFile(saveRoot+'/settings.json',{encoding:'utf8'}))||{}}catch{return {}}}
async function saveSettings(p){
 const settings={...loadNativeSettings(),targetFps:p.autoDetectFrameRate?refreshRate:p.targetFps,
  autoDetectFrameRate:p.autoDetectFrameRate,landscapeUI:p.landscapeUI&&Module.pvzViewport.width>=890,
  sfxVolume:p.sfxVolume,musicVolume:p.musicVolume,screenMode:p.fullscreen?'fullscreen':'windowed',customScale:0,hdr:false};
 Module.pvzLandscapeActive=settings.landscapeUI;
 Module.FS.writeFile(saveRoot+'/settings.json',JSON.stringify(settings,null,2)+'\n');
 try{localStorage.setItem(storageKey,JSON.stringify(p))}catch{}
 await new Promise((resolve,reject)=>Module.FS.syncfs(false,error=>error?reject(error):resolve()));
}
function clearUserSaves(){
 const fs=Module.FS;
 function remove(path){const stat=fs.lstat(path);if(fs.isDir(stat.mode)){for(const name of fs.readdir(path))if(name!=='.'&&name!=='..')remove(path+'/'+name);fs.rmdir(path)}else fs.unlink(path)}
 for(const name of fs.readdir(saveRoot))if(name!=='.'&&name!=='..'&&name!=='settings.json')remove(saveRoot+'/'+name);
}
async function installBaseline(){
 if(running)throw Error('请先退出游戏，再替换存档');
 const {loadSaveBundle}=await saveToolsReady;
 const files=await loadSaveBundle();
 clearUserSaves();
 for(const [name,bytes] of Object.entries(files))Module.FS.writeFile(saveRoot+'/'+name,bytes);
 await syncSaves();
}
function syncSaves(){return new Promise((resolve,reject)=>Module.FS.syncfs(false,error=>error?reject(error):resolve()))}
async function preparePersistentFiles(){
 await new Promise((resolve,reject)=>Module.FS.syncfs(true,error=>error?reject(error):resolve()));
 Module.FS.mkdirTree(saveRoot);
 const {ProgressStorage}=await progressToolsReady;
 progressStorage=new ProgressStorage(Module.FS,saveRoot,{cloudEnabled:!webConfig.bypassBilibiliChecks,
  provideToy:async()=>(await import('./rewards.js')).provideBilibiliToy(),
  report:message=>{document.getElementById('cloud-status').textContent=message},
  uploadState:uploading=>{document.getElementById('cloud-upload-warning').hidden=!uploading}});
 if(await progressStorage.prepare())await syncSaves();
 if(webConfig.bypassBilibiliChecks&&!Module.FS.analyzePath(saveRoot+'/users.dat').exists)await installBaseline();
 progressStorage.watchProfileWrites();
}
const rewardStatus=document.getElementById('reward-status');
document.getElementById('coin-video').href='https://www.bilibili.com/video/'+webConfig.rewardVideoBvid+'/';
document.getElementById('coin-video').onclick=event=>{
 if(webConfig.bypassBilibiliChecks||typeof globalThis.toy?.navigate!=='function')return;
 event.preventDefault();
 // Call within the click activation; awaiting SDK loading would lose it.
 globalThis.toy.navigate({type:'video',id:webConfig.rewardVideoBvid}).catch(error=>{
  rewardStatus.textContent='视频跳转失败：'+(error.message||error)+'；请再次点击网页链接。';
  document.getElementById('coin-video').onclick=null;
 });
};
if(!webConfig.bypassBilibiliChecks)import('./rewards.js').then(m=>m.provideBilibiliToy()).catch(()=>{});
rewardStatus.textContent=webConfig.bypassBilibiliChecks?'本地版：已绕过 B 站接口。':'给 '+webConfig.rewardVideoBvid+' 投满两个币，解锁自定义存档（含完美进度、无尽布阵与 DAT64 导入／导出）；作者本人可直接使用。';
let rewardBusy=false;
async function authorize(){const {authorizeReward,provideBilibiliToy}=await import('./rewards.js');await authorizeReward(webConfig,provideBilibiliToy)}
import('./save-editor-host.js').then(({connectSaveEditor})=>connectSaveEditor({
 module:Module,root:saveRoot,ready:()=>runtimeReady,idle:()=>!rewardBusy,running:()=>running,
 setActive:active=>{rewardBusy=active;start.disabled=active||!runtimeReady},
 authorize,sync:syncSaves,installBaseline,tools:saveToolsReady,lineups:lineupToolsReady,
 status:message=>{rewardStatus.textContent=message}
})).catch(showError);
async function progressAction(operation){
 if(!runtimeReady||running||rewardBusy)return;
 rewardBusy=true;start.disabled=true;
 try{await operation()}catch(error){document.getElementById('cloud-status').textContent=error.message}
 finally{rewardBusy=false;start.disabled=false}
}
document.getElementById('upload-progress').onclick=()=>progressAction(async()=>{
 await progressStorage.save(true);
});
document.getElementById('restore-progress').onclick=()=>progressAction(async()=>{
 const saved=await progressStorage.readCloud();if(!saved)throw Error('当前 B 站账号尚无云端进度');
 const dialog=document.getElementById('cloud-restore-dialog');
 const confirmed=new Promise(resolve=>{dialog.addEventListener('close',()=>resolve(dialog.returnValue==='restore'),{once:true});dialog.returnValue='cancel';dialog.showModal()});
 if(!await confirmed)return;
 progressStorage.apply(saved.files);progressStorage.needsChoice=false;await syncSaves();await progressStorage.save();
});
document.getElementById('save-local').onclick=()=>progressAction(async()=>{
 await syncSaves();document.getElementById('local-save-status').textContent='本地存档已写入浏览器';
});
let startupPhase='页面初始化',startupTimer=0;
function startupDeadline(promise,phase,milliseconds=30000){
 startupPhase=phase;let timer;
 return Promise.race([promise,new Promise((_,reject)=>{timer=setTimeout(()=>reject(Error(phase+'超时，请重新加载；若持续失败，请复制错误信息反馈。')),milliseconds)})]).finally(()=>clearTimeout(timer));
}
async function returnToSettings(){
 Module.pvzMusic.close();Module.pvzMusic.context?.suspend().catch(console.warn);
 clearInterval(fpsTimer);clearInterval(startupTimer);fpsTimer=startupTimer=0;
 document.getElementById('game-fps').hidden=true;
 Module.pvzTextInput.hide();cancelPointers();
 try{await syncSaves();document.getElementById('local-save-status').textContent='本地存档已写入浏览器'}catch(error){document.getElementById('local-save-status').textContent='本地保存失败：'+error.message+'。请重试保存或导出 DAT64，不要刷新页面。'}
 progressStorage?.save().catch(error=>{document.getElementById('cloud-status').textContent='进度备份未完成：'+error.message});
 if(document.fullscreenElement)await document.exitFullscreen().catch(console.warn);
 running=false;document.body.classList.remove('playing','portrait-game');stage.classList.remove('running');
 for(const input of form.elements)input.disabled=false;
 formEdited=false;fillPreferences({...readPreferences(),...loadNativeSettings()});
 document.getElementById('graphics-backend').disabled=document.getElementById('graphics-backend').options.length<2;
 document.getElementById('upload-progress').disabled=document.getElementById('restore-progress').disabled=webConfig.bypassBilibiliChecks;
 statusText.textContent='准备就绪';startupPhase='设置页面';start.disabled=false;scheduleResize();
}
canvas.addEventListener('webglcontextlost',event=>{event.preventDefault();showError('WebGL 图形上下文丢失，浏览器已停止绘制。请重新加载。')});
var Module={canvas,locateFile:name=>'./'+name,pvzRefreshRate:60,pvzMobileInput:mobileInput,
 pvzStartupPhase:phase=>{startupPhase=phase},
 pvzSetFullscreen:enabled=>{if(enabled&&!document.documentElement.requestFullscreen){console.warn('当前浏览器不支持 Fullscreen API');return}const change=enabled?document.documentElement.requestFullscreen():document.fullscreenElement?document.exitFullscreen():Promise.resolve();change.catch(error=>console.warn('浏览器未允许全屏',error))},
 pvzOnQuit:()=>returnToSettings().catch(showError),
 print:message=>console.log(message),printErr:message=>console.error(message),onAbort:showError,pvzOnError:showError,
 monitorRunDependencies:left=>{statusText.textContent=left?'正在准备资源与存档…':'加载完成';startupPhase=statusText.textContent},
 preRun:[function(){Module.FS.mkdir('/persist');Module.FS.mount(Module.IDBFS,{autoPersist:false},'/persist');Module.addRunDependency('savedata');preparePersistentFiles().catch(error=>{Module.pvzStorageError=true;showError('存档准备失败，未清除原有存档：'+error.message)}).finally(()=>Module.removeRunDependency('savedata'))}],
 onRuntimeInitialized:()=>{runtimeReady=!Module.pvzStorageError;if(!formEdited)fillPreferences({...readPreferences(),...loadNativeSettings()});document.getElementById('progress').hidden=true;statusText.textContent='准备就绪';start.disabled=!runtimeReady||!Module.pvzMusic;document.getElementById('upload-progress').disabled=document.getElementById('restore-progress').disabled=webConfig.bypassBilibiliChecks||!runtimeReady;scheduleResize()}
};
fillPreferences(readPreferences());
form.addEventListener('input',()=>{formEdited=true;updateControls()});form.addEventListener('change',()=>{formEdited=true;updateControls()});
new ResizeObserver(scheduleResize).observe(stage);
form.addEventListener('submit',async event=>{
 event.preventDefault();if(!runtimeReady||running||rewardBusy)return;
 const p=preferences();fps.value=p.targetFps;start.disabled=true;
 try{const full=p.fullscreen&&document.documentElement.requestFullscreen?document.documentElement.requestFullscreen().catch(error=>console.warn('浏览器未允许全屏',error)):Promise.resolve();const audio=Module.pvzMusic.unlock();await startupDeadline(Promise.all([audio,textInputReady,full]),"初始化音频与输入");document.body.classList.add('playing');fitCanvas();await saveSettings(p);running=true;lastWidth=Module.pvzViewport.width;stage.classList.add('running');for(const input of form.elements)input.disabled=true;fitCanvas();canvas.focus();startFps();startupPhase="启动游戏";const begin=performance.now();startupTimer=setInterval(()=>{if(document.hidden)return;if(Module._pvz_web_draws?.()>0){clearInterval(startupTimer);startupPhase="游戏运行"}else if(performance.now()-begin>60000)showError("游戏启动超时，尚未完成首帧绘制。")},1000);Module.callMain([])}catch(error){document.body.classList.remove('playing');showError(error.stack||error);start.disabled=false}
});
window.addEventListener('error',event=>showError(event.message));window.addEventListener('unhandledrejection',event=>showError(event.reason?.stack||event.reason));
async function boot(){
 const config=window.PVZGraphicsConfig||{webgpu:false};
 if(config.version){document.title='pvz_perf · 网页 v'+config.version;document.getElementById('web-version').textContent='网页 v'+config.version}
 const dropdown=document.getElementById('graphics-backend');
 document.getElementById('graphics-field').hidden=false;
 dropdown.replaceChildren(new Option('正在检测图形接口…',''));dropdown.disabled=true;
 const {detectGraphics,capabilityText}=await import('./graphics.js');
 const capabilities=await detectGraphics(navigator.gpu,{webgpu:config.webgpu,webgl:!config.webgpu||config.fallback});
 document.getElementById('graphics-capabilities').textContent=capabilityText(capabilities);
 Module.pvzGraphicsCapabilities=capabilities;
 const available=[['webgpu','WebGPU'],['webgl','WebGL 2']].filter(([name])=>capabilities[name]);
 if(!available.length){
  dropdown.replaceChildren(new Option('无可用图形接口',''));
  throw Error(config.fallback?'当前浏览器的 WebGPU 和 WebGL 2 都无法运行本游戏。请查看下方检测结果，或换用其他浏览器。':'此包提供的图形接口无法运行本游戏。请查看下方检测结果。');
 }
 // Preferences cannot make an unavailable API selectable. Reset stale values
 // to auto so browser upgrades can use a newly available backend next time.
 let saved='auto';try{saved=localStorage.getItem('pvz_graphics_backend')||'auto'}catch{}
 const mode=saved==='auto'||available.some(([name])=>name===saved)?saved:'auto';
 if(mode!==saved)try{localStorage.setItem('pvz_graphics_backend',mode)}catch{}
 const backend=mode==='auto'?available[0][0]:mode;
 dropdown.replaceChildren(...(available.length>1?[new Option('自动（优先 WebGPU）','auto')]:[]),...available.map(([name,label])=>new Option(label,name)));
 dropdown.value=available.length>1?mode:backend;dropdown.disabled=available.length<2;
 dropdown.onchange=()=>{try{localStorage.setItem('pvz_graphics_backend',dropdown.value)}catch{}location.reload()};
 document.getElementById('graphics-info').textContent='当前使用 '+(backend==='webgpu'?'WebGPU':'WebGL 2')+'；切换接口会重新加载设置页，存档保留。';
 Module.pvzSelectedBackend=backend;
 const stem=config.webgpu&&backend==='webgl'?'pvz-perf-webgl':'pvz-perf';
 Module.pvzRuntimeStem=stem;
 const response=await fetch('./'+stem+'.wasm.gz');if(!response.ok)throw Error('Wasm 下载失败：'+response.status);
 const packed=new Uint8Array(await response.arrayBuffer());
 const binary=packed[0]===31&&packed[1]===139?await new Response(new Blob([packed]).stream().pipeThrough(new DecompressionStream('gzip'))).arrayBuffer():packed.buffer;
 Module.instantiateWasm=(imports,receive)=>{WebAssembly.instantiate(binary,imports).then(result=>receive(result.instance)).catch(showError);return {}};
 const script=document.createElement('script');script.src='./'+stem+'.js';script.onerror=()=>showError('无法加载游戏 JavaScript');document.body.appendChild(script);
}
window.PVZLauncher={viewportFor,normalizePreferences,preferences,fitCanvas,pointerPosition};
if(!fixedMobileRate)measureRefreshRate().catch(()=>updateControls());
const textInputReady=import('./text-input.js').then(m=>{Module.pvzTextInput=m.createTextInput()});
startupDeadline(Promise.all([textInputReady,saveToolsReady,lineupToolsReady,import('./audio/music-engine.js').then(m=>m.prepareMusic()).then(audio=>{Module.pvzMusic=audio;if(runtimeReady)start.disabled=false}),boot()]),"加载游戏资源",120000).catch(showError);
