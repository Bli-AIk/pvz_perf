import assert from 'node:assert/strict';
const {chromium}=await import(process.env.PVZ_PLAYWRIGHT_MODULE||'playwright-core');
const browser=await chromium.launch({headless:true,executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe'});
try{
 const page=await browser.newPage({viewport:{width:1440,height:1000}}),errors=[],htmlRequests=[];
 await page.addInitScript(()=>{window.toyCalls=0;window.toy={getAuthorRelation:async()=>{window.toyCalls++;return {status:'ok',data:{isAuthor:true}}}}});
 page.on('pageerror',e=>errors.push(String(e)));
 page.on('request',request=>{if(request.isNavigationRequest())htmlRequests.push(request.url())});
 const prefix='/toy/test/v1/';
 await page.route('**'+prefix+'**',async route=>{const response=await route.fetch({url:route.request().url().replace(prefix,'/')});await route.fulfill({response})});
 await page.addInitScript(()=>{if(window.parent!==window)window.fetch=()=>Promise.reject(new TypeError('Failed to fetch: child context blocked'))});
 await page.route('**/web-config.js',route=>route.fulfill({contentType:'text/javascript',body:'window.PVZWebConfig={bypassBilibiliChecks:false,rewardVideoBvid:"BV1hV4P6PEDc"}'}));
 const url='http://127.0.0.1:18088'+prefix+'index.html';
 await page.goto(url);await page.waitForFunction(()=>!document.getElementById('start').disabled,null,{timeout:90000});
 const a=await page.locator('.audio-sfx').boundingBox(),b=await page.locator('.audio-music').boundingBox(),c=await page.locator('.save-management').boundingBox();
 assert.ok(Math.abs(a.x-b.x)<1&&b.y>a.y&&c.x>a.x);
 assert.equal(await page.locator('#coin-video').getAttribute('href'),'https://www.bilibili.com/video/BV1hV4P6PEDc/');
 assert.equal(await page.locator('#cover #export-endless').count(),0);
 await page.locator('#custom-save').click();const editor=page.frameLocator('#save-editor-frame');await editor.locator('#status').filter({hasText:'已打开'}).waitFor();
 await editor.locator('#add-user').click();await editor.locator('#status').filter({hasText:'已新增用户'}).waitFor();await editor.locator('#user-finished').fill('2');await editor.locator('[data-unlock="2"]').check();await editor.locator('#save-user').click();await editor.locator('#status').filter({hasText:'用户进度已保存'}).waitFor();
 // A baseline DAT64 is a test fixture, not a user-facing claim operation.
 await page.evaluate(async()=>{const files=await(await import('./save-tools.js')).loadSaveBundle();Module.FS.writeFile('/persist/development-v331/game1_58.dat64',files['game1_58.dat64'])});
 await editor.locator('#close').click();await page.locator('#custom-save').click();await editor.locator('#status').filter({hasText:'已打开'}).waitFor();
 await editor.locator('[data-tab=files]').click();await editor.locator('#extract').click();await editor.locator('#status').filter({hasText:'已提取阵型'}).waitFor({timeout:180000});
 assert.equal(page.url(),url);assert.equal(await page.locator('#save-editor-dialog').isVisible(),true);
 assert.ok((await editor.locator('#code').inputValue()).length>20);
 await editor.locator('#flags').fill('128');await editor.locator('#seed').fill('114514');await editor.locator('#scene').selectOption({index:3});
 const queriesAtOpen=await page.evaluate(()=>window.toyCalls);
 await page.evaluate(()=>{window.toy.getAuthorRelation=async()=>{throw Error('SDK must not be called inside the modal')};window.toy.getVideoUserActions=async()=>{throw Error('SDK must not be called inside the modal')}});
 await editor.locator('#generate').click();await editor.locator('#status').filter({hasText:'已生成并部署'}).waitFor({timeout:180000});
 assert.equal(page.url(),url);assert.equal(await page.locator('iframe').count(),1);
 const state=await page.evaluate(()=>({bytes:Module.FS.stat('/persist/development-v331/game1_58.dat64').size,magic:new TextDecoder().decode(Module.FS.readFile('/persist/development-v331/game1_58.dat64').slice(0,8))}));
 assert.equal(state.magic,'PVZDAT64');assert.ok(state.bytes>100000);
 await editor.locator('[data-tab=files]').click();const download=page.waitForEvent('download');await editor.locator('#export').click();assert.equal((await download).suggestedFilename(),'game1_58.dat64');
 assert.equal(await page.evaluate(()=>window.toyCalls),queriesAtOpen);
 await editor.locator('#close').click();assert.equal(await page.locator('#save-editor-dialog').isVisible(),false);
 assert.deepEqual(htmlRequests,[url]);
 // Page routing does not intercept AudioWorklet loads. Reopen the persisted
 // generated save on the real server root for the actual gameplay check.
 await page.goto('http://127.0.0.1:18088/');await page.waitForFunction(()=>!document.getElementById('start').disabled,null,{timeout:90000});
 await page.locator('#advanced-settings>summary').click();await page.locator('#aspect').selectOption('21:9');await page.locator('#auto-fps').uncheck();await page.locator('#target-fps').fill('100');
 await page.locator('#start').click();await page.waitForTimeout(16000);
 const click=async(x,y)=>{const b=await page.locator('#canvas').boundingBox();await page.mouse.click(b.x+x*b.width/1400,b.y+y*b.height/600)};
 await click(700,550);await page.waitForFunction(()=>Module._pvz_web_scene()===1,null,{timeout:20000});await page.waitForTimeout(2500);
 await click(850,380);await page.waitForFunction(()=>Module._pvz_web_scene()===7);await click(700,470);await page.waitForFunction(()=>Module._pvz_web_has_board());await page.waitForTimeout(500);await click(500,370);await page.waitForFunction(()=>Module._pvz_web_scene()===2,null,{timeout:15000});
 assert.equal(await page.evaluate(()=>Module._pvz_web_scene()),2);assert.equal(await page.evaluate(()=>Module._pvz_web_paused()),0);
 assert.deepEqual(errors,[]);assert.deepEqual(htmlRequests,[url,'http://127.0.0.1:18088/']);
 console.log(JSON.stringify({nestedToyPath:true,audioLeft:true,videoLink:true,internalExtraction:true,modalExport:true,generateDeploy:true,generatedCheckpointPlayable:true,noSecondaryHtmlNavigation:true,childNetworkBlocked:true,parentResourceBridge:true,noSdkInsideModal:true,errors}));
}finally{await browser.close()}
