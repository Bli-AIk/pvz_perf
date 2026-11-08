import assert from 'node:assert/strict';
import fs from 'node:fs';
const source=fs.readFileSync('tools/wasm/game/runtime-selection.js','utf8');
const {memoryModels,selectMemoryModel,runtimeStem}=await import('data:text/javascript;base64,'+Buffer.from(source).toString('base64'));
const both={webgpu:true,fallback:true,memory64Available:true};
assert.deepEqual(memoryModels(both,true),['wasm32','wasm64']);
assert.deepEqual(memoryModels(both,false),['wasm32']);
assert.equal(selectMemoryModel(memoryModels(both,true),null),'wasm32');
assert.equal(selectMemoryModel(memoryModels(both,true),'wasm64'),'wasm64');
assert.equal(selectMemoryModel(memoryModels(both,false),'wasm64'),'wasm32');
assert.equal(selectMemoryModel(memoryModels(both,true),'invalid'),'wasm32');
assert.deepEqual(memoryModels({},true),['wasm32']);
assert.throws(()=>memoryModels({memory64:true},false),/Memory64/);
assert.deepEqual(memoryModels({memory64:true},true),['wasm64']);
for(const backend of ['webgpu','webgl'])for(const memory of ['wasm32','wasm64']){
 assert.equal(runtimeStem(both,backend,memory),'pvz-perf'+(backend==='webgl'?'-webgl':'')+(memory==='wasm64'?'-wasm64':''));
}
assert.equal(runtimeStem({memory64:true},'webgl','wasm64'),'pvz-perf');
console.log('PASS default32, opt-in64, unavailable/stale selection, all four filenames, standalone diagnostics');
