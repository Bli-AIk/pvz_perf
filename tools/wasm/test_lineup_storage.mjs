import assert from 'node:assert/strict';
// Test the staged distribution: lineup-library.js is a DevTools artifact,
// deliberately not duplicated as hand-maintained source in the game repo.
const directory=process.env.PVZ_TOY_DIRECTORY||'../../out/wasm-webgpu/toy/';
const {LineupStorage}=await import(new URL(directory.replace(/\/?$/,'/')+'lineup-storage.js',import.meta.url));
import {defaultLineups} from './game/lineups.js';
const memory=()=>{const data=new Map();return {getItem:k=>data.get(k)??null,setItem:(k,v)=>data.set(k,v)}};
const cloud={pvz_progress_v1:'untouched'},writes=[];let failCommit=false;
const toy={getCloudStorage:async keys=>Object.fromEntries(keys.filter(k=>Object.hasOwn(cloud,k)).map(k=>[k,cloud[k]])),setCloudStorage:async rows=>{
 if(failCommit&&Object.hasOwn(rows,'pvz_lineups_v1'))throw Error('interrupted');
 for(const [key,value] of Object.entries(rows)){assert.match(key,/^[a-zA-Z0-9_-]{1,128}$/);assert.equal(typeof value,'string');assert.ok(value.length<=1000)}
 writes.push(rows);Object.assign(cloud,rows);
}};
const create=storage=>new LineupStorage(storage||memory(),{cloudEnabled:true,remote:async()=>toy,report(){}});
let a=create();await a.prepare();await a.save(true);assert.equal(writes.length,2);
await a.save();assert.equal(writes.length,2,'Unchanged list should not be uploaded again');
let b=create();await b.prepare();assert.deepEqual(b.library.snapshot().lineups,defaultLineups);
a.library.remove(defaultLineups[2].code);await a.save();b=create();await b.prepare();assert.equal(b.library.snapshot().lineups.some(p=>p.code===defaultLineups[2].code),false);
const deleted=a.library.snapshot();a.library.remove(defaultLineups[0].code);failCommit=true;await assert.rejects(a.save(),/interrupted/);
assert.deepEqual((await create().readCloud()).value,deleted,'Interrupted publication must preserve the previous complete library');
failCommit=false;await a.save();b=create();await b.prepare();assert.deepEqual(b.library.snapshot(),a.library.snapshot());
// Deliberately empty is a valid snapshot, never a request to restore defaults.
a.library.replace({format:1,lineups:[]});await a.save();b=create();await b.prepare();assert.deepEqual(b.library.snapshot().lineups,[]);
const local=memory(),c=create(local);c.library.replace({format:1,lineups:[defaultLineups[0]]});await c.prepare();assert.equal(c.needsChoice,true);await assert.rejects(c.save(),/先选择/);await c.save(true);assert.deepEqual((await create().readCloud()).value.lineups,[defaultLineups[0]]);
const head=JSON.parse(cloud.pvz_lineups_v1),part='pvz_lineups_'+head.slot+'_0',old=cloud[part];cloud[part]='AAAA';await assert.rejects(create().readCloud(),/校验失败/);cloud[part]=old;
assert.equal(cloud.pvz_progress_v1,'untouched','Lineup synchronization must not touch profile or battle data');
const disabled=new LineupStorage(memory(),{cloudEnabled:false,remote:async()=>{throw Error('Offline must not call SDK')},report(){}});await disabled.prepare();await disabled.save();
console.log('Toy KV JSON lineup snapshots: upload/restore, deletion, empty library, interrupted commit, conflicts, digest, namespace and offline isolation passed');
