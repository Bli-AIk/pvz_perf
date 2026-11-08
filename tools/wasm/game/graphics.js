// Probe a separate canvas, then release it: selecting a context on the game
// canvas here would prevent switching that canvas to the other graphics API.
function probeWebGL(){
 const canvas=document.createElement('canvas');canvas.width=canvas.height=2;
 let gl=null,reason='';const textures=[],framebuffers=[],shaders=[];let program=null,vao=null;
 canvas.addEventListener('webglcontextcreationerror',event=>{reason=event.statusMessage||reason});
 try{
  // Keep these in sync with WebWindow.cpp's explicit SDL/EGL framebuffer
  // attributes. Probe the same 2D surface configuration the game will request.
  gl=canvas.getContext('webgl2',{alpha:true,depth:false,stencil:false,antialias:false});
  if(!gl)throw Error(reason||'浏览器无法创建 WebGL 2 上下文');
  const webglLimits={maxTextureDimension2D:gl.getParameter(gl.MAX_TEXTURE_SIZE),maxRenderbufferSize:gl.getParameter(gl.MAX_RENDERBUFFER_SIZE)};
  if(webglLimits.maxTextureDimension2D<4096)throw Error('最大二维纹理仅 '+webglLimits.maxTextureDimension2D+'，此资源包需要 4096');
  // The renderer uses RGBA16F intermediate images, including generated fonts.
  if(!gl.getExtension('EXT_color_buffer_float'))throw Error('缺少 EXT_color_buffer_float，无法绘制浮点中间纹理');
  const texture=(format,width,height,data=null)=>{
   const value=gl.createTexture();textures.push(value);gl.bindTexture(gl.TEXTURE_2D,value);
   gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MIN_FILTER,gl.LINEAR);gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MAG_FILTER,gl.LINEAR);
   gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_WRAP_S,gl.CLAMP_TO_EDGE);gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_WRAP_T,gl.CLAMP_TO_EDGE);
   // Match the native renderer's allocation type, not just framebuffer completeness.
   gl.texImage2D(gl.TEXTURE_2D,0,format,width,height,0,gl.RGBA,format===gl.RGBA16F?gl.FLOAT:gl.UNSIGNED_BYTE,data);
   return value;
  };
  const target=image=>{
   const fbo=gl.createFramebuffer();framebuffers.push(fbo);gl.bindFramebuffer(gl.FRAMEBUFFER,fbo);
   gl.framebufferTexture2D(gl.FRAMEBUFFER,gl.COLOR_ATTACHMENT0,gl.TEXTURE_2D,image,0);
   if(gl.checkFramebufferStatus(gl.FRAMEBUFFER)!==gl.FRAMEBUFFER_COMPLETE)throw Error('无法创建 RGBA16F 绘制目标');
  };
  const shader=(type,source)=>{
   const value=gl.createShader(type);shaders.push(value);gl.shaderSource(value,source);gl.compileShader(value);
   if(!gl.getShaderParameter(value,gl.COMPILE_STATUS))throw Error('WebGL 绘制验证着色器编译失败：'+gl.getShaderInfoLog(value));
   return value;
  };
  const vertex=shader(gl.VERTEX_SHADER,`#version 300 es
precision highp float;
out vec2 uv;
void main(){vec2 p=gl_VertexID==0?vec2(-1.,-1.):gl_VertexID==1?vec2(3.,-1.):vec2(-1.,3.);uv=p*.5+.5;gl_Position=vec4(p,0.,1.);}`);
  const fragment=shader(gl.FRAGMENT_SHADER,`#version 300 es
precision highp float;
uniform highp sampler2D image;
in vec2 uv;out vec4 color;
void main(){color=texture(image,uv);}`);
  program=gl.createProgram();gl.attachShader(program,vertex);gl.attachShader(program,fragment);gl.linkProgram(program);
  if(!gl.getProgramParameter(program,gl.LINK_STATUS))throw Error('WebGL 绘制验证链接失败：'+gl.getProgramInfoLog(program));
  vao=gl.createVertexArray();gl.bindVertexArray(vao);gl.useProgram(program);gl.uniform1i(gl.getUniformLocation(program,'image'),0);
  gl.activeTexture(gl.TEXTURE0);gl.viewport(0,0,2,2);
  const source=texture(gl.RGBA8,1,1,new Uint8Array([153,51,0,128]));
  const floating=texture(gl.RGBA16F,2,2);target(floating);
  gl.clearColor(.2,.4,.6,1);gl.clear(gl.COLOR_BUFFER_BIT);gl.enable(gl.BLEND);
  gl.blendFuncSeparate(gl.SRC_ALPHA,gl.ONE_MINUS_SRC_ALPHA,gl.ONE,gl.ONE_MINUS_SRC_ALPHA);
  gl.bindTexture(gl.TEXTURE_2D,source);gl.drawArrays(gl.TRIANGLES,0,3);
  const output=texture(gl.RGBA8,2,2);target(output);gl.disable(gl.BLEND);
  gl.bindTexture(gl.TEXTURE_2D,floating);gl.drawArrays(gl.TRIANGLES,0,3);
  const pixel=new Uint8Array(4);gl.readPixels(1,1,1,1,gl.RGBA,gl.UNSIGNED_BYTE,pixel);
  const error=gl.getError();if(error!==gl.NO_ERROR)throw Error('WebGL 绘制验证失败，GL error '+error);
  verifyProbePixel(pixel,'WebGL');
  return {webgl:true,webglReason:'',webglLimits};
 }catch(error){return {webgl:false,webglReason:error.message||String(error)}}
 finally{if(gl){gl.bindFramebuffer(gl.FRAMEBUFFER,null);for(const value of framebuffers)gl.deleteFramebuffer(value);for(const value of textures)gl.deleteTexture(value);if(program)gl.deleteProgram(program);for(const value of shaders)gl.deleteShader(value);if(vao)gl.deleteVertexArray(vao);gl.getExtension('WEBGL_lose_context')?.loseContext()}}
}
function verifyProbePixel(pixel,backend){
 const expected=[102,76,76,255];
 if(expected.some((value,i)=>Math.abs(pixel[i]-value)>3))throw Error(backend+' 实际绘制／混合／采样验证失败，像素为 '+Array.from(pixel).join(','));
}
async function probeWebGPU(gpu,signal){
 if(!gpu)throw Error('当前环境没有开放 WebGPU，需要安全上下文及浏览器支持');
 const adapter=await gpu.requestAdapter();if(!adapter)throw Error('没有可用的 WebGPU 适配器');
 signal.throwIfAborted();
 const features=[...adapter.features].sort(),limits={},info=adapter.info||{},details={};
 for(const key of ['maxTextureDimension2D','maxTextureArrayLayers','maxSampledTexturesPerShaderStage','maxBindGroups','maxVertexBufferArrayStride','maxBufferSize'])limits[key]=adapter.limits[key];
 for(const key of ['vendor','architecture','device','description'])if(info[key])details[key]=info[key];
 const requiredFeatures=features.includes('dual-source-blending')?['dual-source-blending']:[];
 if(limits.maxTextureDimension2D<4096)throw Error('最大二维纹理仅 '+limits.maxTextureDimension2D+'，此资源包需要 4096');
 const device=await adapter.requestDevice({requiredFeatures});
 const abort=()=>device.destroy();signal.addEventListener('abort',abort,{once:true});
 try{signal.throwIfAborted();await verifyWebGPUDraw(device)}finally{signal.removeEventListener('abort',abort);device.destroy()}
 return {webgpu:true,features,limits,adapter:details};
}
async function verifyWebGPUDraw(device){
 const textures=[];let readback;
 device.pushErrorScope('validation');
 try{
  const shader=device.createShaderModule({code:`
struct Vertex{ @builtin(position) position:vec4f, @location(0) uv:vec2f }
@vertex fn vs(@builtin(vertex_index) id:u32)->Vertex{
 let p=array<vec2f,3>(vec2f(-1,-1),vec2f(3,-1),vec2f(-1,3))[id];
 var v:Vertex;v.position=vec4f(p,0,1);v.uv=p*.5+.5;return v;
}
@group(0) @binding(0) var image:texture_2d<f32>;
@group(0) @binding(1) var filtering:sampler;
@fragment fn fs(v:Vertex)->@location(0) vec4f{return textureSample(image,filtering,v.uv);}`});
  const layout=device.createBindGroupLayout({entries:[{binding:0,visibility:GPUShaderStage.FRAGMENT,texture:{}},{binding:1,visibility:GPUShaderStage.FRAGMENT,sampler:{}}]});
  const pipelineLayout=device.createPipelineLayout({bindGroupLayouts:[layout]});
  const pipeline=(format,blend)=>device.createRenderPipelineAsync({layout:pipelineLayout,vertex:{module:shader,entryPoint:'vs'},fragment:{module:shader,entryPoint:'fs',targets:[{format,...(blend?{blend}: {})}]},primitive:{topology:'triangle-list'}});
  const blended=await pipeline('rgba16float',{color:{srcFactor:'src-alpha',dstFactor:'one-minus-src-alpha'},alpha:{srcFactor:'one',dstFactor:'one-minus-src-alpha'}});
  const sampled=await pipeline('rgba8unorm');
  const texture=(format,size,usage)=>{const value=device.createTexture({format,size,usage});textures.push(value);return value};
  const source=texture('rgba8unorm',[1,1],GPUTextureUsage.TEXTURE_BINDING|GPUTextureUsage.COPY_DST);
  const floating=texture('rgba16float',[2,2],GPUTextureUsage.RENDER_ATTACHMENT|GPUTextureUsage.TEXTURE_BINDING);
  const output=texture('rgba8unorm',[2,2],GPUTextureUsage.RENDER_ATTACHMENT|GPUTextureUsage.COPY_SRC);
  device.queue.writeTexture({texture:source},new Uint8Array([153,51,0,128]),{bytesPerRow:4},[1,1]);
  const sampler=device.createSampler({minFilter:'linear',magFilter:'linear'});
  const encoder=device.createCommandEncoder();
  const draw=(target,image,program,clearValue)=>{
   const pass=encoder.beginRenderPass({colorAttachments:[{view:target.createView(),loadOp:'clear',storeOp:'store',clearValue}]});
   pass.setPipeline(program);pass.setBindGroup(0,device.createBindGroup({layout,entries:[{binding:0,resource:image.createView()},{binding:1,resource:sampler}]}));pass.draw(3);pass.end();
  };
  draw(floating,source,blended,{r:.2,g:.4,b:.6,a:1});draw(output,floating,sampled,{r:0,g:0,b:0,a:1});
  readback=device.createBuffer({size:256,usage:GPUBufferUsage.COPY_DST|GPUBufferUsage.MAP_READ});
  encoder.copyTextureToBuffer({texture:output,origin:[1,1]},{buffer:readback,bytesPerRow:256},[1,1]);
  device.queue.submit([encoder.finish()]);await readback.mapAsync(GPUMapMode.READ);
  const pixel=new Uint8Array(readback.getMappedRange()).slice(0,4);readback.unmap();
  const error=await device.popErrorScope();if(error)throw Error('WebGPU 绘制验证失败：'+error.message);
  verifyProbePixel(pixel,'WebGPU');
 }finally{readback?.destroy();for(const texture of textures)texture.destroy()}
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
 let timer;const controller=new AbortController();
 result.webgpuProbed=true;
 try{
  Object.assign(result,await Promise.race([probeWebGPU(gpu,controller.signal),new Promise((_,reject)=>{timer=setTimeout(()=>{const error=Error('WebGPU 能力检测超时');controller.abort(error);reject(error)},10000)})]));
  result.backend='webgpu';
 }catch(error){result.reason=error.message||String(error)}
 finally{clearTimeout(timer)}
 if(!result.webgpu&&result.webgl)result.backend='webgl';
 return result;
}
export function capabilityText(result){
 const lines=[result.webgpu?'WebGPU：可用':(result.webgpuProbed?'WebGPU：不可用':'WebGPU：未检测')+(result.reason?'（'+result.reason+'）':''),
  result.webgl?'WebGL 2：可用（RGBA16F 绘制／混合／采样验证通过）':'WebGL 2：无法运行本游戏（'+result.webglReason+'）',
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
