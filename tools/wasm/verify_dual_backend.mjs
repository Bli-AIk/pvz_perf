import assert from 'node:assert/strict';
const {chromium}=await import(process.env.PVZ_PLAYWRIGHT_MODULE);
const browser=await chromium.launch({headless:true,executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',args:['--enable-unsafe-webgpu','--ignore-gpu-blocklist']});
try {
 for(const mode of ['auto','webgl','unavailable']) {
  const context=await browser.newContext();const page=await context.newPage();const errors=[];
  page.on('pageerror',error=>errors.push(String(error)));
  await page.addInitScript(mode=>{if(mode==='webgl')localStorage.setItem('pvz_graphics_backend','webgl');if(mode==='unavailable')Object.defineProperty(navigator,'gpu',{value:undefined});},mode);
  await page.goto(process.env.PVZ_WEBGPU_URL||'http://127.0.0.1:18089/');
  await page.waitForFunction(()=>!document.getElementById('start').disabled,null,{timeout:120000});
  const expected=mode==='auto'?'webgpu':'webgl';
  assert.equal(await page.evaluate(()=>Module.pvzSelectedBackend),expected);
  assert.equal(await page.locator('#graphics-backend option').count(),3);
  assert.equal(await page.evaluate(()=>Module.FS.analyzePath('/persist/development-v331/user1.dat64').exists),true);
  await page.locator('#start').click();
  await page.waitForFunction(()=>Module._pvz_web_scene&&Module._pvz_web_scene()===0,null,{timeout:60000});
  assert.deepEqual(errors,[]);console.log(mode+': '+expected+' loads shared saves and starts');
  await context.close();
 }
}finally{await browser.close()}
