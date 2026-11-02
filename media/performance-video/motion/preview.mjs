import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {buildPage} from './build_page.mjs';
const here=path.dirname(fileURLToPath(import.meta.url));
const port=Number(process.env.PVZ_MOTION_PORT||8784);
http.createServer((req,res)=>{
 if(req.url!=='/'&&req.url!=='/index.html'){res.writeHead(404);res.end();return;}
 const html=buildPage();
 res.writeHead(200,{'Content-Type':'text/html; charset=utf-8','Cache-Control':'no-store'});res.end(html);
}).listen(port,'127.0.0.1',()=>console.log(`Motion preview: http://127.0.0.1:${port}/`));
