import fs from 'node:fs';import os from 'node:os';import path from 'node:path';
const {chromium}=await import(process.env.PVZ_PLAYWRIGHT_MODULE);
const base=process.env.PVZ_WEBGPU_URL||'http://127.0.0.1:18089/';
const output=process.env.PVZ_WEBGPU_OUTPUT||path.join(os.tmpdir(),'pvz-webgpu-verification');fs.mkdirSync(output,{recursive:true});
const browser=await chromium.launch({headless:true,executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',args:process.argv.includes('--browser-defaults')?[]:['--enable-unsafe-webgpu','--ignore-gpu-blocklist']});
const events=[];
try{
 const page=await browser.newPage({viewport:{width:1440,height:900}});
 if(process.argv.includes('--no-webgpu'))await page.addInitScript(()=>Object.defineProperty(navigator,'gpu',{value:undefined}));
 page.on('console',m=>{events.push({type:m.type(),text:m.text()});if(m.type()==='error')console.log(m.text().slice(0,1800))});
 page.on('pageerror',e=>{events.push({type:'pageerror',text:String(e)});console.log('PAGEERROR',String(e))});
 await page.goto(base);
 if(process.argv.includes('--no-webgpu')){
  await page.waitForFunction(()=>document.getElementById('error').textContent.includes('只包含 WebGPU'));
  if(!await page.locator('#start').isDisabled())throw Error('Unsupported WebGPU-only package allowed start');
  console.log('WebGPU unavailable: explicit error, start remains disabled');
 }else{
 await page.waitForFunction(()=>!document.getElementById('start').disabled,null,{timeout:90000});
 const selected=await page.evaluate(()=>Module.pvzSelectedBackend);
 if(selected!=='webgpu')throw Error('WebGPU-only package selected another backend');
 const names=await page.locator('#endless-preset option').allTextContents();
 if(names.slice(0,4).join('|')!=='97无炮|14格|24炮|[终极战术]五角星无炮流')throw Error('Default lineups missing');
 await page.locator('#start').click();
 await page.waitForTimeout(15000);
 console.log('state',await page.evaluate(()=>({backend:Module.pvzGraphicsBackend,selected:Module.pvzSelectedBackend,scene:Module._pvz_web_scene(),error:document.getElementById('error').textContent})));
 await page.screenshot({path:path.join(output,'title.png')});
 const click=async(x,y)=>{const b=await page.locator('#canvas').boundingBox();const w=await page.evaluate(()=>Module.pvzViewport.width);await page.mouse.click(b.x+x*b.width/w,b.y+y*b.height/600)};
 const width=await page.evaluate(()=>Module.pvzViewport.width);
 await click(width/2,550);await page.waitForFunction(()=>Module._pvz_web_scene()===1,null,{timeout:30000});
 await page.waitForTimeout(3000);await page.screenshot({path:path.join(output,'menu.png')});
 console.log('menu',await page.evaluate(()=>({backend:Module.pvzGraphicsBackend,draws:Module._pvz_web_draws(),fps:document.getElementById('game-fps').textContent,error:document.getElementById('error').textContent})));
 if(process.argv.includes('--endless')){
  await click(width/2+150,380);await page.waitForFunction(()=>Module._pvz_web_scene()===7);
  await click(width/2,470);await page.waitForFunction(()=>Module._pvz_web_scene()===3&&Module._pvz_web_has_board());
  await page.waitForTimeout(2000);await page.screenshot({path:path.join(output,'endless.png')});
  await click(width/2-110,395);await page.waitForTimeout(15000);
  await page.screenshot({path:path.join(output,'battle.png')});
  console.log('battle',await page.evaluate(()=>({scene:Module._pvz_web_scene(),draws:Module._pvz_web_draws(),fps:document.getElementById('game-fps').textContent,error:document.getElementById('error').textContent})));
 }
 const errors=events.filter(e=>e.type==='error'||e.type==='pageerror');if(errors.length)throw Error(JSON.stringify(errors));
 }
}finally{fs.writeFileSync(path.join(output,'events.json'),JSON.stringify(events,null,2));await browser.close()}
