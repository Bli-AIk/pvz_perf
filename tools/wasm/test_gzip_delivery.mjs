import assert from 'node:assert/strict';
import fs from 'node:fs';
import {gunzipSync} from 'node:zlib';
const {chromium}=await import(process.env.PVZ_PLAYWRIGHT_MODULE||'playwright-core');
const browser=await chromium.launch({headless:true,executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe'});
try{
 const page=await browser.newPage();
 const packed=fs.readFileSync('out/wasm/toy/pvz-perf.wasm.gz');
 await page.route('**/pvz-perf.wasm.gz',r=>r.fulfill({headers:{'Content-Type':'application/wasm'},body:gunzipSync(packed)}));
 await page.goto('http://127.0.0.1:18088/');await page.waitForFunction(()=>!document.getElementById('start').disabled,null,{timeout:90000});
 const result=await page.evaluate(async()=>{
  const bytes=new Uint8Array(await (await fetch('./pvz-perf.wasm.gz')).arrayBuffer());
  let previousError;try{await new Response(new Blob([bytes]).stream().pipeThrough(new DecompressionStream('gzip'))).arrayBuffer()}catch(error){previousError=error.message}
  return {magic:Array.from(bytes.slice(0,4)),previousError};
 });
 assert.deepEqual(result.magic,[0,97,115,109]);assert.ok(result.previousError);
 const generated=await page.evaluate(async()=>{
  const {decodeLineup,makeRecipe}=await import('./lineups.js');const {runSaveTool}=await import('./save-tools.js');
  const lineup=await decodeLineup('LI43jH/kRlzjZrhRF3RUpjJeOFHFZEXTHtMe0tlawVVE7GI0efJc8npSWMdVLhV261Y=');
  const save=await runSaveTool({op:'generate',recipe:makeRecipe(lineup,128,114514,2)});
  return {bytes:save.bytes.length,metadata:save.metadata};
 });
 assert.equal(generated.metadata.scene,2);assert.equal(generated.metadata.flags,128);assert.ok(generated.bytes>100000);
 console.log(JSON.stringify({alreadyDecodedResponse:true,oldError:result.previousError,fiveStarGeneration:true,plants:generated.metadata.plants,healthyPlants:generated.metadata.healthyPlants}));
}finally{await browser.close()}
