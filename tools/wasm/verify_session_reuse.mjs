import assert from 'node:assert/strict';
const {chromium}=await import(process.env.PVZ_PLAYWRIGHT_MODULE);
const browser=await chromium.launch({headless:true,executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',args:['--enable-unsafe-webgpu','--ignore-gpu-blocklist']});
try{
 const context=await browser.newContext({viewport:{width:1440,height:900}});
 for(const backend of ['webgl','webgpu']){
  const page=await context.newPage(),errors=[],downloads=[];
  page.on('pageerror',error=>errors.push(String(error)));
  page.on('request',request=>{if(/\.(data|wasm\.gz)(\?|$)/.test(request.url()))downloads.push(request.url())});
  await page.addInitScript(backend=>localStorage.setItem('pvz_graphics_backend',backend),backend);
  await page.goto(process.env.PVZ_WEBGPU_URL||'http://127.0.0.1:18089/');
  await page.waitForFunction(()=>!document.getElementById('start').disabled||!document.getElementById('error').hidden,null,{timeout:120000});
  assert.equal(await page.locator('#error').isVisible(),false,await page.locator('#error').textContent());
  if(backend==='webgpu'){
   assert.equal(await page.locator('#target-fps').inputValue(),'120','Backends must share settings');
   assert.equal(await page.locator('#sfx-volume').inputValue(),'43','Backends must share settings');
  }
  await page.evaluate(()=>{window.preparedModule=Module;window.preparedFS=Module.FS});
  await page.locator('#aspect').selectOption('21:9');await page.locator('#auto-fps').uncheck();await page.locator('#target-fps').fill('120');await page.locator('#sfx-volume').fill('43');
  const initial=downloads.length;
  const click=async(x,y)=>{const r=await page.locator('#canvas').boundingBox();await page.mouse.click(r.x+x/1400*r.width,r.y+y/600*r.height)};
  for(let pass=0;pass<3;++pass){
   await page.locator('#start').click();await page.waitForTimeout(16000);await click(700,550);
   await page.waitForFunction(()=>Module._pvz_web_scene()===1||!document.getElementById('error').hidden,null,{timeout:25000});
   assert.equal(await page.locator('#error').isVisible(),false,await page.locator('#error').textContent());
   await page.waitForTimeout(2500);await click(1040,530);await page.waitForTimeout(400);await click(600,395);
   await page.waitForFunction(()=>!document.body.classList.contains('playing')&&!document.getElementById('start').disabled||!document.getElementById('error').hidden,null,{timeout:60000});
   assert.equal(await page.locator('#error').isVisible(),false,await page.locator('#error').textContent());
   assert.ok(await page.evaluate(()=>Module===window.preparedModule&&Module.FS===window.preparedFS),'Quit must not reload the runtime or filesystem');
   assert.equal(await page.evaluate(()=>Module._pvz_web_scene()),-1);
   assert.equal(await page.locator('#target-fps').inputValue(),'120');assert.equal(await page.locator('#sfx-volume').inputValue(),'43');
   assert.equal(downloads.length,initial,'Quit/start downloaded the runtime or resource package again');
   console.log(backend+': start/Quit '+(pass+1)+' reused Wasm, FS and settings');
  }
  assert.deepEqual(errors,[]);await page.close();
 }
}finally{await browser.close()}
