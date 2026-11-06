// Capability probing happens before main, once per page load. Never in the draw loop.
export async function detectGraphics(gpu, mode='auto', hasWebGPU=true){
 const result={backend:'webgl',webgpu:false,reason:'',features:[],limits:{},adapter:{},framesInFlight:2,bindless:false};
 if(!hasWebGPU){result.reason='此包仅包含 WebGL 2';return result}
 try{
  if(!gpu)throw Error('当前环境没有开放 WebGPU，需要安全上下文及浏览器支持');
  const adapter=await gpu.requestAdapter();if(!adapter)throw Error('没有可用的 WebGPU 适配器');
  result.features=[...adapter.features].sort();
  for(const key of ['maxTextureDimension2D','maxTextureArrayLayers','maxSampledTexturesPerShaderStage','maxBindGroups','maxVertexBufferArrayStride','maxBufferSize'])result.limits[key]=adapter.limits[key];
  const info=adapter.info||{};for(const key of ['vendor','architecture','device','description'])if(info[key])result.adapter[key]=info[key];
  const requiredFeatures=result.features.includes('dual-source-blending')?['dual-source-blending']:[];
  const device=await adapter.requestDevice({requiredFeatures});device.destroy();
  result.webgpu=true;if(mode!=='webgl')result.backend='webgpu';
 }catch(error){result.reason=error.message||String(error);if(mode==='webgpu')throw Error('WebGPU 初始化失败：'+result.reason)}
 return result;
}
export function capabilityText(result){
 const lines=[result.webgpu?'WebGPU：可用':'WebGPU：不可用'+(result.reason?'（'+result.reason+'）':''),
  '双源混合：'+(result.features.includes('dual-source-blending')?'支持，WebGPU 路径启用':'不支持，normal/additive 分开提交'),
  '帧资源：两套动态顶点资源，纹理共用','Bindless：未启用，使用缓存绑定组'];
 if(Object.keys(result.adapter).length)lines.push('适配器：'+Object.values(result.adapter).join(' / '));
 if(result.limits.maxTextureDimension2D)lines.push('最大二维纹理：'+result.limits.maxTextureDimension2D,
  '每着色阶段纹理上限：'+result.limits.maxSampledTexturesPerShaderStage,
  '绑定组上限：'+result.limits.maxBindGroups);
 lines.push('其他可选能力：'+(['timestamp-query','shader-f16','float32-filterable','texture-compression-bc','texture-compression-astc'].filter(f=>result.features.includes(f)).join('、')||'无'));
 return lines.join('\n');
}
