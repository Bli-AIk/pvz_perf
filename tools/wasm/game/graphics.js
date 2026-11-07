// Probe a separate canvas, then release it: selecting a context on the game
// canvas here would prevent switching that canvas to the other graphics API.
function probeWebGL(){
 const canvas=document.createElement('canvas');canvas.width=canvas.height=2;
 let gl=null,texture=null,framebuffer=null,reason='';
 canvas.addEventListener('webglcontextcreationerror',event=>{reason=event.statusMessage||reason});
 try{
  gl=canvas.getContext('webgl2');
  if(!gl)throw Error(reason||'浏览器无法创建 WebGL 2 上下文');
  const webglLimits={maxTextureDimension2D:gl.getParameter(gl.MAX_TEXTURE_SIZE),maxRenderbufferSize:gl.getParameter(gl.MAX_RENDERBUFFER_SIZE)};
  if(webglLimits.maxTextureDimension2D<4096)throw Error('最大二维纹理仅 '+webglLimits.maxTextureDimension2D+'，此资源包需要 4096');
  // The renderer uses RGBA16F intermediate images, including generated fonts.
  if(!gl.getExtension('EXT_color_buffer_float'))throw Error('缺少 EXT_color_buffer_float，无法绘制浮点中间纹理');
  texture=gl.createTexture();framebuffer=gl.createFramebuffer();
  gl.bindTexture(gl.TEXTURE_2D,texture);
  gl.texImage2D(gl.TEXTURE_2D,0,gl.RGBA16F,2,2,0,gl.RGBA,gl.HALF_FLOAT,null);
  gl.bindFramebuffer(gl.FRAMEBUFFER,framebuffer);
  gl.framebufferTexture2D(gl.FRAMEBUFFER,gl.COLOR_ATTACHMENT0,gl.TEXTURE_2D,texture,0);
  if(gl.checkFramebufferStatus(gl.FRAMEBUFFER)!==gl.FRAMEBUFFER_COMPLETE)throw Error('无法创建 RGBA16F 绘制目标');
  return {webgl:true,webglReason:'',webglLimits};
 }catch(error){return {webgl:false,webglReason:error.message||String(error)}}
 finally{if(gl){gl.bindFramebuffer(gl.FRAMEBUFFER,null);if(framebuffer)gl.deleteFramebuffer(framebuffer);if(texture)gl.deleteTexture(texture);gl.getExtension('WEBGL_lose_context')?.loseContext()}}
}
async function probeWebGPU(gpu){
 if(!gpu)throw Error('当前环境没有开放 WebGPU，需要安全上下文及浏览器支持');
 const adapter=await gpu.requestAdapter();if(!adapter)throw Error('没有可用的 WebGPU 适配器');
 const features=[...adapter.features].sort(),limits={},info=adapter.info||{},details={};
 for(const key of ['maxTextureDimension2D','maxTextureArrayLayers','maxSampledTexturesPerShaderStage','maxBindGroups','maxVertexBufferArrayStride','maxBufferSize'])limits[key]=adapter.limits[key];
 for(const key of ['vendor','architecture','device','description'])if(info[key])details[key]=info[key];
 const requiredFeatures=features.includes('dual-source-blending')?['dual-source-blending']:[];
 const device=await adapter.requestDevice({requiredFeatures});device.destroy();
 return {webgpu:true,features,limits,adapter:details};
}
// Capability probing happens before main, once per page load. Never in the draw loop.
export async function detectGraphics(gpu, {webgpu:hasWebGPU=true,webgl:hasWebGL=true}={}){
 const result={backend:null,webgpu:false,webgpuProbed:false,reason:'',features:[],limits:{},adapter:{},framesInFlight:2,bindless:false,
  ...(hasWebGL?probeWebGL():{webgl:false,webglReason:'此包未包含 WebGL 2'})};
 if(!hasWebGPU){
  result.reason='此包未包含 WebGPU';
  if(result.webgl)result.backend='webgl';
  return result;
 }
 let timer;
 result.webgpuProbed=true;
 try{
  Object.assign(result,await Promise.race([probeWebGPU(gpu),new Promise((_,reject)=>{timer=setTimeout(()=>reject(Error('WebGPU 能力检测超时')),10000)})]));
  result.backend='webgpu';
 }catch(error){result.reason=error.message||String(error)}
 finally{clearTimeout(timer)}
 if(!result.webgpu&&result.webgl)result.backend='webgl';
 return result;
}
export function capabilityText(result){
 const lines=[result.webgpu?'WebGPU：可用':(result.webgpuProbed?'WebGPU：不可用':'WebGPU：未检测')+(result.reason?'（'+result.reason+'）':''),
  result.webgl?'WebGL 2：可用（RGBA16F 绘制目标验证通过）':'WebGL 2：无法运行本游戏（'+result.webglReason+'）',
  '双源混合：'+(result.features.includes('dual-source-blending')?'支持，WebGPU 路径启用':'不支持，normal/additive 分开提交'),
  '帧资源：两套动态顶点资源，纹理共用','Bindless：未启用，使用缓存绑定组'];
 if(result.webglLimits)lines.push('WebGL 2 最大二维纹理：'+result.webglLimits.maxTextureDimension2D+'；最大绘制目标：'+result.webglLimits.maxRenderbufferSize);
 if(Object.keys(result.adapter).length)lines.push('适配器：'+Object.values(result.adapter).join(' / '));
 if(result.limits.maxTextureDimension2D)lines.push('最大二维纹理：'+result.limits.maxTextureDimension2D,
  '每着色阶段纹理上限：'+result.limits.maxSampledTexturesPerShaderStage,
  '绑定组上限：'+result.limits.maxBindGroups);
 lines.push('其他可选能力：'+(['timestamp-query','shader-f16','float32-filterable','texture-compression-bc','texture-compression-astc'].filter(f=>result.features.includes(f)).join('、')||'无'));
 return lines.join('\n');
}
