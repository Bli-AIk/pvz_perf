import {LineupLibrary,normalizeLineupLibrary} from './lineup-library.js';
import {defaultLineups,decodeLineup} from './lineups.js';
import {encode,decode,digest,unpack,cloudCall,deadline} from './progress-storage.js';
export {LineupLibrary};
const headKey='pvz_lineups_v1',chunkSize=1000,chunkCount=60;
const chunkKey=(slot,index)=>'pvz_lineups_'+slot+'_'+index;
// This is a complete JSON library, not per-row integer IDs. Publishing the
// index last also commits deletions (including a deliberately empty library).
export class LineupStorage {
 constructor(storage,{cloudEnabled,remote,report,uploadState=()=>{}}){
  Object.assign(this,{cloudEnabled,remote,report,uploadState});
  this.library=new LineupLibrary(storage,defaultLineups);this.head=null;this.cloudRead=false;this.lastCloud='';this.needsChoice=false;this.pending=null;this.saving=null;
 }
 async validate(value){const next=normalizeLineupLibrary(value);for(const entry of next.lineups)await decodeLineup(entry.code);return next}
 async readCloud(){
  const toy=await this.remote(),rows=await deadline(cloudCall(()=>toy.getCloudStorage([headKey]))),raw=rows[headKey];
  if(!raw){this.head=null;this.cloudRead=true;this.lastCloud='';return null}
  const head=JSON.parse(raw);
  if(head.format!==1||!['a','b'].includes(head.slot)||!Number.isInteger(head.count)||head.count<1||head.count>chunkCount||!/^[a-f0-9]{64}$/.test(head.digest))throw Error('云端阵型库索引无效');
  const keys=Array.from({length:head.count},(_,i)=>chunkKey(head.slot,i)),data=await deadline(cloudCall(()=>toy.getCloudStorage(keys)));
  if(keys.some(k=>typeof data[k]!=='string'||data[k].length>chunkSize))throw Error('云端阵型库分片缺失');
  const bytes=decode(keys.map(k=>data[k]).join(''));
  if(await digest(bytes)!==head.digest)throw Error('云端阵型库校验失败，保留本地阵型');
  const value=await this.validate(await unpack(bytes));this.head=head;this.cloudRead=true;this.lastCloud=JSON.stringify(value);
  return {value,savedAt:head.savedAt};
 }
 async prepare(){
  if(!this.cloudEnabled)return;
  try{
   const saved=await this.readCloud();
   if(saved&&!this.library.existed){this.library.replace(saved.value);this.report('阵型库已从云端恢复')}
   else if(saved){this.needsChoice=JSON.stringify(this.library.snapshot())!==this.lastCloud;if(this.needsChoice)this.report('本地与云端阵型库不同，请选择上传本地或恢复云端')}
  }catch(error){this.needsChoice=true;this.report('云端阵型库暂不可用，继续使用本地：'+error.message)}
 }
 async writeCloud(value){
  if(!this.cloudEnabled)return;
  if(this.needsChoice)throw Error('本地与云端阵型库不同，请先选择上传本地或恢复云端');
  if(!this.cloudRead)await this.readCloud();
  const text=JSON.stringify(value);if(text===this.lastCloud)return;
  this.uploadState(true);
  try{
   const bytes=new Uint8Array(await new Response(new Blob([text]).stream().pipeThrough(new CompressionStream('gzip'))).arrayBuffer()),encoded=encode(bytes),count=Math.ceil(encoded.length/chunkSize);
   if(count>chunkCount)throw Error('阵型库超过当前云端容量，请导出 JSON 备份；本地阵型已保留');
   const toy=await this.remote(),slot=this.head?.slot==='a'?'b':'a',values={};
   for(let i=0;i<count;++i)values[chunkKey(slot,i)]=encoded.slice(i*chunkSize,(i+1)*chunkSize);
   const head={format:1,slot,count,digest:await digest(bytes),savedAt:Date.now()};
   await deadline(cloudCall(()=>toy.setCloudStorage(values)));
   await deadline(cloudCall(()=>toy.setCloudStorage({[headKey]:JSON.stringify(head)})));
   this.head=head;this.lastCloud=text;this.report('阵型库已备份到当前 B 站账号');
  }finally{this.uploadState(false)}
 }
 save(force=false){
  if(force)this.needsChoice=false;
  if(!this.cloudEnabled)return Promise.resolve();
  this.pending=this.library.snapshot();if(this.saving)return this.saving;
  this.saving=(async()=>{let error;while(this.pending){const next=this.pending;this.pending=null;try{await this.writeCloud(next);error=null}catch(e){error=e}}if(error)throw error})().finally(()=>{this.saving=null});
  return this.saving;
 }
}
