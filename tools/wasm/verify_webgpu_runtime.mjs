import {writeFileSync,mkdirSync} from 'node:fs';import os from 'node:os';import path from 'node:path';
const {chromium}=await import(process.env.PVZ_PLAYWRIGHT_MODULE);
const browser=await chromium.launch({headless:true,executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',args:['--enable-unsafe-webgpu','--ignore-gpu-blocklist']});
try{
 for(const suffix of ['', '?dualSource=off']){
  const page=await browser.newPage();const errors=[];page.on('console',m=>{if(m.type()==='error')errors.push(m.text())});page.on('pageerror',e=>errors.push(String(e)));
  await page.goto('http://127.0.0.1:18089/gpu-probe.html'+suffix);
  await page.waitForFunction(()=>Module.pvzGpuProbe,null,{timeout:60000});
  const result=await page.evaluate(()=>Module.pvzGpuProbe);console.log(suffix||'dual-source auto',JSON.stringify({result,errors}));
  if(!result.ok||errors.length)throw Error(JSON.stringify({result,errors}));await page.close();
 }
}finally{await browser.close()}
