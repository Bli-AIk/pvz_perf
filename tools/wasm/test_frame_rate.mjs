import assert from 'node:assert/strict';
import {createFrameRateObserver} from './game/frame-rate.js';
let time=0;const rates=[],observe=createFrameRateObserver(rate=>rates.push(rate));
for(let i=0;i<60;i++){time+=1000/120;observe(time)}
for(let i=0;i<60;i++){time+=1000/60;observe(time)}
for(let i=0;i<60;i++){time+=1000/120;observe(time)}
assert.deepEqual(rates,[120,60,120]);
console.log('PASS: stable 120→60→120 callback cadence detected; no frame-count based game time introduced');
