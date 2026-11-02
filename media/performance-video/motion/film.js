/* Every visual is a pure function of film time. Seeking and exporting use exactly
   the same renderer; timings are explanatory, never fabricated benchmark traces. */
(()=>{
'use strict';
const canvas=document.getElementById('film'),g=canvas.getContext('2d',{alpha:false});
const W=1920,H=1080;
const VERSION='3.2.5';
const REPOSITORY_URL='https://github.com/YuHao2006/pvz_perf';
const repoLink=document.getElementById('repo-link');
repoLink.href=REPOSITORY_URL;
const C={bg:'#0a131c',panel:'#132431',line:'#2d4352',ink:'#f0f4e9',muted:'#a5b9c5',dim:'#587180',green:'#c6ed80',teal:'#62dacb',orange:'#ffb47c',pink:'#e99aaf',blue:'#7ebaf3'};
const sans='"Microsoft YaHei", "Segoe UI", sans-serif',mono='Consolas, monospace';
const clamp=(x,a=0,b=1)=>Math.max(a,Math.min(b,x));
const lerp=(a,b,t)=>a+(b-a)*t;
const ease=x=>{x=clamp(x);return x*x*(3-2*x);};
const out=x=>1-Math.pow(1-clamp(x),3);
const ramp=(t,a,b)=>ease((t-a)/(b-a));
const mod=(a,b)=>((a%b)+b)%b;
const textWarnings=[],finiteErrors=[];
function text(s,x,y,size=30,color=C.ink,align='left',max=1800,weight=400,font=sans){
 s=String(s);g.font=`${weight} ${size}px ${font}`;
 const measured=g.measureText(s).width;
 if(measured>max){size*=max/measured;g.font=`${weight} ${size}px ${font}`;if(size<20){const warning=`${s}: ${size.toFixed(1)}px`;if(!textWarnings.includes(warning))textWarnings.push(warning);}}
 g.fillStyle=color;g.textAlign=align;g.textBaseline='middle';g.fillText(s,x,y);
}
function rect(x,y,w,h,fill,r=16,stroke=null,lw=2){if(w<=0||h<=0)return;g.beginPath();g.roundRect(x,y,w,h,r);if(fill){g.fillStyle=fill;g.fill();}if(stroke){g.strokeStyle=stroke;g.lineWidth=lw;g.stroke();}}
function line(x1,y1,x2,y2,color=C.line,width=3,dash=[]){g.beginPath();g.setLineDash(dash);g.moveTo(x1,y1);g.lineTo(x2,y2);g.strokeStyle=color;g.lineWidth=width;g.lineCap='round';g.stroke();g.setLineDash([]);}
function circle(x,y,r,fill,stroke=null,lw=2){g.beginPath();g.arc(x,y,r,0,Math.PI*2);if(fill){g.fillStyle=fill;g.fill();}if(stroke){g.strokeStyle=stroke;g.lineWidth=lw;g.stroke();}}
function arrow(x1,y1,x2,y2,color=C.green,width=3){line(x1,y1,x2,y2,color,width);const a=Math.atan2(y2-y1,x2-x1);line(x2,y2,x2-14*Math.cos(a-.5),y2-14*Math.sin(a-.5),color,width);line(x2,y2,x2-14*Math.cos(a+.5),y2-14*Math.sin(a+.5),color,width);}
function tag(s,x,y,w,color=C.teal){rect(x,y,w,44,C.panel,9);text(s,x+w/2,y+22,23,color,'center',w-16,600);}
function glow(x,y,r,color=C.green){g.save();g.globalAlpha=.16;circle(x,y,r,color);g.globalAlpha=.45;circle(x,y,r*.4,color);g.restore();circle(x,y,5,color);}
function wrap(s,x,y,width,size=25,color=C.muted,lineHeight=37){let row='',yy=y;for(const ch of s){g.font=`${size}px ${sans}`;if(ch==='\n'||g.measureText(row+ch).width>width){text(row,x,yy,size,color,'left',width);row=ch==='\n'?'':ch;yy+=lineHeight;}else row+=ch;}if(row)text(row,x,yy,size,color,'left',width);return yy;}
function code(lines,x,y,w,active=-1){rect(x,y,w,lines.length*40+30,'#0e202c',12);lines.forEach((s,i)=>{if(i===active)rect(x+10,y+10+i*40,w-20,40,'#223c3a',6);text(String(i+1).padStart(2,'0'),x+24,y+30+i*40,23,C.dim);text(s,x+76,y+30+i*40,26,i===active?C.green:C.muted,'left',w-100,400,mono);});}
function vectorPlant(x,y,scale=1,phase=0){return ReanimPlayer.draw(g,'PeaShooter','anim_full_idle',phase/2,{x,y:y+10*scale,height:140*scale,rate:18});}
function zombie(x,y,scale=1,phase=0){return ReanimPlayer.draw(g,'Zombie','anim_walk',phase/3,{x,y:y+42*scale,height:135*scale,rate:18});}
function pea(x,y,r=9){circle(x,y,r+5,'#294737');circle(x,y,r,C.green);circle(x-3,y-3,r*.25,C.ink);}
function polygon(points,fill,stroke=C.teal){g.beginPath();points.forEach((p,i)=>i?g.lineTo(...p):g.moveTo(...p));g.closePath();if(fill){g.fillStyle=fill;g.fill();}if(stroke){g.strokeStyle=stroke;g.lineWidth=3;g.stroke();}}
function animatePath(points,p,color=C.green){let lengths=[],total=0;for(let i=1;i<points.length;i++){const d=Math.hypot(points[i][0]-points[i-1][0],points[i][1]-points[i-1][1]);lengths.push(d);total+=d;line(...points[i-1],...points[i],C.line,3);}let d=clamp(p)*total;for(let i=0;i<lengths.length;i++){if(d<=lengths[i]){const f=d/lengths[i];glow(lerp(points[i][0],points[i+1][0],f),lerp(points[i][1],points[i+1][1],f),19,color);break;}d-=lengths[i];}}
const scenes=[
 {id:'opening',chapter:`先看结果 / pvz_perf ${VERSION}`,title:'同一颗低频核心，为什么能跑到近 197 帧？',sub:'Windows · LP14 · 用户设置 11x · 泳池无尽整轮实测',duration:16,captions:[[0,'先看结果：原版平均 43.95 FPS，本项目平均 196.74 FPS。'],[5,'绘制之外，本项目的逻辑更新也接近 197Hz。'],[10,'这些时间省在哪里？从一颗子弹的发射，看每帧反复做的工作。']]},
 {id:'lookup',chapter:'01 / 静态数据表',title:'植物类型与武器类型，直接决定子弹类型',sub:'典型例子：Plant::Fire 的 kFireProjectiles · 图中为简化伪代码',duration:24,captions:[[0,'豌豆射手发豌豆，寒冰射手发冰豌豆；玉米投手还要区分玉米粒与黄油。'],[8,'把确定的子弹类型映射写进二维常量表，植物类型选行，武器类型选列。'],[16,'玉米粒与黄油落在同一行的不同列，取出数据即可，不必再做一次类型判断。']]},
 {id:'table-lookup',chapter:'01 / 测量',title:'子弹类型选择：相同夹具下的局部成本',sub:'单位 ns/次 · 当前数据表声明 vs 原 switch 与黄油特判模型',duration:10,table:'selection',captions:[[0,'这组测试只覆盖子弹类型选择与共同校验，具体范围见表下注明。'],[5,'子弹类型选好了，接下来还得找到它会击中的僵尸。']]},
 {id:'rows',chapter:'02 / 缩小集合',title:'这颗豌豆，只关心这一行',sub:'对象仍在同一个池中；行索引提供候选集合',duration:24,captions:[[0,'再看全池扫描：别的行也被访问，然后才因行号不同被排除。'],[8,'建立行索引以后，查询只进入当前行，仍按正确顺序做碰撞判定。'],[16,'图中对象是示意。实际微基准使用 1024 只僵尸，其中本行 171 只。']]},
 {id:'mask',chapter:'02 / 缩小集合',title:'没有工作的轨道，连读取也省掉',sub:'活动位图 · 混合／抖动／附件工作',duration:16,captions:[[0,'同样的筛选也用于动画：活动位图直接指出这一拍需要处理的轨道。'],[8,'依次取出最低有效位，处理后清除它。掩码归零，这轮工作就结束。']]},
 {id:'table-candidates',chapter:'02 / 测量',title:'让遍历接近真正的工作量',sub:'行索引、植物格桶、活动轨道与占用绘制组',duration:12,table:'candidates',captions:[[0,'从行索引到活动位图，都在缩小访问范围；表下列出了各项测量边界。'],[7,'对象找到了，动画内部还要回答：该用哪一条轨道？']]},
 {id:'identity',chapter:'03 / 编译期身份',title:'名字只需要理解一次',sub:'reanim：轨道枚举 + 生成的最小完美哈希表',duration:24,captions:[[0,'如果每次都靠名字找轨道，就在反复解释同一个名字；这一步可以前移到编译期。'],[8,'轨道名变成枚举键，再用生成的查找表定位，校验后直接取轨道下标。'],[16,'粒子的具名查找也改用枚举和前缀区间，减少反复解析名字的工作。']]},
 {id:'table-identity',chapter:'03 / 测量',title:'减少运行时身份解析',sub:'单位 ns/查询 · reanim 测量当前 ID 扫描与生产 API',duration:12,table:'identity',captions:[[0,'表中 reanim 的对照已使用整数 ID；这里测的是继续缩短查找路径的成本。'],[7,'确定了该用哪些轨道，接下来把它们高效地画出来。']]},
 {id:'batch',chapter:'04 / 提交画面',title:'保持顺序，把兼容绘制接在一起',sub:'普通混合 N · 加亮混合 A · 双源混合支持时',duration:24,captions:[[0,'画面里的普通和加亮效果交替出现，会因混合状态切换而拆开批次。'],[8,'统一混合规则后，相邻兼容图元进入同一批，仍按原顺序叠加。'],[16,'双源混合减少混合状态造成的断批；它与 bindless 的纹理绑定优化各有分工。']]},
 {id:'quad',chapter:'04 / 动态数据',title:'两个三角形，共用四个角',sub:'静态索引复用顶点 · 动态顶点每个 40 字节',duration:20,captions:[[0,'批次可以合并，每个矩形提交的数据也能减少：六个顶点中，有两个角重复。'],[7,'把相同角合并，静态索引仍然描述两个三角形。'],[14,'每个矩形的动态顶点从 240 字节减到 160 字节，少传三分之一。']]},
 {id:'frames',chapter:'04 / 帧资源',title:'轮换几何缓冲，共享素材纹理',sub:'两套帧资源 · 单槽 65536 顶点 · 不复制全部图片',duration:20,captions:[[0,'数据少了，还要避免过早覆盖它。两套帧资源轮换使用，素材纹理保持共享。'],[7,'第三次绘制先检查 A 的 fence；尚未完成，就跳过本次绘制，逻辑继续更新。'],[14,'这样让下一帧有机会提前准备；共享资源修改等操作仍可能需要同步。']]},
 {id:'table-gpu',chapter:'04 / 测量',title:'提交结构与 CPU 绘制成本',sub:'前两行是计数；后两行是固定场景的 DX12 CPU 计时',duration:12,table:'gpu',captions:[[0,'提交次数、数据量和 CPU 绘制成本分别记录，各项测量范围见表下。'],[6,'绘制负担减轻以后，还有一个问题：刷新率变了，僵尸能否走到同一个位置？']]},
 {id:'legacy-motion',chapter:'05 / 回看原版旧算法',title:'原版旧算法：先求速度，再累加位置',sub:'先解释原版 1073 的移动方法 · 原版逻辑固定 100Hz',duration:24,captions:[[0,'先回看原版：用相邻 ground 帧的横坐标差，乘动画速率得到速度。'],[6,'原版再用这次取到的速度乘本次步长，把位移累加到僵尸的世界位置。'],[13,'沿用原版算法改变步长时，一步可能跨过速度变化点，取样差异就会影响位移。'],[19,'除了这种离散积分的差异，原版反复累加浮点位置也会叠加舍入误差。']]},
 {id:'motion-steps',chapter:'05 / 原版算术重建实验',title:'重建原版算法：不同步长，终点不同',sub:'普通僵尸 ground 轨道 · 原算术重建实验，不是原版 EXE 变帧率实测',duration:12,captions:[[0,'把原版算术放入多步长实验：起点、速率和时长相同，累加得到的终点仍有差异。'],[6,'为避开对步长分割的依赖，本项目改用相位直接求位移。']]},
 {id:'phase',chapter:'05 / 本项目的相位插值',title:'本项目：按相位直接求位移',sub:'整数纳秒记录时间 · 在相邻关键帧间插值位置与角度',duration:26,captions:[[0,'本项目由当前时间求帧号与纳秒余数，再在相邻关键帧之间直接插值位移。'],[8,'周期内，位置等于基础位移加插值位移；完成一整个周期，才累加一次周期距离。'],[17,'这先解决不同步长下的一致性。求出的帧号和余数，还能交给绘制继续复用。']]},
 {id:'trig',chapter:'05 / 共享计算',title:'一条轨道的变换，四个角一起用',sub:'每个 skew 轴一组 sin / cos · 共享角度归一化',duration:24,captions:[[0,'沿着同一份帧状态，绘制还要插值角度，再求 sin 和 cos 来构造变换。'],[7,'同一个角度只归一化一次，再求两项多项式；两个 skew 轴各算一组。'],[15,'由这两组值构造一次矩阵，矩形四角共用计算结果。']]},
 {id:'table-time',chapter:'05 / 测量',title:'把推进、查询和三角函数分开看',sub:'局部成本不代表完整一帧 · 不同测量范围不强算倍率',duration:12,table:'time',captions:[[0,'共享 sin/cos 后，这一局部计算成本降低约 14%；整帧收益仍要单独测量。'],[7,'既然四个角能复用结果，把这些计算交给 GPU，会更划算吗？']]},
 {id:'tradeoff',chapter:'06 / 取舍',title:'并行并不自动减少工作',sub:'本项目当前负载：reanim 与粒子仍在 CPU 处理',duration:18,captions:[[0,'我们试过。CPU 每条轨道算一次变换，朴素顶点着色却可能在四个角上重复计算。'],[7,'GPU 动画和线程流水线试验都未达到本项目的性能门槛，现行版本保留 CPU 路径。'],[13,'这是已测实现与设备上的取舍。接下来，把留下的方案放回完整关卡检验。']]},
 {id:'whole',chapter:'07 / 整局验证',title:'回到开头那一整局无尽',sub:'LP14 · 用户设置 11x · 本项目目标 240Hz · 基准采自 3.2.4',duration:20,captions:[[0,'现在回到开头的结果：同源基准阵型，从第 0 波测到第 20 波。'],[9,'原版逻辑固定 100Hz，本项目的绘制和逻辑都接近 197Hz。'],[15,'这是整套实现共同作用的整轮平均值，下面展开实际战斗中的变化。']]},
 {id:'desktop-traces',chapter:'07 / 原始时间序列',title:'整轮 FPS 与 CPU 占用',sub:'原版与本项目 · 同源阵型；步长与渲染器不同 · 各轮起点归零，波次未对齐',duration:6.5,trace:'desktop',captions:[[0,'绿色看 FPS，橙色看 CPU 占用；整轮平均值背后的高低起伏都保留在这里。']]},
 {id:'external',chapter:'08 / 横向参照',title:'再与精华版作一个横向参照',sub:'相同阵型、相似出怪 · 比较仅用于视频 · 非严格同局',duration:18,captions:[[0,'看完原版，再对照精华版：电脑版解除绘制帧率限制，逻辑仍为 100Hz。'],[8,'电脑比较呈现表现，手机则另测电池侧整机功耗；两者的测量范围分别标明。']]},
 {id:'external-traces',chapter:'08 / 电脑原始曲线',title:'相似阵型下，各自的整轮表现',sub:'精华版 RTSS 呈现计数 / 本项目游戏绘制计数 · 不同计数口径',duration:6.5,trace:'desktop',captions:[[0,'先展开电脑曲线：同为 LP14、用户设置 11x，阵型相同，出怪轨迹相似。']]},
 {id:'phone-frequency',chapter:'08 / 手机原始频率',title:'换到手机，看四个 CPU 簇的频率',sub:'骁龙 8 Gen 3 · policy 频率（GHz）· 各轮有效放电区间起点归零',duration:6.5,trace:'phone',captions:[[0,'换到手机，橙色是精华版，绿色是本项目；这里记录 CPU 簇频率，非线程驻留。']]},
 {id:'phone-power',chapter:'08 / 手机原始功耗',title:'再看整段功耗与时间加权平均',sub:'电池侧整机功耗 · 包含屏幕、风扇与系统',duration:6.5,trace:'phone',captions:[[0,'功耗也看完整采样区间的平均值；两轮温度与时长差异一并列出。']]},
 {id:'closing',chapter:`pvz_perf / ${VERSION}`,title:'每一份性能，都要有来处',sub:'源码、原始测量和测量范围，都可以追溯',duration:10,captions:[[0,'从减少重复工作，到验证一整局的结果：实现与原始数据，都在这个仓库里。']]}
];
let total=0;for(const s of scenes){s.start=total;total+=s.duration;}
function background(t){g.fillStyle=C.bg;g.fillRect(0,0,W,H);const gr=g.createRadialGradient(1500,330,0,1500,330,1100);gr.addColorStop(0,'#142e37');gr.addColorStop(1,C.bg);g.fillStyle=gr;g.fillRect(0,0,W,H);g.fillStyle='#1c303d';for(let x=48;x<W;x+=48)for(let y=240;y<920;y+=48)g.fillRect(x,y,2,2);}
function header(s,t){text('pvz_perf',72,47,25,C.green,'left',400,700,mono);text(s.chapter,1848,47,24,C.muted,'right');text(s.title,72,122,54,C.ink,'left',1760,700);text(s.sub,76,185,27,C.muted,'left',1750);line(72,221,1848,221,C.line,1);}
function footer(s,t,global){const cap=s.captions.filter(([at])=>t>=at).at(-1)?.[1]||'';rect(54,948,1812,74,'#0c1822',12);text(cap,960,984,32,C.ink,'center',1710,500);text(s.trace?'原始采样展开 · 未平滑 / 未补点':s.table?'原始测量数据 · 详见性能文档':'过程动画为机制示意，非运行耗时比例',72,1040,21,C.dim);text('AI配音',1848,1040,22,C.dim,'right',250,400);rect(0,1072,W,8,'#1c303d',0);rect(0,1072,W*global/total,8,C.green,0);for(const c of scenes)line(W*c.start/total,1072,W*c.start/total,1080,C.bg,3);}
function tableScene(s,t){const data=EVIDENCE[s.table],x=96,w=1728,y=282;const col=[x+30,x+1000,x+1450];text(data.columns[0],col[0],y,28,C.muted,'left',810);text(data.columns[1],col[1],y,26,C.orange,'center',390);text(data.columns[2],col[2],y,26,C.green,'center',440);line(x,y+35,x+w,y+35,C.line,2);data.rows.forEach((row,i)=>{const yy=y+96+i*108;const p=out((t-.25-i*.3)/1.3);g.save();g.globalAlpha=p;g.translate(0,(1-p)*14);rect(x,yy-42,w,86,i%2?'#152732':'#10212d',8);text(row[0],col[0],yy,31,C.ink,'left',820,500);text(row[1],col[1],yy,34,C.orange,'center',390,600,row[1].match(/^[\d.% W]+$/)?mono:sans);text(row[2],col[2],yy,34,C.green,'center',440,600,row[2].match(/^[\d.% W]+$/)?mono:sans);g.restore();});wrap(data.note,x+10,835,w-20,25,C.muted,38);}
const drawFns={};
function repositoryLink(){
 rect(410,827,1100,72,'#132c34',12,C.teal,1);
 text(REPOSITORY_URL.replace('https://',''),960,863,33,C.teal,'center',1040,500,mono);
}
drawFns.opening=(t)=>{
 const p=ramp(t,0,1.5);
 const results=[{x:100,label:'原版 1073',fps:43.95,color:C.orange,rate:'逻辑更新 100.00 Hz'},
  {x:1040,label:'pvz_perf',fps:196.74,color:C.green,rate:'逻辑更新 196.76 Hz · 上限 240 Hz'}];
 results.forEach(({x,label,fps,color,rate})=>{
  rect(x,270,780,314,C.panel,16,color,2);
  text(label,x+36,319,35,color,'left',700,600);
  text(fps.toFixed(2),x+35,431,136,color,'left',590,700,mono);
  text('FPS',x+642,471,36,C.muted,'left',110,600,mono);
  rect(x+36,508,708,10,'#253c45',5);rect(x+36,508,708*fps/240*p,10,color,5);
  text(rate,x+36,551,29,C.muted,'left',715);
 });
 arrow(911,428,1005,428,C.ink,5);
 rect(100,615,1720,105,'#1c352a',12);
 for(let i=1;i<17;i++)line(100+i*101,615,100+i*101,720,'#2b4737',1);
 vectorPlant(196,696,.65,t*2);
 for(let i=0;i<4;i++)zombie(1060+i*178-mod(t*18,50),673,.6,t*3+i);
 for(let i=0;i<5;i++)pea(260+mod(t*125+i*139,650),660,7);
 text('同源基准阵型 · 逻辑步长、战斗轨迹与渲染器不同',960,753,25,C.muted,'center',1700);
 text('更多更新，为什么反而花得更少？',960,794,39,C.green,'center',1700,600);
 repositoryLink();
};
drawFns.lookup=(t)=>{
 tag('分支选择子弹类型',105,260,400,C.orange);tag('植物选行 · 武器选列',1030,260,500,C.green);
 const cases=[['PEASHOOTER',0,'PEA'],['SNOWPEA',5,'SNOWPEA'],['KERNELPULT',34,'KERNEL']];
 const queries=[[0,0],[1,0],[2,0],[2,1]];
 const [selected,weapon]=queries[Math.floor(t/6)%queries.length],cycle=mod(t,6),step=Math.min(selected,Math.floor(cycle/.7));
 for(let i=0;i<3;i++){const yy=360+i*106,active=i===step;rect(115,yy,670,74,active?'#453a31':C.panel,10,active?C.orange:null);
  text(`${cases[i][0]} → ${cases[i][2]}`,143,yy+37,27,active?C.orange:C.muted,'left',620,400,mono);
  if(i<2)arrow(430,yy+81,430,yy+99,C.dim,2);if(active)glow(92,yy+37,16,C.orange);
 }
 const slots=[[0,'PEA','PEA'],[5,'SNOWPEA','SNOWPEA'],[7,'PEA','PEA'],[34,'KERNEL','BUTTER']];
 text('植物',1090,330,25,C.muted,'center');text('主武器 [0]',1340,330,25,C.muted,'center');text('副武器 [1]',1640,330,25,C.muted,'center');
 slots.forEach(([index,...values],i)=>{const yy=360+i*81,hit=index===cases[selected][1];rect(1040,yy,750,66,C.panel,9,hit?C.green:null);
  text(`[${index}]`,1090,yy+33,28,hit?C.green:C.dim,'center',95,600,mono);
  values.forEach((value,column)=>{const active=hit&&column===weapon,x=1200+column*300;if(active)rect(x,yy+6,280,54,'#304a32',7,C.green,2);text(value,x+140,yy+33,29,active?C.green:C.muted,'center',270,500,mono);});
 });
 const row=slots.findIndex(s=>s[0]===cases[selected][1]),result=slots[row][weapon+1];
 tag(`type ${cases[selected][1]}`,851,320,156,C.teal);
 animatePath([[929,378],[929,393+row*81],[1022,393+row*81]],clamp(cycle/.9),C.green);
 text(`子弹类型 = [${cases[selected][1]}][${weapon}] = ${result}`,1062,711,31,C.green,'left',750,600,mono);
 text(selected===2?(weapon?'玉米投手 + 副武器 → 再覆盖成 BUTTER':'玉米投手 + 主武器 → 保留 KERNEL'):'图中展示部分植物槽位',113,696,26,selected===2?C.orange:C.muted,'left',770);
 code(['switch (type) { /* 选择子弹类型 */ }','if (type == KERNELPULT && weapon == 1)','  projectile = BUTTER;'],103,765,800,selected===2?1:0);
 code(['static constexpr Projectile table[][2] = {','  /* PEA, SNOWPEA, KERNEL / BUTTER ... */','};  projectile = table[type][weapon];'],1015,765,800,cycle<.9?0:2);
};
drawFns.rows=(t)=>{
 const chosen=2,sw=ramp(t,7,9),gridX=100,gridY=290,cw=130,rh=94;
 const pass=mod(t,7)/7;
 for(let r=0;r<5;r++){const is=r===chosen;g.save();g.globalAlpha=is?1:lerp(1,.27,sw);rect(gridX,gridY+r*rh,1140,rh-7,is?'#284332':'#162e28',8);text(String(r+1),gridX+24,gridY+r*rh+44,23,is?C.green:C.dim);for(let c=0;c<7;c++){const x=350+c*121+Math.sin(r+c)*12,y=gridY+r*rh+47;const idx=r*7+c;const scan=sw<.5?Math.floor(pass*35)===idx:is&&Math.floor(pass*7)===c;if(scan)rect(x-31,y-34,65,80,null,8,C.orange,3);zombie(x,y,.49,t*3+c); }g.restore();}
 vectorPlant(204,gridY+chosen*rh+70,.65,t*2);const shot=mod(t,2)/2,hitX=350+Math.sin(chosen)*12;
 if(shot<.8)pea(lerp(260,hitX,shot/.8),gridY+chosen*rh+34,10);
 else{g.save();g.globalAlpha=1-(shot-.8)/.2;circle(hitX,gridY+chosen*rh+34,16+(shot-.8)*100,null,C.green,3);g.restore();}
 const xx=1350;tag(sw<.5?'全池扫描':'本行候选',xx,300,360,sw<.5?C.orange:C.green);text(sw<.5?'35':'7',xx+180,447,120,sw<.5?C.orange:C.green,'center',350,700,mono);text('示意图访问槽位',xx+180,538,28,C.muted,'center');
 if(sw>.1){g.save();g.globalAlpha=sw;for(let i=0;i<7;i++){rect(xx+i%4*98,609+Math.floor(i/4)*77,78,54,C.panel,9,C.green,1);text(String(i),xx+i%4*98+39,636+Math.floor(i/4)*77,27,C.green,'center');}g.restore();}
 code(sw<.5?['for (z : allZombies)','  if (z.row == row) test(z);']:['for (z : rows[row])','  test(z);'],110,798,1125,sw<.5?1:0);
};
drawFns.mask=(t)=>{
 const active=[2,6,11,15],cycle=mod(t,8),n=Math.min(4,Math.floor(cycle/1.5));
 text('轨道状态',112,312,29,C.muted);text('activeMask',112,568,29,C.muted,'left',500,400,mono);
 for(let i=0;i<16;i++){const x=110+i*106;const a=active.includes(i);const done=active.indexOf(i)<n&&a;rect(x,363,92,118,a?'#304536':C.panel,9,a?C.green:null);text(String(i).padStart(2,'0'),x+46,392,24,C.muted,'center',90,400,mono);text(a?'工作':'空',x+46,445,27,a?C.green:C.dim,'center');rect(x,608,92,70,C.panel,8);text(a&&!done?'1':'0',x+46,644,42,a&&!done?C.green:C.dim,'center',90,600,mono);if(a&&!done&&active.indexOf(i)===n){arrow(x+46,598,x+46,493,C.green);glow(x+46,580,19);}if(done)text('✓',x+46,527,34,C.teal,'center');}
 code(['i = trailingZeroCount(activeMask);','activeMask &= activeMask - 1;','updateTrack(i);'],110,765,1700,Math.floor(cycle*2)%3);
};
drawFns.identity=(t)=>{
 const cy=mod(t,8)/8;
 tag('资源编译期',100,267,270,C.orange);tag('游戏运行期',1005,267,270,C.green);
 rect(100,360,495,250,C.panel);text('Zombie_walk',133,418,32,C.ink,'left',440,400,mono);text('anim_head',133,475,32,C.ink,'left',440,400,mono);text('anim_body',133,532,32,C.muted,'left',440,400,mono);
 animatePath([[611,481],[700,481],[700,381],[790,381]],mod(t/4,1),C.orange);tag('枚举键',790,353,155,C.orange);tag('生成表',790,500,155,C.orange);
 const x=1040,y=386;for(let i=0;i<6;i++){rect(x,y+i*59,678,51,C.panel,7,i===3?C.green:null);text(`${String(i).padStart(2,'0')}     ${['...','...','...','(type, trackId) → track 7','...','...'][i]}`,x+22,y+26+i*59,28,i===3?C.green:C.dim,'left',640,400,mono);}
 animatePath([[980,308],[1420,308],[1420,359],[1755,359],[1755,589],[1729,589]],cy,C.green);
 tag('键匹配校验',1260,774,270,C.teal);arrow(1538,796,1602,796,C.teal);tag('track 7',1621,774,180,C.green);
 code(['key = pack(type, trackId);','index = generatedLookup(key);'],100,754,970,cy>.5?1:0);
};
drawFns.batch=(t)=>{
 const merge=ramp(t,6,11),x0=133,n=12,step=138;
 text('原始绘制顺序',108,295,28,C.muted);
 for(let i=0;i<n;i++){const x=x0+i*step,add=i%3===1;rect(x,338,108,85,add?'#44382e':'#233b40',12,add?C.orange:C.teal,2);text(add?'A':'N',x+54,381,38,add?C.orange:C.teal,'center',100,700,mono);text(String(i),x+54,448,23,C.dim,'center');}
 text('兼容状态下的提交',108,536,28,C.muted);
 const width=lerp(108,1640,merge);rect(x0,584,width,141,'#20352b',13,C.green,2);
 if(merge<.99)for(let i=1;i<n;i++)rect(lerp(x0+i*step,x0,merge),584,lerp(108,1640,merge),141,null,13,`rgba(198,237,128,${1-merge})`,2);
 for(let i=0;i<n;i++){const x=x0+i*step,add=i%3===1;const py=lerp(510,640,ramp(t,1+i*.17,2+i*.17));circle(x+54,py,13,add?C.orange:C.teal);if(t>2+i*.17)line(x+54,478,x+54,py-25,C.dim,2,[4,7]);}
 text(merge<.5?'切换状态 → 分开提交':'统一混合规则 → 保持顺序合批',960,684,31,C.green,'center',1500,600);
 code(['append(sprite, blendParameters);','flushWhenStateIsIncompatible();'],110,785,1700,mod(t,4)<2?0:1);
};
drawFns.quad=(t)=>{
 const p=ramp(t,4,10),a=[[290,350],[660,350],[290,700]],b=[[728,385],[728,735],[358,735]];
 const end=[[290,350],[660,350],[290,700],[660,350],[660,700],[290,700]];
 const start=[...a,...b];const pts=start.map((v,i)=>[lerp(v[0],end[i][0],p),lerp(v[1],end[i][1],p)]);
 polygon(pts.slice(0,3),'#214142',C.teal);polygon(pts.slice(3),'#35442c',C.green);
 pts.forEach(([x,y],i)=>{circle(x,y,12,i<3?C.teal:C.green,C.bg,3);if(p<.8)text(String(i),x+(i%2?22:-25),y-26,26,C.ink,'center',60,600,mono);});
 if(p>.8){[[290,350,'0'],[660,350,'1'],[290,700,'2'],[660,700,'3']].forEach(([x,y,l])=>text(l,x-25,y-25,28,C.ink,'center',70,600,mono));}
 text(p<.5?'6 个独立顶点':'4 个共享顶点',492,809,39,p<.5?C.orange:C.green,'center');
 const xx=1000;tag('动态顶点上传',xx,310,530,C.teal);
 rect(xx,413,620,58,C.panel,10);rect(xx,413,620*lerp(1,2/3,p),58,lerp(1,2/3,p)>.9?C.orange:C.green,10);
 text(`${Math.round(lerp(240,160,p))} bytes`,xx,529,72,C.ink,'left',690,600,mono);
 const ix=[0,1,2,1,3,2];text('静态索引',xx,636,28,C.muted);ix.forEach((v,i)=>{const x=xx+i*114;rect(x,683,94,70,C.panel,10);text(v,x+47,718,34,C.green,'center',80,600,mono);});
 text('两组三角形仍然照常绘制',xx,819,30,C.muted);
};
drawFns.frames=(t)=>{
 const unit=2.4,round=Math.floor(t/unit),p=mod(t,unit)/unit;
 tag('主线程',100,275,240,C.teal);tag('GPU',1540,275,240,C.green);
 const sequence=[0,1,0,0,1,0,1,1];const skip=round%8===2||round%8===6,slot=sequence[round%8];
 const yy=[420,648];for(let i=0;i<2;i++){rect(600,yy[i]-50,650,135,C.panel,13,i===slot?(skip?C.orange:C.green):C.line,3);text(`帧槽 ${i===0?'A':'B'}`,632,yy[i]-5,35,i===slot?C.green:C.muted,'left',320,600);text('命令 / 顶点 / 索引 / fence',632,yy[i]+42,27,C.muted);text(i===slot?(skip?'未完成':'可提交'):'另一帧资源',1214,yy[i]-5,26,i===slot?(skip?C.orange:C.green):C.dim,'right');}
 animatePath([[330,415],[500,415],[500,yy[slot]],[590,yy[slot]]],clamp(p*2),C.teal);
 if(!skip){animatePath([[1265,yy[slot]],[1410,yy[slot]],[1410,415],[1570,415]],clamp((p-.4)*2),C.green);}else{line(1300,yy[slot]-18,1332,yy[slot]+14,C.orange,4);line(1300,yy[slot]+14,1332,yy[slot]-18,C.orange,4);text('跳过本次绘制尝试',1390,yy[slot]+60,28,C.orange,'center',600);}
 rect(1550,362,230,145,'#243d31',12);text('绘制',1665,431,42,C.green,'center');
 rect(600,811,650,80,'#24332d',12);text('共同引用：素材纹理 / 图集',925,851,32,C.green,'center');line(925,760,925,800,C.dim,2,[6,7]);
 circle(220,684,61,C.panel,C.teal,3);const a=t*3;line(220,684,220+43*Math.sin(a),684-43*Math.cos(a),C.teal,4);text('逻辑继续',220,786,31,C.teal,'center');text(`绘制尝试 ${round+1}`,138,547,30,C.muted);
};
// A deliberately simplified speed profile explains discrete integration.
// Its coordinates are diagram data, not game measurements. The next scene
// uses archived native results, separately labelled with their exact scope.
const legacyProfile=[40,140,30,100,55,145,35,80];
function legacyVelocityAt(seconds){return legacyProfile[Math.min(7,Math.floor(clamp(seconds,0,.999999)*8))];}
function legacySteps(hz){
 const points=[];
 for(let i=0;i<hz;i++){
  const a=i/hz,b=(i+1)/hz;
  points.push({a,b,velocity:legacyVelocityAt(b)});
 }
 return points;
}
drawFns['legacy-motion']=(t)=>{
 const cursor=mod(Math.max(0,t-4)/5,1),k=Math.min(7,Math.floor(cursor*8));
 const keys=[0];for(const velocity of legacyProfile)keys.push(keys.at(-1)+velocity/8);
 const first=Math.min(4,Math.max(0,k-1));
 tag('原版旧算法 · ground 提供运动参考，世界位置逐步累加',100,263,1720,C.orange);
 keys.slice(first,first+5).forEach((v,i)=>{const x=160+i*170;circle(x,361,12,first+i===k?C.green:C.dim);text(`K${first+i}: ${v}`,x,405,24,C.muted,'center',165,400,mono);if(i<4)arrow(x+19,361,x+150,361,C.line,2);});
 const difference=keys[k+1]-keys[k];
 tag(`相邻帧差  ${difference.toFixed(2)}`,980,326,340,C.orange);
 arrow(1338,348,1400,348,C.orange,3);tag('× 动画速率 → 速度',1420,326,400,C.orange);
 code(['// 原版 1073 旧算法 · 等价展开','k = frameFromPhase(phase);','v = (ground[k+1].x - ground[k].x) * animRate;','dx = v * deltaSeconds;','worldX -= dx;'],100,465,1720,1+Math.min(3,Math.floor(t/1.8)%4));
 [100,30].forEach((hz,index)=>{
  const x=145+index*940,y=750,w=740,h=145;
  text(`${hz}Hz 步长：每一步用一次取样速度`,x,y-33,27,index?C.orange:C.teal,'left',820,500);
  const color=index?C.orange:C.teal;
  g.save();g.beginPath();g.rect(x,y,w,h);g.clip();
  for(const step of legacySteps(hz)){
   const xx=x+step.a*w,yy=y+h-step.velocity/150*h;
   g.globalAlpha=step.a<=cursor?.48:.12;
   rect(xx,yy,Math.max(1,(step.b-step.a)*w-1),y+h-yy,color,0);
  }
  g.globalAlpha=1;
  for(let i=0;i<legacyProfile.length;i++){
   const yy=y+h-legacyProfile[i]/150*h;
   line(x+i/8*w,yy,x+(i+1)/8*w,yy,C.ink,2);
   if(i)line(x+i/8*w,y+h-legacyProfile[i-1]/150*h,x+i/8*w,yy,C.ink,2);
  }
  line(x+cursor*w,y,x+cursor*w,y+h,C.green,2);g.restore();
  line(x,y+h,x+w,y+h,C.dim,2);
  text('速度变化与取样面积示意 · 非实测曲线',x,y+h+25,22,C.muted,'left',820);
 });
};
const rootComparison=WEB_INPUTS.rootMotion;
function rootComparisonState(){
 const reference=rootComparison.samples[0].phaseX;
 return {reference,rows:[100,120,240].map(hz=>{
  const source=rootComparison.samples.find(row=>row.hz===hz);
  return {...source,error:source.legacyX-reference};
 })};
}
drawFns['motion-steps']=(t)=>{
 const {reference,rows}=rootComparisonState(),appear=ease(t/1.5);
 tag(`同一起点 x = ${rootComparison.startX}`,100,268,490,C.teal);
 tag(`相同动画速率 ${rootComparison.animationRate} · 相同运行时间 60s`,650,268,870,C.teal);
 const origin=820,scale=205;
 text('相对相位求值终点的偏差 / px（横向放大）',1040,363,29,C.muted,'center',1300);
 for(let i=0;i<=3;i++){
  const x=origin+i*scale;line(x,410,x,828,C.line,1,[5,6]);text(String(i),x,866,25,C.muted,'center',100,400,mono);
 }
 rows.forEach((row,i)=>{
  const y=484+i*133,x=origin+row.error*scale;
  text(`原版算术 · ${row.hz}Hz`,130,y,33,C.orange,'left',640,600);
  text(`累加终点 x = ${row.legacyX.toFixed(6)}`,130,y+42,25,C.muted,'left',640,400,mono);
  g.save();g.globalAlpha=appear;
  line(origin,y,x,y,C.orange,5);circle(origin,y,9,C.green);
  ReanimPlayer.draw(g,'Zombie','anim_walk',t,{x,y:y+23,height:83,rate:12});
  text(`+${row.error.toFixed(3)} px`,x+48,y+12,27,C.orange,'left',300,600,mono);
  g.restore();
 });
 line(origin,405,origin,827,C.green,3);
 text(`本项目相位求值参考：三个频率均到达 x = ${reference.toFixed(6)}`,130,909,29,C.green,'left',1700,500);
};

function phaseState(t){
 const period=1.25,frame=Math.floor(t/period)%5,fraction=mod(t,period)/period;
 const values=[0,-70,-150,-200,-285,-330];
 const base=Math.floor(t/(5*period))*values[5];
 const offset=lerp(values[frame],values[frame+1],fraction);
 return {frame,fraction,base,offset,worldX:base+offset,screenX:1010+(base+offset)*.56,cycleSeconds:5*period};
}
drawFns.phase=(t)=>{
 const {frame,fraction,base,offset,screenX,cycleSeconds}=phaseState(t);
 const x0=155,yy=424,dx=320;
 for(let i=0;i<6;i++){const x=x0+i*dx;line(x,390,x,450,i===frame?C.green:C.line,3);circle(x,yy,10,i===frame?C.green:C.dim);text(`K${i}`,x,350,27,C.muted,'center',100,400,mono);if(i<5)line(x+13,yy,x+dx-13,yy,C.line,4);}
 const px=x0+(frame+fraction)*dx;glow(px,yy,24);line(px,452,px,541,C.green,3);tag(`frame ${frame}  +  ${fraction.toFixed(2)}`,clamp(px-150,90,1510),550,300,C.green);
 line(145,768,1115,768,C.line,3);
 const walkClip=ReanimPlayer.definitions.Zombie.clip('anim_walk');
 ReanimPlayer.draw(g,'Zombie','anim_walk',t,{x:screenX,y:742,height:128,rate:walkClip.count/cycleSeconds});
 text(`基础位移 (${base})  +  插值位移 (${offset.toFixed(1)})`,143,838,29,C.ink,'left',980);
 arrow(1060,881,850,881,C.green,3);text('向左前进，位移为负',143,882,27,C.muted,'left',650);
 code(['age = nowNs - frameBirthNs;','n = age / dt;','r = age % dt;','frame = (frame + n) % count;','frameBirthNs += n * dt;','alpha = float(r) / float(dt);','value = lerp(key[f], key[next], alpha);'],1140,602,680,Math.floor(t*2)%7);
 tag('本项目 · 整数纳秒 + 关键帧插值',143,267,650,C.green);tag('关键帧示意 · 实际处理包含循环边界',1120,267,650,C.muted);
};
drawFns.trig=(t)=>{
 const a=t*.65,cx=410,cy=515,r=180,px=cx+Math.cos(a)*r,py=cy-Math.sin(a)*r;
 circle(cx,cy,r,null,C.line,3);line(cx-r-45,cy,cx+r+55,cy,C.dim,2);line(cx,cy-r-55,cx,cy+r+55,C.dim,2);line(cx,cy,px,py,C.ink,4);circle(px,py,10,C.ink);line(px,py,px,cy,C.teal,4);line(px,py,cx,py,C.green,4);text('cos θ',cx,cy+r+67,32,C.teal,'center');text('sin θ',cx-280,cy,32,C.green,'center');
 const bx=1065,by=365;tag('FastSinCos × 两个 skew 轴',950,272,730,C.green);
 const q=[[-140,-100],[140,-100],[140,100],[-140,100]],ang=Math.sin(t*.45)*.48;
 const transform=v=>[1390+(v[0]*Math.cos(ang)-v[1]*Math.sin(ang))*1.35,540+(v[0]*Math.sin(ang)+v[1]*Math.cos(ang))*1.35];
 polygon(q.map(transform),'#233e3b',C.teal);q.map(transform).forEach(([x,y],i)=>{circle(x,y,12,C.green);text(String(i),x+22,y-23,27,C.ink,'center',80,600,mono);});
 line(...transform(q[0]),...transform(q[2]),C.dim,2,[8,8]);
 animatePath([[680,515],[790,515],[790,765],[1390,765],[1390,710]],mod(t/3,1),C.green);
 code(['FastSinCos(skewX, sx, cx);','FastSinCos(skewY, sy, cy);','matrix = buildTransform(sx, cx, sy, cy);'],110,779,1080,Math.floor(t)%3);text('同一矩阵 → 四个角',1320,841,32,C.green,'center',600,600);
};
drawFns.tradeoff=(t)=>{
 tag('CPU · 轨道级复用',100,290,700,C.teal);tag('朴素顶点级实现 · 可能重复',1090,290,700,C.orange);
 rect(160,417,285,150,C.panel,14);text('变换 × 1',303,492,43,C.teal,'center',270,600);
 for(let i=0;i<4;i++){const y=399+i*106;circle(720,y+27,22,C.green);text(`角 ${i}`,778,y+27,27,C.muted);animatePath([[460,492],[560,492],[560,y+27],[689,y+27]],mod(t/3+i*.15,1),C.teal);rect(1140,y-15,480,88,C.panel,12);text(`顶点 ${i} → 变换`,1380,y+28,32,C.orange,'center',440);glow(1660,y+26,13,C.orange);}
 wrap('已有试验没有达到当前项目的性能门槛，保留 CPU 路径。',120,853,1680,32,C.ink,42);
};
function bar(x,y,w,value,max,label,color,showValue,p){text(label,x,y-30,29,C.muted);rect(x,y,w,48,C.panel,8);rect(x,y,w*value/max*out(p),48,color,8);text(showValue,x+w+22,y+24,40,color,'left',240,600,mono);}
drawFns.whole=(t)=>{
 const p=ramp(t,0,3);tag('平均绘制帧率',110,274,380,C.teal);bar(145,385,1160,43.95,240,'原版 1073',C.orange,'43.95 FPS',p);bar(145,548,1160,196.74,240,'pvz_perf · 240Hz 上限',C.green,'196.74 FPS',p);
 const rows=[['逻辑更新','100.00 Hz','196.76 Hz'],['进程 CPU / 单核 100%','90.87%','89.55%'],['CPU 时间 / 绘制帧','20.676 ms','4.552 ms']];rows.forEach((r,i)=>{const y=712+i*63;text(r[0],145,y,27,C.muted);text(r[1],968,y,31,C.orange,'center',300,600,mono);text(r[2],1390,y,31,C.green,'center',360,600,mono);});
};
drawFns.external=(t)=>{
 const p=ramp(t,0,3);tag('电脑 · 平均绘制 / 呈现 FPS',100,270,760,C.teal);tag('手机 · 平均整机功耗',1020,270,760,C.teal);
 bar(140,405,480,129.57,220,'精华版 · RTSS 呈现计数',C.orange,'129.57',p);bar(140,580,480,196.74,220,'本项目 · 游戏绘制计数',C.green,'196.74',p);
 bar(1050,405,450,2.657,3,'精华版 · 335.29 秒',C.orange,'2.657 W',p);bar(1050,580,450,2.438,3,'本项目 · 380.01 秒',C.green,'2.438 W',p);
 line(946,340,946,740,C.line,2);text('CPU 占用：96.41% / 89.55%',145,758,30,C.muted);text('不是进程独占功耗',1050,758,30,C.muted);wrap('相同阵型 / 相似出怪；非严格同局；不同 FPS 口径；手机温度不同且未同步测 FPS。',110,859,1690,27,C.muted);
};
const traces=WEB_INPUTS.traces;
function traceProgress(t){return ease(t/1.5);}
function axes(x,y,w,h,xMax,yMax,{color=C.muted,xTicks=4,yTicks=4,xLabel='采样时间 / s',unit='',rightMax=null}={}){
 for(let i=0;i<=yTicks;i++){const yy=y+h-i*h/yTicks;line(x,yy,x+w,yy,C.line,1);text((yMax*i/yTicks).toFixed(yMax<=5?1:0),x-12,yy,22,color,'right',70,400,mono);if(rightMax!==null)text((rightMax*i/yTicks).toFixed(0),x+w+12,yy,22,C.orange,'left',80,400,mono);}
 for(let i=0;i<=xTicks;i++){const xx=x+i*w/xTicks;line(xx,y,xx,y+h,C.line,1);text((xMax*i/xTicks).toFixed(0),xx,y+h+26,22,C.muted,'center',100,400,mono);}
 line(x,y,x,y+h,C.muted,1);line(x,y+h,x+w,y+h,C.muted,1);text(unit,x,y-23,23,color);text(xLabel,x+w/2,y+h+62,23,C.muted,'center',600);
}
function plotMeasured(points,{x,y,w,h,xMax,yMax,field,cursor,color,step=false,timeField=0,gapLimit=2,clip=true,showMarker=true}){
 if(clip){g.save();g.beginPath();g.rect(x,y,w,h);g.clip();}
 const selected=points.filter(r=>r[timeField]<=cursor);g.beginPath();let prev=null;
 for(const r of selected){const xx=x+r[timeField]/xMax*w,yy=y+h-r[field]/yMax*h;
  if(!prev||r[timeField]-prev[timeField]>gapLimit)g.moveTo(xx,yy);
  else{if(step)g.lineTo(xx,y+h-prev[field]/yMax*h);g.lineTo(xx,yy);}prev=r;
 }
 g.lineWidth=2.5;g.strokeStyle=color;g.lineJoin='round';g.lineCap='round';g.stroke();
 if(prev&&showMarker){const xx=x+prev[timeField]/xMax*w,yy=y+h-prev[field]/yMax*h;circle(xx,yy,5,color);}
 if(clip)g.restore();return prev;
}
function desktopPair(t,leftKey){
 const sets=[traces.desktop[leftKey],traces.desktop.project],maxTime=Math.ceil(Math.max(...sets.map(d=>d.duration))/50)*50;
 const fpsMax=Math.ceil(Math.max(240,...sets.flatMap(d=>d.points.map(r=>r[1])))/40)*40;
 const cpuMax=Math.ceil(Math.max(100,...sets.flatMap(d=>d.points.map(r=>r[2])))/20)*20;
 const cursor=traceProgress(t)*maxTime;
 sets.forEach((d,i)=>{const x=140+i*960,y=408,w=675,h=355;text(d.label,x+w/2,287,31,C.ink,'center',850,600);
  line(x,342,x+50,342,C.green,4);text('FPS',x+65,342,27,C.green);line(x+255,342,x+305,342,C.orange,4);text('CPU %（单核=100）',x+320,342,25,C.orange,'left',355);
  axes(x,y,w,h,maxTime,fpsMax,{color:C.green,unit:'FPS',rightMax:cpuMax});text('CPU %',x+w+10,y-23,23,C.orange);
  const now=plotMeasured(d.points,{x,y,w,h,xMax:maxTime,yMax:fpsMax,field:1,cursor,color:C.green});
  plotMeasured(d.points,{x,y,w,h,xMax:maxTime,yMax:cpuMax,field:2,cursor,color:C.orange});
  const cx=x+Math.min(cursor,d.duration)/maxTime*w;line(cx,y,cx,y+h,'#d8e7ed',1.5,[5,6]);
  if(now){text(`${now[0].toFixed(1)} s`,x,869,28,C.muted,'left',220,400,mono);text(`${now[1].toFixed(1)} FPS`,x+240,869,29,C.green,'left',260,600,mono);text(`${now[2].toFixed(1)}% CPU`,x+470,869,29,C.orange,'left',250,600,mono);}
 });
}
drawFns['desktop-traces']=t=>desktopPair(t,'original');
drawFns['external-traces']=t=>desktopPair(t,'ultimate');
drawFns['phone-frequency']=t=>{
 const sets=[traces.phone.ultimate,traces.phone.project],maxTime=Math.ceil(Math.max(...sets.map(d=>d.duration))/50)*50,cursor=traceProgress(t)*maxTime;
 line(112,268,162,268,C.orange,4);text('精华版',176,268,25,C.orange);line(341,268,391,268,C.green,4);text('pvz_perf',405,268,25,C.green);text('完整有效采样',1790,268,26,C.muted,'right');
 const titles=['CPU 0–1 · A520 · 2.27GHz 上限','CPU 2–4 · A720 · 3.15GHz 上限','CPU 5–6 · A720 · 2.96GHz 上限','CPU 7 · X4 · 3.30GHz 上限'];
 titles.forEach((title,i)=>{const x=145+(i%2)*940,y=385+Math.floor(i/2)*295,w=727,h=151;
  text(title,x, y-49,28,C.ink,'left',810,600);axes(x,y,w,h,maxTime,3.6,{unit:'GHz',yTicks:3,xLabel:'采样时间 / s'});
  sets.forEach((d,j)=>plotMeasured(d.points,{x,y,w,h,xMax:maxTime,yMax:3.6,field:3+i,cursor,color:j?C.green:C.orange,step:true,timeField:1,gapLimit:3}));
  line(x+cursor/maxTime*w,y,x+cursor/maxTime*w,y+h,'#d8e7ed',1,[5,5]);
 });
};
drawFns['phone-power']=t=>{
 const sets=[traces.phone.ultimate,traces.phone.project],maxTime=Math.ceil(Math.max(...sets.map(d=>d.duration))/50)*50;
 const yMax=Math.ceil(Math.max(...sets.flatMap(d=>d.points.map(r=>r[2]))));const cursor=traceProgress(t)*maxTime;
 sets.forEach((d,i)=>{const x=145+i*940,y=375,w=740,h=367,color=i?C.green:C.orange;text(d.label,x+w/2,292,34,color,'center',800,600);
  axes(x,y,w,h,maxTime,yMax,{color,unit:'整机功耗 / W'});
  plotMeasured(d.points,{x,y,w,h,xMax:maxTime,yMax,field:2,cursor,color,timeField:1,gapLimit:3,showMarker:t<1.5});
  if(t<1.5)line(x+Math.min(cursor,d.duration)/maxTime*w,y,x+Math.min(cursor,d.duration)/maxTime*w,y+h,'#d8e7ed',1.5,[5,6]);
  const s=d.summary;
  text(`时间加权平均  ${s.mean_device_w.toFixed(3)} W`,x,845,34,color,'left',800,600);
  text(`有效 ${s.discharge_seconds.toFixed(2)} s · ${s.battery_temp_c_start} → ${s.battery_temp_c_end} °C`,x,894,26,C.muted,'left',800);
 });
};
drawFns.closing=(t)=>{
 const labels=['数据查表','候选集合','编译期身份','提交复用','精确时间'];labels.forEach((s,i)=>{const x=130+i*350,yy=410+Math.sin(t*.7+i)*8;circle(x+124,yy,76,C.panel,C.green,2);text(String(i+1).padStart(2,'0'),x+124,yy,48,C.green,'center',130,600,mono);text(s,x+124,yy+139,33,C.ink,'center',310,600);if(i<4)animatePath([[x+213,yy],[x+383,yy]],mod(t*.45,1),C.teal);});
 text('先减少工作，再证明收益。',960,752,66,C.ink,'center',1660,700);repositoryLink();
};
function draw(time){
 time=clamp(time,0,total-1/10000);const s=scenes.find(s=>time<s.start+s.duration)||scenes.at(-1),t=time-s.start;
 repoLink.hidden=s.id!=='opening'&&s.id!=='closing';
 g.setTransform(1,0,0,1,0,0);g.globalAlpha=1;background(time);
 const enter=out(t/.55),exit=clamp((s.duration-t)/.35);g.save();g.globalAlpha=enter*exit;g.translate(0,(1-enter)*12);
 header(s,t);if(s.table)tableScene(s,t);else drawFns[s.id](t);g.restore();footer(s,t,time);
 const matrix=g.getTransform();if(!Number.isFinite(matrix.a)||!Number.isFinite(matrix.e)){const err=`non-finite at ${time}`;if(!finiteErrors.includes(err))finiteErrors.push(err);}
 return {id:s.id,time};
}
window.FILM={draw,scenes,duration:total,textWarnings,finiteErrors,traces,phaseState,rootComparisonState,legacySteps,repositoryUrl:REPOSITORY_URL,version:VERSION};
let playing=false,current=0,last=0;const seek=document.getElementById('seek'),play=document.getElementById('play'),clock=document.getElementById('clock'),chapters=document.getElementById('chapters');seek.max=total;
function format(t){return `${Math.floor(t/60)}:${String(Math.floor(t%60)).padStart(2,'0')}`;}
function update(){draw(current);seek.value=current;clock.textContent=`${format(current)} / ${format(total)}`;}
play.onclick=()=>{playing=!playing;play.textContent=playing?'暂停':'播放';last=performance.now();};
seek.oninput=()=>{current=Number(seek.value);update();};
scenes.forEach(s=>{const o=document.createElement('option');o.value=s.start;o.textContent=`${format(s.start)}  ${s.title}`;chapters.append(o);});
chapters.onchange=()=>{current=Number(chapters.value)+.7;update();};
document.addEventListener('keydown',e=>{if(e.code==='Space'&&e.target.tagName!=='SELECT'){e.preventDefault();play.click();}});
document.getElementById('track-bounds').onchange=e=>{ReanimPlayer.debugBounds=e.target.checked;update();};
function tick(now){if(playing){current+=(now-last)/1000;if(current>=total){current=0;playing=false;play.textContent='播放';}update();}last=now;requestAnimationFrame(tick);}requestAnimationFrame(tick);draw(.75);clock.textContent=`0:00 / ${format(total)}`;
play.disabled=true;ReanimPlayer.ready.then(()=>{play.disabled=false;draw(.75);}).catch(e=>{textWarnings.push(String(e));document.getElementById('clock').textContent='原版素材加载失败：'+e.message;});
})();
