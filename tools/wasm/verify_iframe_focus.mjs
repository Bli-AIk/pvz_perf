import assert from 'node:assert/strict';
import fs from 'node:fs';
const {chromium}=await import(process.env.PVZ_PLAYWRIGHT_MODULE||'playwright-core');
const browser=await chromium.launch({headless:true,executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe'});
const results=[];
try{
 const page=await browser.newPage({viewport:{width:1600,height:960}});
 await page.route('**/focus-fixture',route=>route.fulfill({contentType:'text/html',body:`<style>body{margin:0}iframe{border:0;width:100vw;height:100vh}button{position:fixed;right:0;top:0;z-index:2}aside{display:none;position:fixed;right:0;top:40px;width:350px;height:100vh;background:#333}</style><iframe id="game" src="http://127.0.0.1:18088/"></iframe><button onclick="document.querySelector('iframe').style.width='calc(100vw - 350px)';document.querySelector('aside').style.display='block'">Open sidebar</button><aside>Host sidebar</aside>`}));
 await page.goto('http://127.0.0.1:18088/focus-fixture');
 const game=await (await page.locator('#game').elementHandle()).contentFrame();
 await game.waitForURL('http://127.0.0.1:18088/',{timeout:45000,waitUntil:'domcontentloaded'});
 await game.waitForFunction(()=>!document.getElementById('start').disabled,null,{timeout:90000});
 await game.locator('#auto-fps').uncheck();await game.locator('#target-fps').fill('240');await game.locator('#aspect').selectOption('21:9');
 await game.locator('#start').click();await game.waitForTimeout(16000);
 const click=async(x,y)=>{const b=await game.locator('#canvas').boundingBox();await page.mouse.click(b.x+x*b.width/1400,b.y+y*b.height/600)};
 const state=()=>game.evaluate(()=>({paused:Module._pvz_web_paused(),active:Module._pvz_web_active(),focus:document.hasFocus(),element:document.activeElement?.tagName}));
 await click(700,550);await game.waitForFunction(()=>Module._pvz_web_scene()===1);await game.waitForTimeout(2500);
 await click(850,380);await game.waitForFunction(()=>Module._pvz_web_scene()===7);await click(700,470);await game.waitForFunction(()=>Module._pvz_web_has_board());
 await click(500,370);await game.waitForTimeout(1500);
 await game.waitForFunction(()=>!Module._pvz_web_paused());
 await page.keyboard.press('Space');await game.waitForFunction(()=>!!Module._pvz_web_paused());
 results.push({step:'paused',...await state()});
 await page.getByRole('button',{name:'Open sidebar'}).click();await page.waitForTimeout(300);
 results.push({step:'sidebar',...await state()});
 await click(620,450);await game.waitForTimeout(300);
 results.push({step:'resume click',...await state()});
 await page.screenshot({path:process.env.TEMP+'/pvz-web-iframe-focus.png'});
 assert.equal((await state()).paused,0,'Resume click failed after host sidebar stole focus');
 await page.keyboard.press('Space');await game.waitForFunction(()=>!!Module._pvz_web_paused());
 await page.keyboard.press('Space');await game.waitForFunction(()=>!Module._pvz_web_paused());
 results.push({step:'space toggles',...await state()});
 assert.equal((await state()).active,1);
}finally{
 console.log(JSON.stringify(results));fs.writeFileSync(process.env.TEMP+'/pvz-web-iframe-focus.json',JSON.stringify(results,null,2));await browser.close();
}
