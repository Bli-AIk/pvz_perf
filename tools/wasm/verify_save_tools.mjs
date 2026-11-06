import fs from 'node:fs';
import assert from 'node:assert/strict';
const {chromium}=await import(process.env.PVZ_PLAYWRIGHT_MODULE);
const browser=await chromium.launch({headless:true,executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',args:['--autoplay-policy=no-user-gesture-required']});
try{
 const page=await browser.newPage();page.on('console',m=>{if(m.type()==='error'||m.text().includes('save tool:'))console.log(m.text())});page.on('pageerror',e=>console.log('PAGE ERROR',String(e)));
 await page.goto(process.env.PVZ_SAVE_TEST_URL||'http://127.0.0.1:18088/');await page.waitForFunction(()=>!document.getElementById('start').disabled,null,{timeout:90000});
 console.log('runtime ready');
 const testFlags=process.argv.includes('--zero-flags')?0:128;
 const result=await page.evaluate(async flags=>{
  const {runSaveTool,loadSaveBundle}=await import('./save-tools.js');
  const {decodeLineup,makeRecipe,encodeLineup}=await import('./lineups.js');
  const code=document.getElementById('lineup-code').value;
  const lineup=await decodeLineup(code);
  const generated=await runSaveTool({op:'generate',recipe:makeRecipe(lineup,flags,114514,3,4321)});
  window.testGenerated=generated.bytes;
  const extracted=await runSaveTool({op:'extract'},generated.bytes);
  return {metadata:generated.metadata,roundtrip:extracted.metadata,code:await encodeLineup(extracted.metadata),length:generated.bytes.length};
 },testFlags);
 assert.equal(result.metadata.flags,testFlags);assert.equal(result.metadata.seed,114514);assert.equal(result.metadata.sun,4321);assert.equal(result.metadata.mowers,0);assert.equal(result.roundtrip.sun,4321);assert.equal(result.roundtrip.mowers,0);assert.equal(result.metadata.scene,3);assert.equal(result.metadata.plants,85);assert.equal(result.metadata.healthyPlants,85);assert.equal(result.metadata.cards,0);
 assert.deepEqual(result.metadata.cells,result.roundtrip.cells);assert.equal(result.metadata.repickPending,true);assert.equal(result.metadata.music.tune,6);assert.equal(result.metadata.music.order,0x7A);
 console.log(JSON.stringify(result));
 const scenes=process.argv.includes('--quick')?[]:await page.evaluate(async()=>{
  const {runSaveTool}=await import('./save-tools.js');const results=[];
  for(let scene=0;scene<6;scene++){
   const cells=Array(54).fill(0);cells[9]=1024|256|(scene>=4?128:0);
   const result=await runSaveTool({op:'generate',recipe:{cells,flags:128,seed:114514,scene,rake:0}});
   results.push(result.metadata);
  }
  return results;
 });
 for(let scene=0;scene<scenes.length;scene++){assert.equal(scenes[scene].scene,scene);assert.equal(scenes[scene].flags,128);assert.equal(scenes[scene].cards,0);assert.equal(scenes[scene].healthyPlants,scene>=4?2:1);assert.equal(scenes[scene].repickPending,true);assert.equal(scenes[scene].music.tune,6);assert.equal(scenes[scene].music.order,0x7A)}
 if(scenes.length)console.log('PASS: all six scenes serialize, empty previous hand and chooser boundary, full-health plants, DAT64 roundtrip');
 // Deploy the generated fog checkpoint into the launcher's FS, then exercise
 // the real menu/Continue/load/draw path with independent UI settings.
 await page.evaluate(()=>Module.FS.writeFile('/persist/development-v331/game1_58.dat64',window.testGenerated));
 await page.locator('#auto-fps').uncheck();await page.locator('#target-fps').fill('120');await page.locator('#aspect').selectOption('21:9');
 await page.locator('#sfx-volume').fill('37');await page.locator('#music-volume').fill('42');await page.locator('#launch-fullscreen').check();
 await page.locator('#start').click();await page.waitForTimeout(16000);
 const settings=await page.evaluate(()=>JSON.parse(Module.FS.readFile('/persist/development-v331/settings.json',{encoding:'utf8'})));
 assert.equal(settings.sfxVolume,37);assert.equal(settings.musicVolume,42);assert.equal(settings.screenMode,'fullscreen');
 assert.equal(await page.evaluate(()=>!!document.fullscreenElement),true);
 const click=async(x,y)=>{const b=await page.locator('#canvas').boundingBox();await page.mouse.click(b.x+x*b.width/1400,b.y+y*b.height/600)};
 await click(700,550);await page.waitForFunction(()=>Module._pvz_web_scene()===1,null,{timeout:20000});await page.waitForTimeout(2500);
 await click(850,380);await page.waitForFunction(()=>Module._pvz_web_scene()===7);await click(700,470);await page.waitForFunction(()=>Module._pvz_web_has_board());await page.waitForTimeout(500);await click(500,370);await page.waitForTimeout(2500);
 await page.waitForFunction(()=>Module._pvz_web_scene()===2,null,{timeout:15000});await page.waitForTimeout(6500);
 const music=await page.evaluate(()=>Array.from(Module.pvzMusic.voices.values()).filter(v=>v.playing).map(v=>({tempo:v.tempo,speed:v.speed,order:v.position&65535})));
 const expected=await page.evaluate(async data=>{
  const {MusicMixer}=await import('./audio/music-mixer.js');const mixer=await MusicMixer.create(Module.pvzMusic.compiled,48000);
  try{mixer.command({op:'load',id:1,data:Uint8Array.from(atob(data),c=>c.charCodeAt(0))});const states=[];for(const order of [0x7A,0x7D]){mixer.command({op:'seek',id:1,order,row:0,revision:0});mixer.command({op:'play',id:1});states.push(mixer.positions()[0])}return states}finally{mixer.close()}
 },fs.readFileSync('assets/runtime/sounds/mainmusic.mo3').toString('base64'));
 assert.equal(result.metadata.music.tempo,expected[0].tempo);assert.equal(result.metadata.music.speed,expected[0].speed);
 assert.equal(music[0].tempo,expected[0].tempo);assert.equal(music[0].speed,expected[0].speed);
 console.log('Chooser state and natural music:',JSON.stringify({scene:2,generated:result.metadata.music,playing:music}));
 await page.screenshot({path:process.env.TEMP+'/pvz-generated-chooser.png'});
 for(const seed of [0,1,2,3,4,5,6,7,16,27]){await click(22+(seed%8)*53+25,123+Math.floor(seed/8)*70+35);await page.waitForTimeout(100)}
 await page.waitForTimeout(500);await page.screenshot({path:process.env.TEMP+'/pvz-generated-selected.png'});await click(230,565);await page.waitForTimeout(1500);await page.screenshot({path:process.env.TEMP+'/pvz-generated-started.png'});
 await page.waitForFunction(()=>Module._pvz_web_scene()===3,null,{timeout:20000});await page.waitForTimeout(2000);
 const state=await page.evaluate(()=>({scene:Module._pvz_web_scene(),paused:Module._pvz_web_paused(),music:Array.from(Module.pvzMusic.voices.values()).filter(v=>v.playing).map(v=>({tempo:v.tempo,speed:v.speed,order:v.position&65535}))}));
 assert.equal(state.scene,3);assert.equal(state.paused,0);assert.equal(state.music[0].speed,expected[1].speed);assert.equal(state.music[0].tempo,expected[1].tempo);
 console.log('Battle natural music:',JSON.stringify(state));
}finally{await browser.close()}
