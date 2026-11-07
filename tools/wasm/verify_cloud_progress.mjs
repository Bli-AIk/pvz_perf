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
  window.cloudRows=JSON.parse(sessionStorage.getItem('mock-cloud-rows')||'{}');window.cloudWrites=0;window.failCloud=false;
  window.toy={getCloudStorage:async keys=>Object.fromEntries(keys.filter(k=>Object.hasOwn(cloudRows,k)).map(k=>[k,cloudRows[k]])),setCloudStorage:async values=>{if(window.failCloud)throw Error('simulated cloud outage');window.cloudWrites++;Object.assign(cloudRows,values)}};
 });
 await page.reload();await ready();
 const original=await page.evaluate(()=>Array.from(Module.FS.readFile('/persist/development-v331/user1.dat64')));
 const battle=await page.evaluate(()=>Array.from(Module.FS.readFile('/persist/development-v331/game1_58.dat64')));
 await page.locator('#upload-progress').click();
 await page.waitForFunction(()=>document.getElementById('cloud-status').textContent.includes('已备份到')&&!document.getElementById('start').disabled);
 const payload=await page.evaluate(async()=>{
  const head=JSON.parse(cloudRows.pvz_progress_v1),text=Array.from({length:head.count},(_,i)=>cloudRows['pvz_progress_'+head.slot+'_'+i]).join('');
  const bytes=Uint8Array.from(atob(text),c=>c.charCodeAt(0));return JSON.parse(await new Response(new Blob([bytes]).stream().pipeThrough(new DecompressionStream('gzip'))).text());
 });
 assert.deepEqual(Object.keys(payload).sort(),['user1.dat64','users.dat']);
 assert.equal(await page.evaluate(()=>window.cloudWrites),4);
 await page.locator('#upload-progress').click();await page.waitForFunction(()=>!document.getElementById('start').disabled);assert.equal(await page.evaluate(()=>window.cloudWrites),4);
 await page.evaluate(()=>{window.failCloud=true;const p='/persist/development-v331/user1.dat64',b=Module.FS.readFile(p);b[200]^=1;Module.FS.writeFile(p,b)});
 await page.locator('#restore-progress').click();await page.locator('#cloud-restore-dialog').waitFor({state:'visible'});
 await page.locator('#cloud-restore-dialog button[value=restore]').click();
 await page.waitForFunction(()=>document.getElementById('cloud-status').textContent.includes('已备份到'));
 assert.deepEqual(await page.evaluate(()=>Array.from(Module.FS.readFile('/persist/development-v331/user1.dat64'))),original);
 assert.deepEqual(await page.evaluate(()=>Array.from(Module.FS.readFile('/persist/development-v331/game1_58.dat64'))),battle);
 page.on('dialog',d=>d.accept());
 await page.evaluate(()=>{window.failCloud=false;toy.getAuthorRelation=async()=>({status:'ok',data:{isAuthor:true}})});
 const oldHead=await page.evaluate(()=>cloudRows.pvz_lineups_v1);
 await page.locator('#custom-save').click();const editor=page.frameLocator('#save-editor-frame');await editor.locator('#status').filter({hasText:'已打开'}).waitFor();await editor.locator('[data-tab=lineup]').click();
 await editor.locator('#preset').selectOption({label:'24炮'});await editor.locator('#status').filter({hasText:'已载入 24炮'}).waitFor();await editor.locator('#delete-lineup').click();await editor.locator('#editor-confirm button[value=confirm]').click();await editor.locator('#status').filter({hasText:'已从列表删除'}).waitFor();
 await page.waitForFunction(old=>cloudRows.pvz_lineups_v1!==old&&document.getElementById('cloud-upload-warning').hidden,oldHead);
 await editor.locator('#close').click();
 await page.evaluate(()=>{sessionStorage.setItem('mock-cloud-rows',JSON.stringify(cloudRows));localStorage.removeItem('pvz_lineup_library_v1')});
 await page.reload();await ready();
 assert.deepEqual(await page.evaluate(()=>JSON.parse(localStorage.getItem('pvz_lineup_library_v1')).lineups.map(p=>p.name)),['97无炮','14格','[终极战术]五角星无炮流']);
 assert.deepEqual(await page.evaluate(()=>Array.from(Module.FS.readFile('/persist/development-v331/game1_58.dat64'))),battle);
 console.log('Bilibili-config UI: profile + JSON lineup cloud upload/restore, unchanged-write suppression, deletion surviving local reset and untouched battle DAT64 passed against SDK mock');
}finally{await browser.close()}
