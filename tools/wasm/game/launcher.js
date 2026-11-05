const statusText=document.getElementById('status'),start=document.getElementById('start'),stage=document.getElementById('stage');
const form=document.getElementById('launch-form'),fps=document.getElementById('target-fps');
const autoFps=document.getElementById('auto-fps'),aspect=document.getElementById('aspect');
const landscape=document.getElementById('landscape-ui'),volume=document.getElementById('sfx-volume');
const canvas=document.getElementById('canvas');
const mobileInput=/Android|iPhone|iPad|iPod/i.test(navigator.userAgent)||
 (navigator.platform==='MacIntel'&&navigator.maxTouchPoints>1)||matchMedia('(pointer:coarse)').matches;
const saveRoot='/persist/development-v331';
const storageKey='pvz_perf_web_launcher_v1';
// Distribution policy is read once, outside the game loop and user preferences.
const webConfig=window.PVZWebConfig;
if(typeof webConfig?.bypassBilibiliChecks!=='boolean')throw Error('缺少有效的 web-config.js 配置');
let fpsTimer=0;
function startFps(){const label=document.getElementById("game-fps");label.hidden=false;let time=performance.now(),draws=Module._pvz_web_draws();clearInterval(fpsTimer);fpsTimer=setInterval(()=>{const now=performance.now(),next=Module._pvz_web_draws();label.textContent="FPS "+((next-draws)*1000/(now-time)).toFixed(1);time=now;draws=next},1000)}
let runtimeReady=false,running=false,formEdited=false,refreshRate=60,resizeScheduled=false,lastWidth=0;
function readPreferences(){try{return JSON.parse(localStorage.getItem(storageKey)||'{}')||{}}catch{return {}}}
function normalizePreferences(value){
 const finite=(x,fallback)=>Number.isFinite(Number(x))?Number(x):fallback;
 return {targetFps:Math.max(30,Math.min(1000000000,Math.round(finite(value.targetFps,100)))),
  autoDetectFrameRate:value.autoDetectFrameRate!==false,landscapeUI:value.landscapeUI===true,
  sfxVolume:Math.max(0,Math.min(100,Math.round(finite(value.sfxVolume,100)))),
  aspect:['auto','4:3','16:9','21:9'].includes(value.aspect)?value.aspect:'auto'};
}
function fillPreferences(value){const p=normalizePreferences(value);fps.value=p.targetFps;autoFps.checked=p.autoDetectFrameRate;landscape.checked=p.landscapeUI;volume.value=p.sfxVolume;aspect.value=p.aspect;updateControls()}
function preferences(){return normalizePreferences({targetFps:fps.value,autoDetectFrameRate:autoFps.checked,landscapeUI:landscape.checked,sfxVolume:volume.value,aspect:aspect.value})}
function showError(message){const box=document.getElementById('error');box.hidden=false;box.textContent=String(message);console.error(message)}
function updateControls(){fps.disabled=autoFps.checked;document.getElementById('volume-value').value=volume.value;document.getElementById('refresh-info').textContent=`浏览器绘制节奏约 ${refreshRate} Hz（估算），启动时应用。`;scheduleResize()}
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
window.addEventListener('blur',cancelPointers);
document.addEventListener('visibilitychange',()=>{if(document.hidden)cancelPointers()});
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
  sfxVolume:p.sfxVolume,screenMode:'windowed',customScale:0,hdr:false};
 Module.pvzLandscapeActive=settings.landscapeUI;
 Module.FS.writeFile(saveRoot+'/settings.json',JSON.stringify(settings,null,2)+'\n');
 try{localStorage.setItem(storageKey,JSON.stringify(p))}catch{}
 await new Promise((resolve,reject)=>Module.FS.syncfs(false,error=>error?reject(error):resolve()));
}
async function installBaseline(perfectOnly=false){
 const catalog=await catalogReady;
 const preset=perfectOnly?catalog.presets[0]:catalog.presets.find(p=>p.id===document.getElementById('endless-preset').value);
 if(!preset)throw Error('未找到所选无尽存档');
 const names=perfectOnly?['users.dat','user1.dat64']:['users.dat','user1.dat64',preset.checkpoint];
 // Fetch every file before modifying the active development slot.
 const files=await Promise.all(names.map(async name=>{const response=await fetch('./'+preset.directory+'/'+name);if(!response.ok)throw Error('预置存档读取失败：'+name);return [name,new Uint8Array(await response.arrayBuffer())]}));
 for(const [name,bytes] of files)Module.FS.writeFile(saveRoot+'/'+name,bytes);
 await new Promise((resolve,reject)=>Module.FS.syncfs(false,error=>error?reject(error):resolve()));
}
const rewardStatus=document.getElementById('reward-status');
const resetBaseline=document.getElementById('reset-baseline');
resetBaseline.hidden=!webConfig.bypassBilibiliChecks;
rewardStatus.textContent=webConfig.bypassBilibiliChecks?'本地版：已绕过 B 站接口，可直接领取。':webConfig.perfectSaveRequirement==='coin'?'完美存档需投币，无尽存档需关注作者。':'关注作者可领取完美档与无尽档，作者本人可直接领取。';
let rewardBusy=false;
async function claimReward(kind){
 if(!runtimeReady||running||rewardBusy)return;
 rewardBusy=true;const controls=[document.getElementById('claim-perfect'),document.getElementById('claim-endless'),resetBaseline,start];
 for(const button of controls)button.disabled=true;
 try{
  const {authorizeReward,provideBilibiliToy}=await import('./rewards.js');
  await authorizeReward(kind,webConfig,provideBilibiliToy);
  await installBaseline(kind==='perfect');
  rewardStatus.textContent=kind==='perfect'?'完美存档已复制，可开始游戏。':'完美进度与泳池无尽基准已复制，可开始游戏。';
 }catch(error){rewardStatus.textContent=String(error.message||error)}
 finally{rewardBusy=false;for(const button of controls)button.disabled=false}
}
document.getElementById('claim-perfect').onclick=()=>claimReward('perfect');
document.getElementById('claim-endless').onclick=()=>claimReward('endless');
resetBaseline.onclick=()=>{if(webConfig.bypassBilibiliChecks)claimReward('endless')};
var Module={canvas,locateFile:name=>'./'+name,pvzRefreshRate:60,pvzMobileInput:mobileInput,
 pvzSetFullscreen:enabled=>{if(enabled&&!document.documentElement.requestFullscreen){console.warn('当前浏览器不支持 Fullscreen API');return}const change=enabled?document.documentElement.requestFullscreen():document.fullscreenElement?document.exitFullscreen():Promise.resolve();change.catch(error=>console.warn('浏览器未允许全屏',error))},
 pvzOnQuit:()=>{Module.pvzMusic.close();Module.pvzMusic.context?.close();clearInterval(fpsTimer);const leave=document.fullscreenElement?document.exitFullscreen():Promise.resolve();leave.finally(()=>location.reload())},
 print:message=>console.log(message),printErr:message=>console.error(message),onAbort:showError,
 monitorRunDependencies:left=>{statusText.textContent=left?'正在准备资源与存档…':'加载完成'},
 preRun:[function(){Module.FS.mkdir('/persist');Module.FS.mount(Module.IDBFS,{autoPersist:true},'/persist');Module.addRunDependency('savedata');Module.FS.syncfs(true,async error=>{if(error)console.warn('存档持久化不可用',error);try{Module.FS.mkdirTree(saveRoot);if(webConfig.bypassBilibiliChecks&&!Module.FS.analyzePath(saveRoot+'/users.dat').exists)await installBaseline();}catch(error){showError(error)}finally{Module.removeRunDependency('savedata')}})}],
 onRuntimeInitialized:()=>{runtimeReady=true;if(!formEdited)fillPreferences({...readPreferences(),...loadNativeSettings()});document.getElementById('progress').hidden=true;statusText.textContent='准备就绪';start.disabled=!Module.pvzMusic;scheduleResize()}
};
fillPreferences(readPreferences());
form.addEventListener('input',()=>{formEdited=true;updateControls()});form.addEventListener('change',()=>{formEdited=true;updateControls()});
new ResizeObserver(scheduleResize).observe(stage);
form.addEventListener('submit',async event=>{
 event.preventDefault();if(!runtimeReady||running||rewardBusy)return;
 const p=preferences();fps.value=p.targetFps;start.disabled=true;
 try{const audio=Module.pvzMusic.unlock();await Promise.all([audio,textInputReady]);document.body.classList.add('playing');fitCanvas();await saveSettings(p);running=true;lastWidth=Module.pvzViewport.width;stage.classList.add('running');for(const input of form.elements)input.disabled=true;fitCanvas();canvas.focus();startFps();Module.callMain([])}catch(error){document.body.classList.remove('playing');showError(error.stack||error);start.disabled=false}
});
window.addEventListener('error',event=>showError(event.message));window.addEventListener('unhandledrejection',event=>showError(event.reason?.stack||event.reason));
async function boot(){const response=await fetch('./pvz-perf.wasm.gz');if(!response.ok)throw Error('Wasm 下载失败：'+response.status);const packed=new Uint8Array(await response.arrayBuffer());const binary=packed[0]===31&&packed[1]===139?await new Response(new Blob([packed]).stream().pipeThrough(new DecompressionStream('gzip'))).arrayBuffer():packed.buffer;Module.instantiateWasm=(imports,receive)=>{WebAssembly.instantiate(binary,imports).then(result=>receive(result.instance)).catch(showError);return {}};const script=document.createElement('script');script.src='./pvz-perf.js';script.onerror=()=>showError('无法加载游戏 JavaScript');document.body.appendChild(script)}
window.PVZLauncher={viewportFor,normalizePreferences,preferences,fitCanvas,pointerPosition};
measureRefreshRate().catch(()=>updateControls());
const textInputReady=import('./text-input.js').then(m=>{Module.pvzTextInput=m.createTextInput()});
const catalogReady=fetch('./save-catalog.json').then(async response=>{
 if(!response.ok)throw Error('无法读取预置存档清单');
 const catalog=await response.json();
 if(!catalog.presets?.length)throw Error('预置存档清单为空');
 const select=document.getElementById('endless-preset');
 for(const preset of catalog.presets){
  if(!/^[a-zA-Z0-9_-]+$/.test(preset.directory)||!/^game1_\d+\.dat64$/.test(preset.checkpoint))throw Error('预置存档路径无效');
  const option=document.createElement('option');option.value=preset.id;option.textContent=preset.name;select.appendChild(option);
 }
 return catalog;
});
Promise.all([textInputReady,catalogReady,import('./audio/music-engine.js').then(m=>m.prepareMusic()).then(audio=>{Module.pvzMusic=audio;if(runtimeReady)start.disabled=false}),boot()]).catch(showError);
