import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
const {chromium}=await import(process.env.PVZ_PLAYWRIGHT_MODULE);
const base=process.env.PVZ_WEBGPU_URL||'http://127.0.0.1:18089/';
const output=path.join(os.tmpdir(),'pvz-startup-compat');fs.mkdirSync(output,{recursive:true});
const browser=await chromium.launch({headless:true,executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',args:['--enable-unsafe-webgpu','--ignore-gpu-blocklist']});
try{
 for(const backend of ['webgl','webgpu']){
  const context=await browser.newContext(backend==='webgl'?{viewport:{width:390,height:844},hasTouch:true,isMobile:true,userAgent:'Mozilla/5.0 (iPhone; CPU iPhone OS 18_0 like Mac OS X) AppleWebKit/605.1.15 Version/18.0 Mobile/15E148 Safari/604.1'}:{viewport:{width:1280,height:900}});
  await context.addInitScript(backend=>{
   localStorage.setItem('pvz_graphics_backend',backend);
   if(backend==='webgl')Object.defineProperty(navigator,'gpu',{value:undefined});
  },backend);
  const page=await context.newPage(),errors=[],htmlRequests=[];
  page.on('pageerror',e=>errors.push(e.message));
  // Toy can return a viewer/password page for every secondary HTML resource.
  // The editor and isolated DAT64 runtime must not make any such request.
  await page.route('**/*.html',route=>{
   if(route.request().url()===base||route.request().isNavigationRequest()&&route.request().frame()===page.mainFrame())return route.continue();
   htmlRequests.push(route.request().url());
   return route.fulfill({contentType:'text/html',body:'<!doctype html><h1>需要访问密码</h1>'});
  });
  await page.goto(base);
  await page.waitForFunction(()=>!document.getElementById('start').disabled||!document.getElementById('error').hidden,null,{timeout:120000});
  assert.equal(await page.locator('#error').isVisible(),false,await page.locator('#error').innerText());
  assert.equal(await page.evaluate(()=>Module.pvzSelectedBackend),backend);
  await page.locator('#custom-save').click();
  const editor=page.frameLocator('#save-editor-frame');
  await editor.locator('#status').filter({hasText:'已打开'}).waitFor();
  assert.equal(await editor.locator('#profile').isVisible(),true);
  await editor.locator('[data-tab=lineup]').click();await editor.locator('#generate').click();
  await editor.locator('#status').filter({hasText:'已生成并部署 game1_58.dat64'}).waitFor({timeout:180000});
  await editor.locator('#close').click();assert.deepEqual(htmlRequests,[]);
  await page.locator('#start').click();
  await page.waitForFunction(()=>Module._pvz_web_loading_complete?.()&&document.getElementById('startup-status').hidden||!document.getElementById('error').hidden,null,{timeout:120000});
  assert.equal(await page.locator('#error').isVisible(),false,await page.locator('#error').innerText());
  assert.ok(await page.evaluate(()=>Module._pvz_web_draws())>0);
  assert.equal(await page.evaluate(()=>Module.pvzMusic.backend),'AudioWorklet');
  assert.equal(await page.evaluate(()=>Module.pvzMusic.context.state),'running');
  assert.equal(await page.evaluate(()=>typeof Module.instantiateWasm),'undefined');
  assert.ok(await page.evaluate(()=>Module.pvzMemory.buffer.byteLength)>0,'Memory diagnostics must read the live Wasm Memory, not an unexported heap view');
  assert.deepEqual(errors,[]);
  await page.screenshot({path:path.join(output,backend+'.png')});
  console.log(backend+': verified draw probe, no secondary HTML/password page, DAT64 generation, audio and resource-complete startup');
  await context.close();
 }
}finally{await browser.close()}
