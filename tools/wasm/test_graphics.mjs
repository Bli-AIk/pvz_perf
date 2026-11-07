import assert from 'node:assert/strict';
import {detectGraphics,capabilityText} from './game/graphics.js';
let glAvailable=true,floatAvailable=true,framebufferComplete=true,released=0,textureLimit=4096,drawValid=true,shaderValid=true;
const gl={RGBA16F:0x881a,FRAMEBUFFER_COMPLETE:0x8cd5,NO_ERROR:0,
 getParameter:()=>textureLimit,
 getExtension:name=>name==='WEBGL_lose_context'?{loseContext(){released++}}:floatAvailable?{}:null,
 createTexture:()=>({}),createFramebuffer:()=>({}),checkFramebufferStatus:()=>framebufferComplete?0x8cd5:0,
 createShader:()=>({}),getShaderParameter:()=>shaderValid,getShaderInfoLog:()=> 'test shader failure',
 createProgram:()=>({}),getProgramParameter:()=>true,createVertexArray:()=>({}),getUniformLocation:()=>0,getError:()=>0,
 readPixels:(x,y,w,h,format,type,data)=>data.set(drawValid?[102,76,76,255]:[0,0,0,0])};
for(const name of ['bindTexture','texParameteri','texImage2D','bindFramebuffer','framebufferTexture2D','deleteFramebuffer','deleteTexture',
 'shaderSource','compileShader','attachShader','linkProgram','bindVertexArray','useProgram','uniform1i','activeTexture',
 'viewport','clearColor','clear','enable','disable','blendFuncSeparate','drawArrays','deleteProgram','deleteShader','deleteVertexArray'])gl[name]=()=>{};
globalThis.document={createElement:()=>({addEventListener(){},getContext:()=>glAvailable?gl:null})};
const limits={maxTextureDimension2D:8192,maxSampledTexturesPerShaderStage:16,maxBindGroups:4};
let requests=0,destroyed=0,requested,gpuDrawValid=true,gpuValidationError=null;
globalThis.GPUShaderStage={FRAGMENT:2};globalThis.GPUTextureUsage={TEXTURE_BINDING:4,COPY_DST:2,COPY_SRC:1,RENDER_ATTACHMENT:16};
globalThis.GPUBufferUsage={COPY_DST:8,MAP_READ:1};globalThis.GPUMapMode={READ:1};
const device=()=>({destroy(){destroyed++},pushErrorScope(){},popErrorScope:async()=>gpuValidationError,
 createShaderModule:()=>({}),createBindGroupLayout:()=>({}),createPipelineLayout:()=>({}),createRenderPipelineAsync:async()=>({}),
 createTexture:()=>({createView:()=>({}),destroy(){}}),createSampler:()=>({}),createBindGroup:()=>({}),
 createBuffer:()=>({mapAsync:async()=>{},getMappedRange:()=>new Uint8Array(gpuDrawValid?[102,76,76,255]:[0,0,0,0]).buffer,unmap(){},destroy(){}}),
 createCommandEncoder:()=>({beginRenderPass:()=>({setPipeline(){},setBindGroup(){},draw(){},end(){}}),copyTextureToBuffer(){},finish:()=>({})}),
 queue:{writeTexture(){},submit(){}}});
const adapter={features:new Set(['dual-source-blending','timestamp-query']),limits,info:{vendor:'test'},requestDevice:async options=>{requests++;requested=options;return device()}};
const gpu={requestAdapter:async()=>adapter};
let result=await detectGraphics(gpu);assert.equal(result.backend,'webgpu');assert.deepEqual(requested.requiredFeatures,['dual-source-blending']);assert.equal(destroyed,1);assert.match(capabilityText(result),/两套/);
result=await detectGraphics(undefined);assert.equal(result.backend,'webgl');assert.match(result.reason,/安全上下文/);
result=await detectGraphics({requestAdapter:async()=>null});assert.equal(result.backend,'webgl');
result=await detectGraphics({requestAdapter:async()=>({...adapter,requestDevice:async()=>{throw Error('device denied')}})});assert.equal(result.backend,'webgl');assert.match(result.reason,/device denied/);
result=await detectGraphics({requestAdapter:async()=>({...adapter,features:new Set()})});assert.equal(result.backend,'webgpu');assert.deepEqual(requested.requiredFeatures,[]);
const before=requests;result=await detectGraphics(gpu,{webgpu:false});assert.equal(result.backend,'webgl');assert.equal(result.webgpuProbed,false);assert.equal(requests,before);
result=await detectGraphics(gpu,{webgl:false});assert.equal(result.backend,'webgpu');assert.equal(result.webgl,false);assert.match(result.webglReason,/未包含/);
glAvailable=false;result=await detectGraphics(undefined);assert.equal(result.backend,null);assert.equal(result.webgl,false);assert.match(result.webglReason,/上下文/);
result=await detectGraphics(gpu);assert.equal(result.backend,'webgpu');assert.equal(result.webgl,false);
glAvailable=true;floatAvailable=false;result=await detectGraphics(undefined);assert.equal(result.backend,null);assert.match(result.webglReason,/EXT_color_buffer_float/);
floatAvailable=true;framebufferComplete=false;result=await detectGraphics(undefined);assert.equal(result.backend,null);assert.match(result.webglReason,/RGBA16F/);
framebufferComplete=true;drawValid=false;result=await detectGraphics(undefined);assert.equal(result.backend,null);assert.match(result.webglReason,/实际绘制.*像素/);
drawValid=true;shaderValid=false;result=await detectGraphics(undefined);assert.equal(result.backend,null);assert.match(result.webglReason,/着色器编译/);
shaderValid=true;gpuDrawValid=false;result=await detectGraphics(gpu);assert.equal(result.backend,'webgl');assert.match(result.reason,/实际绘制.*像素/);
gpuDrawValid=true;gpuValidationError={message:'probe validation error'};result=await detectGraphics(gpu);assert.equal(result.backend,'webgl');assert.match(result.reason,/probe validation error/);
gpuValidationError=null;
assert.ok(released>0);
textureLimit=2048;result=await detectGraphics(undefined);assert.equal(result.backend,null);assert.match(result.webglReason,/2048.*4096/);
console.log('Graphics capability selection and fallback: passed');
