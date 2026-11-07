import assert from 'node:assert/strict';
import {SoundEngine,createPCMBuffer} from './game/audio/sound-engine.js';
const context={destination:{},createGain:()=>({connect(){},disconnect(){}}),
 createBuffer(channels,frames,rate){const data=Array.from({length:channels},()=>new Float32Array(frames));return {numberOfChannels:channels,length:frames,sampleRate:rate,getChannelData:c=>data[c]}}};
const input=new Float32Array([.5,-.5,.25,-.25]),buffer=createPCMBuffer(context,input,2,22050);
assert.deepEqual([...buffer.getChannelData(0)],[.5,.25]);
assert.deepEqual([...buffer.getChannelData(1)],[-.5,-.25]);
input.fill(0);assert.equal(buffer.getChannelData(0)[0],.5,'PCM must outlive the Wasm loading allocation');
assert.throws(()=>createPCMBuffer(context,new Float32Array(3),2,22050));
const engine=new SoundEngine(context);engine.loadPCM(7,new Float32Array([.1,.2]),1,11025);await engine.finish();assert.equal(engine.sources.get(7).buffer.length,2);
const au=new Uint8Array(26),v=new DataView(au.buffer);
[0x2e736e64,24,2,1,8000,1].forEach((n,i)=>v.setUint32(i*4,n));au[24]=255;au[25]=127;
engine.load(8,au);await engine.finish();assert.equal(engine.sources.get(8).buffer.sampleRate,8000);
assert.equal(typeof context.decodeAudioData,'undefined');
await assert.rejects(()=>engine.decode(new Uint8Array(24)),/Unsupported sound encoding/);
console.log('PCM transfer, AU decode and browser-codec independence: passed');
