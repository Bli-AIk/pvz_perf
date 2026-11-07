import {createOpenMpt} from './openmpt-runtime.js';

// Only the browser audio thread owns decoder instances and PCM buffers.
export class MusicMixer {
 static async create(compiled,rate,entropy){
  let used=0;
  const api=await new Promise((resolve,reject)=>createOpenMpt({
   randomFill:entropy?(view=>{if(used+view.length>entropy.length)throw Error('Audio decoder entropy pool exhausted');view.set(entropy.subarray(used,used+view.length));used+=view.length;return 0}):undefined,
   instantiateWasm(imports,receive){const instance=new WebAssembly.Instance(compiled,imports);receive(instance);return instance.exports},
   onRuntimeInitialized(){resolve(this)},onAbort:reject,printErr:message=>console.error(message)
  }));
  return new MusicMixer(api,rate);
 }
 constructor(api,rate){this.api=api;this.rate=rate;this.voices=new Map();this.output=api._malloc(8192*8);this.frames=0}
 prime(v){
  if(v.primed)return;
  // OpenMPT executes destination-row commands on the first read, whereas
  // BASS resolves them during play/seek. Keep that frame for normal rendering
  // so later tempo/speed restoration is not overwritten or one sample lost.
  const count=this.api._openmpt_module_read_interleaved_float_stereo(v.mod,this.rate,1,this.output);
  const pcm=new Float32Array(this.api.heap().buffer,this.output,2);
  v.pendingFrame=count===1;v.firstLeft=pcm[0];v.firstRight=pcm[1];v.primed=true;
 }
 command(c){
  const m=this.api;
  if(c.op==='load'){
   const data=m._malloc(c.data.length);m.heap().set(c.data,data);
   const ext=m._openmpt_module_ext_create_from_memory(data,c.data.length,0,0,0,0,0,0,0);m._free(data);
   if(!ext)throw Error('Cannot decode MO3 voice '+c.id);
   const mod=m._openmpt_module_ext_get_module(ext),name=m._malloc(12),table=m._malloc(64);
   m.heap().set(new TextEncoder().encode('interactive\0'),name);
   if(!m._openmpt_module_ext_get_interface(ext,name,table,64))throw Error('Missing libopenmpt interactive interface');
   const ptrs=Array.from(new Uint32Array(m.heap().buffer,table,16));m._free(name);m._free(table);
   this.voices.set(c.id,{ext,mod,ptrs,gain:0,amplify:1,playing:false,paused:false,revision:0,channels:new Map(),primed:false,pendingFrame:false});return;
  }
  const v=this.voices.get(c.id);if(!v)return;
  const call=(index,...args)=>m.functionAt(v.ptrs[index])(v.ext,...args);
  switch(c.op){
   case 'free':m._openmpt_module_ext_destroy(v.ext);this.voices.delete(c.id);break;
   case 'play':
    if(c.restart){m._openmpt_module_set_position_order_row(v.mod,0,0);v.primed=false;v.pendingFrame=false}
    this.prime(v);
    v.playing=true;v.paused=false;break;
   case 'stop':v.playing=false;v.paused=false;break;
   case 'pause':v.paused=true;break;
   case 'resume':v.paused=false;v.playing=true;break;
   case 'seek':
    m._openmpt_module_set_position_order_row(v.mod,c.order,c.row);
    for(const [channel,volume] of v.channels)call(8,channel,volume);
    v.revision=c.revision;v.primed=false;v.pendingFrame=false;
    // Night music switches the already-playing silent voice to order 76/77
    // without another Play call. Resolve its row before MusicResync sets BPM.
    if(v.playing)this.prime(v);
    break;
   case 'loop':m._openmpt_module_set_repeat_count(v.mod,c.value?-1:0);break;
   case 'gain':v.gain=c.value;break;
   case 'channel':v.channels.set(c.channel,c.value);call(8,c.channel,c.value);break;
   case 'tempo':call(1,Math.round(c.value));break;
   case 'speed':call(0,Math.round(c.value));break;
   case 'amplify':v.amplify=c.value;break;
  }
 }
 render(left,right){
  left.fill(0);right.fill(0);const m=this.api,n=left.length;
  for(const v of this.voices.values()){
   if(!v.playing||v.paused)continue;
   const gain=v.gain*v.amplify,offset=v.pendingFrame?1:0;
   if(v.pendingFrame){left[0]+=v.firstLeft*gain;right[0]+=v.firstRight*gain;v.pendingFrame=false}
   const count=n>offset?m._openmpt_module_read_interleaved_float_stereo(v.mod,this.rate,n-offset,this.output):0;
   const pcm=new Float32Array(m.heap().buffer,this.output,count*2);
   for(let i=0;i<count;++i){left[i+offset]+=pcm[i*2]*gain;right[i+offset]+=pcm[i*2+1]*gain}
   if(count<n-offset)v.playing=false;
  }
  this.frames+=n;
 }
 positions(){
  const m=this.api;
  return Array.from(this.voices,([id,v])=>({id,revision:v.revision,playing:v.playing,paused:v.paused,
   position:(m._openmpt_module_get_current_order(v.mod)&65535)|((m._openmpt_module_get_current_row(v.mod)&65535)<<16),
   tempo:m._openmpt_module_get_current_tempo(v.mod),speed:m._openmpt_module_get_current_speed(v.mod)}));
 }
 close(){for(const id of this.voices.keys())this.command({op:'free',id});this.api._free(this.output)}
}
