import assert from 'node:assert/strict';
import {detectGraphics,capabilityText} from './game/graphics.js';
let glAvailable=true,floatAvailable=true,framebufferComplete=true,released=0,textureLimit=4096;
const gl={RGBA16F:0x881a,FRAMEBUFFER_COMPLETE:0x8cd5,
 getParameter:()=>textureLimit,
 getExtension:name=>name==='WEBGL_lose_context'?{loseContext(){released++}}:floatAvailable?{}:null,
 createTexture:()=>({}),createFramebuffer:()=>({}),checkFramebufferStatus:()=>framebufferComplete?0x8cd5:0};
for(const name of ['bindTexture','texImage2D','bindFramebuffer','framebufferTexture2D','deleteFramebuffer','deleteTexture'])gl[name]=()=>{};
globalThis.document={createElement:()=>({addEventListener(){},getContext:()=>glAvailable?gl:null})};
const limits={maxTextureDimension2D:8192,maxSampledTexturesPerShaderStage:16,maxBindGroups:4};
let requests=0,destroyed=0,requested;
const adapter={features:new Set(['dual-source-blending','timestamp-query']),limits,info:{vendor:'test'},requestDevice:async options=>{requests++;requested=options;return {destroy(){destroyed++}}}};
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
assert.ok(released>0);
textureLimit=2048;result=await detectGraphics(undefined);assert.equal(result.backend,null);assert.match(result.webglReason,/2048.*4096/);
console.log('Graphics capability selection and fallback: passed');
