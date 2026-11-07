import assert from 'node:assert/strict';
import {createStartupMonitor} from './game/startup.js';
let time=0,hidden=false,tick,scheduled=false,ready=0,error='',state={loaded:false,draws:500,updates:500};
const monitor=createStartupMonitor({readState:()=>state,now:()=>time,hidden:()=>hidden,
 schedule:callback=>{tick=callback;scheduled=true;return 1},cancel:()=>{scheduled=false},timeoutMs:1000,
 onProgress(){},onReady:()=>ready++,onError:message=>{error=message}});
monitor.start();time=400;tick();assert.equal(ready,0,'early loading-screen draws are not startup completion');
hidden=true;time=2000;tick();assert.equal(error,'','background time must not expire startup');
hidden=false;time=2100;state.loaded=true;tick();assert.equal(ready,0,'wait for a frame after the resource barrier');
time=2200;state.draws++;tick();assert.equal(ready,1);assert.equal(scheduled,false);
state.loaded=false;monitor.start();monitor.setPhase('上传图集');time+=1001;tick();
assert.match(error,/上传图集.*资源完成=false/);assert.equal(scheduled,false);
monitor.start();monitor.stop();assert.equal(scheduled,false);
console.log('Startup resource barrier, later frame, timeout and background handling: passed');
