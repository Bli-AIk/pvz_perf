import assert from 'node:assert/strict';
const {chromium}=await import(process.env.PVZ_PLAYWRIGHT_MODULE||'playwright-core');
const browser=await chromium.launch({headless:true,executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe'});
try{
 for(const bypass of [true,false]){
  const context=await browser.newContext(),page=await context.newPage();
  await page.route('**/web-config.js',route=>route.fulfill({contentType:'text/javascript',body:'window.PVZWebConfig='+JSON.stringify({bypassBilibiliChecks:bypass,rewardVideoBvid:'BV1hV4P6PEDc'})}));
  await page.route('**/development-save/**',route=>route.abort());
  const errors=[];page.on('pageerror',error=>errors.push(String(error)));
  await page.goto(process.env.PVZ_WEBGPU_URL||'http://127.0.0.1:18089/');await page.waitForFunction(()=>!document.getElementById('start').disabled,null,{timeout:120000});
  assert.equal(await page.evaluate(()=>Module.FS.analyzePath('/persist/development-v331/users.dat').exists),bypass);
  if(!bypass){
   await page.evaluate(()=>{window.toy={getAuthorRelation:async()=>({status:'ok',data:{isFollowing:true,isAuthor:false}}),getVideoUserActions:async()=>({status:'ok',items:[{bvid:'BV1hV4P6PEDc',status:'ok',coinCount:1}]})}});
   await page.locator('#custom-save').click();await page.waitForFunction(()=>!document.getElementById('custom-save').disabled);assert.equal(await page.locator('dialog[open]').count(),0);assert.match(await page.locator('#reward-status').textContent(),/投满两个币/);
   await page.evaluate(()=>window.toy.getVideoUserActions=async()=>({status:'ok',items:[{bvid:'BV1hV4P6PEDc',status:'ok',coinCount:2}]}));
  }
  await page.locator('#custom-save').click();const editor=page.frameLocator('#save-editor-frame');await editor.locator('#status').filter({hasText:'已打开'}).waitFor();
  assert.equal(await editor.locator('#profile').isVisible(),true);assert.equal(await editor.locator('#perfect').count(),0);
  await editor.locator('#add-user').click();await editor.locator('#status').filter({hasText:'已新增用户'}).waitFor();
  assert.equal(await editor.locator('#user-coins').inputValue(),'0');
  await editor.locator('#close').click();
  if(!bypass)await page.evaluate(()=>{window.toy.getAuthorRelation=async()=>({status:'ok',data:{isAuthor:true}});window.toy.getVideoUserActions=async()=>{throw Error('Author must not need coins')}});
  await page.locator('#custom-save').click();await page.locator('#save-editor-dialog').waitFor({state:'visible'});assert.equal(await page.locator('#coin-video').getAttribute('href'),'https://www.bilibili.com/video/BV1hV4P6PEDc/');
  assert.deepEqual(errors,[]);console.log(JSON.stringify({bypass,twoCoins:true,author:true,oneCoinRejected:true,userEditor:true,freshUserDefaults:true,blockedDat64Paths:true}));await context.close();
 }
}finally{await browser.close()}
