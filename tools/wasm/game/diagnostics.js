// Opt-in, bounded breadcrumbs. A missing normal exit is not proof of a crash:
// closing a tab and an OS process kill can leave the same final record.
export function createDiagnostics({storage,readState,environment,defaultEnabled=false,now=()=>Date.now(),
 schedule=fn=>setInterval(fn,10000),cancel=id=>clearInterval(id)}){
 const key='pvz_diagnostics_v1',flag='pvz_diagnostics_enabled_v1';
 const read=k=>{try{return storage.getItem(k)}catch{return null}};
 const preference=read(flag);
 let enabled=preference===null?defaultEnabled:preference==='true',timer=null,current=null,previous=null,storageError='';
 try{previous=JSON.parse(read(key)||'null')}catch{}
 const persist=()=>{try{storage.setItem(key,JSON.stringify(current));storageError=''}catch(error){storageError=String(error.message||error)}};
 const stop=()=>{if(timer!==null)cancel(timer);timer=null};
 function record(type,detail=''){
  if(!enabled)return;
  if(!current)current={format:1,startedAt:new Date(now()).toISOString(),normalQuit:false,environment:environment(),events:[]};
  let state;try{state=readState()}catch(error){state={readError:String(error.message||error)}}
  current.events.push({at:new Date(now()).toISOString(),type,detail:String(detail).slice(0,3000),state});
  if(current.events.length>90)current.events.splice(0,current.events.length-90);
  persist();
 }
 return {
  get enabled(){return enabled},
  setEnabled(value){enabled=!!value;try{storage.setItem(flag,String(enabled))}catch{}if(!enabled)stop();else record('enabled')},
  record,
  start(){if(!enabled)return;stop();record('game-start');current.normalQuit=false;persist();timer=schedule(()=>record('sample'))},
  quit(){if(!enabled)return;stop();record('game-quit');current.normalQuit=true;persist()},
  pagehide(){record('pagehide');stop()},
  report(){return {notice:'诊断记录不包含存档或账号。Wasm 字节数不是进程总内存；异常中止标记不能单独证明崩溃原因。',storageError,previous,current}}
 };
}
