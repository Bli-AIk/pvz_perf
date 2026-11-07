import assert from 'node:assert/strict';
import {defaultLineups,decodeLineup,encodeLineup,makeRecipe} from './game/lineups.js';
assert.deepEqual(defaultLineups.map(x=>x.name),['97无炮','14格','24炮','[终极战术]五角星无炮流']);
for(const item of defaultLineups){const decoded=await decodeLineup(item.code);const roundtrip=await decodeLineup(await encodeLineup(decoded));assert.deepEqual(roundtrip.cells,decoded.cells);console.log(item.name,'scene',decoded.scene,'occupied cells',decoded.cells.filter(Boolean).length)}
console.log('Default lineup codes and recipe parameters: passed');

const lineup=await decodeLineup(defaultLineups[0].code);
assert.equal(makeRecipe(lineup,128,114514,2,4321).sun,4321);
assert.equal(makeRecipe(lineup,128,114514,2).sun,9990);
for(const value of [-1,9991,NaN,1.2])assert.throws(()=>makeRecipe(lineup,128,114514,2,value),/阳光/);
