// UI-only adapter. The editor never accesses the live Board or shared actors.
// All writes are validated by the existing isolated save tool while the game
// is stopped. Source of the bundled editor: YuHao2006/PvzDevTools/saveEditor.
export function connectSaveEditor({module,root,ready,idle,running,setActive,authorize,sync,installBaseline,tools,lineups,status}){
 const button=document.getElementById('custom-save'),dialog=document.getElementById('save-editor-dialog'),frame=document.getElementById('save-editor-frame');
 let documentText=null,granted=false,busy=false,opening=false;
 const channel='pvz-save-editor-v1',origin=location.origin;
 const names=()=>module.FS.readdir(root).filter(name=>/^game[1-9]\d*_\d+\.dat64$/.test(name)).sort();
 const read=name=>{if(!/^game[1-9]\d*_\d+\.dat64$/.test(name)||!module.FS.analyzePath(root+'/'+name).exists)throw Error('没有可用的关卡 DAT64');const bytes=module.FS.readFile(root+'/'+name);if(bytes.length>32*1024*1024)throw Error('DAT64 超过 32 MiB');return bytes};
 const custom=()=>{try{const values=JSON.parse(localStorage.getItem('pvz_custom_lineups')||'[]');return Array.isArray(values)?values:[]}catch{return []}};
 function send(value){frame.contentWindow?.postMessage({channel,...value},origin)}
 async function initial(){const m=await lineups,first=m.defaultLineups[0];send({op:'init',payload:{presets:m.defaultLineups,custom:custom(),saves:names(),code:first.code,lineup:await m.decodeLineup(first.code)}})}
 button.onclick=async()=>{
  if(!ready()||!idle()||running()||opening||busy)return;
  opening=true;button.disabled=true;setActive(true);
  try{
   await authorize();
   if(!documentText){const response=await fetch('./save-editor.html');if(!response.ok)throw Error('存档修改器读取失败（HTTP '+response.status+'）');documentText=await response.text()}
   granted=true;dialog.showModal();
   if(!frame.hasAttribute('srcdoc'))frame.srcdoc=documentText;else await initial();
  }catch(error){granted=false;status(error.message||String(error))}
  finally{opening=false;button.disabled=false;if(!dialog.open)setActive(false)}
 };
 dialog.addEventListener('cancel',event=>{if(busy)event.preventDefault()});
 dialog.addEventListener('close',()=>{granted=false;setActive(false)});
 window.addEventListener('message',async event=>{
  if(event.source!==frame.contentWindow||event.origin!==origin||event.data?.channel!==channel||!granted||!dialog.open)return;
  const {id,op,payload={}}=event.data;
  if(op==='ready'){try{await initial()}catch(error){status(error.message)}return}
  if(busy){send({id,error:'上一项存档操作尚未完成'});return}
  busy=true;button.disabled=true;
  try{
   if(running()||!ready())throw Error('请先从游戏菜单退出，再修改存档');
   let result={};const saveTools=await tools;
   if(op==='close'){dialog.close()}
   else if(op==='perfect'){await installBaseline();result={saves:names()}}
   else if(op==='save-lineup'){
    const m=await lineups,decoded=await m.decodeLineup(payload.code),name=String(payload.name||'我的阵型').trim().slice(0,60),items=m.mergeLineups(custom()).slice(m.defaultLineups.length);
    if(!m.mergeLineups(items).some(p=>p.code===decoded.code))items.push({name,code:decoded.code});
    localStorage.setItem('pvz_custom_lineups',JSON.stringify(items));result={custom:items};
   }
   else if(op==='extract'){await sync();result=await saveTools.runSaveTool({op:'extract'},read(payload.name))}
   else if(op==='export'){await sync();result={bytes:read(payload.name)}}
   else if(op==='generate'){
    if(!module.FS.analyzePath(root+'/user1.dat64').exists)throw Error('请先在“用户进度”页部署完美存档，建立 1 号用户');
    const r=payload.recipe;if(!r||!Array.isArray(r.cells)||r.cells.length!==54||r.cells.some(c=>!Number.isInteger(c)||c<0||c>65535||(c>>>10)>48))throw Error('阵型格子内容无效');
    const m=await lineups,recipe=m.makeRecipe(r,r.flags,r.seed,r.scene,r.sun);
    const generated=await saveTools.runSaveTool({op:'generate',recipe});if(!generated.bytes)throw Error('存档工具没有生成 DAT64');
    module.FS.writeFile(root+'/game1_58.dat64',generated.bytes);await sync();result={saves:names()};
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
