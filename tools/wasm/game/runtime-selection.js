// Runtime selection changes only which game binary is loaded. Resources,
// settings and save storage are shared by all memory models/backends.
export function supportsMemory64(){
 try{return WebAssembly.validate(new Uint8Array([0,97,115,109,1,0,0,0,5,3,1,4,1]))}catch{return false}
}
export function memoryModels(config,supported){
 if(config.memory64){
  if(!supported)throw Error('此诊断包需要 Wasm Memory64；当前浏览器不支持。64 位手机或操作系统不等于浏览器支持 Memory64。');
  return ['wasm64'];
 }
 return config.memory64Available&&supported?['wasm32','wasm64']:['wasm32'];
}
export function selectMemoryModel(models,saved){return models.includes(saved)?saved:models[0]}
export function runtimeStem(config,backend,memoryModel){
 const base=config.webgpu&&backend==='webgl'?'pvz-perf-webgl':'pvz-perf';
 return base+(memoryModel==='wasm64'&&!config.memory64?'-wasm64':'');
}
