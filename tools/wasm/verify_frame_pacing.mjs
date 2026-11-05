import fs from 'node:fs';
import path from 'node:path';
const {chromium}=await import(process.env.PVZ_PLAYWRIGHT_MODULE || 'playwright-core');
const output=path.resolve('out/wasm/verification');fs.mkdirSync(output,{recursive:true});
const browser=await chromium.launch({headless:true,executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',args:['--enable-webgl','--ignore-gpu-blocklist']});
const events=[];const target=Number(process.argv.find(x=>x.startsWith('--target='))?.split('=')[1]||240);
try {
 const portrait=process.argv.includes('--portrait');
 const page=await browser.newPage(portrait?{viewport:{width:390,height:844},isMobile:true,hasTouch:true}:{viewport:{width:1440,height:900}});
 if(process.argv.includes('--audio-fallback'))await page.addInitScript(()=>Object.defineProperty(AudioContext.prototype,'audioWorklet',{get(){return undefined}}));
 page.on('console',message=>events.push({type:message.type(),text:message.text()}));
 page.on('pageerror',error=>events.push({type:'pageerror',text:String(error)}));
 await page.goto('http://127.0.0.1:18088/');
 await page.waitForFunction(()=>!document.getElementById('start').disabled,null,{timeout:90000});
 const baseline=await page.evaluate(()=>Module.FS.stat('/persist/development-v331/game1_58.dat64').size);
 if(baseline!==673781)throw Error('Canonical development save missing');await page.evaluate(()=>{const p='/persist/development-v331/game1_58.dat64',b=Module.FS.readFile(p),v=new DataView(b.buffer,b.byteOffset,b.byteLength);v.setFloat32(673701,155,true);v.setFloat32(673705,3,true);Module.FS.writeFile(p,b)});
 const firstAid=await page.evaluate(()=>{const b=Module.FS.readFile('/persist/development-v331/user1.dat64');return new DataView(b.buffer,b.byteOffset,b.byteLength).getInt32(540,true)});
 if(firstAid!==1)throw Error('Completed development profile lacks Wall-nut First Aid');
 await page.locator('#auto-fps').uncheck();await page.locator('#target-fps').fill(String(target));
 await page.locator('#aspect').selectOption('21:9');await page.locator('#sfx-volume').fill('37');
 if(portrait)await page.locator('#landscape-ui').check();
 await page.screenshot({path:path.join(output,'launcher.png')});
 await page.locator('#start').click();
 await page.waitForFunction(()=>document.body.classList.contains('playing')&&Module.FS.analyzePath('/persist/development-v331/settings.json').exists || document.getElementById('error').textContent,null,{timeout:20000});
 if(await page.locator('#error').textContent())throw Error(await page.locator('#error').textContent());
 const state=await page.evaluate(()=>({settings:JSON.parse(Module.FS.readFile('/persist/development-v331/settings.json',{encoding:'utf8'})),fullscreen:!!document.fullscreenElement,width:document.getElementById('stage').clientWidth,height:document.getElementById('stage').clientHeight}));
 if(state.fullscreen || (!portrait&&(state.width!==1440 || state.height!==900)))throw Error('Game must fill the webpage, not enter display fullscreen');
 if(portrait&&!await page.evaluate(()=>document.body.classList.contains('portrait-game')&&Module.pvzViewport.width===1400))throw Error('Portrait phone did not retain landscape dimensions');
 if(state.settings.targetFps!==target || state.settings.sfxVolume!==37)throw Error('Launcher settings not applied');
 if(portrait&&!state.settings.landscapeUI)throw Error('Portrait phone disabled landscape UI');
 const click=async(x,y)=>{const b=await page.locator('#canvas').boundingBox();if(portrait)await page.touchscreen.tap(b.x+(1-y/600)*b.width,b.y+x/1400*b.height);else await page.mouse.click(b.x+x*b.width/1400,b.y+y*b.height/600)};
 await page.waitForTimeout(16000);await click(700,550);
 await page.waitForFunction(()=>Module._pvz_web_scene()===1,null,{timeout:15000});
 await page.waitForTimeout(2500); // The menu scene starts before its entrance animation finishes.
 await page.waitForFunction(()=>/^FPS \d/.test(document.getElementById('game-fps').textContent));
 await page.screenshot({path:path.join(output,'menu.png')});
 const sample=await page.evaluate(async()=>{const t=performance.now(),d=Module._pvz_web_draws();let raf=0;await new Promise(resolve=>{const step=()=>{raf++;if(performance.now()-t<3000)requestAnimationFrame(step);else resolve()};requestAnimationFrame(step)});return{seconds:(performance.now()-t)/1000,draws:Module._pvz_web_draws()-d,raf}});console.log(JSON.stringify(sample));if(sample.draws>Math.min(sample.raf,target*sample.seconds)*1.2||sample.draws<Math.min(sample.raf,target*sample.seconds)*.7)throw Error('Unexpected draw/RAF pacing ratio');}finally{await browser.close()}
