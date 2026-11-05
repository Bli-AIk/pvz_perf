export function createTextInput(){
 const bar=document.getElementById('web-text-input'),field=document.getElementById('web-text-value');
 let session=0,pending=null;
 const ascii=value=>value.replace(/[^\x20-\x7e]/g,'');
 function publish(action=0){
  const before=field.value,start=field.selectionStart,end=field.selectionEnd;
  const text=ascii(before).slice(0,field.maxLength);
  if(text!==before){field.value=text;field.setSelectionRange(ascii(before.slice(0,start)).length,ascii(before.slice(0,end)).length)}
  pending={session,text,start:field.selectionStart,end:field.selectionEnd,action};
 }
 function position(){const viewport=window.visualViewport;bar.style.bottom=`${viewport?Math.max(0,innerHeight-viewport.offsetTop-viewport.height):0}px`}
 field.addEventListener('input',()=>publish());
 for(const type of ['keydown','keyup','keypress'])field.addEventListener(type,e=>{
  e.stopPropagation();
  if(type==='keydown'&&(e.key==='Enter'||e.key==='Escape')){e.preventDefault();publish(e.key==='Enter'?1:2)}
 });
 bar.addEventListener('submit',e=>{e.preventDefault();publish(1)});
 window.visualViewport?.addEventListener('resize',position);window.visualViewport?.addEventListener('scroll',position);
 const state={
  active:false,
  show(text,id,limit,cursor,selection){
   session=id;pending=null;state.active=true;bar.hidden=false;field.maxLength=limit;field.value=ascii(text);
   field.setSelectionRange(selection<0?cursor:Math.min(cursor,selection),selection<0?cursor:Math.max(cursor,selection));
   position();field.focus({preventScroll:true});
  },
  focus(){if(state.active)field.focus({preventScroll:true})},
  sync(text){if(state.active&&field.value!==text){field.value=text;field.setSelectionRange(text.length,text.length)}},
  hide(){state.active=false;pending=null;bar.hidden=true;if(document.activeElement===field)document.getElementById('canvas').focus({preventScroll:true});window.dispatchEvent(new Event('resize'))},
  take(){const value=pending;pending=null;return value}
 };
 return state;
}
