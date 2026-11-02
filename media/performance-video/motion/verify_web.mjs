import {chromium} from 'playwright-core';
import {pathToFileURL} from 'node:url';
import fs from 'node:fs';
import path from 'node:path';
import {createHash} from 'node:crypto';
import {buildPage,defaultOutput} from './build_page.mjs';
const file=path.join(defaultOutput,'pvz-perf-motion.html');fs.writeFileSync(file,buildPage());
const browser=await chromium.launch({headless:true,executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe'});
const errors=[];
try{
 const page=await browser.newPage({viewport:{width:1280,height:800}});page.on('pageerror',e=>errors.push(String(e)));
 await page.goto(pathToFileURL(file).href);await page.evaluate(()=>Promise.all([ReanimPlayer.ready,document.fonts.ready]));
 const state=await page.evaluate(()=>{
  const p=ReanimPlayer;
  const expected=[['PeaShooter',24,104,'anim_full_idle',79,25],['SunFlower',29,29,'anim_idle',4,25],['Zombie',42,504,'anim_walk',44,47]];
  for(const [name,tracks,frames,clip,start,count] of expected){const d=p.definitions[name],c=d.clip(clip);if(d.tracks.length!==tracks||d.tracks[0].frames.length!==frames||c.start!==start||c.count!==count)throw Error('Original clip range mismatch: '+name);
   // Exercise a whole loop, including interpolation and both sides of its wrap.
   const ctx=document.getElementById('film').getContext('2d');for(let i=0;i<150;i++)p.draw(ctx,name,clip,i/37,{x:300,y:700,height:300});
   const first=p.frameAt(d,c,0,18),wrapped=p.frameAt(d,c,count/18,18);if(first.before!==wrapped.before||Math.abs(wrapped.fraction)>1e-9)throw Error('Loop boundary mismatch');
  }
  // Check the axis/sign convention independently at a right angle.
  const m=p.matrix({x:7,y:11,kx:90,ky:90,sx:2,sy:3});const want=[0,2,-3,0,7,11];m.forEach((v,i)=>{if(Math.abs(v-want[i])>1e-10)throw Error('Skew axis convention mismatch');});
  // The authored body already loops in place: _ground must not be subtracted
  // from its drawing transform. Only the caller applies world displacement.
  const zd=p.definitions.Zombie,zc=zd.clip('anim_walk');
  for(const time of [0,1,zc.count/18-1e-7,zc.count/18+1e-7]){
   let bodyMatrix=null;
   const recorder={globalAlpha:1,save(){},restore(){},translate(){},scale(){},
    transform(...m){this.matrix=m;},drawImage(image){if(image===p.images.IMAGE_REANIM_ZOMBIE_BODY)bodyMatrix=this.matrix;}};
   p.draw(recorder,'Zombie','anim_walk',time,{rate:18});
   const authored=p.sample(zd.findTrack('Zombie_body'),p.frameAt(zd,zc,time,18));
   if(!bodyMatrix||Math.abs(bodyMatrix[4]-authored.x)>1e-9)throw Error('Root displacement applied twice');
  }
  let previous=Infinity;
  for(let i=0;i<=2600;i++){const value=FILM.phaseState(i/100);if(value.base>0||value.offset>0||value.screenX>previous+1e-9)throw Error('Root example moves backwards');previous=value.screenX;}
  for(const boundary of [6.25,12.5,18.75,25]){const a=FILM.phaseState(boundary-1e-6),b=FILM.phaseState(boundary+1e-6);if(b.screenX>a.screenX||Math.abs(b.screenX-a.screenX)>.001)throw Error('Root example jumps at cycle boundary');}
  const comparison=FILM.rootComparisonState();
  if(comparison.reference!==117.130432||comparison.rows.map(r=>r.hz).join(',')!=='100,120,240')throw Error('Root-motion archive selection changed');
  const archived=[[118.171516,1.041084],[120.027794,2.897362],[117.8918,.761368]];
  comparison.rows.forEach((row,i)=>{if(row.legacyX!==archived[i][0]||Math.abs(row.error-archived[i][1])>1e-6||row.phaseX!==comparison.reference)throw Error('Root-motion endpoints differ from native evidence');});
  const integrated=hz=>FILM.legacySteps(hz).reduce((sum,p)=>sum+(p.b-p.a)*p.velocity,0);
  if(Math.abs(integrated(100)-integrated(30))<.01)throw Error('Velocity-sampling diagram does not demonstrate different step partitions');
  // Keep algorithm ownership visible even when seeking directly into chapter 5.
  const legacy=FILM.scenes.find(s=>s.id==='legacy-motion'),phase=FILM.scenes.find(s=>s.id==='phase');
  if(!legacy.title.includes('原版')||!legacy.captions[0][1].includes('原版')||!phase.title.includes('本项目')||!phase.captions[0][1].includes('本项目'))throw Error('Root-motion algorithm ownership is ambiguous');
  // Sampling must not mutate sparse inherited source state.
  for(const d of Object.values(p.definitions))for(const tr of d.tracks)for(const v of tr.frames)for(const k of ['x','y','kx','ky','sx','sy','f','a'])if(!Number.isFinite(v[k])||v[k]===-10000)throw Error('Unresolved sparse value');
  for(const s of FILM.scenes)for(const f of [0,.001,.23,.78,.999])FILM.draw(s.start+s.duration*f);
  const repo=document.getElementById('repo-link');
  for(const id of ['opening','closing']){
   const scene=FILM.scenes.find(s=>s.id===id);FILM.draw(scene.start+2);
   if(repo.hidden||repo.href!==FILM.repositoryUrl||repo.target!=='_blank'||!repo.relList.contains('noopener'))throw Error('Missing repository link in '+id);
   const a=repo.getBoundingClientRect(),c=document.getElementById('film').getBoundingClientRect();
   if(Math.abs((a.x-c.x)/c.width-410/1920)>.00001||Math.abs((a.y-c.y)/c.height-827/1080)>.00001)throw Error('Repository click target does not match Canvas label');
  }
  FILM.draw(FILM.scenes.find(s=>s.id==='lookup').start+2);
  if(!repo.hidden)throw Error('Repository click target obscures an explanation scene');
  const traces=WEB_INPUTS.traces;
  for(const d of Object.values(traces.desktop))if(d.cpuFrequency!==null)throw Error('Invented desktop CPU frequency');
  for(const d of Object.values(traces.phone))if(d.fps!==null||d.cpuPercent!==null)throw Error('Mixed phone captures');
  return {definitions:expected.map(a=>({name:a[0],tracks:a[1],frames:a[2],clip:a[3],start:a[4],count:a[5]})),pngCount:Object.keys(p.images).length,scenes:FILM.scenes.length,duration:FILM.duration,textWarnings:FILM.textWarnings,finiteErrors:FILM.finiteErrors};
 });
 const actorTime=await page.evaluate(()=>FILM.scenes.find(s=>s.id==='rows').start+14.3);
 await page.evaluate(t=>FILM.draw(t),actorTime);const before=await page.locator('canvas').screenshot();
 await page.evaluate(t=>FILM.draw(t),actorTime+.25);const different=await page.locator('canvas').screenshot();
 await page.evaluate(()=>FILM.draw(300));await page.evaluate(t=>FILM.draw(t),actorTime);const repeated=await page.locator('canvas').screenshot();
 const hash=b=>createHash('sha256').update(b).digest('hex');
 if(hash(before)!==hash(repeated)||hash(before)===hash(different))throw Error('Non-deterministic seek or static reanim');
 await page.selectOption('#chapters',String(await page.evaluate(()=>FILM.scenes.find(s=>s.id==='phone-frequency').start)));
 await page.locator('#track-bounds').check();
 if(!await page.evaluate(()=>ReanimPlayer.debugBounds))throw Error('Track boundary control failed');
 await page.locator('#track-bounds').uncheck();
 await page.locator('#play').click();await page.waitForTimeout(250);await page.locator('#play').click();
 const controlState=await page.evaluate(()=>({playing:document.getElementById('play').textContent,seek:Number(document.getElementById('seek').value),chapter:document.getElementById('chapters').value}));
 if(!(controlState.seek>Number(controlState.chapter)+.7)||controlState.playing!=='播放')throw Error('Playback controls failed');
 if(errors.length||state.textWarnings.length||state.finiteErrors.length)throw Error(JSON.stringify({errors,state}));
 const result={...state,errors,deterministicSeek:true,animatedPixels:true,controls:true,rootMotionContinuousLeft:true,noDoubleGroundTransform:true,rootMotionArchivedEndpoints:true,velocityIntegrationDiagram:true,rootMotionOwnershipExplicit:true,openingAndClosingRepositoryLinks:true,originalSourceHashesIn:'web-input-provenance.json',mp4Exported:false};
 fs.writeFileSync(path.join(defaultOutput,'web-verification.json'),JSON.stringify(result,null,2));console.log(JSON.stringify(result));
}finally{await browser.close();}
