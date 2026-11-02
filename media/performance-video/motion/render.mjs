// Render our own deterministic frontend in a private headless browser.
// No desktop/browser UI automation, screen recording, or game interaction.
import { chromium } from 'playwright-core';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { spawnSync } from 'node:child_process';
import { buildPage } from './build_page.mjs';
const here=path.dirname(fileURLToPath(import.meta.url));
const args=process.argv.slice(2);
const option=(name,fallback)=>{const i=args.indexOf(name);return i<0?fallback:args[i+1];};
const out=path.resolve(option('--output',path.join(here,'../../../out/media/performance-v3.2.4-motion')));
fs.mkdirSync(out,{recursive:true});
if(!args.includes('--preview')&&!args.includes('--export-mp4'))throw Error('Use --preview for webpage checks. MP4 export requires explicit --export-mp4.');
const html=buildPage();
const htmlPath=path.join(out,'pvz-perf-motion.html');fs.writeFileSync(htmlPath,html);
const browser=await chromium.launch({executablePath:option('--browser','C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe'),headless:true,args:['--disable-background-timer-throttling','--disable-renderer-backgrounding','--disable-backgrounding-occluded-windows','--no-first-run']});
try{
 const page=await browser.newPage({viewport:{width:1920,height:1152},deviceScaleFactor:1});
 const errors=[];page.on('pageerror',e=>errors.push(String(e)));
 await page.goto(pathToFileURL(htmlPath).href);await page.evaluate(()=>Promise.all([document.fonts.ready,ReanimPlayer.ready]));
 const timeline=await page.evaluate(()=>FILM.scenes.map(({start,duration,id,title,captions})=>({start,duration,id,title,captions})));
 fs.writeFileSync(path.join(out,'timeline.json'),JSON.stringify(timeline,null,2));
 const duration=timeline.at(-1).start+timeline.at(-1).duration;
 console.log(JSON.stringify({out,duration,scenes:timeline.length}));
 if(args.includes('--preview')){
  fs.mkdirSync(path.join(out,'preview'),{recursive:true});
  for(const s of timeline){for(const f of [.22,.62,.91]){const time=s.start+s.duration*f;await page.evaluate(t=>FILM.draw(t),time);await page.locator('canvas').screenshot({path:path.join(out,'preview',`${s.id}-${Math.round(f*100)}.png`)});}}
  const audit=await page.evaluate(()=>({textWarnings:FILM.textWarnings,finiteErrors:FILM.finiteErrors}));
  fs.writeFileSync(path.join(out,'preview-audit.json'),JSON.stringify({errors,...audit},null,2));
  console.log(JSON.stringify({preview:true,errors,...audit}));
 }else{
  const fps=Number(option('--fps',60));
  const from=Number(option('--from',0));const end=Math.min(duration,Number(option('--to',duration)));
  const dest=path.join(out,option('--name','pvz_perf-v3.2.5-motion'));
  const rawPath=dest+'.h264';const fd=fs.openSync(rawPath,'w');
  let bytes=0;
  await page.exposeFunction('sendEncoded',(base64)=>{const b=Buffer.from(base64,'base64');fs.writeSync(fd,b);bytes+=b.length;});
  await page.exposeFunction('reportProgress',msg=>console.log(JSON.stringify(msg)));
  const result=await page.evaluate(async({from,end,fps})=>{
   const canvas=document.querySelector('canvas');
   const config={codec:'avc1.64002a',width:1920,height:1080,bitrate:12000000,framerate:fps,latencyMode:'quality',hardwareAcceleration:'prefer-software',avc:{format:'annexb'}};
   const support=await VideoEncoder.isConfigSupported(config);if(!support.supported)throw Error('H.264 WebCodecs encoder unavailable');
   let writes=Promise.resolve(),encoderError=null,outputs=0;
   const encoder=new VideoEncoder({output(chunk){const b=new Uint8Array(chunk.byteLength);chunk.copyTo(b);let s='';for(let i=0;i<b.length;i+=16384)s+=String.fromCharCode(...b.subarray(i,i+16384));writes=writes.then(()=>window.sendEncoded(btoa(s)));outputs++;},error(e){encoderError=String(e);}});
   encoder.configure(config);
   const frames=Math.round((end-from)*fps);const began=performance.now();
   for(let i=0;i<frames;i++){
    FILM.draw(from+i/fps);const frame=new VideoFrame(canvas,{timestamp:Math.round(i*1e6/fps),duration:Math.round(1e6/fps)});
    encoder.encode(frame,{keyFrame:i%(fps*2)===0});frame.close();
    if(encoderError)throw Error(encoderError);
    if(i%60===59){await encoder.flush();await writes;}
    if(i%(fps*10)===0)await window.reportProgress({seconds:(i/fps).toFixed(1),total:frames/fps,elapsed:((performance.now()-began)/1000).toFixed(1)});
   }
   await encoder.flush();await writes;encoder.close();
   return {frames,outputs,fps,from,end,textWarnings:FILM.textWarnings,finiteErrors:FILM.finiteErrors};
  },{from,end,fps});
  fs.closeSync(fd);
  if(errors.length||result.finiteErrors.length)throw Error(JSON.stringify({errors,result}));
  const mux=spawnSync('ffmpeg',['-hide_banner','-loglevel','warning','-y','-r',String(fps),'-i',rawPath,'-c:v','copy','-video_track_timescale','30000','-movflags','+faststart',dest+'.mp4'],{encoding:'utf8'});
  if(mux.status!==0)throw Error(mux.stderr);fs.unlinkSync(rawPath);
  fs.writeFileSync(dest+'-render.json',JSON.stringify({...result,bytes,errors},null,2));
  console.log(JSON.stringify({completed:dest+'.mp4',...result,bytes}));
 }
}finally{await browser.close();}
