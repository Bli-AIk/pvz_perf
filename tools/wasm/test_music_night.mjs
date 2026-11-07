// Run against the shipped decoder and source mixer, not a mock audio backend.
// node tools/wasm/test_music_night.mjs out/wasm-webgpu/toy
import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
import {createRequire} from 'node:module';
import {pathToFileURL} from 'node:url';
globalThis.require=createRequire(import.meta.url);
globalThis.__dirname=process.cwd();
const distribution=path.resolve(process.argv[2]||'out/wasm-webgpu/toy');
const runtime=pathToFileURL(path.join(distribution,'audio/openmpt-runtime.js')).href;
const source=fs.readFileSync('tools/wasm/game/audio/music-mixer.js','utf8')
 .replace("'./openmpt-runtime.js'",JSON.stringify(runtime));
const {MusicMixer}=await import('data:text/javascript;base64,'+Buffer.from(source).toString('base64'));
const compiled=await WebAssembly.compile(fs.readFileSync(path.join(distribution,'audio/libopenmpt.wasm')));
const mixer=await MusicMixer.create(compiled,48000);
const command=(op,rest={})=>mixer.command({id:1,op,...rest});
command('load',{data:fs.readFileSync('assets/runtime/sounds/mainmusic.mo3')});
command('loop',{value:true});command('gain',{value:1});
const left=new Float32Array(128),right=new Float32Array(128);
function render(seconds){let peak=0;for(let i=0;i<seconds*48000;i+=left.length){mixer.render(left,right);for(const x of left)peak=Math.max(peak,Math.abs(x))}return peak}
try{
 command('seek',{order:92,row:0,revision:1});command('play');
 assert.equal(render(20),0,'night holding loop must stay silent');
 for(const order of [76,77]){
  // The controller seeks the running standby voice, then resynchronizes it.
  // No Play command follows: the old mixer lost these attributes on read.
  command('seek',{order,row:0,revision:order});
  command('tempo',{value:123});command('speed',{value:4});
  mixer.render(left,right);
  assert.equal(mixer.positions()[0].tempo,123);
  assert.equal(mixer.positions()[0].speed,4);
  assert.equal(mixer.positions()[0].position&65535,order);
  assert.ok(render(.5)>.01,'climax must become audible after standby');
  command('pause');const position=mixer.positions()[0].position;
  assert.equal(render(.25),0);assert.equal(mixer.positions()[0].position,position);
  command('resume');assert.ok(render(.25)>.001);
  command('gain',{value:0});assert.equal(render(.25),0,'completed fade must be silent');
  command('gain',{value:1});
 }
 command('stop');command('seek',{order:48,row:0,revision:100});command('play');
 command('tempo',{value:125});command('speed',{value:4});mixer.render(left,right);
 assert.equal(mixer.positions()[0].tempo,125);assert.equal(mixer.positions()[0].speed,4);
 assert.ok(render(.5)>.01,'normal night theme must resume');
 console.log('PASS shipped MO3: night standby, both live climax seeks, BPM/speed retention, pause, fade silence and restart');
}finally{mixer.close()}
