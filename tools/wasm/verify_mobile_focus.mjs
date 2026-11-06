import assert from 'node:assert/strict';
const {chromium}=await import(process.env.PVZ_PLAYWRIGHT_MODULE);
const browser=await chromium.launch({headless:true,executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',args:['--enable-unsafe-webgpu','--ignore-gpu-blocklist']});
const mobile={viewport:{width:1200,height:700},hasTouch:true,userAgent:'Mozilla/5.0 (Linux; Android 14) AppleWebKit/537.36 Chrome/130.0.0.0 Mobile Safari/537.36 BiliApp'};
try{
 const context=await browser.newContext(mobile);const page=await context.newPage();
 await page.route('**/web-config.js',route=>route.fulfill({contentType:'text/javascript',body:'window.PVZWebConfig={bypassBilibiliChecks:false,rewardVideoBvid:"BV1hV4P6PEDc"};'}));
 await page.goto('http://127.0.0.1:18089/');
 await page.waitForFunction(()=>!document.getElementById('start').disabled,null,{timeout:120000});
 assert.equal(await page.locator('#target-fps').inputValue(),'60');
 assert.equal(await page.locator('#auto-fps').isChecked(),false);assert.equal(await page.locator('#auto-fps').isDisabled(),true);
 assert.equal(await page.evaluate(()=>typeof Module.pvzObserveFrame),'undefined');
 assert.equal(await page.evaluate(()=>PVZLauncher.normalizePreferences({autoDetectFrameRate:true}).autoDetectFrameRate),false);
 await page.locator('#target-fps').fill('90');assert.equal(await page.locator('#target-fps').inputValue(),'90');
 console.log('Mobile Toy: default 60, editable target, no refresh observer, saved auto-follow ignored');await context.close();
 for(const backend of ['webgpu','webgl']){
  const context=await browser.newContext(mobile);const page=await context.newPage();
  await page.addInitScript(backend=>localStorage.setItem('pvz_graphics_backend',backend),backend);
  await page.goto('http://127.0.0.1:18089/');await page.waitForFunction(()=>!document.getElementById('start').disabled,null,{timeout:120000});
  await page.locator('#start').click();await page.waitForFunction(()=>Module._pvz_web_scene&&Module._pvz_web_scene()===0,null,{timeout:60000});
  await page.evaluate(()=>{window.focus=()=>{};window.dispatchEvent(new Event('blur'))});
  await page.waitForFunction(()=>Module._pvz_web_active()===0);
  const b=await page.locator('#canvas').boundingBox();await page.touchscreen.tap(b.x+b.width/2,b.y+b.height*.5);
  await page.waitForFunction(()=>Module._pvz_web_active()===1);
  console.log(backend+': touch restores native focus while window.focus is ignored');await context.close();
 }
}finally{await browser.close()}
