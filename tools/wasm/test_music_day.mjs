// Real Web/OpenMPT playback; position values remain raw decoder rows.
// The Web BASS adapter applies MUSIC_PSCALER; mixer caches and Seek stay raw.
import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
import {createRequire} from 'node:module';
import {pathToFileURL} from 'node:url';
globalThis.require=createRequire(import.meta.url);
globalThis.__dirname=process.cwd();
const distribution=path.resolve(process.argv[2]||'out/wasm-four/toy');
const runtime=pathToFileURL(path.join(distribution,'audio/openmpt-runtime.js')).href;
const source=fs.readFileSync('tools/wasm/game/audio/music-mixer.js','utf8')
 .replace("'./openmpt-runtime.js'",JSON.stringify(runtime));
const {MusicMixer}=await import('data:text/javascript;base64,'+Buffer.from(source).toString('base64'));
const compiled=await WebAssembly.compile(fs.readFileSync(path.join(distribution,'audio/libopenmpt.wasm')));
const mixer=await MusicMixer.create(compiled,48000);
const command=(op,rest={})=>mixer.command({id:1,op,...rest});
const left=new Float32Array(128),right=new Float32Array(128);
function render(seconds){let peak=0;for(let i=0;i<seconds*48000;i+=left.length){mixer.render(left,right);for(const x of left)peak=Math.max(peak,Math.abs(x))}return peak}
try{
 command('load',{data:fs.readFileSync('assets/runtime/sounds/mainmusic.mo3')});
 command('loop',{value:true});
 for(let channel=0;channel<30;channel++)command('channel',{channel,value:channel>=24&&channel<=26?1:0});
 command('seek',{order:0,row:31,revision:1});command('play');
 assert.equal(mixer.positions()[0].position>>>16,31);
 command('gain',{value:0});assert.equal(render(.01),0,'queued drums must be muted');
 command('seek',{order:0,row:32,revision:2});
 assert.equal(mixer.positions()[0].position>>>16,32,'Web bridge must expose raw row 32, not scaled twice');
 command('gain',{value:1});assert.ok(render(.5)>.01,'day drum channels 24..26 contain no audible output');
 command('pause');const paused=mixer.positions()[0].position;
 assert.equal(render(.25),0);assert.equal(mixer.positions()[0].position,paused);
 command('resume');assert.ok(render(.5)>.01);
 command('gain',{value:0});assert.equal(render(.5),0,'released drums must return to silence');
 command('seek',{order:1,row:0,revision:3});command('gain',{value:1});
 assert.ok(render(.5)>.01,'day drums must be audible on a subsequent crowd cycle');
 console.log('PASS real Web MO3 day drums: raw row 31/32, channel isolation, mute/unmute, pause/resume, release and retrigger');
}finally{mixer.close()}
