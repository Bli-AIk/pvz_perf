// Frozen USER64 v13 / users.dat v14 contract. Existing garden/Zombatar bytes
// are retained verbatim; editing these fields never constructs a game object.
const magic='PVZUSR64',encoder=new TextEncoder(),decoder=new TextDecoder();
function require(value,message){if(!value)throw Error(message)}
function integer(value,min,max,label){require(Number.isInteger(value)&&value>=min&&value<=max,label+'超出允许范围');return value}
export function validName(value){require(typeof value==='string'&&/^[\x20-\x7e]{1,20}$/.test(value)&&value.trim()===value,'用户名需为 1～20 个 ASCII 字符，首尾不能有空格');return value}
export function validId(value){return integer(value,1,2147483646,'用户 ID')}
export function decodeSummary(bytes){
 const view=new DataView(bytes.buffer,bytes.byteOffset,bytes.byteLength);let at=0;
 const read=(size,method)=>{require(at+size<=bytes.length,'用户列表不完整');const value=view[method](at,true);at+=size;return value};
 require(read(4,'getUint32')===14,'仅支持 users.dat v14');const count=read(2,'getUint16'),profiles=[];require(count<=1000,'用户数量异常');
 for(let i=0;i<count;i++){const size=read(2,'getUint16');require(at+size<=bytes.length,'用户名不完整');const name=decoder.decode(bytes.subarray(at,at+size));at+=size;const useSeq=read(4,'getUint32'),id=validId(read(4,'getUint32'));profiles.push({name,useSeq,id})}
 require(at===bytes.length,'用户列表有多余数据');validateProfiles(profiles);return profiles;
}
export function validateProfiles(profiles){require(Array.isArray(profiles)&&profiles.length<=1000,'用户列表无效');const ids=new Set(),names=new Set();for(const p of profiles){validId(p.id);validName(p.name);integer(p.useSeq,0,4294967294,'使用序号');require(!ids.has(p.id)&&!names.has(p.name.toLowerCase()),'用户 ID 或名称重复');ids.add(p.id);names.add(p.name.toLowerCase())}}
export function encodeSummary(profiles){validateProfiles(profiles);const bytes=new Uint8Array(6+profiles.reduce((n,p)=>n+10+encoder.encode(p.name).length,0)),v=new DataView(bytes.buffer);v.setUint32(0,14,true);v.setUint16(4,profiles.length,true);let at=6;for(const p of profiles){const name=encoder.encode(p.name);v.setUint16(at,name.length,true);bytes.set(name,at+2);at+=2+name.length;v.setUint32(at,p.useSeq,true);v.setUint32(at+4,p.id,true);at+=8}return bytes}
function layout(bytes){require(bytes instanceof Uint8Array&&bytes.length>=894&&decoder.decode(bytes.subarray(0,8))===magic,'用户明细不是有效的 USER64');const v=new DataView(bytes.buffer,bytes.byteOffset,bytes.byteLength);require(v.getInt32(8,true)===13,'仅支持 USER64 v13');const plants=v.getInt32(824,true);integer(plants,0,200,'花园植物数');const achievements=828+plants*88;require(achievements+45<=bytes.length,'花园数据不完整');const zombatars=v.getInt32(achievements+41,true);integer(zombatars,0,100,'大头贴数');require(bytes.length===894+plants*88+zombatars*72,'用户明细长度不正确');return {v,achievements,mini:achievements+45+zombatars*72,plants,zombatars}}
export function decodeProfile(bytes){const {v,achievements,plants,zombatars}=layout(bytes);return {level:v.getInt32(12,true),coins:v.getInt32(16,true)*10,finished:v.getInt32(20,true),records:Array.from({length:100},(_,i)=>v.getInt32(24+i*4,true)),purchases:Array.from({length:80},(_,i)=>v.getInt32(424+i*4,true)),unlocks:[8,9,14].map(i=>v.getInt32(744+i*4,true)!==0),achievements:Array.from({length:20},(_,i)=>v.getUint8(achievements+i*2)!==0),plants,zombatars}}
export function newProfile(){const bytes=new Uint8Array(894);bytes.set(encoder.encode(magic));const v=new DataView(bytes.buffer);v.setInt32(8,13,true);v.setInt32(12,1,true);return bytes}
// Same completed USER64 preset as pvz_perf's distributed perfect profile.
// Always create fresh bytes: adding a user must never alter another user's data.
export function perfectProfile(){
 const records=Array(100).fill(0),purchases=Array(80).fill(0);
 for(let mode=1;mode<=70;mode++)if(mode!==35)records[mode]=1;
 for(let mode=61;mode<=65;mode++)records[mode]=10;
 for(let mode=66;mode<=70;mode++)records[mode]=5;
 records[71]=1000;
 for(const item of [0,1,2,3,4,5,6,7,8,13,16,17,18,19,20,22,23,25,27,29])purchases[item]=1;
 purchases[9]=2;purchases[21]=4;purchases[24]=3;
 for(const item of [14,15])purchases[item]=1020;
 for(const item of [26,28])purchases[item]=1010;
 const bytes=patchProfile(newProfile(),{level:1,coins:999990,finished:2,records,purchases,unlocks:[true,true,true],achievements:Array(20).fill(true)}),{v,mini}=layout(bytes);
 // Already seen Stinky and upsell; suppress the first-use Zombatar notice.
 v.setInt32(812,1,true);v.setInt32(816,1,true);v.setUint8(mini+20,1);
 return bytes;
}
export function patchProfile(original,patch){
 const bytes=original.slice(),{v,achievements,mini}=layout(bytes),previous=new DataView(original.buffer,original.byteOffset,original.byteLength);
 if('level'in patch)v.setInt32(12,integer(patch.level,1,50,'冒险关卡'),true);
 if('coins'in patch){integer(patch.coins,0,999990,'金币');require(patch.coins%10===0,'金币需为 10 的倍数');v.setInt32(16,patch.coins/10,true)}
 if('finished'in patch)v.setInt32(20,integer(patch.finished,0,2147483647,'冒险通关次数'),true);
 if(patch.records){require(Array.isArray(patch.records)&&patch.records.length===100,'挑战记录数组无效');patch.records.forEach((value,i)=>v.setInt32(24+i*4,integer(value,0,2147483647,'挑战记录'),true));for(let i=0;i<20;i++)if(patch.records[i+1]!==previous.getInt32(24+(i+1)*4,true))v.setUint8(mini+i,patch.records[i+1]>0?1:0)}
 if(patch.purchases){require(Array.isArray(patch.purchases)&&patch.purchases.length===80,'商店记录数组无效');patch.purchases.forEach((value,i)=>v.setInt32(424+i*4,integer(value,0,2147483647,'购买记录'),true))}
 if(patch.unlocks){require(Array.isArray(patch.unlocks)&&patch.unlocks.length===3,'解锁状态无效');[8,9,14].forEach((field,i)=>v.setInt32(744+field*4,patch.unlocks[i]?1:0,true))}
 if(patch.achievements){require(Array.isArray(patch.achievements)&&patch.achievements.length===20,'成就状态无效');patch.achievements.forEach((earned,i)=>{if(!!earned===(v.getUint8(achievements+i*2)!==0))return;v.setUint8(achievements+i*2,earned?1:0);v.setUint8(achievements+i*2+1,earned?1:0)})}
 return bytes;
}

// Build a complete mutation plan first. The host commits it only while stopped.
export class ProfileStore {
 constructor(fs,root){this.fs=fs;this.root=root}
 names(){return this.fs.readdir(this.root).filter(n=>n!=='.'&&n!=='..')}
 rows(){return this.fs.analyzePath(this.root+'/users.dat').exists?decodeSummary(this.fs.readFile(this.root+'/users.dat')):[]}
 details(id){const path=this.root+'/user'+id+'.dat64';return this.fs.analyzePath(path).exists?this.fs.readFile(path):newProfile()}
 list(){return this.rows().map(row=>({...row,progress:decodeProfile(this.details(row.id))}))}
 owned(id){return this.names().filter(name=>name==='user'+id+'.dat64'||new RegExp('^game'+id+'_\\d+\\.dat64$').test(name))}
 add({perfect=false}={}){const rows=this.rows();let id=1;while(rows.some(p=>p.id===id)||this.owned(id).length)++id;validId(id);let name=(perfect?'Perfect':'Player')+id;while(rows.some(p=>p.name.toLowerCase()===name.toLowerCase()))name+='x';validName(name);rows.push({id,name,useSeq:Math.max(0,...rows.map(p=>p.useSeq))+1});validateProfiles(rows);return {id,writes:new Map([['user'+id+'.dat64',perfect?perfectProfile():newProfile()],['users.dat',encodeSummary(rows)]]),removes:[]}}
 edit({oldId,id,name,patch}){validId(oldId);validId(id);validName(name);const rows=this.rows(),user=rows.find(p=>p.id===oldId);if(!user)throw Error('目标用户不存在');if(id!==oldId&&(rows.some(p=>p.id===id)||this.owned(id).length))throw Error('目标 ID 已被用户或关卡存档占用');const details=patchProfile(this.details(oldId),patch);user.id=id;user.name=name;validateProfiles(rows);const writes=new Map(),removes=[];if(id!==oldId)for(const file of this.owned(oldId)){removes.push(file);if(file.startsWith('game'))writes.set(file.replace('game'+oldId+'_','game'+id+'_'),this.fs.readFile(this.root+'/'+file))}writes.set('user'+id+'.dat64',details);writes.set('users.dat',encodeSummary(rows));return {id,writes,removes}}
 remove(id){validId(id);const rows=this.rows();if(!rows.some(p=>p.id===id))throw Error('目标用户不存在');return {writes:new Map([['users.dat',encodeSummary(rows.filter(p=>p.id!==id))]]),removes:this.owned(id)}}
}

export const modes=[
  {
    "id": 0,
    "name": "冒险模式",
    "symbol": "GAMEMODE_ADVENTURE",
    "checkpoint": true
  },
  {
    "id": 1,
    "name": "宝石迷阵",
    "symbol": "GAMEMODE_MINIGAME_BEGHOULED",
    "checkpoint": true
  },
  {
    "id": 2,
    "name": "宝石迷阵转转看",
    "symbol": "GAMEMODE_MINIGAME_BEGHOULED_TWIST",
    "checkpoint": true
  },
  {
    "id": 3,
    "name": "雪橇区",
    "symbol": "GAMEMODE_MINIGAME_BOBSLED_BONANZA",
    "checkpoint": true
  },
  {
    "id": 4,
    "name": "植物列阵（柱子模式）",
    "symbol": "GAMEMODE_MINIGAME_COLUMN",
    "checkpoint": true
  },
  {
    "id": 5,
    "name": "僵王博士的复仇",
    "symbol": "GAMEMODE_MINIGAME_FINAL_BOSS",
    "checkpoint": true
  },
  {
    "id": 6,
    "name": "隐形食脑者",
    "symbol": "GAMEMODE_MINIGAME_INVISIGHOUL",
    "checkpoint": true
  },
  {
    "id": 7,
    "name": "坚不可摧",
    "symbol": "GAMEMODE_MINIGAME_LAST_STAND",
    "checkpoint": true
  },
  {
    "id": 8,
    "name": "小僵尸大麻烦",
    "symbol": "GAMEMODE_MINIGAME_LITTLE_TROUBLE",
    "checkpoint": true
  },
  {
    "id": 9,
    "name": "蹦蹦舞会",
    "symbol": "GAMEMODE_MINIGAME_POGO_PARTY",
    "checkpoint": true
  },
  {
    "id": 10,
    "name": "传送门之战",
    "symbol": "GAMEMODE_MINIGAME_PORTAL_COMBAT",
    "checkpoint": true
  },
  {
    "id": 11,
    "name": "种子雨",
    "symbol": "GAMEMODE_MINIGAME_RAINING_SEEDS",
    "checkpoint": true
  },
  {
    "id": 12,
    "name": "观星",
    "symbol": "GAMEMODE_MINIGAME_SEEING_STARS",
    "checkpoint": true
  },
  {
    "id": 13,
    "name": "老虎机",
    "symbol": "GAMEMODE_MINIGAME_SLOT_MACHINE",
    "checkpoint": true
  },
  {
    "id": 14,
    "name": "僵尸快跑",
    "symbol": "GAMEMODE_MINIGAME_SPEED",
    "checkpoint": true
  },
  {
    "id": 15,
    "name": "坚果保龄球",
    "symbol": "GAMEMODE_MINIGAME_WALLNUT_BOWLING",
    "checkpoint": true
  },
  {
    "id": 16,
    "name": "坚果保龄球 2",
    "symbol": "GAMEMODE_MINIGAME_WALLNUT_BOWLING_2",
    "checkpoint": true
  },
  {
    "id": 17,
    "name": "植物僵尸",
    "symbol": "GAMEMODE_MINIGAME_WAR_AND_PEAS",
    "checkpoint": true
  },
  {
    "id": 18,
    "name": "植物僵尸 2",
    "symbol": "GAMEMODE_MINIGAME_WAR_AND_PEAS_2",
    "checkpoint": true
  },
  {
    "id": 19,
    "name": "锤僵尸",
    "symbol": "GAMEMODE_MINIGAME_WHACK_A_ZOMBIE",
    "checkpoint": true
  },
  {
    "id": 20,
    "name": "僵尸水族馆",
    "symbol": "GAMEMODE_MINIGAME_ZOMBIQUARIUM",
    "checkpoint": true
  },
  {
    "id": 21,
    "name": "隐藏关 · Air Raid",
    "symbol": "GAMEMODE_LIMBO_AIR_RAID",
    "checkpoint": true
  },
  {
    "id": 22,
    "name": "隐藏关 · Art Challenge Sunflower",
    "symbol": "GAMEMODE_LIMBO_ART_CHALLENGE_SUNFLOWER",
    "checkpoint": true
  },
  {
    "id": 23,
    "name": "隐藏关 · Art Challenge Wallnut",
    "symbol": "GAMEMODE_LIMBO_ART_CHALLENGE_WALLNUT",
    "checkpoint": true
  },
  {
    "id": 24,
    "name": "隐藏关 · Big Time",
    "symbol": "GAMEMODE_LIMBO_BIG_TIME",
    "checkpoint": true
  },
  {
    "id": 25,
    "name": "隐藏关 · Bungee Blitz",
    "symbol": "GAMEMODE_LIMBO_BUNGEE_BLITZ",
    "checkpoint": true
  },
  {
    "id": 26,
    "name": "隐藏关 · Grave Danger",
    "symbol": "GAMEMODE_LIMBO_GRAVE_DANGER",
    "checkpoint": true
  },
  {
    "id": 27,
    "name": "隐藏关 · High Gravity",
    "symbol": "GAMEMODE_LIMBO_HIGH_GRAVITY",
    "checkpoint": true
  },
  {
    "id": 28,
    "name": "隐藏关 · Ice",
    "symbol": "GAMEMODE_LIMBO_ICE",
    "checkpoint": true
  },
  {
    "id": 29,
    "name": "隐藏关 · Resodded",
    "symbol": "GAMEMODE_LIMBO_RESODDED",
    "checkpoint": true
  },
  {
    "id": 30,
    "name": "隐藏关 · Shovel",
    "symbol": "GAMEMODE_LIMBO_SHOVEL",
    "checkpoint": true
  },
  {
    "id": 31,
    "name": "隐藏关 · Squirrel",
    "symbol": "GAMEMODE_LIMBO_SQUIRREL",
    "checkpoint": true
  },
  {
    "id": 32,
    "name": "隐藏关 · Stormy Night",
    "symbol": "GAMEMODE_LIMBO_STORMY_NIGHT",
    "checkpoint": true
  },
  {
    "id": 33,
    "name": "隐藏关 · Sunny Day",
    "symbol": "GAMEMODE_LIMBO_SUNNY_DAY",
    "checkpoint": true
  },
  {
    "id": 34,
    "name": "禅境花园",
    "symbol": "GAMEMODE_LIMBO_ZEN_GARDEN",
    "checkpoint": false
  },
  {
    "id": 35,
    "name": "开场演示",
    "symbol": "GAMEMODE_INTRO",
    "checkpoint": false
  },
  {
    "id": 36,
    "name": "我是僵尸 1",
    "symbol": "GAMEMODE_PUZZLE_I_ZOMBIE_1",
    "checkpoint": true
  },
  {
    "id": 37,
    "name": "我是僵尸 2",
    "symbol": "GAMEMODE_PUZZLE_I_ZOMBIE_2",
    "checkpoint": true
  },
  {
    "id": 38,
    "name": "我是僵尸 3",
    "symbol": "GAMEMODE_PUZZLE_I_ZOMBIE_3",
    "checkpoint": true
  },
  {
    "id": 39,
    "name": "我是僵尸 4",
    "symbol": "GAMEMODE_PUZZLE_I_ZOMBIE_4",
    "checkpoint": true
  },
  {
    "id": 40,
    "name": "我是僵尸 5",
    "symbol": "GAMEMODE_PUZZLE_I_ZOMBIE_5",
    "checkpoint": true
  },
  {
    "id": 41,
    "name": "我是僵尸 6",
    "symbol": "GAMEMODE_PUZZLE_I_ZOMBIE_6",
    "checkpoint": true
  },
  {
    "id": 42,
    "name": "我是僵尸 7",
    "symbol": "GAMEMODE_PUZZLE_I_ZOMBIE_7",
    "checkpoint": true
  },
  {
    "id": 43,
    "name": "我是僵尸 8",
    "symbol": "GAMEMODE_PUZZLE_I_ZOMBIE_8",
    "checkpoint": true
  },
  {
    "id": 44,
    "name": "我是僵尸 9",
    "symbol": "GAMEMODE_PUZZLE_I_ZOMBIE_9",
    "checkpoint": true
  },
  {
    "id": 45,
    "name": "我是僵尸无尽",
    "symbol": "GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS",
    "checkpoint": true
  },
  {
    "id": 46,
    "name": "砸罐子 1",
    "symbol": "GAMEMODE_SCARY_POTTER_1",
    "checkpoint": true
  },
  {
    "id": 47,
    "name": "砸罐子 2",
    "symbol": "GAMEMODE_SCARY_POTTER_2",
    "checkpoint": true
  },
  {
    "id": 48,
    "name": "砸罐子 3",
    "symbol": "GAMEMODE_SCARY_POTTER_3",
    "checkpoint": true
  },
  {
    "id": 49,
    "name": "砸罐子 4",
    "symbol": "GAMEMODE_SCARY_POTTER_4",
    "checkpoint": true
  },
  {
    "id": 50,
    "name": "砸罐子 5",
    "symbol": "GAMEMODE_SCARY_POTTER_5",
    "checkpoint": true
  },
  {
    "id": 51,
    "name": "砸罐子 6",
    "symbol": "GAMEMODE_SCARY_POTTER_6",
    "checkpoint": true
  },
  {
    "id": 52,
    "name": "砸罐子 7",
    "symbol": "GAMEMODE_SCARY_POTTER_7",
    "checkpoint": true
  },
  {
    "id": 53,
    "name": "砸罐子 8",
    "symbol": "GAMEMODE_SCARY_POTTER_8",
    "checkpoint": true
  },
  {
    "id": 54,
    "name": "砸罐子 9",
    "symbol": "GAMEMODE_SCARY_POTTER_9",
    "checkpoint": true
  },
  {
    "id": 55,
    "name": "砸罐子无尽",
    "symbol": "GAMEMODE_SCARY_POTTER_ENDLESS",
    "checkpoint": true
  },
  {
    "id": 56,
    "name": "生存无尽 · 白天",
    "symbol": "GAMEMODE_SURVIVAL_ENDLESS_STAGE_1",
    "checkpoint": true
  },
  {
    "id": 57,
    "name": "生存无尽 · 黑夜",
    "symbol": "GAMEMODE_SURVIVAL_ENDLESS_STAGE_2",
    "checkpoint": true
  },
  {
    "id": 58,
    "name": "生存无尽 · 泳池",
    "symbol": "GAMEMODE_SURVIVAL_ENDLESS_STAGE_3",
    "checkpoint": true
  },
  {
    "id": 59,
    "name": "生存无尽 · 浓雾",
    "symbol": "GAMEMODE_SURVIVAL_ENDLESS_STAGE_4",
    "checkpoint": true
  },
  {
    "id": 60,
    "name": "生存无尽 · 屋顶",
    "symbol": "GAMEMODE_SURVIVAL_ENDLESS_STAGE_5",
    "checkpoint": true
  },
  {
    "id": 61,
    "name": "生存困难 · 白天",
    "symbol": "GAMEMODE_SURVIVAL_HARD_STAGE_1",
    "checkpoint": true
  },
  {
    "id": 62,
    "name": "生存困难 · 黑夜",
    "symbol": "GAMEMODE_SURVIVAL_HARD_STAGE_2",
    "checkpoint": true
  },
  {
    "id": 63,
    "name": "生存困难 · 泳池",
    "symbol": "GAMEMODE_SURVIVAL_HARD_STAGE_3",
    "checkpoint": true
  },
  {
    "id": 64,
    "name": "生存困难 · 浓雾",
    "symbol": "GAMEMODE_SURVIVAL_HARD_STAGE_4",
    "checkpoint": true
  },
  {
    "id": 65,
    "name": "生存困难 · 屋顶",
    "symbol": "GAMEMODE_SURVIVAL_HARD_STAGE_5",
    "checkpoint": true
  },
  {
    "id": 66,
    "name": "生存普通 · 白天",
    "symbol": "GAMEMODE_SURVIVAL_NORMAL_STAGE_1",
    "checkpoint": true
  },
  {
    "id": 67,
    "name": "生存普通 · 黑夜",
    "symbol": "GAMEMODE_SURVIVAL_NORMAL_STAGE_2",
    "checkpoint": true
  },
  {
    "id": 68,
    "name": "生存普通 · 泳池",
    "symbol": "GAMEMODE_SURVIVAL_NORMAL_STAGE_3",
    "checkpoint": true
  },
  {
    "id": 69,
    "name": "生存普通 · 浓雾",
    "symbol": "GAMEMODE_SURVIVAL_NORMAL_STAGE_4",
    "checkpoint": true
  },
  {
    "id": 70,
    "name": "生存普通 · 屋顶",
    "symbol": "GAMEMODE_SURVIVAL_NORMAL_STAGE_5",
    "checkpoint": true
  },
  {
    "id": 71,
    "name": "智慧树",
    "symbol": "GAMEMODE_TREE_OF_WISDOM",
    "checkpoint": false
  },
  {
    "id": 72,
    "name": "自定义关卡",
    "symbol": "GAMEMODE_CUSTOM_LEVEL",
    "checkpoint": true
  }
]
;
