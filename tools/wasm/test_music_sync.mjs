import assert from 'node:assert/strict';
import {prepareMusic} from './game/audio/music-engine.js';
const original={fetch:globalThis.fetch,setTimeout:globalThis.setTimeout,AudioContext:globalThis.AudioContext,AudioWorkletNode:globalThis.AudioWorkletNode};
let behavior='ack',posts=0;
globalThis.fetch=async()=>({ok:true,arrayBuffer:async()=>new Uint8Array([0,97,115,109,1,0,0,0]).buffer});
globalThis.setTimeout=(fn,delay,...args)=>original.setTimeout(fn,delay===5000?15:delay,...args);
class Context{
 constructor(){this.destination={};this.state='running';this.currentTime=0;this.audioWorklet={addModule:async()=>{}}}
 resume(){this.state='running';return Promise.resolve()}
 close(){this.state='closed';return Promise.resolve()}
 createGain(){return {gain:{setValueAtTime(){}},connect(){},disconnect(){}}}
}
class Worklet{
 constructor(){this.port={close(){},postMessage:c=>{posts++;if(c.op!=='flush')return;queueMicrotask(()=>{
  if(behavior==='ack')this.port.onmessage?.({data:{flush:c.token,voices:[]}});
  else if(behavior==='error')this.port.onmessage?.({data:{error:'decoder failed on voice 7',op:'load',id:7}});
  else if(behavior==='processor-error')this.onprocessorerror?.({});
 })}}}
 connect(){queueMicrotask(()=>this.port.onmessage?.({data:{ready:true}}))}
 disconnect(){}
}
globalThis.AudioContext=Context;globalThis.AudioWorkletNode=Worklet;
try{
 let engine=await prepareMusic();await engine.unlock();assert.notEqual(await engine.flush(),false);assert.equal(!!engine.disabled,false);
 for(const mode of ['silent','error','processor-error']){
  behavior=mode;engine=await prepareMusic();await engine.unlock();const warnings=[];engine.onWarning=message=>warnings.push(message);
  await assert.doesNotReject(async()=>assert.equal(await engine.flush(),false),'Audio failure must not reject the game call');
  assert.equal(engine.disabled,true);assert.equal(engine.flushes.size,0);assert.equal(engine.node,null);assert.equal(warnings.length,1);
  if(mode==='error')assert.match(engine.error,/decoder failed on voice 7/,'The original error must not be replaced with a timeout');
  const before=posts;assert.equal(await engine.flush(),false);assert.equal(posts,before,'A failed node must not incur another five-second wait');
  const stream=engine.loadStream(new Float32Array(8),2,22050);engine.command(stream,'play');engine.context.currentTime=2;assert.equal(engine.command(stream,'position'),176400,'Muted credits must still have a timeline');engine.command(stream,'pause');engine.context.currentTime=3;assert.equal(engine.command(stream,'position'),176400);engine.command(stream,'resume');engine.context.currentTime=4;assert.equal(engine.command(stream,'position'),264600);
  engine.close();assert.equal(engine.context,null);behavior='ack';await engine.unlock();assert.equal(await engine.flush(),true,'A new session may retry audio');
 }
 behavior='ack';engine=await prepareMusic();await engine.unlock();engine.context.state='suspended';const before=posts;assert.equal(await engine.flush(),false);assert.equal(posts,before);assert.equal(engine.disabled,false);await engine.context.resume();assert.equal(await engine.flush(),true);
 behavior='silent';const interrupted=engine.flush();engine.context.state='suspended';assert.equal(await interrupted,false);assert.equal(engine.disabled,false,'Host suspension during a barrier must not permanently disable music');await engine.context.resume();
 behavior='silent';const cancelled=engine.flush();engine.close();assert.equal(await cancelled,false);assert.equal(engine.flushes.size,0);
 behavior='ack';engine=await prepareMusic();await engine.unlock();engine.mixer={positions:()=>[],close(){}};assert.equal(await engine.flush(),true,'ScriptProcessor fallback must use its synchronous mixer');
 const liveContext=engine.context,originalResume=liveContext.resume;let resumes=0;
 liveContext.resume=function(){resumes++;this.state='running';return Promise.resolve()};
 engine.resumeIfInterrupted();assert.equal(resumes,0,'Normal input must not issue resume commands');
 for(const state of ['interrupted','suspended']){liveContext.state=state;engine.resumeIfInterrupted();assert.equal(liveContext.state,'running','Recovery must execute synchronously inside the input event')}
 assert.equal(resumes,2);assert.equal(engine.context,liveContext,'Recovery must retain the audio graph and positions');
 liveContext.state='interrupted';liveContext.resume=()=>Promise.reject(Error('Host still owns audio'));engine.resumeIfInterrupted();await new Promise(resolve=>original.setTimeout(resolve,0));
 liveContext.resume=()=>{throw Error('Host refused resume')};assert.doesNotThrow(()=>engine.resumeIfInterrupted());
 liveContext.state='closed';liveContext.resume=()=>{throw Error('Closed context must not resume')};engine.resumeIfInterrupted();
 liveContext.resume=originalResume;liveContext.state='running';engine.context=null;engine.resumeIfInterrupted();engine.context=liveContext;
 console.log('Audio interruption: gesture-synchronous resume, no running/closed-context requests, rejected retry and graph preservation passed');
 console.log('Music sync: ack, timeout, command/processor errors, pending cancellation, before/during-barrier suspension, restart and synchronous fallback passed');
}finally{Object.assign(globalThis,original)}
