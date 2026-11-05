import {MusicMixer} from './music-mixer.js';
import {SoundEngine} from './sound-engine.js';
export async function prepareMusic(){
 const response=await fetch('./audio/libopenmpt.wasm');if(!response.ok)throw Error('Cannot load MO3 decoder');
 const compiled=await WebAssembly.compile(await response.arrayBuffer());
 return new MusicEngine(compiled);
}
class MusicEngine {
 constructor(compiled){this.compiled=compiled;this.voices=new Map();this.next=1;this.error='';this.backend='pending';this.flushes=new Map();this.nextFlush=1}
 async unlock(){
  if(this.context){await this.context.resume();return}
  const context=this.context=new AudioContext({latencyHint:'interactive'});
  const resumed=context.resume();this.sound=new SoundEngine(context);this.master=context.createGain();this.master.connect(context.destination);
  const receive=data=>{if(data.error){this.error=data.error;console.error(data.error)}
   if(data.voices)for(const row of data.voices){const v=this.voices.get(row.id);if(v&&v.revision===row.revision)Object.assign(v,row)}
   if(data.flush){const done=this.flushes.get(data.flush);if(done){this.flushes.delete(data.flush);done()}}};
  this.receive=receive;
  if(context.audioWorklet){
   await context.audioWorklet.addModule('./audio/music-worklet.js');
   this.node=new AudioWorkletNode(context,'pvz-music',{numberOfInputs:0,numberOfOutputs:1,outputChannelCount:[2],processorOptions:{compiled:this.compiled,entropy:crypto.getRandomValues(new Uint8Array(65536))}});
   await new Promise((resolve,reject)=>{this.node.port.onmessage=event=>{if(event.data.ready)resolve();else if(event.data.error)reject(Error(event.data.error));else receive(event.data)};this.node.onprocessorerror=()=>reject(Error('Music audio thread failed'))});
   this.send=c=>this.node.port.postMessage(c,c.data?[c.data.buffer]:[]);this.backend='AudioWorklet';
  }else{
   // Plain HTTP on a LAN has no AudioWorklet. Keep that development case usable.
   const mixer=await MusicMixer.create(this.compiled,context.sampleRate);this.mixer=mixer;
   this.node=context.createScriptProcessor(2048,0,2);this.node.onaudioprocess=event=>{try{mixer.render(event.outputBuffer.getChannelData(0),event.outputBuffer.getChannelData(1));receive({voices:mixer.positions()})}catch(error){receive({error:String(error)})}};
   this.send=c=>mixer.command(c);this.backend='ScriptProcessor fallback';
  }
  this.node.connect(this.master);await resumed;
 }
 flush(){
  if(this.mixer){this.receive({voices:this.mixer.positions()});return Promise.resolve()}
  const token=this.nextFlush++;
  return new Promise((resolve,reject)=>{
   const timer=setTimeout(()=>{this.flushes.delete(token);reject(Error('Music state synchronization timed out'))},5000);
   this.flushes.set(token,()=>{clearTimeout(timer);resolve()});this.send({op:'flush',token});
  });
 }
 load(data,stream){
  const id=this.next++,v={id,stream,playing:false,paused:false,position:0,revision:0,tempo:155,speed:3,gain:0,flags:0,offset:0};this.voices.set(id,v);
  if(!stream)this.send({op:'load',id,data});
  else this.context.decodeAudioData(data.buffer).then(buffer=>{if(!this.voices.has(id))return;v.buffer=buffer;v.gainNode=this.context.createGain();v.gainNode.gain.value=v.gain;v.gainNode.connect(this.master);if(v.playing&&!v.paused)this.startStream(v)}).catch(error=>{this.error=String(error);console.error(error)});
  return id;
 }
 startStream(v){
  v.source?.stop();if(!v.buffer)return;
  const source=v.source=this.context.createBufferSource();source.buffer=v.buffer;source.loop=!!(v.flags&4);source.connect(v.gainNode);
  v.started=this.context.currentTime;source.onended=()=>{if(v.source===source&&!source.loop){v.playing=false;v.offset=0}};
  source.start(0,Math.min(v.offset,Math.max(0,v.buffer.duration-.001)));
 }
 streamPosition(v){return v.offset+(v.playing&&!v.paused&&v.source?this.context.currentTime-v.started:0)}
 command(id,op,value=0,extra=0){
  if(op==='master'){this.master.gain.setValueAtTime(value,this.context.currentTime);return 1}
  const v=this.voices.get(id);if(!v)return 0;
  if(op==='free'){v.source?.stop();v.gainNode?.disconnect();if(!v.stream)this.send({id,op});this.voices.delete(id);return 1}
  if(op==='state')return v.paused?3:v.playing?1:0;
  if(op==='position')return v.stream?Math.floor(this.streamPosition(v)*(v.buffer?v.buffer.sampleRate*v.buffer.numberOfChannels*2:192000)):v.position;
  if(op==='tempoGet')return v.tempo;if(op==='speedGet')return v.speed;
  if(op==='flags'){const old=v.flags;v.flags=(old&~extra)|(value&extra);if(v.source)v.source.loop=!!(v.flags&4);if(!v.stream)this.send({id,op:'loop',value:!!(v.flags&4)});return v.flags}
  if(op==='seek'){
   v.revision++;v.position=value;
   if(v.stream){v.offset=value/(v.buffer?v.buffer.sampleRate*v.buffer.numberOfChannels*2:192000);if(v.playing&&!v.paused)this.startStream(v)}
   else this.send({id,op,revision:v.revision,order:value&65535,row:(value>>>16)&65535});return 1;
  }
  if(op==='gain'){v.gain=value;if(v.gainNode)v.gainNode.gain.setValueAtTime(value,this.context.currentTime)}
  if(op==='play'){if(value)v.offset=0;v.playing=true;v.paused=false;if(v.stream)this.startStream(v)}
  if(op==='stop'){v.playing=false;v.paused=false;v.source?.stop();v.source=null;v.offset=0}
  if(op==='pause'){if(v.stream){v.offset=this.streamPosition(v);v.source?.stop();v.source=null}v.paused=true}
  if(op==='resume'){v.paused=false;v.playing=true;if(v.stream)this.startStream(v)}
  if(!v.stream)this.send({id,op,value,channel:extra,restart:!!value});return 1;
 }
 close(){this.sound?.close();for(const id of this.voices.keys())this.command(id,'free')}
}
