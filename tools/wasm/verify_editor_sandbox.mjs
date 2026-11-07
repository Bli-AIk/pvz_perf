import assert from 'node:assert/strict';
const {chromium}=await import(process.env.PVZ_PLAYWRIGHT_MODULE);
const browser=await chromium.launch({headless:true,executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',args:['--enable-unsafe-webgpu','--ignore-gpu-blocklist']});
try{
 const base=process.env.PVZ_WEBGPU_URL||'http://127.0.0.1:18089/',page=await browser.newPage(),native=[];
 page.on('dialog',dialog=>{native.push(dialog.type());dialog.dismiss()});
 await page.route('**/toy-sandbox-test.html',route=>route.fulfill({contentType:'text/html',body:'<!doctype html><iframe id="toy" sandbox="allow-scripts allow-same-origin allow-downloads" src="./index.html" style="width:1280px;height:900px"></iframe>'}));
 await page.goto(new URL('toy-sandbox-test.html',base).href);
 const toy=page.frameLocator('#toy'),editor=toy.frameLocator('#save-editor-frame');
 await toy.locator('#start:enabled').waitFor({timeout:120000});await toy.locator('#custom-save').click();await editor.locator('#status').filter({hasText:'已打开'}).waitFor();await editor.locator('[data-tab=lineup]').click();
 const count=()=>editor.locator('#preset option').evaluateAll(rows=>rows.filter(p=>p.value).length);
 await editor.locator('#preset').selectOption({label:'24炮'});await editor.locator('#status').filter({hasText:'已载入 24炮'}).waitFor();await editor.locator('#delete-lineup').click();
 await editor.locator('#editor-confirm').waitFor({state:'visible',timeout:3000});
 await editor.locator('#editor-confirm button[value=cancel]').click();assert.equal(await count(),4,'Cancel must leave the library unchanged');
 await editor.locator('#delete-lineup').click();await editor.locator('#editor-confirm button[value=confirm]').click();await editor.locator('#status').filter({hasText:'已从列表删除'}).waitFor();assert.equal(await count(),3);
 await editor.locator('#close').click();await page.reload();await toy.locator('#start:enabled').waitFor({timeout:120000});await toy.locator('#custom-save').click();await editor.locator('#status').filter({hasText:'已打开'}).waitFor();assert.equal(await count(),3);
 assert.deepEqual(native,[],'No native alert/confirm/prompt should be required');
 console.log('Nested Toy-like iframe without allow-modals: HTML confirmation, cancel, deletion and reload persistence passed');
}finally{await browser.close()}
