import fs from 'node:fs';
const {chromium}=await import(process.env.PVZ_PLAYWRIGHT_MODULE||'playwright-core');
const browser=await chromium.launch({headless:true,executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe'});
try{
 const page=await browser.newPage({viewport:{width:800,height:600},deviceScaleFactor:1});
 await page.goto('http://127.0.0.1:18088/');await page.waitForFunction(()=>!document.getElementById('start').disabled,null,{timeout:90000});
 await page.locator('#aspect').selectOption('4:3');await page.locator('#auto-fps').uncheck();await page.locator('#target-fps').fill('100');
 await page.locator('#start').click();await page.waitForTimeout(16000);
 if(await page.evaluate(()=>Module._pvz_web_scene())!==0)throw Error('Expected title/loading scene');
 const bounds=await page.locator('#canvas').boundingBox();if(bounds.width!==800||bounds.height!==600)throw Error('Cover must be native 800x600');
 await page.locator('#game-fps').evaluate(element=>element.style.visibility='hidden');
 fs.mkdirSync('out/wasm/branding',{recursive:true});await page.locator('#canvas').screenshot({path:'out/wasm/branding/toy-cover-800x600.png'});
 console.log('Captured actual title/loading scene at native 800x600; no scaling or browser UI');
}finally{await browser.close()}
