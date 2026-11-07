import assert from 'node:assert/strict';
const {chromium}=await import(process.env.PVZ_PLAYWRIGHT_MODULE);
const browser=await chromium.launch({headless:true,executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',args:['--enable-unsafe-webgpu','--ignore-gpu-blocklist']});
const base=process.env.PVZ_WEBGPU_URL||'http://127.0.0.1:18089/';
try{
 for(const mode of ['missing-ack','command-error','module-error','script-processor']){
  const context=await browser.newContext(),page=await context.newPage(),errors=[];page.on('pageerror',e=>errors.push(e.message));
  await page.addInitScript(mode=>{
   localStorage.setItem('pvz_graphics_backend','webgl');
   // Only accelerate the music barrier timeout, not game time or render pacing.
   const timer=window.setTimeout;window.setTimeout=(fn,delay,...args)=>timer(fn,mode==='missing-ack'&&delay===5000?40:delay,...args);
   if(mode==='script-processor')Object.defineProperty(AudioContext.prototype,'audioWorklet',{get(){return undefined}});
   if(mode==='module-error'){
    const OriginalContext=AudioContext;
    window.AudioContext=class extends OriginalContext{constructor(...args){super(...args);this.audioWorklet.addModule=async()=>{throw Error('injected module failure')}}};
   }
  },mode);
  await page.goto(base);await page.waitForFunction(()=>!document.getElementById('start').disabled||!document.getElementById('error').hidden,null,{timeout:120000});
  assert.equal(await page.locator('#error').isVisible(),false,await page.locator('#error').textContent());
  if(mode==='missing-ack')await page.evaluate(()=>{
   const engine=Module.pvzMusic,unlock=engine.unlock.bind(engine);
   engine.unlock=async()=>{await unlock();const receive=engine.node.port.onmessage;engine.node.port.onmessage=event=>{if(!event.data.flush)receive(event)}};
  });
  await page.locator('#start').click();
  await page.waitForFunction(()=>Module._pvz_web_loading_complete?.()&&Module._pvz_web_draws()>0||!document.getElementById('error').hidden,null,{timeout:120000});
  // Exercise the real worklet's catch/postMessage path, not an HTTP route:
  // Chromium may serve worklet imports outside Page.route interception.
  if(mode==='command-error')await page.evaluate(()=>Module.pvzMusic.send({op:'load',id:999999,data:new Uint8Array([1,2,3,4])}));
  if(mode!=='script-processor')await page.waitForFunction(()=>Module.pvzMusic.disabled||!document.getElementById('error').hidden,null,{timeout:15000});
  assert.equal(await page.locator('#error').isVisible(),false,await page.locator('#error').textContent());
  const state=await page.evaluate(()=>({backend:Module.pvzMusic.backend,disabled:Module.pvzMusic.disabled,error:Module.pvzMusic.error,pending:Module.pvzMusic.flushes.size,effects:Module.pvzMusic.sound.sources.size,heap:Module.pvzMemory?.buffer.byteLength}));
  assert.ok(state.effects>0);assert.equal(state.pending,0);assert.ok(state.heap>0);
  if(mode==='script-processor'){assert.equal(state.backend,'ScriptProcessor fallback');assert.equal(state.disabled,false);assert.equal(await page.evaluate(()=>Module.pvzMusic.flush().then(Boolean)),true)}
  else{assert.equal(state.disabled,true);assert.equal(await page.locator('#audio-warning').isVisible(),true);if(mode==='command-error')assert.match(state.error,/Cannot decode MO3 voice 999999/)}
  const previous=await page.evaluate(()=>Module._pvz_web_draws());await page.waitForTimeout(100);assert.ok(await page.evaluate(()=>Module._pvz_web_draws())>previous,'Rendering must continue after audio failure');
  assert.deepEqual(errors,[]);console.log(mode+': loaded, kept effects and rendering, no fatal error; '+JSON.stringify(state));await context.close();
 }
}finally{await browser.close()}
