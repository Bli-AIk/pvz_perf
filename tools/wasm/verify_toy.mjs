import fs from 'node:fs';
import path from 'node:path';
const {chromium}=await import(process.env.PVZ_PLAYWRIGHT_MODULE || 'playwright-core');
const output=path.resolve('out/wasm/verification');fs.mkdirSync(output,{recursive:true});
const browser=await chromium.launch({headless:true,executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',args:['--enable-webgl','--ignore-gpu-blocklist']});
const events=[];
try {
 const portrait=process.argv.includes('--portrait');
 const page=await browser.newPage(portrait?{viewport:{width:390,height:844},isMobile:true,hasTouch:true}:{viewport:{width:1440,height:900}});
 if(process.argv.includes('--audio-fallback'))await page.addInitScript(()=>Object.defineProperty(AudioContext.prototype,'audioWorklet',{get(){return undefined}}));
 page.on('console',message=>events.push({type:message.type(),text:message.text()}));
 page.on('pageerror',error=>events.push({type:'pageerror',text:String(error)}));
 await page.goto('http://127.0.0.1:18088/');
 await page.waitForFunction(()=>!document.getElementById('start').disabled,null,{timeout:90000});
 const baseline=await page.evaluate(()=>Module.FS.stat('/persist/development-v331/game1_58.dat64').size);
 if(baseline!==673781)throw Error('Canonical development save missing');
 const firstAid=await page.evaluate(()=>{const b=Module.FS.readFile('/persist/development-v331/user1.dat64');return new DataView(b.buffer,b.byteOffset,b.byteLength).getInt32(540,true)});
 if(firstAid!==1)throw Error('Completed development profile lacks Wall-nut First Aid');
 await page.locator('#auto-fps').uncheck();await page.locator('#target-fps').fill('240');
 await page.locator('#aspect').selectOption('21:9');await page.locator('#sfx-volume').fill('37');
 if(portrait)await page.locator('#landscape-ui').check();
 await page.screenshot({path:path.join(output,'launcher.png')});
 await page.locator('#start').click();
 await page.waitForFunction(()=>document.body.classList.contains('playing')&&Module.FS.analyzePath('/persist/development-v331/settings.json').exists || document.getElementById('error').textContent,null,{timeout:20000});
 if(await page.locator('#error').textContent())throw Error(await page.locator('#error').textContent());
 const state=await page.evaluate(()=>({settings:JSON.parse(Module.FS.readFile('/persist/development-v331/settings.json',{encoding:'utf8'})),fullscreen:!!document.fullscreenElement,width:document.getElementById('stage').clientWidth,height:document.getElementById('stage').clientHeight}));
 if(state.fullscreen || (!portrait&&(state.width!==1440 || state.height!==900)))throw Error('Game must fill the webpage, not enter display fullscreen');
 if(portrait&&!await page.evaluate(()=>document.body.classList.contains('portrait-game')&&Module.pvzViewport.width===1400))throw Error('Portrait phone did not retain landscape dimensions');
 if(state.settings.targetFps!==240 || state.settings.sfxVolume!==37)throw Error('Launcher settings not applied');
 if(portrait&&!state.settings.landscapeUI)throw Error('Portrait phone disabled landscape UI');
 const click=async(x,y)=>{const b=await page.locator('#canvas').boundingBox();if(portrait)await page.touchscreen.tap(b.x+(1-y/600)*b.width,b.y+x/1400*b.height);else await page.mouse.click(b.x+x*b.width/1400,b.y+y*b.height/600)};
 await page.waitForTimeout(16000);await click(700,550);
 await page.waitForFunction(()=>Module._pvz_web_scene()===1,null,{timeout:15000});
 await page.waitForTimeout(2500); // The menu scene starts before its entrance animation finishes.
 await page.waitForFunction(()=>/^FPS \d/.test(document.getElementById('game-fps').textContent));
 await page.screenshot({path:path.join(output,'menu.png')});
 const audio=await page.evaluate(async()=>{
  const e=Module.pvzMusic,a=e.context.createAnalyser(),sink=e.context.createGain();sink.gain.value=0;e.master.connect(a);a.connect(sink);sink.connect(e.context.destination);
  const samples=new Float32Array(a.fftSize);let peak=0;
  for(let i=0;i<12;++i){await new Promise(r=>setTimeout(r,30));a.getFloatTimeDomainData(samples);for(const x of samples)peak=Math.max(peak,Math.abs(x))}
  e.master.disconnect(a);a.disconnect();sink.disconnect();
  const voice=Array.from(e.voices.values()).find(v=>!v.stream&&v.playing&&!v.paused&&v.gain>0);if(!voice)throw Error('No playing module music');
  e.command(voice.id,'pause');await new Promise(r=>setTimeout(r,100));const paused=voice.position;
  await new Promise(r=>setTimeout(r,150));const held=voice.position;e.command(voice.id,'resume');await new Promise(r=>setTimeout(r,250));
  return {backend:e.backend,state:e.context.state,peak,pauseHeld:paused===held,resumed:voice.position!==held,error:e.error};
 });
 if(audio.peak<=.000001||!audio.pauseHeld||!audio.resumed||audio.error)throw Error('Music graph/pause test failed: '+JSON.stringify(audio));
 console.log('audio',JSON.stringify(audio));
 const sound=await page.evaluate(async()=>{
  const e=Module.pvzMusic,s=e.sound,entries=Array.from(s.sources.entries());
  if(!entries.length||entries.some(([,v])=>!v.buffer))throw Error('Sound preload incomplete: '+entries.length+' sources, '+entries.filter(([,v])=>!v.buffer).length+' pending');
  const a=e.context.createAnalyser(),sink=e.context.createGain();sink.gain.value=0;s.master.connect(a);a.connect(sink);sink.connect(e.context.destination);
  const id=entries.find(([,v])=>v.buffer.duration>.3)[0],started=s.play(id,1000,true,1,0,0),samples=new Float32Array(a.fftSize);let peak=0;
  for(let i=0;i<8;++i){await new Promise(r=>setTimeout(r,30));a.getFloatTimeDomainData(samples);for(const x of samples)peak=Math.max(peak,Math.abs(x))}
  const offset=s.pause(1000),paused=!s.playing(1000);s.resume(1000,offset,true);const resumed=s.playing(1000);
  s.parameters(1000,.5,0,5000);const pan=s.channels.get(1000).pan.pan.value;s.stop(1000);s.master.disconnect(a);a.disconnect();sink.disconnect();
  return {buffers:entries.length,started,peak,paused,resumed,pan,baseLatency:e.context.baseLatency,outputLatency:e.context.outputLatency};
 });
 if(!sound.started||sound.peak<=.000001||!sound.paused||!sound.resumed||sound.pan!==.5)throw Error('Sound graph test failed: '+JSON.stringify(sound));
 console.log('sound',JSON.stringify(sound));

 const endless=process.argv.includes('--endless');
 if(endless){
  await click(850,380);await page.waitForFunction(()=>Module._pvz_web_scene()===7);
  await click(700,470);await page.waitForFunction(()=>Module._pvz_web_scene()===3&&Module._pvz_web_has_board());
  await page.screenshot({path:path.join(output,'endless-continue.png')});
 } else {
  await click(1040,530);await page.waitForTimeout(400);await click(600,395);
  await page.waitForFunction(()=>!document.body.classList.contains('playing')&&!document.getElementById('start').disabled,null,{timeout:90000});
  if(await page.locator('#start').textContent()!=='开始游戏')throw Error('Quit left an extra restart step');
  if(await page.locator('#game-fps').isVisible())throw Error('FPS overlay remained after Quit');
  await page.waitForFunction(()=>getComputedStyle(document.getElementById('cover')).display!=='none');
  await page.screenshot({path:path.join(output,'after-quit.png')});
  const settings=await page.evaluate(()=>JSON.parse(Module.FS.readFile('/persist/development-v331/settings.json',{encoding:'utf8'})));
  if(settings.targetFps!==240 || settings.sfxVolume!==37)throw Error('Settings did not survive page reload');
 }
 const errors=events.filter(e=>e.type==='error'||e.type==='pageerror');if(errors.length)throw Error(JSON.stringify(errors));
 const result={browser:'Edge headless',audio,sound,checks:['music_graph_and_pause_resume','predecoded_sound_graph_pan_pause_resume','development_profile','page_fill_without_fullscreen_api','settings','fps',endless?'endless_checkpoint_load':'quit_to_settings_and_reload'],errors};
 fs.writeFileSync(path.join(output,endless?'endless-result.json':'result.json'),JSON.stringify(result,null,2));console.log(JSON.stringify(result));
}finally{fs.writeFileSync(path.join(output,'events.json'),JSON.stringify(events,null,2));await browser.close();}
