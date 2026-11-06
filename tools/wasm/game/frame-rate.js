export function createFrameRateObserver(publish){
 let previous=0,samples=[],candidate=0,repeats=0;
 return time=>{
  if(previous){const dt=time-previous;if(dt>=1&&dt<=80)samples.push(dt);else samples=[]}
  previous=time;if(samples.length<12)return;
  const sorted=samples.slice().sort((a,b)=>a-b);samples=[];
  const raw=1000/sorted[6],rates=[30,48,50,60,72,75,90,100,120,144,165,180,200,240,360,480];
  const nearest=rates.reduce((a,b)=>Math.abs(b-raw)<Math.abs(a-raw)?b:a);
  const rate=Math.max(30,Math.round(Math.abs(nearest-raw)/raw<.05?nearest:raw));
  // Require two windows; one missed presentation or a temporary task is not a
  // display-mode change. This observes callback cadence, not panel hardware Hz.
  repeats=rate===candidate?repeats+1:1;candidate=rate;if(repeats===2)publish(rate);
 };
}
