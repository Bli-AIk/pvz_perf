import assert from 'node:assert/strict';
const {chromium}=await import(process.env.PVZ_PLAYWRIGHT_MODULE||'playwright-core');
const browser=await chromium.launch({headless:true,executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe'});
try{
 for(const bypass of [true,false]){
  const context=await browser.newContext(),page=await context.newPage(),external=[];
  const config={bypassBilibiliChecks:bypass,perfectSaveRequirement:'following',rewardVideoBvids:[]};
  await page.route('**/web-config.js',route=>route.fulfill({contentType:'text/javascript',body:'window.PVZWebConfig='+JSON.stringify(config)}));
  page.on('request',request=>{if(!request.url().startsWith('http://127.0.0.1:18088/'))external.push(request.url())});
  const errors=[];page.on('pageerror',error=>errors.push(String(error)));
  await page.goto('http://127.0.0.1:18088/');
  await page.waitForFunction(()=>!document.getElementById('start').disabled,null,{timeout:90000});
  assert.equal(await page.evaluate(()=>Module.FS.analyzePath('/persist/development-v331/users.dat').exists),bypass);
  assert.equal(await page.locator('#reset-baseline').isVisible(),bypass);
  if(!bypass){
   await page.evaluate(()=>{window.toy={getVideoUserActions:async()=>({status:'ok',items:[{status:'ok',coinCount:0}]}),getAuthorRelation:async()=>({status:'ok',data:{isFollowing:false}})}});
   await page.locator('#claim-perfect').click();await page.waitForFunction(()=>document.getElementById('reward-status').textContent.includes('尚未检测到'));
   assert.equal(await page.evaluate(()=>Module.FS.analyzePath('/persist/development-v331/users.dat').exists),false);
   await page.evaluate(()=>window.toy.getAuthorRelation=async()=>({status:'ok',data:{isFollowing:true}}));
  }
  await page.locator('#claim-perfect').click();await page.waitForFunction(()=>document.getElementById('reward-status').textContent.includes('完美存档已复制'));
  assert.equal(await page.evaluate(()=>{const b=Module.FS.readFile('/persist/development-v331/user1.dat64');return new DataView(b.buffer,b.byteOffset,b.byteLength).getInt32(540,true)}),1);
  if(!bypass){
   await page.evaluate(()=>window.toy.getAuthorRelation=async()=>({status:'ok',data:{isFollowing:false}}));
   await page.locator('#claim-endless').click();await page.waitForFunction(()=>document.getElementById('reward-status').textContent.includes('尚未检测到关注'));
   assert.equal(await page.evaluate(()=>Module.FS.analyzePath('/persist/development-v331/game1_58.dat64').exists),false);
   await page.evaluate(()=>window.toy.getAuthorRelation=async()=>({status:'ok',data:{isFollowing:false,isAuthor:true}}));
  }
  await page.locator('#claim-endless').click();await page.waitForFunction(()=>document.getElementById('reward-status').textContent.includes('泳池无尽基准已复制'));
  assert.equal(await page.evaluate(()=>Module.FS.stat('/persist/development-v331/game1_58.dat64').size),673781);
  assert.deepEqual(external,[]);assert.deepEqual(errors,[]);
  console.log(JSON.stringify({bypass,perfect:true,firstAid:true,endless:true,externalRequests:external.length,sdk:bypass?'not called':'mocked'}));
  await context.close();
 }
}finally{await browser.close()}
