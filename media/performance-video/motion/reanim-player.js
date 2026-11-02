// A small original-reanim player for this film, independent of the game runtime.
// Sparse XML fields inherit the preceding value; image/f metadata come from the
// preceding key, while x/y/skew/scale/alpha interpolate. Track order is authored.
(()=>{
'use strict';
const defaults={x:0,y:0,kx:0,ky:0,sx:1,sy:1,f:0,a:1,i:''};
const numeric=['x','y','kx','ky','sx','sy','f','a'];
const interpolated=['x','y','kx','ky','sx','sy','a'];
const images={},definitions={};
const mod=(a,b)=>((a%b)+b)%b;
const hiddenZombie=['anim_cone','anim_bucket','anim_screendoor','zombie_flaghand',
 'zombie_duckytube','anim_tongue','zombie_mustache','zombie_outerarm_screendoor',
 'zombie_innerarm_screendoor'];
function parse(name,xml){
 const doc=new DOMParser().parseFromString('<reanim>'+xml+'</reanim>','application/xml');
 if(doc.querySelector('parsererror'))throw Error(`Invalid original reanim ${name}`);
 const tracks=Array.from(doc.documentElement.children).filter(e=>e.tagName==='track').map(e=>{
  let last={...defaults};
  const frames=Array.from(e.children).filter(c=>c.tagName==='t').map(t=>{
   const next={...last};
   for(const field of t.children){const key=field.tagName,value=field.textContent;
    if(numeric.includes(key)&&value!==''){const n=Number(value);if(!Number.isFinite(n))throw Error(`Invalid ${name}.${key}`);if(n!==-10000)next[key]=n;}
    else if(key==='i'&&value)next.i=value;
    else if((key==='font'||key==='text')&&value)next[key]=value;
   }
   last=next;return next;
  });
  return {name:e.querySelector('name')?.textContent||'',frames};
 });
 if(!tracks.length||tracks.some(t=>t.frames.length!==tracks[0].frames.length))throw Error(`Unequal original track lengths: ${name}`);
 const def={name,fps:Number(doc.querySelector('fps')?.textContent||12),tracks,
  findTrack(trackName){const track=tracks.find(t=>t.name.toLowerCase()===trackName.toLowerCase());if(!track)throw Error(`${name}: missing track ${trackName}`);return track;},
  clip(trackName){const track=this.findTrack(trackName);const visible=track.frames.map((f,i)=>f.f>=0?i:-1).filter(i=>i>=0);if(!visible.length)throw Error(`Empty clip ${trackName}`);return {name:trackName,start:visible[0],count:visible.at(-1)-visible[0]+1};},
  ground:tracks.find(t=>t.name==='_ground'),bounds:new Map()};
 return def;
}
function frameAt(def,clip,seconds,rate=def.fps){
 // Original REANIM_LOOP: mAnimTime advances by rate/frameCount; rendering spans
 // count-1 interpolation intervals. The final authored frame closes the cycle.
 const phase=mod(seconds*rate/clip.count,1),position=clip.start+phase*Math.max(0,clip.count-1);
 const before=Math.floor(position);return {before,after:Math.min(before+1,clip.start+clip.count-1),fraction:position-before,phase};
}
function sample(track,state){const a=track.frames[state.before],b=track.frames[state.after],r={f:a.f,i:a.i};for(const k of interpolated)r[k]=a[k]+(b[k]-a[k])*state.fraction;return r;}
function matrix(v){const x=v.kx*Math.PI/180,y=v.ky*Math.PI/180;return [Math.cos(x)*v.sx,Math.sin(x)*v.sx,-Math.sin(y)*v.sy,Math.cos(y)*v.sy,v.x,v.y];}
function visible(def,t,hidden=[]){return ![...(def.name==='Zombie'?hiddenZombie:[]),...hidden].some(prefix=>t.name.toLowerCase().startsWith(prefix.toLowerCase()));}
function bounds(def,clip){
 if(def.bounds.has(clip.name))return def.bounds.get(clip.name);
 let x0=Infinity,y0=Infinity,x1=-Infinity,y1=-Infinity;
 for(let f=clip.start;f<clip.start+clip.count;f++){
  const state={before:f,after:f,fraction:0};
  for(const track of def.tracks){if(!visible(def,track))continue;const v=sample(track,state),image=images[v.i];if(v.f<0||v.a<=0||!image)continue;
   const [a,b,c,d,x,y]=matrix(v);for(const [u,w] of [[0,0],[image.width,0],[0,image.height],[image.width,image.height]]){const px=a*u+c*w+x,py=b*u+d*w+y;x0=Math.min(x0,px);x1=Math.max(x1,px);y0=Math.min(y0,py);y1=Math.max(y1,py);}
  }
 }
 const result={x0,y0,x1,y1,width:x1-x0,height:y1-y0};if(!Object.values(result).every(Number.isFinite))throw Error(`No visible image in ${def.name}.${clip.name}`);def.bounds.set(clip.name,result);return result;
}
function draw(ctx,name,clipName,seconds,{x=0,y=0,height=150,rate,mirror=false,alpha=1,hidden=[]}={}){
 const def=definitions[name];if(!api.loaded)return null;if(!def)throw Error(`Unknown reanim ${name}`);
 const clip=def.clip(clipName),state=frameAt(def,clip,seconds,rate),box=bounds(def,clip),scale=height/box.height;
 let rendered=0;
 ctx.save();ctx.globalAlpha*=alpha;ctx.translate(x,y);ctx.scale(mirror?-scale:scale,scale);ctx.translate(-(box.x0+box.x1)/2,-box.y1);
 for(const track of def.tracks){if(!visible(def,track,hidden))continue;const v=sample(track,state),image=images[v.i];if(v.f<0||v.a<=0||!image)continue;
  if(v.f!==0)throw Error(`Unsupported multi-cel bitmap in selected clip: ${name}/${track.name}`);
  // _ground is a non-rendered motion reference, not a parent transform.
  // Body tracks already loop in place; the caller owns world displacement.
  const m=matrix(v);ctx.save();ctx.globalAlpha*=Math.max(0,Math.min(1,v.a));ctx.transform(...m);ctx.drawImage(image,0,0);
  if(api.debugBounds){ctx.lineWidth=1/scale;ctx.strokeStyle='#ffcf73';ctx.strokeRect(0,0,image.width,image.height);}ctx.restore();rendered++;
 }
 ctx.restore();return {...state,rendered,clip,bounds:box};
}
const api={loaded:false,debugBounds:false,definitions,images,parse,frameAt,sample,matrix,draw,bounds,visible};
api.ready=(async()=>{
 for(const [name,xml] of Object.entries(WEB_INPUTS.reanim.xml))definitions[name]=parse(name,xml);
 await Promise.all(Object.entries(WEB_INPUTS.reanim.images).map(async([id,src])=>{const image=new Image();image.src=src;await image.decode();images[id]=image;}));
 for(const [name,clip] of [['PeaShooter','anim_full_idle'],['SunFlower','anim_idle'],['Zombie','anim_walk']])bounds(definitions[name],definitions[name].clip(clip));
 api.loaded=true;return api;
})();
window.ReanimPlayer=api;
})();
