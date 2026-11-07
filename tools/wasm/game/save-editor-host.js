// UI-only adapter. The editor never accesses the live Board or shared actors.
// All writes are validated by the existing isolated save tool while the game
// is stopped. Source of the bundled editor: YuHao2006/PvzDevTools/saveEditor.
import {ProfileStore,modes} from './save-codec.js';
export function connectSaveEditor({module,root,ready,idle,running,setActive,authorize,sync,tools,lineups,status}){
 const button=document.getElementById('custom-save'),dialog=document.getElementById('save-editor-dialog'),frame=document.getElementById('save-editor-frame');
 let documentText=null,granted=false,busy=false,opening=false;
 const channel='pvz-save-editor-v1',origin=location.origin;
 const names=()=>module.FS.readdir(root).filter(name=>/^game[1-9]\d*_\d+\.dat64$/.test(name)).sort();
 const read=name=>{if(!/^game[1-9]\d*_\d+\.dat64$/.test(name)||!module.FS.analyzePath(root+'/'+name).exists)throw Error('没有可用的关卡 DAT64');const bytes=module.FS.readFile(root+'/'+name);if(bytes.length>32*1024*1024)throw Error('DAT64 超过 32 MiB');return bytes};
 let store;
 const custom=()=>{try{const values=JSON.parse(localStorage.getItem('pvz_custom_lineups')||'[]');return Array.isArray(values)?values:[]}catch{return []}};
 const deleted=()=>{try{return JSON.parse(localStorage.getItem('pvz_deleted_lineups')||'[]')}catch{return []}};
 const presets=async()=>{const m=await lineups,removed=deleted();return m.defaultLineups.filter(p=>!removed.includes(p.code))};
 const slot=name=>{const match=/^game([1-9]\d*)_(\d+)\.dat64$/.exec(name);if(!match||!modes[Number(match[2])]?.checkpoint)throw Error('文件名需为存在战斗存档的 game用户ID_模式编号.dat64，编号见对照页');const id=Number(match[1]);if(!store.rows().some(p=>p.id===id))throw Error('目标用户不存在，请先在“用户进度”页新增用户');return {id,mode:Number(match[2])}};
 async function commit(plan){
  const fs=module.FS,changed=new Set([...plan.writes.keys(),...plan.removes]),before=new Map();
  for(const name of changed)before.set(name,fs.analyzePath(root+'/'+name).exists?fs.readFile(root+'/'+name).slice():null);
  try{for(const [name,bytes] of plan.writes)fs.writeFile(root+'/'+name,bytes);for(const name of plan.removes)if(!plan.writes.has(name)&&fs.analyzePath(root+'/'+name).exists)fs.unlink(root+'/'+name);await sync()}
  catch(error){for(const [name,bytes] of before){if(bytes)fs.writeFile(root+'/'+name,bytes);else if(fs.analyzePath(root+'/'+name).exists)fs.unlink(root+'/'+name)}try{await sync()}catch{}throw Error('用户修改未完成，已保留原始数据：'+error.message)}
  return {id:plan.id,profiles:store.list(),saves:names()};
 }
 function send(value){frame.contentWindow?.postMessage({channel,...value},origin)}
 async function initial(){const m=await lineups,visible=await presets(),added=custom(),first=[...visible,...added][0]||m.defaultLineups[0];send({op:'init',payload:{presets:visible,custom:added,profiles:store.list(),saves:names(),code:first.code,lineup:await m.decodeLineup(first.code)}})}
 button.onclick=async()=>{
  if(!ready()||!idle()||running()||opening||busy)return;
  opening=true;button.disabled=true;setActive(true);
  try{
   await authorize();
   store ||= new ProfileStore(module.FS,root);
   if(!documentText){const response=await fetch('./save-editor.html');if(!response.ok)throw Error('存档修改器读取失败（HTTP '+response.status+'）');documentText=await response.text()}
   granted=true;dialog.showModal();
   if(!frame.hasAttribute('srcdoc'))frame.srcdoc=documentText;else await initial();
  }catch(error){granted=false;if(dialog.open)dialog.close();status(error.message||String(error))}
  finally{opening=false;button.disabled=false;if(!dialog.open)setActive(false)}
 };
 dialog.addEventListener('cancel',event=>{if(busy)event.preventDefault()});
 dialog.addEventListener('close',()=>{granted=false;setActive(false)});
 window.addEventListener('message',async event=>{
  if(event.source!==frame.contentWindow||event.origin!==origin||event.data?.channel!==channel||!granted||!dialog.open)return;
  const {id,op,payload={}}=event.data;
  if(op==='ready'){try{await initial()}catch(error){dialog.close();status(error.message)}return}
  if(busy){send({id,error:'上一项存档操作尚未完成'});return}
  busy=true;button.disabled=true;
  try{
   if(running()||!ready())throw Error('请先从游戏菜单退出，再修改存档');
   let result={};const saveTools=await tools;
   if(op==='close'){dialog.close()}
   else if(op==='add-user')result=await commit(store.add());
   else if(op==='edit-user')result=await commit(store.edit(payload));
   else if(op==='delete-user')result=await commit(store.remove(payload.id));
   else if(op==='delete-lineup'){
    const m=await lineups,items=custom().filter(p=>p.code!==payload.code),removed=deleted();
    if(m.defaultLineups.some(p=>p.code===payload.code)&&!removed.includes(payload.code))removed.push(payload.code);
    localStorage.setItem('pvz_custom_lineups',JSON.stringify(items));localStorage.setItem('pvz_deleted_lineups',JSON.stringify(removed));result={presets:await presets(),custom:items};
   }
   else if(op==='save-lineup'){
    const m=await lineups,decoded=await m.decodeLineup(payload.code),name=String(payload.name||'我的阵型').trim().slice(0,60),items=m.mergeLineups(custom()).slice(m.defaultLineups.length);
    if(!m.mergeLineups(items).some(p=>p.code===decoded.code))items.push({name,code:decoded.code});
    localStorage.setItem('pvz_custom_lineups',JSON.stringify(items));localStorage.setItem('pvz_deleted_lineups',JSON.stringify(deleted().filter(code=>code!==decoded.code)));result={custom:items,presets:await presets()};
   }
   else if(op==='extract'){const target=slot(payload.name);await sync();result=await saveTools.runSaveTool({op:'extract',mode:target.mode},read(payload.name),store.details(target.id))}
   else if(op==='export'){await sync();result={bytes:read(payload.name)}}
   else if(op==='generate'){
    const name=payload.name||'game1_58.dat64',target=slot(name);
    const r=payload.recipe;if(!r||!Array.isArray(r.cells)||r.cells.length!==54||r.cells.some(c=>!Number.isInteger(c)||c<0||c>65535||(c>>>10)>48))throw Error('阵型格子内容无效');
    const m=await lineups,recipe=m.makeRecipe(r,r.flags,r.seed,r.scene,r.sun);
    const generated=await saveTools.runSaveTool({op:'generate',mode:target.mode,level:payload.level||1,recipe},null,store.details(target.id));if(!generated.bytes)throw Error('存档工具没有生成 DAT64');
    if(target.mode===0){
     const user=store.rows().find(p=>p.id===target.id);
     const plan=store.edit({oldId:target.id,id:target.id,name:user.name,patch:{level:payload.level||1}});
     plan.writes.set(name,generated.bytes);result=await commit(plan);
    }else{module.FS.writeFile(root+'/'+name,generated.bytes);await sync();result={saves:names()}}
   }
   else if(op==='import'){
    const match=/^game([1-9]\d*)_(\d+)\.dat64$/.exec(payload.name);
    if(!match)throw Error('文件名必须为 game用户编号_模式编号.dat64');
    if(!module.FS.analyzePath(root+'/user'+match[1]+'.dat64').exists)throw Error('目标用户不存在，请先在游戏中创建并退出');
    const bytes=payload.bytes;if(!(bytes instanceof Uint8Array)||bytes.length>32*1024*1024||new TextDecoder().decode(bytes.subarray(0,8))!=='PVZDAT64')throw Error('DAT64 文件无效');
    await saveTools.runSaveTool({op:'validate',mode:Number(match[2])},bytes);
    module.FS.writeFile(root+'/'+payload.name,bytes);await sync();result={saves:names()};
   }
   else throw Error('不支持的存档操作');
   send({id,result});
  }catch(error){send({id,error:error.message||String(error)})}
  finally{busy=false;button.disabled=false}
 });
}
