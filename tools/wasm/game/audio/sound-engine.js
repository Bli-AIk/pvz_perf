export function createPCMBuffer(context,samples,channels,rate){
 if(!Number.isInteger(channels)||channels<1||channels>2||!Number.isInteger(rate)||rate<=0||!samples.length||samples.length%channels)throw Error('Invalid PCM audio');
 const frames=samples.length/channels,buffer=context.createBuffer(channels,frames,rate);
 for(let channel=0;channel<channels;++channel){const output=buffer.getChannelData(channel);for(let i=0;i<frames;++i)output[i]=samples[i*channels+channel]}
 return buffer;
}
export class SoundEngine {
 constructor(context){this.context=context;this.sources=new Map();this.channels=new Map();this.master=context.createGain();this.master.connect(context.destination)}
 async decode(bytes){
  const header=new DataView(bytes.buffer,bytes.byteOffset,bytes.byteLength);
  if(bytes.length>=24&&header.getUint32(0)===0x2e736e64){
   const offset=header.getUint32(4),size=header.getUint32(8),encoding=header.getUint32(12),rate=header.getUint32(16),channels=header.getUint32(20);
   if(encoding!==1||!channels||!rate||offset>bytes.length)throw Error('Unsupported AU audio format');
   const count=Math.min(size===0xffffffff?bytes.length-offset:size,bytes.length-offset),frames=Math.floor(count/channels);
   const buffer=this.context.createBuffer(channels,frames,rate);
   for(let channel=0;channel<channels;++channel){const output=buffer.getChannelData(channel);for(let i=0;i<frames;++i){const u=(~bytes[offset+i*channels+channel])&255;const value=((((u&15)<<3)+132)<<((u&112)>>4))-132;output[i]=(u&128?-value:value)/32768}}
   return buffer;
  }
  // Keep WAV support for authored sound packs; shipped OGG never reaches the
  // browser codec path. Pass only this view, not unrelated ArrayBuffer bytes.
  if(bytes.length>=12&&header.getUint32(0)===0x52494646&&header.getUint32(8)===0x57415645)
   return this.context.decodeAudioData(bytes.buffer.slice(bytes.byteOffset,bytes.byteOffset+bytes.byteLength));
  throw Error('Unsupported sound encoding; OGG must be decoded by Wasm');
 }
 loadPCM(id,samples,channels,rate){this.sources.set(id,{buffer:createPCMBuffer(this.context,samples,channels,rate),promise:Promise.resolve()})}
 load(id,bytes){const entry={buffer:null};entry.promise=this.decode(bytes).then(buffer=>{entry.buffer=buffer});entry.promise.catch(()=>{});this.sources.set(id,entry)}
 async finish(){await Promise.all(Array.from(this.sources.values(),s=>s.promise))}
 release(id){this.sources.delete(id)}
 voice(channel){
  let v=this.channels.get(channel);if(v)return v;
  const gain=this.context.createGain(),pan=this.context.createStereoPanner();gain.connect(pan);pan.connect(this.master);
  v={gain,pan,source:null,offset:0,started:0,rate:1,playing:false,loop:false};this.channels.set(channel,v);return v;
 }
 position(v){const value=v.offset+(v.playing?(this.context.currentTime-v.started)*v.rate:0);return v.loop&&v.buffer?value%v.buffer.duration:value}
 play(id,channel,loop,volume,pitch,pan){
  const buffer=this.sources.get(id)?.buffer;if(!buffer)return false;
  this.stop(channel);const v=this.voice(channel);v.buffer=buffer;v.loop=loop;v.offset=0;v.rate=2**(pitch/12);
  v.gain.gain.value=Math.max(0,volume);v.pan.pan.value=Math.max(-1,Math.min(1,pan/10000));this.start(v);return true;
 }
 start(v){
  const node=v.source=this.context.createBufferSource();node.buffer=v.buffer;node.loop=v.loop;node.playbackRate.value=v.rate;node.connect(v.gain);
  node.onended=()=>{node.disconnect();if(v.source===node){v.source=null;v.playing=false}};
  v.started=this.context.currentTime;v.playing=true;node.start(0,v.loop?v.offset%v.buffer.duration:Math.min(v.offset,v.buffer.duration));
 }
 stop(channel){const v=this.channels.get(channel);if(v){v.source?.stop();v.source=null;v.playing=false;v.offset=0}}
 playing(channel){return this.channels.get(channel)?.playing||false}
 pause(channel){const v=this.channels.get(channel);if(!v)return 0;v.offset=this.position(v);v.source?.stop();v.source=null;v.playing=false;return Math.round(v.offset*1000)}
 resume(channel,offset,loop){const v=this.channels.get(channel);if(v?.buffer){v.offset=offset/1000;v.loop=loop;this.start(v)}}
 parameters(channel,volume,pitch,pan){const v=this.channels.get(channel);if(!v)return;v.offset=this.position(v);v.started=this.context.currentTime;v.rate=2**(pitch/12);v.gain.gain.value=Math.max(0,volume);v.pan.pan.value=Math.max(-1,Math.min(1,pan/10000));if(v.source)v.source.playbackRate.value=v.rate}
 volume(value){this.master.gain.value=Math.max(0,value)}
 close(){for(const channel of this.channels.keys())this.stop(channel);for(const v of this.channels.values()){v.gain.disconnect();v.pan.disconnect()}this.channels.clear();this.sources.clear();this.master.disconnect()}
}
