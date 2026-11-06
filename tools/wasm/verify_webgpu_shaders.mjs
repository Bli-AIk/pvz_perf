import fs from 'node:fs';
const {chromium}=await import(process.env.PVZ_PLAYWRIGHT_MODULE);
const browser=await chromium.launch({headless:true,executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',args:['--enable-unsafe-webgpu','--ignore-gpu-blocklist']});
try {
 const page=await browser.newPage();await page.goto('http://127.0.0.1:18088/');
 const shaders=[...fs.readFileSync('pvzPerf/SexyAppFramework/graphics/webgpu/WebGPUShaders.cpp','utf8').matchAll(/R"WGSL\(([\s\S]*?)\)WGSL"/g)].map(m=>m[1]);
 const result=await page.evaluate(async shaders=>{
  const adapter=await navigator.gpu?.requestAdapter();if(!adapter)throw Error('No WebGPU adapter');
  const dual=adapter.features.has('dual-source-blending');const device=await adapter.requestDevice({requiredFeatures:dual?['dual-source-blending']:[]});const errors=[];
  shaders=dual?[shaders[0],'enable dual_source_blending;\n'+shaders[0]+shaders[1],...shaders.slice(2)]:[shaders[0],...shaders.slice(2)];
  for(let i=0;i<shaders.length;i++){
   const module=device.createShaderModule({code:shaders[i]});const info=await module.getCompilationInfo();
   errors.push(...info.messages.map(m=>({shader:i,type:m.type,line:m.lineNum,message:m.message})));
  }
  device.destroy();return {adapter:adapter.info,dualSource:dual,errors};
 },shaders);
 console.log(JSON.stringify(result,null,2));if(result.errors.some(m=>m.type==='error'))process.exitCode=1;
}finally{await browser.close()}
