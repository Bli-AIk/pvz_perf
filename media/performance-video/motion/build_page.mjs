import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
export const here=path.dirname(fileURLToPath(import.meta.url));
export const defaultOutput=path.resolve(here,'../../../out/media/performance-v3.2.4-motion');
export function buildPage(){
 const tables=JSON.parse(fs.readFileSync(path.join(here,'../tables.json'),'utf8'));
 const inputs=JSON.parse(fs.readFileSync(path.join(defaultOutput,'web-inputs.json'),'utf8'));
 tables.selection=inputs.selectionBenchmark;
 const safeJson=x=>JSON.stringify(x).replaceAll('<','\\u003c');
 const script=['reanim-player.js','film.js'].map(n=>fs.readFileSync(path.join(here,n),'utf8')).join('\n');
 return fs.readFileSync(path.join(here,'index.html'),'utf8')
  .replace('/*__DATA__*/',()=>`window.EVIDENCE=${safeJson(tables)};window.WEB_INPUTS=${safeJson(inputs)};`)
  .replace('/*__FILM__*/',()=>script);
}
if(process.argv[1]&&path.resolve(process.argv[1])===fileURLToPath(import.meta.url)){
 fs.mkdirSync(defaultOutput,{recursive:true});const file=path.join(defaultOutput,'pvz-perf-motion.html');fs.writeFileSync(file,buildPage());console.log(file);
}
