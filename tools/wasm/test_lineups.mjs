import assert from 'node:assert/strict';
import {defaultLineups,mergeLineups,decodeLineup,encodeLineup,makeRecipe} from './game/lineups.js';
assert.deepEqual(defaultLineups.map(x=>x.name),['97无炮','14格','24炮','[终极战术]五角星无炮流']);
for(const item of defaultLineups){const decoded=await decodeLineup(item.code);const roundtrip=await decodeLineup(await encodeLineup(decoded));assert.deepEqual(roundtrip.cells,decoded.cells);console.log(item.name,'scene',decoded.scene,'occupied cells',decoded.cells.filter(Boolean).length)}
const merged=mergeLineups([{...defaultLineups[3],name:'旧自定义名字'},{name:'我的阵型',code:'custom-code'},null]);
assert.equal(merged.length,5);assert.equal(merged[3].name,defaultLineups[3].name);assert.equal(merged[4].name,'我的阵型');assert.equal(mergeLineups({}).length,4);
console.log('Default lineups and custom retention: passed');

const lineup=await decodeLineup(defaultLineups[0].code);
assert.equal(makeRecipe(lineup,128,114514,2,4321).sun,4321);
assert.equal(makeRecipe(lineup,128,114514,2).sun,9990);
for(const value of [-1,9991,NaN,1.2])assert.throws(()=>makeRecipe(lineup,128,114514,2,value),/阳光/);
