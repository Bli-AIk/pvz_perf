import {SoundEngine,createPCMBuffer} from './sound-engine.js';
export async function prepareMusic(){
 const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),15000);
 try{
  const response=await fetch('./audio/libopenmpt.wasm',{signal:controller.signal});if(!response.ok)throw Error('Cannot load MO3 decoder: '+response.status);
  return new MusicEngine(await WebAssembly.compile(await response.arrayBuffer()));
 }catch(error){return new MusicEngine(null,'MO3 解码器准备失败：'+(error.message||error))}
 finally{clearTimeout(timer)}
}
class MusicEngine {
 constructor(compiled,loadError=''){this.compiled=compiled;this.loadError=loadError;this.voices=new Map();this.next=1;this.error='';this.disabled=false;this.backend='pending';this.flushes=new Map();this.nextFlush=1;this.send=()=>{}}
 disableMusic(reason){
  if(this.disabled)return;
  this.disabled=true;this.error=String(reason?.message||reason);this.backend='BGM disabled';this.send=()=>{};
  const node=this.node,mixer=this.mixer;this.node=null;this.mixer=null;
  try{if(node){node.onprocessorerror=null;if(node.port){node.port.onmessage=null;node.port.close()}node.disconnect()}}catch{}
  try{mixer?.close()}catch{}
  for(const v of this.voices.values())if(v.stream){v.offset=this.streamPosition(v);v.started=this.context.currentTime;try{v.source?.stop();v.gainNode?.disconnect()}catch{}v.source=null;v.buffer=null;v.gainNode=null}
  for(const done of this.flushes.values())done(false);this.flushes.clear();
  console.warn(this.error);this.onWarning?.(this.error);
 }
 resumeIfInterrupted(){
  const context=this.context;
  if(!context||context.state==='running'||context.state==='closed')return;
  // iOS can interrupt audio independently of the game's focus state. Invoke
  // resume inside the input event, before any await loses user activation.
  // An interruption may still be owned by the host; allow the next gesture
  // to retry rather than holding a pending promise or rebuilding the graph.
  try{context.resume().catch(()=>{})}catch{}
 }
 async unlock(){
  if(this.context){await this.context.resume();return}
  const context=this.context=new AudioContext({latencyHint:'interactive'});
  // Resume in the user gesture. Await readiness only after the worklet is in
  // the active graph: some hosts defer audio-thread work for disconnected nodes.
  const resumed=context.resume();resumed.catch(()=>{});
  this.sound=new SoundEngine(context);this.master=context.createGain();this.master.connect(context.destination);
  const receive=data=>{if(this.disabled)return;if(data.error){this.disableMusic((data.op?'['+data.op+(data.id?' voice '+data.id:'')+'] ':'')+data.error);return}
   if(data.voices)for(const row of data.voices){const v=this.voices.get(row.id);if(v&&v.revision===row.revision)Object.assign(v,row)}
   if(data.flush){const done=this.flushes.get(data.flush);if(done){this.flushes.delete(data.flush);done(true)}}};
  this.receive=receive;
  try{
   if(!this.compiled)throw Error(this.loadError);
   if(context.audioWorklet){
    let timer;
    await Promise.race([context.audioWorklet.addModule('./audio/music-worklet.js'),new Promise((_,reject)=>{timer=setTimeout(()=>reject(Error('加载音频工作线程超时')),15000)})]).finally(()=>clearTimeout(timer));
    this.node=new AudioWorkletNode(context,'pvz-music',{numberOfInputs:0,numberOfOutputs:1,outputChannelCount:[2],processorOptions:{compiled:this.compiled,entropy:crypto.getRandomValues(new Uint8Array(65536))}});
    await new Promise((resolve,reject)=>{
     const timer=setTimeout(()=>reject(Error('音频工作线程初始化超时')),15000);
     const finish=(error)=>{clearTimeout(timer);if(error)reject(error);else resolve()};
     this.node.port.onmessage=event=>{if(event.data.ready)finish();else if(event.data.error)finish(Error(event.data.error));else receive(event.data)};
     this.node.onprocessorerror=()=>finish(Error('Music audio thread failed'));
     this.node.connect(this.master);
    });
    this.node.port.onmessage=event=>receive(event.data);
    this.node.onprocessorerror=()=>receive({error:'Music audio thread failed'});
    this.send=c=>this.node.port.postMessage(c,c.data?[c.data.buffer]:[]);this.backend='AudioWorklet';
   }else{
   // Plain HTTP on a LAN has no AudioWorklet. Keep that development case usable.
   const {MusicMixer}=await import('./music-mixer.js');
   const mixer=await MusicMixer.create(this.compiled,context.sampleRate);this.mixer=mixer;
   this.node=context.createScriptProcessor(2048,0,2);this.node.onaudioprocess=event=>{try{mixer.render(event.outputBuffer.getChannelData(0),event.outputBuffer.getChannelData(1));receive({voices:mixer.positions()})}catch(error){receive({error:String(error)})}};
   this.send=c=>mixer.command(c);this.backend='ScriptProcessor fallback';
    this.node.connect(this.master);
   }
   await resumed;
  }catch(error){
   // Keep WebAudio sound effects alive. An unavailable music worker must not
   // reject the Wasm startup or leave it waiting on a dead message port.
   this.disableMusic(error);
  }
 }
 flush(){
  if(this.disabled||this.context?.state!=='running')return Promise.resolve(false);
  if(this.mixer){try{this.receive({voices:this.mixer.positions()});return Promise.resolve(true)}catch(error){this.disableMusic(error);return Promise.resolve(false)}}
  const token=this.nextFlush++,audioTime=this.context.currentTime;
  return new Promise(resolve=>{
   const timer=setTimeout(()=>{
    // A host may suspend/interupt audio while the barrier is in flight. That
    // is an ordinary pause, not evidence that the processor has crashed.
    if(this.context?.state!=='running'){const done=this.flushes.get(token);this.flushes.delete(token);done?.(false);return}
    this.disableMusic('Music state synchronization timed out (context='+this.context.state+', audio advanced='+Math.round((this.context.currentTime-audioTime)*1000)+'ms)');
   },5000);
   this.flushes.set(token,result=>{clearTimeout(timer);resolve(result)});
   try{this.send({op:'flush',token})}catch(error){this.disableMusic(error)}
  });
 }
 load(data,stream){
  if(stream)throw Error('OGG streams must be decoded by Wasm');
  const id=this.next++,v={id,stream,playing:false,paused:false,position:0,positionScale:1,revision:0,tempo:155,speed:3,gain:0,flags:0,offset:0};this.voices.set(id,v);
  this.send({op:'load',id,data});
  return id;
 }
 loadStream(samples,channels,rate){
  const id=this.next++,buffer=this.disabled?null:createPCMBuffer(this.context,samples,channels,rate),gainNode=this.disabled?null:this.context.createGain();if(gainNode){gainNode.gain.value=0;gainNode.connect(this.master)}
  this.voices.set(id,{id,stream:true,playing:false,paused:false,position:0,revision:0,tempo:155,speed:3,gain:0,flags:0,offset:0,byteRate:rate*channels*2,buffer,gainNode});return id;
 }
 startStream(v){
  v.source?.stop();v.started=this.context.currentTime;if(!v.buffer)return;
  const source=v.source=this.context.createBufferSource();source.buffer=v.buffer;source.loop=!!(v.flags&4);source.connect(v.gainNode);
  source.onended=()=>{if(v.source===source&&!source.loop){v.playing=false;v.offset=0}};
  source.start(0,Math.min(v.offset,Math.max(0,v.buffer.duration-.001)));
 }
 streamPosition(v){return v.offset+(v.playing&&!v.paused&&(v.source||!v.buffer)?this.context.currentTime-v.started:0)}
 command(id,op,value=0,extra=0){
  if(op==='master'){this.master.gain.setValueAtTime(value,this.context.currentTime);return 1}
  const v=this.voices.get(id);if(!v)return 0;
  if(op==='free'){v.source?.stop();v.gainNode?.disconnect();if(!v.stream)this.send({id,op});this.voices.delete(id);return 1}
  if(op==='state')return v.paused?3:v.playing?1:0;
  if(op==='position'){
   if(v.stream)return Math.floor(this.streamPosition(v)*v.byteRate);
   // BASS MUSIC_PSCALER multiplies returned row progress, not Seek's raw row.
   // The 1073 controller requests scale 4 (half-pattern=128, last row=252).
   // Decoder/cache positions stay raw, so pause -> row/4 -> Seek round-trips.
   if(v.position===0xffffffff||v.position===-1)return 0xffffffff;
   return ((v.position&65535)|((((v.position>>>16)*v.positionScale)&65535)<<16))>>>0;
  }
  if(op==='pscaler'){
   if(v.stream||!Number.isInteger(value)||value<1)return 0;
   v.positionScale=value;return 1;
  }
  if(op==='tempoGet')return v.tempo;if(op==='speedGet')return v.speed;
  if(op==='tempo')v.tempo=value;if(op==='speed')v.speed=value;
  if(op==='flags'){const old=v.flags;v.flags=(old&~extra)|(value&extra);if(v.source)v.source.loop=!!(v.flags&4);if(!v.stream)this.send({id,op:'loop',value:!!(v.flags&4)});return v.flags}
  if(op==='seek'){
   v.revision++;v.position=value;
   if(v.stream){v.offset=value/v.byteRate;if(v.playing&&!v.paused)this.startStream(v)}
   else this.send({id,op,revision:v.revision,order:value&65535,row:(value>>>16)&65535});return 1;
  }
  if(op==='gain'){v.gain=value;if(v.gainNode)v.gainNode.gain.setValueAtTime(value,this.context.currentTime)}
  if(op==='play'){if(value)v.offset=0;v.playing=true;v.paused=false;if(v.stream)this.startStream(v)}
  if(op==='stop'){v.playing=false;v.paused=false;v.source?.stop();v.source=null;v.offset=0}
  if(op==='pause'){if(v.stream){v.offset=this.streamPosition(v);v.source?.stop();v.source=null}v.paused=true}
  if(op==='resume'){v.paused=false;v.playing=true;if(v.stream)this.startStream(v)}
  if(!v.stream)this.send({id,op,value,channel:extra,restart:!!value});return 1;
 }
 close(){
  for(const done of this.flushes.values())done(false);this.flushes.clear();
  this.sound?.close();for(const id of this.voices.keys())this.command(id,'free');
  if(this.disabled){this.master?.disconnect();this.context?.close().catch(()=>{});this.context=null;this.sound=null;this.backend='pending';this.disabled=false}
  else if(this.context)this.sound=new SoundEngine(this.context);
  this.error='';this.onWarning?.('');
 }
}
