import assert from 'node:assert/strict';
const {chromium}=await import(process.env.PVZ_PLAYWRIGHT_MODULE||'playwright-core');
const browser=await chromium.launch({headless:true,executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe'});
try{
 const page=await browser.newPage({viewport:{width:390,height:844},isMobile:true,hasTouch:true});
 const errors=[];page.on('pageerror',e=>errors.push(String(e)));
 await page.route('**/web-config.js',route=>route.fulfill({contentType:'text/javascript',body:'window.PVZWebConfig={bypassBilibiliChecks:false,rewardVideoBvids:[]}'}));
 await page.goto('http://127.0.0.1:18088/');await page.waitForFunction(()=>!document.getElementById('start').disabled,null,{timeout:90000});
 await page.locator('#aspect').selectOption('21:9');await page.locator('#start').click();await page.waitForTimeout(16000);
 const b=await page.locator('#canvas').boundingBox();await page.touchscreen.tap(b.x+(1-550/600)*b.width,b.y+700/1400*b.height);
 await page.locator('#web-text-value').waitFor({state:'visible',timeout:15000});
 assert.equal(await page.locator('#web-text-value').evaluate(e=>e===document.activeElement),true);
 await page.locator('#web-text-value').fill('web中文User7');
 assert.equal(await page.locator('#web-text-value').inputValue(),'webUser7');
 await page.waitForTimeout(200);await page.getByRole('button',{name:'完成',exact:true}).click();
 await page.locator('#web-text-value').waitFor({state:'hidden',timeout:10000});
 const users=await page.evaluate(()=>new TextDecoder().decode(Module.FS.readFile('/persist/development-v331/users.dat')));
 assert.ok(users.includes('webUser7'),'ASCII name did not reach the real game profile');
 assert.deepEqual(errors,[]);
 console.log(JSON.stringify({mobileInput:true,realInputFocused:true,nonAsciiRejected:true,profileName:'webUser7',errors}));
}finally{await browser.close()}
