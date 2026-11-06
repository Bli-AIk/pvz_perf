import assert from 'node:assert/strict';
const {chromium}=await import(process.env.PVZ_PLAYWRIGHT_MODULE||'playwright-core');
const browser=await chromium.launch({headless:true,executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe'});
try{
 for(const bypass of [true,false]){
  const context=await browser.newContext(),page=await context.newPage();
  await page.route('**/web-config.js',route=>route.fulfill({contentType:'text/javascript',body:'window.PVZWebConfig='+JSON.stringify({bypassBilibiliChecks:bypass,rewardVideoBvid:'BV1hV4P6PEDc'})}));
  // Toy must not have to serve arbitrary .dat64 extensions for bundled saves.
  await page.route('**/development-save/**',route=>route.abort());
  const errors=[];page.on('pageerror',error=>errors.push(String(error)));
  await page.goto('http://127.0.0.1:18088/');await page.waitForFunction(()=>!document.getElementById('start').disabled,null,{timeout:90000});
  assert.equal(await page.evaluate(()=>Module.FS.analyzePath('/persist/development-v331/users.dat').exists),bypass);
  if(!bypass){
   await page.evaluate(()=>{window.toy={getAuthorRelation:async()=>({status:'ok',data:{isFollowing:true,isAuthor:false}}),getVideoUserActions:async()=>({status:'ok',items:[{bvid:'BV1hV4P6PEDc',status:'ok',coinCount:1}]})}});
   for(const button of ['#claim-perfect','#claim-endless']){await page.locator(button).click();await page.waitForFunction(()=>!document.getElementById('claim-perfect').disabled);assert.equal(await page.locator('dialog[open]').count(),0);assert.equal(await page.evaluate(()=>Module.FS.analyzePath('/persist/development-v331/users.dat').exists),false)}
   await page.evaluate(()=>window.toy.getVideoUserActions=async()=>({status:'ok',items:[{bvid:'BV1hV4P6PEDc',status:'ok',coinCount:2}]}));
   await page.route('**/save-bundle.json',r=>r.fulfill({status:503,body:'unavailable'}));
   await page.locator('#claim-perfect').click();await page.waitForFunction(()=>document.getElementById('reward-status').textContent.includes('503'));assert.equal(await page.locator('dialog[open]').count(),0);
   await page.unroute('**/save-bundle.json');
  }
  await page.evaluate(()=>{Module.FS.writeFile('/persist/development-v331/user2.dat64','old');Module.FS.writeFile('/persist/development-v331/game2_58.dat64','old');Module.FS.writeFile('/persist/development-v331/settings.json','{"sfxVolume":37}');});
  await page.locator('#claim-perfect').click();await page.locator('#replace-save-dialog button[value=cancel]').click();await page.waitForFunction(()=>!document.getElementById('claim-perfect').disabled);
  assert.equal(await page.evaluate(()=>Module.FS.analyzePath('/persist/development-v331/user2.dat64').exists),true);
  await page.locator('#claim-perfect').click();await page.locator('#replace-save-dialog button[value=replace]').click();await page.waitForFunction(()=>document.getElementById('reward-status').textContent.includes('无尽基准已部署'));
  const state=await page.evaluate(()=>{const fs=Module.FS,r='/persist/development-v331',b=fs.readFile(r+'/user1.dat64');return {aid:new DataView(b.buffer,b.byteOffset,b.byteLength).getInt32(540,true),old:fs.analyzePath(r+'/user2.dat64').exists,oldLevel:fs.analyzePath(r+'/game2_58.dat64').exists,size:fs.stat(r+'/game1_58.dat64').size,settings:fs.readFile(r+'/settings.json',{encoding:'utf8'})}});
  assert.equal(state.aid,1);assert.equal(state.old,false);assert.equal(state.oldLevel,false);assert.equal(state.size,673781);assert.equal(state.settings,'{"sfxVolume":37}');
  if(!bypass)await page.evaluate(()=>{window.toy.getAuthorRelation=async()=>({status:'ok',data:{isAuthor:true}});window.toy.getVideoUserActions=async()=>{throw Error('Author must not need coins')}});
  await page.locator('#claim-endless').click();await page.locator('#endless-dialog').waitFor({state:'visible'});assert.equal(await page.locator('#endless-scene option').count(),6);assert.equal(await page.locator('#endless-flags').inputValue(),'128');assert.equal(await page.locator('#endless-seed').inputValue(),'114514');assert.equal(await page.locator('#coin-video').getAttribute('href'),'https://www.bilibili.com/video/BV1hV4P6PEDc/');
  const download=page.waitForEvent('download');await page.locator('#export-endless').click();assert.equal((await download).suggestedFilename(),'game1_58.dat64');await page.locator('#close-endless').click();
  assert.deepEqual(errors,[]);console.log(JSON.stringify({bypass,twoCoins:true,author:true,oneCoinRejected:true,warningCancel:true,oldUsersRemoved:true,settingsRetained:true,baselineAutomatic:true,blockedDat64Paths:true,export:true}));await context.close();
 }
}finally{await browser.close()}
