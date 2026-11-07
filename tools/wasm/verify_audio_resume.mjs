import assert from 'node:assert/strict';
const {chromium}=await import(process.env.PVZ_PLAYWRIGHT_MODULE);
const browser=await chromium.launch({headless:true,executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',args:['--enable-unsafe-webgpu','--ignore-gpu-blocklist']});
try{
 const page=await browser.newPage(),errors=[];page.on('pageerror',error=>errors.push(error.message));
 await page.addInitScript(()=>localStorage.setItem('pvz_graphics_backend','webgl'));
 await page.goto(process.env.PVZ_WEBGPU_URL||'http://127.0.0.1:18089/');
 await page.waitForFunction(()=>!document.getElementById('start').disabled,null,{timeout:120000});
 await page.locator('#start').click();
 await page.waitForFunction(()=>Module._pvz_web_loading_complete?.()&&Module._pvz_web_draws()>0,null,{timeout:120000});
 await page.evaluate(async()=>{
  const context=Module.pvzMusic.context,resume=context.resume.bind(context);
  window.resumeCalls=0;context.resume=()=>{window.resumeCalls++;return resume()};await context.suspend();
 });
 await page.locator('#canvas').click({position:{x:15,y:15}});
 await page.waitForFunction(()=>Module.pvzMusic.context.state==='running');
 assert.ok(await page.evaluate(()=>window.resumeCalls)>0,'Canvas input must restore suspended audio');
 const before=await page.evaluate(()=>window.resumeCalls);
 await page.locator('#canvas').click({position:{x:20,y:15}});
 assert.equal(await page.evaluate(()=>window.resumeCalls),before,'Running audio must not incur resume requests');
 const result=await page.evaluate(async()=>{
  const engine=Module.pvzMusic,context=engine.context,sound=engine.sound;
  const analyser=context.createAnalyser(),mute=context.createGain();mute.gain.value=0;
  sound.master.connect(analyser);analyser.connect(mute);mute.connect(context.destination);
  const samples=new Float32Array(Math.round(context.sampleRate*.3));samples.fill(.25);
  const id=2147483647,channel=2147483647;sound.loadPCM(id,samples,1,context.sampleRate);
  const started=performance.now();sound.play(id,channel,false,1,0,0);
  const sourceCreatedSynchronously=!!sound.channels.get(channel)?.source;
  const data=new Float32Array(analyser.fftSize);let graphSignalMs=null;
  for(let i=0;i<100;i++){
   analyser.getFloatTimeDomainData(data);
   if(data.some(value=>Math.abs(value)>.01)){graphSignalMs=performance.now()-started;break}
   await new Promise(resolve=>setTimeout(resolve,5));
  }
  sound.stop(channel);sound.release(id);sound.master.disconnect(analyser);analyser.disconnect();mute.disconnect();
  return {state:context.state,sourceCreatedSynchronously,graphSignalMs,sampleRate:context.sampleRate,baseLatency:context.baseLatency,outputLatency:context.outputLatency,scope:'Desktop Chromium graph timing only; not iPhone or acoustic end-to-end latency'};
 });
 assert.equal(result.sourceCreatedSynchronously,true);assert.notEqual(result.graphSignalMs,null);
 assert.deepEqual(errors,[]);console.log(JSON.stringify(result,null,2));
}finally{await browser.close()}
