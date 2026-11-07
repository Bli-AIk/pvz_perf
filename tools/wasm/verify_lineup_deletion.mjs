import assert from 'node:assert/strict';
import {encodeLineup,defaultLineups} from './game/lineups.js';
const {chromium}=await import(process.env.PVZ_PLAYWRIGHT_MODULE);
const browser=await chromium.launch({headless:true,executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',args:['--enable-unsafe-webgpu','--ignore-gpu-blocklist']});
try{
 const page=await browser.newPage();page.on('dialog',d=>d.accept());
 const base=process.env.PVZ_WEBGPU_URL||'http://127.0.0.1:18089/';
 const ready=()=>page.waitForFunction(()=>!document.getElementById('start').disabled,null,{timeout:120000});
 const editor=page.frameLocator('#save-editor-frame');
 const open=async()=>{await page.locator('#custom-save').click();await editor.locator('#status').filter({hasText:'已打开'}).waitFor();await editor.locator('[data-tab=lineup]').click()};
 const entries=()=>editor.locator('#preset option').evaluateAll(options=>options.filter(o=>o.value).map(o=>({name:o.textContent,code:o.value})));
 const consistent=async()=>{const s=await editor.locator('#preset').evaluate(select=>({selected:select.value,label:select.selectedOptions[0]?.textContent,code:document.getElementById('code').value,disabled:document.getElementById('delete-lineup').disabled}));assert.ok(s.selected===s.code||s.selected===''&&s.label.includes('未保存')&&s.disabled,JSON.stringify(s));return s};
 const select=async name=>{await editor.locator('#preset').selectOption({label:name});await editor.locator('#status').filter({hasText:'已载入 '+name}).waitFor();await consistent()};
 const remove=async()=>{await editor.locator('#delete-lineup').click();await editor.locator('#editor-confirm button[value=confirm]').click();await editor.locator('#status').filter({hasText:'已从列表删除'}).waitFor();await consistent()};
 await page.goto(base);await ready();await open();await select('24炮');await remove();
 assert.deepEqual((await entries()).map(p=>p.name),['97无炮','14格','[终极战术]五角星无炮流']);
 assert.equal((await consistent()).code,defaultLineups[2].code);
 await editor.locator('#close').click();await open();assert.equal((await consistent()).selected,'');
 await editor.locator('#close').click();await page.reload();await ready();await open();
 assert.equal((await consistent()).selected,defaultLineups[0].code);
 const custom=[];
 for(let n=0;n<2;n++){
  const cells=Array(54).fill(0);if(n)cells[0]=(48<<10)|256;
  const code=await encodeLineup({scene:2,rake:0,cells});
  await editor.locator('#code').fill(code);await editor.locator('#load-code').click();await editor.locator('#status').filter({hasText:'已读取阵型码'}).waitFor();
  await editor.locator('#lineup-name').fill('回归阵型'+n);await editor.locator('#add-lineup').click();await editor.locator('#status').filter({hasText:'阵型已保存'}).waitFor();
  custom.push(await consistent());
 }
 await select('回归阵型0');await editor.locator('#add-lineup').click();await editor.locator('#status').filter({hasText:'阵型已保存'}).waitFor();
 assert.equal((await consistent()).selected,custom[0].code,'saving an existing entry must not select the last custom entry');
 await remove();assert.ok(!(await entries()).some(p=>p.code===custom[0].code));assert.ok((await entries()).some(p=>p.code===custom[1].code));
 await editor.locator('#close').click();await page.reload();await ready();await open();
 assert.ok(!(await entries()).some(p=>p.code===custom[0].code));
 for(const entry of await entries()){await select(entry.name);await remove()}
 await editor.locator('#close').click();await page.reload();await ready();await open();
 assert.deepEqual(await entries(),[]);assert.equal((await consistent()).selected,'');
 const empty=await editor.locator('#tile-info').textContent();assert.match(empty,/0 株主植物/);
 console.log('Lineup deletion: stable identity, retained draft, reopen/reload, duplicate save, custom deletion and empty library passed');
}finally{await browser.close()}
