import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
const {chromium}=await import(process.env.PVZ_PLAYWRIGHT_MODULE);
const base=process.env.PVZ_WEBGPU_URL||'http://127.0.0.1:18089/';
const output=path.join(os.tmpdir(),'pvz-web-graphics-failures');fs.mkdirSync(output,{recursive:true});
const browser=await chromium.launch({headless:true,executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',args:['--enable-unsafe-webgpu','--ignore-gpu-blocklist']});
const evidence=[];
try{
 const cases=[
  {name:'neither-api',gpu:false,gl:false,error:/WebGPU 和 WebGL 2 都无法运行/},
  {name:'missing-float-target',gpu:false,float:false,error:/EXT_color_buffer_float/},
  {name:'manual-webgl',saved:'webgl',backend:'webgl',options:['auto','webgpu','webgl']},
  {name:'saved-webgpu-unavailable',gpu:false,saved:'webgpu',backend:'webgl',options:['webgl'],reset:true},
  {name:'saved-webgl-unavailable',gl:false,saved:'webgl',backend:'webgpu',options:['webgpu'],reset:true},
  {name:'native-webgpu-failure',backend:'webgpu',options:['auto','webgpu','webgl'],nativeError:/WebGPU adapter unavailable/},
  {name:'native-webgl-failure',saved:'webgl',backend:'webgl',options:['auto','webgpu','webgl'],nativeError:/WebGL|OpenGL|context/i}
 ];
 for(const test of cases){
  const context=await browser.newContext();
  await context.addInitScript(test=>{
   if(test.saved&&!localStorage.getItem('pvz_graphics_backend'))localStorage.setItem('pvz_graphics_backend',test.saved);
   if(test.gpu===false)Object.defineProperty(navigator,'gpu',{value:undefined});
   const original=HTMLCanvasElement.prototype.getContext;
   HTMLCanvasElement.prototype.getContext=function(type,...args){
    if(type==='webgl2'&&(test.gl===false||test.name==='native-webgl-failure'&&this.id==='canvas'))return null;
    const gl=original.call(this,type,...args);
    if(type==='webgl2'&&gl&&test.float===false){
     const getExtension=gl.getExtension.bind(gl);
     gl.getExtension=name=>name==='EXT_color_buffer_float'?null:getExtension(name);
    }
    return gl;
   };
   if(navigator.gpu){
    const request=navigator.gpu.requestAdapter.bind(navigator.gpu);window.adapterCalls=0;
    navigator.gpu.requestAdapter=(...args)=>{
     window.adapterCalls++;
     return test.name==='native-webgpu-failure'&&window.adapterCalls>1?Promise.resolve(null):request(...args);
    };
   }
  },test);
  const page=await context.newPage();await page.goto(base);
  if(test.error){
   await page.locator('#error').waitFor({state:'visible',timeout:30000});
   const message=await page.locator('#error').innerText();
   assert.match(message,test.error);
   assert.equal(await page.evaluate(()=>typeof Module.callMain),'undefined');
   assert.equal(await page.locator('#start').isDisabled(),true);
   assert.deepEqual(await page.locator('#graphics-backend option').evaluateAll(options=>options.map(o=>o.value)),['']);
   evidence.push({test:test.name,message});
  }else{
   await page.waitForFunction(()=>!document.getElementById('start').disabled||!document.getElementById('error').hidden,null,{timeout:120000});
   assert.equal(await page.locator('#error').isVisible(),false,await page.locator('#error').innerText());
   const options=await page.locator('#graphics-backend option').evaluateAll(options=>options.map(o=>o.value));
   assert.deepEqual(options,test.options);
   assert.equal(await page.locator('#graphics-backend').isDisabled(),test.options.length===1);
   assert.equal(await page.evaluate(()=>Module.pvzSelectedBackend),test.backend);
   if(test.reset)assert.equal(await page.evaluate(()=>localStorage.getItem('pvz_graphics_backend')),'auto');
   await page.locator('#start').click();
   if(test.nativeError){
    await page.locator('#error').waitFor({state:'visible',timeout:60000});
    const message=await page.locator('#error').innerText();
    assert.match(message,test.nativeError);
    evidence.push({test:test.name,message});
    if(test.name==='native-webgpu-failure'){
     await page.locator('#graphics-retry-webgl').click();
     await page.waitForFunction(()=>Module.pvzSelectedBackend==='webgl'&&!document.getElementById('start').disabled,null,{timeout:120000});
     await page.locator('#start').click();
     await page.waitForFunction(()=>Module._pvz_web_draws?.()>3,null,{timeout:60000});
     assert.equal(await page.locator('#error').isVisible(),false);
     evidence.at(-1).retryBackend='webgl';
    }
   }else{
    await page.waitForFunction(()=>Module._pvz_web_draws?.()>3||!document.getElementById('error').hidden,null,{timeout:60000});
    assert.equal(await page.locator('#error').isVisible(),false,await page.locator('#error').innerText());
    assert.ok(await page.evaluate(()=>Module._pvz_web_draws())>3);
    if(test.name==='manual-webgl')assert.equal(await page.evaluate(()=>window.adapterCalls),1);
    evidence.push({test:test.name,backend:test.backend,options});
   }
  }
  await page.screenshot({path:path.join(output,test.name+'.png')});
  console.log(test.name+': passed');await context.close();
 }
 fs.writeFileSync(path.join(output,'results.json'),JSON.stringify(evidence,null,2));
}finally{await browser.close()}
