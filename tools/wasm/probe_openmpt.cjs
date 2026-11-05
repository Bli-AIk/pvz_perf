const fs=require('fs'),vm=require('vm'),path=require('path');
if(!process.argv[2])throw Error('Usage: node tools/wasm/probe_openmpt.cjs <official bin/wasm directory>');
const root=path.resolve(process.argv[2])+'/';
// The official bundle keeps heap views in its script scope, not on Module.
const ctx=vm.createContext({require,__dirname:root,__filename:root+'libopenmpt.js',process,console,performance,TextDecoder,TextEncoder,URL,WebAssembly,fetch,setTimeout,clearTimeout});
ctx.libopenmpt={onRuntimeInitialized(){const m=ctx.libopenmpt;for(const name of ['mainmusic.mo3','mainmusic_hihats.mo3']){
 const data=fs.readFileSync(path.resolve(__dirname,'../../assets/runtime/sounds',name)),p=m._malloc(data.length);ctx.HEAPU8.set(data,p);
 const ext=m._openmpt_module_ext_create_from_memory(p,data.length,0,0,0,0,0,0,0);const mod=m._openmpt_module_ext_get_module(ext);if(!mod)throw Error('MO3 load failed');
 const frames=48000,out=m._malloc(frames*8);let sum=0,peak=0,count=0;for(let second=0;second<10;second++){const read=m._openmpt_module_read_interleaved_float_stereo(mod,48000,frames,out);for(const x of new Float32Array(ctx.HEAPU8.buffer).subarray(out/4,out/4+read*2)){sum+=x*x;peak=Math.max(peak,Math.abs(x));count++}}
 const id=m._malloc(12),iface=m._malloc(64);ctx.HEAPU8.set(Buffer.from('interactive\0'),id);
 const interactive=!!m._openmpt_module_ext_get_interface(ext,id,iface,64);
 let channelVolume=false,tempo=false;
 if(interactive){const slots=new Uint32Array(ctx.HEAPU8.buffer,iface,16),table=ctx.wasmTable;
  channelVolume=!!table.get(slots[8])(ext,0,.5)&&Math.abs(table.get(slots[9])(ext,0)-.5)<1e-6;
  const tempoSet=table.get(slots[2])(ext,1.1),tempoGet=table.get(slots[3])(ext);tempo=!!tempoSet&&Math.abs(tempoGet-1.1)<.001;}
 const seek=m._openmpt_module_set_position_order_row(mod,1,0);console.log(JSON.stringify({name,interactive,channelVolume,tempo,channels:m._openmpt_module_get_num_channels(mod),orders:m._openmpt_module_get_num_orders(mod),samples:count,peak,rms:Math.sqrt(sum/count),seekOrder1Seconds:seek,position:m._openmpt_module_get_position_seconds(mod)}));if(!interactive||!channelVolume||!tempo||peak<=0||count!==960000)throw Error('MO3 capability test failed');m._openmpt_module_ext_destroy(ext);m._free(id);m._free(iface);m._free(out);m._free(p);
}}};vm.runInContext(fs.readFileSync(root+'libopenmpt.js','utf8'),ctx);
