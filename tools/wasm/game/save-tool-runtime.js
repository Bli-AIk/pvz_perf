// Loaded only in a short-lived, same-origin tool iframe. No gameplay main loop.
const job=parent.PVZSaveToolJob;
if(!job)throw Error('离线工具必须从自定义无尽窗口启动');
// Keep resource loading in the working parent context. A srcdoc child does
// not inherit the host's fetch adapter or its request/security context.
window.fetch=(resource,options)=>job.fetchResource(resource,options);
const noSound=new Proxy({finish:async()=>{}},{get:(target,key)=>target[key]||(()=>0)});
window.Module={canvas:document.getElementById('canvas'),locateFile:name=>new URL(name,job.baseURL).href,
 pvzMusic:{sound:noSound},pvzMobileInput:false,pvzRefreshRate:60,
 pvzToolComplete:json=>job.resolve({metadata:JSON.parse(json),bytes:Module.FS.analyzePath('/tool/generated.dat64').exists?Module.FS.readFile('/tool/generated.dat64'):null}),
 pvzToolFail:error=>job.reject(Error(error)),onAbort:error=>job.reject(Error(String(error))),
 preRun:[()=>{Module.FS.mkdirTree('/tool');for(const [name,bytes] of Object.entries(job.files))Module.FS.writeFile('/tool/'+name,bytes);Module.FS.writeFile('/tool/request.json',JSON.stringify(job.request))}],
 onRuntimeInitialized:()=>{try{Module.callMain(['--save-tool'])}catch(error){job.reject(error)}},
 print:()=>{},printErr:message=>console.warn('save tool:',message)
};
window.addEventListener('error',event=>job.reject(Error(event.message)));
window.addEventListener('unhandledrejection',event=>job.reject(event.reason));
try{
 const stem=job.runtimeStem;
 const response=await fetch('./'+stem+'.wasm.gz');
 const packed=new Uint8Array(await response.arrayBuffer());
 let binary;
 try{binary=packed[0]===31&&packed[1]===139?await new Response(new Blob([packed]).stream().pipeThrough(new DecompressionStream('gzip'))).arrayBuffer():packed.buffer}
 catch(error){throw Error('离线工具 Wasm 解压失败：'+(error.message||error))}
 Module.instantiateWasm=(imports,receive)=>{WebAssembly.instantiate(binary,imports).then(result=>receive(result.instance)).catch(job.reject);return {}};
 const script=document.createElement('script');script.src='./'+stem+'.js';script.onerror=()=>job.reject(Error('无法加载离线工具脚本'));document.head.appendChild(script);
}catch(error){job.reject(error)}
