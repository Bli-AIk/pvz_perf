import assert from 'node:assert/strict';
const {chromium}=await import(process.env.PVZ_PLAYWRIGHT_MODULE);
const browser=await chromium.launch({headless:true,executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',args:['--enable-unsafe-webgpu','--ignore-gpu-blocklist']});
try{
 const context=await browser.newContext({viewport:{width:1440,height:900}});
 const page=await context.newPage(),errors=[],requests=[];
 page.on('pageerror',e=>errors.push(String(e)));
 page.on('request',r=>{if(/pvz-perf.*\.wasm\.gz/.test(r.url()))requests.push(r.url())});
 await page.goto(process.env.PVZ_WEBGPU_URL||'http://127.0.0.1:18092/');
 const ready=async()=>{
  await page.waitForFunction(()=>!document.getElementById('start').disabled||!document.getElementById('error').hidden,null,{timeout:120000});
  assert.equal(await page.locator('#error').isVisible(),false,await page.locator('#error').textContent());
 };
 await ready();assert.equal(await page.locator('#memory-model').inputValue(),'wasm32');
 assert.deepEqual(await page.locator('#memory-model option').evaluateAll(xs=>xs.map(x=>x.value)),['wasm32','wasm64']);
 const change=async(id,value)=>{
  if(await page.locator(id).inputValue()===value)return;
  await page.locator('#advanced-settings').evaluate(el=>el.open=true);
  await Promise.all([page.waitForEvent('load'),page.locator(id).selectOption(value)]);await ready();
 };
 const click=async(x,y)=>{const r=await page.locator('#canvas').boundingBox();await page.mouse.click(r.x+x/1400*r.width,r.y+y/600*r.height)};
 let completed=0,checkpoint=null;
 for(const [memory,backend] of [['wasm32','webgpu'],['wasm64','webgpu'],['wasm64','webgl'],['wasm32','webgl']]){
  await change('#memory-model',memory);await change('#graphics-backend',backend);
  const stem='pvz-perf'+(backend==='webgl'?'-webgl':'')+(memory==='wasm64'?'-wasm64':'');
  assert.equal(await page.evaluate(()=>Module.pvzRuntimeStem),stem);
  assert.equal(await page.evaluate(()=>Module.pvzMemoryModel),memory);
  const metadata=await page.evaluate(async()=>{
   const {runSaveTool}=await import('./save-tools.js');
   const bytes=Module.FS.readFile('/persist/development-v331/game1_58.dat64');
   return (await runSaveTool({op:'extract'},bytes)).metadata;
  });
  if(checkpoint)assert.deepEqual(metadata,checkpoint,'The same DAT64 must decode identically across memory models/backends');
  else checkpoint=metadata;
  if(completed){
   assert.equal(await page.locator('#sfx-volume').inputValue(),'43');
   assert.equal(await page.evaluate(()=>Module.FS.readFile('/persist/development-v331/memory-model-probe.txt',{encoding:'utf8'})),'shared');
  }
  await page.evaluate(async()=>{Module.FS.writeFile('/persist/development-v331/memory-model-probe.txt','shared');await new Promise((resolve,reject)=>Module.FS.syncfs(false,e=>e?reject(e):resolve()))});
  await page.locator('#advanced-settings').evaluate(el=>el.open=true);
  await page.locator('#aspect').selectOption('21:9');await page.locator('#auto-fps').uncheck();
  await page.locator('#target-fps').fill('120');await page.locator('#sfx-volume').fill('43');
  await page.locator('#start').click();
  await page.waitForFunction(()=>Module._pvz_web_loading_complete?.()&&Module._pvz_web_draws()>3||!document.getElementById('error').hidden,null,{timeout:120000});
  assert.equal(await page.locator('#error').isVisible(),false,await page.locator('#error').textContent());
  await page.waitForTimeout(2500); // Title entrance follows resource readiness.
  await click(700,550);await page.waitForFunction(()=>Module._pvz_web_scene()===1,null,{timeout:25000});
  await page.waitForTimeout(2500);await click(1040,530);await page.waitForTimeout(400);await click(600,395);
  await page.waitForFunction(()=>!document.body.classList.contains('playing')&&!document.getElementById('start').disabled,null,{timeout:60000});
  assert.equal(await page.evaluate(()=>Module._pvz_web_scene()),-1);
  assert.equal(await page.locator('#memory-model').isDisabled(),false);
  console.log('PASS '+memory+'/'+backend+': exact runtime, loaded/drawn, normal Quit, shared settings/IDBFS');
  ++completed;
 }
 assert.deepEqual(errors,[]);await context.close();
}finally{await browser.close()}
