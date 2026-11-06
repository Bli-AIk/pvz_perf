const statusText=document.getElementById('status'),start=document.getElementById('start'),stage=document.getElementById('stage');
const form=document.getElementById('launch-form'),fps=document.getElementById('target-fps');
const autoFps=document.getElementById('auto-fps'),aspect=document.getElementById('aspect');
const landscape=document.getElementById('landscape-ui'),volume=document.getElementById('sfx-volume');
const canvas=document.getElementById('canvas');
const musicVolume=document.getElementById('music-volume'),launchFullscreen=document.getElementById('launch-fullscreen');
const saveToolsReady=import('./save-tools.js'),lineupToolsReady=import('./lineups.js');
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
function startFps(){const label=document.getElementById("game-fps");label.hidden=false;let time=performance.now(),draws=Module._pvz_web_draws();clearInterval(fpsTimer);fpsTimer=setInterval(()=>{const now=performance.now(),next=Module._pvz_web_draws();label.textContent="FPS "+((next-draws)*1000/(now-time)).toFixed(1);time=now;draws=next},1000)}
let runtimeReady=false,running=false,formEdited=false,refreshRate=60,resizeScheduled=false,lastWidth=0;
function readPreferences(){try{return JSON.parse(localStorage.getItem(storageKey)||'{}')||{}}catch{return {}}}
function normalizePreferences(value){
 const finite=(x,fallback)=>Number.isFinite(Number(x))?Number(x):fallback;
 return {targetFps:Math.max(30,Math.min(1000000000,Math.round(finite(value.targetFps,mobileUA?60:100)))),
  autoDetectFrameRate:!fixedMobileRate&&(typeof value.autoDetectFrameRate==='boolean'?value.autoDetectFrameRate:!mobileUA),landscapeUI:value.landscapeUI===true,
  sfxVolume:Math.max(0,Math.min(100,Math.round(finite(value.sfxVolume,100)))),
  musicVolume:Math.max(0,Math.min(100,Math.round(finite(value.musicVolume,100)))),
  fullscreen:typeof value.screenMode==='string'?value.screenMode==='fullscreen':value.fullscreen===true,
  aspect:['auto','4:3','16:9','21:9'].includes(value.aspect)?value.aspect:'auto'};
}
function fillPreferences(value){const p=normalizePreferences(value);fps.value=p.targetFps;autoFps.checked=p.autoDetectFrameRate;landscape.checked=p.landscapeUI;volume.value=p.sfxVolume;musicVolume.value=p.musicVolume;launchFullscreen.checked=p.fullscreen;aspect.value=p.aspect;updateControls()}
function preferences(){return normalizePreferences({targetFps:fps.value,autoDetectFrameRate:autoFps.checked,landscapeUI:landscape.checked,sfxVolume:volume.value,musicVolume:musicVolume.value,fullscreen:launchFullscreen.checked,aspect:aspect.value})}
function showError(message){const box=document.getElementById('error');box.hidden=false;box.textContent=String(message);console.error(message)}
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
function confirmSaveReplacement(){
 const dialog=document.getElementById('replace-save-dialog');
 return new Promise(resolve=>{dialog.addEventListener('close',()=>resolve(dialog.returnValue==='replace'),{once:true});dialog.returnValue='cancel';dialog.showModal()});
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
rewardStatus.textContent=webConfig.bypassBilibiliChecks?'本地版：已绕过 B 站接口。':'给 '+webConfig.rewardVideoBvid+' 投满两个币，解锁领取完美存档与自定义无尽；作者本人可直接使用。';
let rewardBusy=false;
async function authorize(){const {authorizeReward,provideBilibiliToy}=await import('./rewards.js');await authorizeReward(webConfig,provideBilibiliToy)}
async function claimReward(kind){
 if(!runtimeReady||running||rewardBusy)return;
 rewardBusy=true;const controls=[document.getElementById('claim-perfect'),document.getElementById('claim-endless'),start];
 for(const button of controls)button.disabled=true;
 try{
  await authorize();
  if(kind==='custom'){refreshInternalSaves();document.getElementById('endless-dialog').showModal();return}
  // Download/validate before confirmation; a failed transfer never deletes data.
  await (await saveToolsReady).loadSaveBundle();
  if(!await confirmSaveReplacement())return;
  await installBaseline();
  rewardStatus.textContent='完美存档和无尽基准已部署，可开始游戏。';
 }catch(error){rewardStatus.textContent=String(error.message||error)}
 finally{rewardBusy=false;for(const button of controls)button.disabled=false}
}
document.getElementById('claim-perfect').onclick=()=>claimReward('perfect');
document.getElementById('claim-endless').onclick=()=>claimReward('custom');
const endlessDialog=document.getElementById('endless-dialog'),endlessStatus=document.getElementById('endless-status');
const codeInput=document.getElementById('lineup-code'),presetSelect=document.getElementById('endless-preset');
let lineups=[],defaultCode='',defaultLineupCount=0;
const lineupsReady=lineupToolsReady.then(({mergeLineups,defaultLineups})=>{
 defaultCode=defaultLineups[0].code;defaultLineupCount=defaultLineups.length;
 let saved=[];try{saved=JSON.parse(localStorage.getItem('pvz_custom_lineups')||'[]')}catch{}
 lineups=mergeLineups(saved);fillLineups();codeInput.value=defaultCode;
});
function fillLineups(){presetSelect.replaceChildren();for(let i=0;i<lineups.length;i++){const o=document.createElement('option');o.value=i;o.textContent=lineups[i].name;presetSelect.append(o)}}
presetSelect.onchange=async()=>{codeInput.value=lineups[Number(presetSelect.value)].code;try{const decoded=await (await lineupToolsReady).decodeLineup(codeInput.value);document.getElementById('endless-scene').value=decoded.scene}catch(error){endlessStatus.textContent=error.message}};
document.getElementById('close-endless').onclick=()=>{if(!rewardBusy)endlessDialog.close()};
endlessDialog.addEventListener('cancel',event=>{if(rewardBusy)event.preventDefault()});
async function saveToolAction(operation){
 if(running||rewardBusy)return;
 rewardBusy=true;start.disabled=true;
 const buttons=[...endlessDialog.querySelectorAll('button,input,select,textarea')];for(const b of buttons)b.disabled=true;
 try{endlessStatus.textContent='离线工具处理中…';await operation()}catch(error){endlessStatus.textContent=String(error.message||error)}
 finally{rewardBusy=false;start.disabled=false;for(const b of buttons)b.disabled=false}
}
document.getElementById('add-lineup').onclick=()=>saveToolAction(async()=>{
 const decoded=await (await lineupToolsReady).decodeLineup(codeInput.value),name=document.getElementById('lineup-name').value.trim()||'自定义阵型 '+lineups.length;
 lineups.push({name,code:decoded.code});localStorage.setItem('pvz_custom_lineups',JSON.stringify(lineups.slice(defaultLineupCount)));fillLineups();presetSelect.value=lineups.length-1;endlessStatus.textContent='阵型已添加。';
});
function refreshInternalSaves(preferred){
 const select=document.getElementById('internal-save'),previous=preferred||select.value;
 const names=Module.FS.readdir(saveRoot).filter(name=>/^game\d+_\d+\.dat64$/.test(name)).sort();
 select.replaceChildren();
 for(const name of names){const option=document.createElement('option');option.value=name;option.textContent=name;select.append(option)}
 if(names.includes(previous))select.value=previous;else if(names.includes('game1_58.dat64'))select.value='game1_58.dat64';
 if(!names.length){const option=document.createElement('option');option.value='';option.textContent='当前没有已保存的关卡';select.append(option)}
}
function selectedInternalSave(){
 const name=document.getElementById('internal-save').value;
 if(!/^game\d+_\d+\.dat64$/.test(name)||!Module.FS.analyzePath(saveRoot+'/'+name).exists)throw Error('没有可用的关卡档。请先在游戏中保存退出，或领取完美存档部署基准。');
 return {name,bytes:Module.FS.readFile(saveRoot+'/'+name)};
}
document.getElementById('extract-lineup').onclick=()=>saveToolAction(async()=>{
 await syncSaves();const {bytes}=selectedInternalSave();
 if(bytes.length>32*1024*1024)throw Error('DAT64 超过 32 MiB');
 const result=await (await saveToolsReady).runSaveTool({op:'extract'},bytes);
 codeInput.value=await (await lineupToolsReady).encodeLineup(result.metadata);document.getElementById('endless-scene').value=result.metadata.scene;document.getElementById('endless-sun').value=result.metadata.sun;
 endlessStatus.textContent='已提取 '+result.metadata.plants+' 株植物；不包含血量，可添加到列表。';
});
document.getElementById('generate-endless').onclick=()=>saveToolAction(async()=>{
 if(!Module.FS.analyzePath(saveRoot+'/user1.dat64').exists)throw Error('请先领取完美存档，建立 1 号用户。');
 const {decodeLineup,makeRecipe}=await lineupToolsReady;
 const recipe=makeRecipe(await decodeLineup(codeInput.value),Number(document.getElementById('endless-flags').value),Number(document.getElementById('endless-seed').value),Number(document.getElementById('endless-scene').value),Number(document.getElementById('endless-sun').value));
 const result=await (await saveToolsReady).runSaveTool({op:'generate',recipe});
 if(!result.bytes)throw Error('离线工具未生成 DAT64');
 Module.FS.writeFile(saveRoot+'/game1_58.dat64',result.bytes);await syncSaves();refreshInternalSaves('game1_58.dat64');
 endlessStatus.textContent='已生成并部署：'+recipe.flags+' 旗，随机种子 '+recipe.seed+'，阳光 '+recipe.sun+'，无小推车。进入泳池无尽继续游戏。';
});
document.getElementById('export-endless').onclick=async()=>{
 if(running||rewardBusy||!runtimeReady)return;
 try{await syncSaves();const {name,bytes}=selectedInternalSave();(await saveToolsReady).downloadSave(bytes,name);endlessStatus.textContent='已导出 '+name+'。'}catch(error){endlessStatus.textContent=error.message}
};
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
 try{const full=p.fullscreen&&document.documentElement.requestFullscreen?document.documentElement.requestFullscreen().catch(error=>console.warn('浏览器未允许全屏',error)):Promise.resolve();const audio=Module.pvzMusic.unlock();await Promise.all([audio,textInputReady,full]);document.body.classList.add('playing');fitCanvas();await saveSettings(p);running=true;lastWidth=Module.pvzViewport.width;stage.classList.add('running');for(const input of form.elements)input.disabled=true;fitCanvas();canvas.focus();startFps();Module.callMain([])}catch(error){document.body.classList.remove('playing');showError(error.stack||error);start.disabled=false}
});
window.addEventListener('error',event=>showError(event.message));window.addEventListener('unhandledrejection',event=>showError(event.reason?.stack||event.reason));
async function boot(){
 const config=window.PVZGraphicsConfig||{webgpu:false};
 if(config.version){document.title='pvz_perf · 网页 v'+config.version;document.getElementById('web-version').textContent='网页 v'+config.version}
 const dropdown=document.getElementById('graphics-backend');
 document.getElementById('graphics-field').hidden=false;
 dropdown.replaceChildren(...(config.fallback?[new Option('自动（优先 WebGPU）','auto'),new Option('WebGPU','webgpu'),new Option('WebGL 2','webgl')]:[new Option(config.webgpu?'WebGPU':'WebGL 2',config.webgpu?'webgpu':'webgl')]));
 let mode=config.webgpu?'webgpu':'webgl';
 if(config.fallback){try{mode=localStorage.getItem('pvz_graphics_backend')||'auto'}catch{}if(!['auto','webgpu','webgl'].includes(mode))mode='auto';}
 dropdown.value=mode;dropdown.disabled=!config.fallback;
 dropdown.onchange=()=>{try{localStorage.setItem('pvz_graphics_backend',dropdown.value)}catch{}location.reload()};
 const {detectGraphics,capabilityText}=await import('./graphics.js');
 const capabilities=await detectGraphics(navigator.gpu,mode==='webgpu'?'webgpu':'auto',config.webgpu);
 document.getElementById('graphics-capabilities').textContent=capabilityText(capabilities);
 if(config.webgpu&&!config.fallback&&!capabilities.webgpu)throw Error('此包只包含 WebGPU，当前设备不能启动：'+capabilities.reason);
 const backend=mode==='webgl'?'webgl':capabilities.webgpu?'webgpu':'webgl';
 document.getElementById('graphics-info').textContent='当前使用 '+(backend==='webgpu'?'WebGPU':'WebGL 2')+'；切换接口会重新加载设置页，存档保留。';
 Module.pvzSelectedBackend=backend;Module.pvzGraphicsCapabilities=capabilities;
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
Promise.all([textInputReady,saveToolsReady,lineupsReady,import('./audio/music-engine.js').then(m=>m.prepareMusic()).then(audio=>{Module.pvzMusic=audio;if(runtimeReady)start.disabled=false}),boot()]).catch(showError);
