import assert from 'node:assert/strict';
const {chromium}=await import(process.env.PVZ_PLAYWRIGHT_MODULE);
const browser=await chromium.launch({headless:true,executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',args:['--enable-unsafe-webgpu','--ignore-gpu-blocklist']});
try{
 const page=await browser.newPage({viewport:{width:390,height:844},hasTouch:true}),errors=[];
 page.on('pageerror',error=>errors.push(error.message));
 await page.addInitScript(()=>localStorage.setItem('pvz_graphics_backend','webgl'));
 await page.goto(process.env.PVZ_DIAGNOSTICS_URL||'http://127.0.0.1:18090/');
 await page.waitForFunction(()=>!document.getElementById('start').disabled,null,{timeout:120000});
 assert.equal(await page.locator('#diagnostics-enabled').isChecked(),true);
 assert.equal(await page.locator('#diagnostics-settings').getAttribute('open'),'');
 await page.locator('#start').click();
 await page.waitForFunction(()=>Module._pvz_web_loading_complete?.()&&Module._pvz_web_draws()>0,null,{timeout:120000});
 await page.waitForFunction(()=>JSON.parse(localStorage.getItem('pvz_diagnostics_v1')).events.some(event=>event.state.phase==='游戏运行'));
 await page.reload();
 await page.waitForFunction(()=>!document.getElementById('start').disabled,null,{timeout:120000});
 await page.locator('#export-diagnostics').click();
 const report=JSON.parse(await page.locator('#diagnostics-text').inputValue());
 assert.equal(report.previous.normalQuit,false);
 assert.ok(report.previous.events.some(event=>event.type==='game-start'));
 assert.ok(report.previous.events.some(event=>event.state.wasmBytes>0));
 assert.ok(report.previous.events.some(event=>event.state.phase==='游戏运行'));
 assert.ok(report.current.events.some(event=>event.type==='export'));
 const download=page.waitForEvent('download');await page.locator('#diagnostics-download').click();
 assert.equal((await download).suggestedFilename(),'pvz-diagnostics.json');
 assert.deepEqual(errors,[]);
 console.log(JSON.stringify({defaultEnabled:true,previousSessionRecovered:true,previousEvents:report.previous.events.length,exportDownload:true,pageErrors:errors}));
}finally{await browser.close()}
