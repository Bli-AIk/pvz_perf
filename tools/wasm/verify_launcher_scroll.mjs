import assert from 'node:assert/strict';
const {chromium}=await import(process.env.PVZ_PLAYWRIGHT_MODULE);
const browser=await chromium.launch({headless:true,executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',args:['--enable-unsafe-webgpu','--ignore-gpu-blocklist']});
const base=process.env.PVZ_WEBGPU_URL||'http://127.0.0.1:18089/';
try{
 for(const embedded of [false,true]){
  const context=await browser.newContext({viewport:{width:390,height:720},isMobile:true,hasTouch:true,userAgent:'Mozilla/5.0 (Linux; Android 14) AppleWebKit/537.36 Chrome/157.0.0.0 Mobile Safari/537.36'}),page=await context.newPage();
  const address=embedded?new URL('scroll-sandbox.html',base).href:base;
  if(embedded)await page.route('**/scroll-sandbox.html',r=>r.fulfill({contentType:'text/html',body:'<!doctype html><meta name="viewport" content="width=device-width,initial-scale=1"><style>html,body{margin:0;height:100%;overflow:hidden}iframe{border:0;width:100%;height:100%}</style><iframe id="toy" sandbox="allow-scripts allow-same-origin" src="./index.html"></iframe>'}));
  await page.goto(address);const view=embedded?page.frameLocator('#toy'):page;
  await view.locator('#start:enabled').waitFor({timeout:120000});await view.locator('#advanced-settings>summary').click();
  const state=await view.locator('body').evaluate(()=>({height:innerHeight,scroll:document.scrollingElement.scrollHeight,cover:getComputedStyle(document.getElementById('cover')).position,overflow:getComputedStyle(document.documentElement).overflowY}));
  assert.ok(state.scroll>state.height);assert.equal(state.cover,'relative');assert.equal(state.overflow,'auto');
  const cdp=await context.newCDPSession(page);
  const swipe=async(start,end)=>{
   await cdp.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[{x:14,y:start}]});
   for(let i=1;i<=10;++i){await cdp.send('Input.dispatchTouchEvent',{type:'touchMove',touchPoints:[{x:14,y:start+(end-start)*i/10}]});await page.waitForTimeout(20)}
   await cdp.send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});await page.waitForTimeout(100);
  };
  for(let i=0;i<2;++i)await swipe(600,100);
  const frame=embedded?page.frames().find(f=>f.url()===new URL('index.html',base).href):page.mainFrame();
  await frame.waitForFunction(()=>scrollY>40,null,{timeout:5000});
  const down=await frame.evaluate(()=>scrollY);assert.ok(down>40);
  assert.ok(await view.locator('.note').evaluate(node=>node.getBoundingClientRect().bottom<=innerHeight),'The final settings note must be reachable');
  for(let i=0;i<2;++i)await swipe(100,600);
  await frame.waitForFunction(()=>scrollY<20,null,{timeout:5000});
  assert.equal(await view.locator('#start').isVisible(),true);
  console.log((embedded?'Nested Toy iframe':'Direct mobile page')+': native touch scroll down/up, long settings and reachable start passed');await context.close();
 }
}finally{await browser.close()}
