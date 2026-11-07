import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import {defaultLineups} from './game/lineups.js';
const {chromium}=await import(process.env.PVZ_PLAYWRIGHT_MODULE);
const browser=await chromium.launch({headless:true,executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',args:['--enable-unsafe-webgpu','--ignore-gpu-blocklist']});
const base=process.env.PVZ_WEBGPU_URL||'http://127.0.0.1:18089/';
try{
 const page=await browser.newPage();page.on('dialog',d=>d.accept());
 await page.goto(base);await page.waitForFunction(()=>!document.getElementById('start').disabled,null,{timeout:120000});
 await page.locator('#custom-save').click();const editor=page.frameLocator('#save-editor-frame');
 await editor.locator('#status').filter({hasText:'已打开'}).waitFor();await editor.locator('[data-tab=lineup]').click();
 await editor.locator('summary').filter({hasText:'阵型库 JSON'}).click();
 const expected={format:1,lineups:[defaultLineups[2],{...defaultLineups[0],name:'我的 JSON 阵型'}]};
 await editor.locator('#lineups-file').setInputFiles({name:'lineups.json',mimeType:'application/json',buffer:Buffer.from(JSON.stringify(expected))});
 await editor.locator('#import-lineups').click();await editor.locator('#editor-confirm button[value=confirm]').click();await editor.locator('#status').filter({hasText:'已导入阵型库 JSON'}).waitFor();
 const values=await editor.locator('#preset option').evaluateAll(items=>items.filter(o=>o.value).map(o=>({name:o.textContent,code:o.value})));assert.deepEqual(values,expected.lineups);
 const download=page.waitForEvent('download');await editor.locator('#export-lineups').click();assert.deepEqual(JSON.parse(await fs.readFile(await (await download).path(),'utf8')),expected);
 await editor.locator('#close').click();await page.reload();await page.waitForFunction(()=>!document.getElementById('start').disabled,null,{timeout:120000});
 await page.locator('#custom-save').click();await editor.locator('#status').filter({hasText:'已打开'}).waitFor();assert.deepEqual(await editor.locator('#preset option').evaluateAll(items=>items.filter(o=>o.value).map(o=>o.textContent)),expected.lineups.map(p=>p.name));
 // The same JSON artifact must work independently, with no game instance.
 const standalone=await browser.newPage(),html=await fs.readFile(new URL('../bin/web/save-editor.html',import.meta.url),'utf8');
 standalone.on('dialog',d=>d.accept());await standalone.route('**/standalone-editor.html',r=>r.fulfill({contentType:'text/html',body:html}));await standalone.goto(new URL('standalone-editor.html',base).href);
 await standalone.locator('[data-tab=lineup]').click();await standalone.locator('summary').filter({hasText:'阵型库 JSON'}).click();
 await standalone.locator('#lineups-file').setInputFiles({name:'lineups.json',mimeType:'application/json',buffer:Buffer.from(JSON.stringify(expected))});await standalone.locator('#import-lineups').click();await standalone.locator('#editor-confirm button[value=confirm]').click();await standalone.locator('#status').filter({hasText:'已导入阵型库 JSON'}).waitFor();
 await standalone.reload();assert.deepEqual(await standalone.locator('#preset option').evaluateAll(items=>items.filter(o=>o.value).map(o=>o.textContent)),expected.lineups.map(p=>p.name));
 console.log('Hosted and standalone DevTools: JSON lineup import/export, ordering, names and reload persistence passed');
}finally{await browser.close()}
