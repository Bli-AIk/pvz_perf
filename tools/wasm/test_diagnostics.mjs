import assert from 'node:assert/strict';
import {createDiagnostics} from './game/diagnostics.js';
const data=new Map(),storage={getItem:key=>data.get(key)||null,setItem:(key,value)=>data.set(key,value)};
let reads=0,scheduled=0,cancelled=0,tick;
const make=()=>createDiagnostics({storage,environment:()=>({version:'test'}),readState:()=>{reads++;return {wasmBytes:128*1024*1024}},
 schedule:fn=>{scheduled++;tick=fn;return scheduled},cancel:()=>cancelled++});
const first=make();first.start();first.record('phase');first.quit();
assert.equal(reads,0);assert.equal(scheduled,0);assert.equal(data.size,0,'Disabled diagnostics must not sample or write storage');
first.setEnabled(true);first.start();for(let i=0;i<200;i++)tick();
assert.equal(first.report().current.events.length,90);assert.equal(first.report().current.normalQuit,false);
const second=make();second.record('page-ready');assert.equal(second.report().previous.events.length,90,'Unexpected process exit must leave the previous snapshot');
assert.equal(second.report().previous.normalQuit,false);second.start();second.quit();assert.equal(second.report().current.normalQuit,true);
second.setEnabled(false);const before=reads;second.record('sample');assert.equal(reads,before);assert.ok(cancelled>0);
const third=make();assert.equal(third.report().previous.normalQuit,true);assert.equal(third.report().current,null);
const broken=createDiagnostics({storage:{getItem:()=>null,setItem:()=>{throw Error('quota')}},environment:()=>({}),readState:()=>({})});
assert.doesNotThrow(()=>{broken.setEnabled(true);broken.record('error','test')});assert.match(broken.report().storageError,/quota/);
console.log('Diagnostics: zero disabled sampling/writes, bounded history, previous-session retention, normal quit and storage failure passed');
