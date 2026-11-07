import assert from 'node:assert/strict';
const {chromium}=await import(process.env.PVZ_PLAYWRIGHT_MODULE);
const browser=await chromium.launch({headless:true,executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',args:['--enable-unsafe-webgpu','--ignore-gpu-blocklist']});
try{
 const context=await browser.newContext(),page=await context.newPage(),base=process.env.PVZ_WEBGPU_URL||'http://127.0.0.1:18089/';
 const ready=async()=>{await page.waitForFunction(()=>!document.getElementById('start').disabled||!document.getElementById('error').hidden,null,{timeout:120000});assert.equal(await page.locator('#error').isVisible(),false,await page.locator('#error').textContent())};
 await page.goto(base);await ready();
 // Establish the ordinary local profile, then exercise the Bilibili config
 // against a protocol mock. This is not evidence of live account permissions.
 await page.route('**/web-config.js',route=>route.fulfill({contentType:'text/javascript',body:'window.PVZWebConfig={bypassBilibiliChecks:false,rewardVideoBvid:"BV1hV4P6PEDc"};'}));
 await page.addInitScript(()=>{
  window.cloudRows={};window.cloudWrites=0;window.failCloud=false;
  window.toy={getCloudStorage:async keys=>Object.fromEntries(keys.filter(k=>Object.hasOwn(cloudRows,k)).map(k=>[k,cloudRows[k]])),setCloudStorage:async values=>{if(window.failCloud)throw Error('simulated cloud outage');window.cloudWrites++;Object.assign(cloudRows,values)}};
 });
 await page.reload();await ready();
 const original=await page.evaluate(()=>Array.from(Module.FS.readFile('/persist/development-v331/user1.dat64')));
 const battle=await page.evaluate(()=>Array.from(Module.FS.readFile('/persist/development-v331/game1_58.dat64')));
 await page.locator('#upload-progress').click();
 await page.waitForFunction(()=>document.getElementById('cloud-status').textContent.includes('已备份到'));
 const payload=await page.evaluate(async()=>{
  const head=JSON.parse(cloudRows.pvz_progress_v1),text=Array.from({length:head.count},(_,i)=>cloudRows['pvz_progress_'+head.slot+'_'+i]).join('');
  const bytes=Uint8Array.from(atob(text),c=>c.charCodeAt(0));return JSON.parse(await new Response(new Blob([bytes]).stream().pipeThrough(new DecompressionStream('gzip'))).text());
 });
 assert.deepEqual(Object.keys(payload).sort(),['user1.dat64','users.dat']);
 assert.equal(await page.evaluate(()=>window.cloudWrites),2);
 await page.locator('#upload-progress').click();await page.waitForTimeout(100);assert.equal(await page.evaluate(()=>window.cloudWrites),2);
 await page.evaluate(()=>{window.failCloud=true;const p='/persist/development-v331/user1.dat64',b=Module.FS.readFile(p);b[200]^=1;Module.FS.writeFile(p,b)});
 await page.locator('#restore-progress').click();await page.locator('#cloud-restore-dialog').waitFor({state:'visible'});
 await page.locator('#cloud-restore-dialog button[value=restore]').click();
 await page.waitForFunction(()=>document.getElementById('cloud-status').textContent.includes('已备份到'));
 assert.deepEqual(await page.evaluate(()=>Array.from(Module.FS.readFile('/persist/development-v331/user1.dat64'))),original);
 assert.deepEqual(await page.evaluate(()=>Array.from(Module.FS.readFile('/persist/development-v331/game1_58.dat64'))),battle);
 console.log('Bilibili-config UI: profile cloud upload/restore, unchanged-write suppression and untouched local battle DAT64 passed against SDK mock');
}finally{await browser.close()}
