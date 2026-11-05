# 性能测试工具

性能观测、微基准、性能夹具生成器和结果分析工具统一放在本目录。
tools保留生产构建／资源／转换工具及便捷的Release启动、采样入口；实际采样实现和独立基准在本目录。
本目录 build 下的可执行文件、DLL 和中间产物可重新生成，不进入 Git。
新采样输出默认写系统Temp，入账证据保存在Tests/evidence；游戏本身直接在Release运行。
统一无尽输入使用 Tests 根目录的 DAT/DAT64。

主要入口：

- run-linux-game-perf.py：原生 Linux 外部只读 FPS/CPU 采集，游戏所有线程绑定
  Linux CPU14（LP14），采集器绑定 Linux CPU15（LP15）。读取已有 mDrawCount，
  统计的是游戏绘制帧，不是显示器实际呈现帧。
- report-linux-game-perf.py：归档 Linux credits/无尽采样，仅分析 segment=game，
  生成无尽 FPS/CPU 时间曲线、区间 CPU ms/绘制帧及文件哈希；需要 matplotlib 与中文字体。
- run-windows-game-perf.py、windows_game_probe.py：外部只读采样游戏已有计数器及进程 CPU 时间。
- report-windows-game-perf.py：仅分析 segment=game，生成 FPS/CPU 时间叠加图、
  区间平均 CPU ms/绘制帧、原始记录与汇总；绘图依赖 matplotlib。
- analyze-credits.py：重算已人工确认的本轮 credits 共同区间；保留项目场景 5
  被误标 other-scene 的原始证据，另存 CPU/帧明细、汇总与 SHA256，不生成图片。
- measure-linux-process.py：原生 Linux 的进程 CPU 观测。
- game_perf_layout.cpp：用当前源码生成当前项目的内存字段偏移。
- fps_probe.cpp：窗口变化帧和呈现采样，不作为游戏精确帧计数。
- monitor_cpu_native.c：按 QPC 实际采样间隔计算进程 CPU 占用。
- bench_*.cpp、对应运行及夹具生成脚本：独立微基准。
- refresh-small-benchmarks.py：串行重测历史小基准，并提取当前生产表声明到相同夹具；
  历史模型和当前表声明分开登记，不把模型结果称为当前完整函数性能。
- run-pinned-benchmark.cpp：原生创建子进程，恢复执行前固定LP14；不通过cmd start拼接命令。
- retail_machine_code_probe.cpp：x86独立进程映射英文1073 EXE，验证纯叶函数的直接机器码调用。
- production_leaf_benchmark.cpp：同源编译x86原版机器码调用器和x64实际游戏查询，保留输入／返回值向量；原版地址只支持已校验英文1073 EXE。
- production_structure_benchmark.cpp：用当前真实对象类型核对轨道选择、行查询和四层格桶；选择算法桩与完整业务更新成本分别标记。
- build-production-benchmark.py：复用现有Release对象和LTO链接独立夹具，输出到Temp；预检并复制x64 BASS，缺失时可显式传入`--bass-dll`，不改游戏EXE。
- run-production-small-benchmarks.py：原版—当前—当前—原版串行采样，再测两个真实结构进程，核对每项五轮checksum和完整结果向量；不会将x86／x64差值当成纯算法提速。
- production_frozen_draw.cpp：恢复归档固定DrawScreen场景，128次预热、640次CPU绘制采样；flush／fence／readback在计时区间之外。
- zombie_frame_cursor_benchmark.cpp：历史独立候选实验，包含已经否决的倒数缓存和旧模拟布局；不登记为当前生产路径，原始证据保留。
- reanim_frame_cache_benchmark.cpp：v4实际RootMotion／GetFrameTime查询与时钟推进分开计时；冻结v3.6算术模型独立标记，包含真实Zombie根位移基准及无界面异常退出。当前Reanimation在回绕时累加基础位移；采样返回的cycles仅为相对缓存帧的临时回绕量，不能当作累计周期。
- zombie_root_motion_benchmark.cpp：共享帧缓存之前的原速度累加算术与RootMotion对照，保留为历史诊断；其手动修改lastUpdate的查询不代表当前逐步推进的生产热路径，预解析轨道也不替代实际API。完整证据见`Tests/evidence/zombie-root-motion-20261029`。

机器码调用器须用MSVC x86编译`production_leaf_benchmark.cpp`，定义`PVZ_RETAIL_MACHINE_CODE`，
链接`/BASE:0x10000000 /DYNAMICBASE:NO`；可执行样例命令见
[原版构建记录](../evidence/production-small-benchmarks-20261029/build-retail.cmd)。
当前两个夹具通过`build-production-benchmark.py --build out/x64-win32-vulkan --source <源码> --output <Temp目录>`生成，
随后把两个当前EXE、原版调用器、已核验原版EXE及同树PAK传给`run-production-small-benchmarks.py`的
`--current-probe`、`--structure-probe`、`--retail-probe`、`--retail`、`--pak`和新的`--output`目录。
正式采样期间不并行构建／运行其他基准，不恢复已退役运行路径以填充指标。

正式性能测量仍由主对话串行执行，Windows 使用 LP14。
Windows 观测器直接执行构建目录已有的 pvz_game_perf_layout，从其输出生成字段偏移；
准备和启动测量不会触发 CMake。布局工具须与被测程序对应，源码布局改变时在测量前更新。
游戏界面由用户操作：第一次 Enter 开始观测，第 3 秒启动或恢复场景，
默认持续到游戏 Quit／进程退出才结束观测，再次 Enter 不会停止；只有显式设置时长上限才提前结束。
旧夹具的存档哈希若与 Tests 的当前基准不同会直接拒绝。
正式采样必须显式指定 --multiplier 11x 或 25x；工具不修改倍频，
也不将构建期间的解禁频率自动记成锁频测量。prepare-only 不要求倍频标签。
启动游戏不传 -nosound。runner 读取原版注册表的音乐、音效和静音设置，
将同组音量写入项目的独立设置文件，并保存两侧配置证据。
samples.csv 保存有效区间的平均 CPU ms/绘制帧，measurement.json 保存累计 CPU 时间、
绘制帧数与二者之比；不代表逐帧独立计时。
跨程序对拍须对齐声音条件，未对齐的历史记录必须注明，不能发布严格同条件提升百分比。
旧微基准的历史输入、编译器安装路径和方法在各脚本中明确记录，
没有迁移成当前版本实玩 FPS 结论。

手动观察可使用 `observe-windows-game.py`，传入当前程序与配套布局工具，
通过 `--appdata <测试存档目录>` 和 `--settings-file <测试设置文件>` 隔离数据。
程序仍绑定 LP14，采集器使用其他 CPU；工具不调整倍频，未指定锁频标签的观察
不作为正式锁频测量。默认结果写入系统 Temp，图表适应超过 100 FPS 的样本。
`--no-open-chart` 只保存图表，不打开查看器。
布局工具输出 `boardCounterBytes`，现代关卡纳秒时钟按 64 位读取，
原版及旧布局保留 32 位计数。运行前需重新构建 `pvz_game_perf_layout`，
并确保它与被测程序对应。

## 原生 Linux 无尽采集

先构建与被测程序配套的布局工具和校验清单，两套后端分别准备：

```sh
cmake --build out/x64-x11-vulkan --target pvz_linux_perf_tools --parallel 8
cmake --build out/x64-wayland-vulkan --target pvz_linux_perf_tools --parallel 8
```

不修改游戏计数器，不注入代码，不写进程内存，也不需要 sudo。
运行器作为游戏父进程，满足默认 Linux ptrace 权限限制；不能改为先手动启动游戏
再让无关进程读取。启动采集不会自动重新编译，程序或布局工具哈希变化时拒绝测量，
要求重新构建上述目标。

准备好玩家 ID1 的全通关 DAT64 后，正常退出所有游戏，再在终端执行其中一项：

```sh
./out/x64-x11-vulkan/Release/measure-endless.sh
./out/x64-wayland-vulkan/Release/measure-endless.sh
```

每次运行都会验证 Tests/manifest.json 中的无尽基准哈希，然后覆盖实际存档目录的
game1_58.dat64，保证从同一关卡状态开始；users.dat 与 user1.dat64 不会被运行器替换。
默认存档目录为 ~/.local/share/PlantsVsZombiesModern，可用 PVZ_APPDATA_DIR 指定独立目录。
不要把 PVZ_APPDATA_DIR 指向仓库 Tests/，后者必须保持只读基准用途。
启动前可核对 Linux CPU14/15 的拓扑；编号不自动证明与 Windows 同名 LP 对应。

进入生存无尽并准备恢复后，回到终端按 Enter，随后 3 秒内切回游戏、恢复关卡。
默认不限制采集时长，退出游戏结束采集。启动后 8 秒开始等待 Enter；
--seconds 可设置可选的时长上限，--warmup 可调整启动等待。菜单、暂停、失焦、其他模式和
棋盘切换区间保留原始记录，但不进入有效区间汇总。菜单烟测不算正式游戏性能结果。

输出位于对应构建目录的 measurements/<场景>-<时间>/：samples.csv 同步记录
FPS、update Hz、单核 CPU 百分比、CPU ms/绘制帧、波次和游戏计数器，
measurement.json 记录程序/布局/配置/关卡哈希、亲和性、CPU 拓扑与频率信息。
CPU 时间来自 /proc 的进程累计 user+system tick，默认每秒采样，存在 tick 量化。
有效区间的 FPS、CPU 和 CPU ms/帧按真实时间和帧数加权，不能当作逐帧 CPU 计时。
工具只设 CPU 亲和性，不锁倍频，不将 scaling_cur_freq 当作锁频证据。

音频或驱动库可能放宽新线程的继承亲和性；运行器在初始化后及采样前逐线程锁定
CPU14，采样期间若发现任一线程离开 CPU14 则拒绝本轮，不静默纠正后继续出成绩。

生成与总账配套的分析（输出目录必须尚不存在；中文字体可用 --font 指定）：

```sh
python3 Tests/performance/report-linux-game-perf.py \
  --credits out/x64-wayland-vulkan/measurements/credits-20261018-014530-612976 \
  --endless out/x64-wayland-vulkan/measurements/endless-20261018-014936-174820 \
  --destination /tmp/pvz-linux-report --max-ratio 25x
```

脚本核对原始采样、场景、模式、亲和性和测量汇总，保留原时间轴、剔除区间缺口，
不平滑、不生成逐帧独立耗时。归档中的事后硬件/设置快照单独注明采集时点，
不得用它们倒推整轮频率、音量或缩放开关状态。
--max-ratio 仅登记用户已确认的最高倍频限制，允许降频，不等于全程恒定频率，
也不改写采集器原始元数据。本轮用户确认的上限为 25x。


## 桌面 Vulkan 精灵 GPU 实验（显式运行）

`sexy_vulkan_sprite_benchmark` 是 Windows 的 `EXCLUDE_FROM_ALL` 目标，
链接带专用实验宏的独立 renderer 库，不替换正式库或游戏。
用匹配编译器的 VsDevCmd 环境构建，避免只找到 cl 而缺少配套标准库头文件：

```powershell
cmake --build out/x64-win32-vulkan --config Release --target sexy_vulkan_sprite_benchmark
$bench = 'out/x64-win32-vulkan/SexyAppFramework/graphics/vulkan/sexy_vulkan_sprite_benchmark.exe'
& $bench --vendor=0x10de --validate "--output=$env:TEMP/pvz-sprite-validation"
& $bench --vendor=0x10de --samples=128 --rounds=8 "--output=$env:TEMP/pvz-sprite-timing"
& $bench --vendor=0x8086 --indexed-quads --validate "--output=$env:TEMP/pvz-quads-intel-validation"
& $bench --vendor=0x8086 --indexed-quads --samples=128 --rounds=8 "--output=$env:TEMP/pvz-quads-intel-timing"
```

默认实验 B 使用精简生成图像 shader；`--indexed-quads` 则让 B 保留原 shader，
改用四顶点和静态索引。A 始终使用原路径。`--validate` 只做像素／同步校验，
计时运行不启用 validation。CSV 的 `gpu_scene_us` 是 GPU timestamp 区间；
`host_submit_calls_us` 只是主机录制调用的 wall time，不是进程 CPU 时间，
不含 BeginScene／EndScene，不能相加成完整帧时间。
输入为固定合成精灵，不能替代完整无尽关卡或手机实测。
原始输出写系统 Temp，未锁频结果不写入正式性能总账。
实现、当前实验边界和正确性记录见
[GPU 专项](../../docs/gpu-submission.md)。

额外互斥实验选项：`--compare-index-widths` 比较 16／32 位索引；
`--baked-filters` 配合 `--bake-format=rgba16f|rgba8|rgba8-manual` 比较派生滤镜；
`--trim-input=PATH` 读取损失无关的 BGRA 裁切夹具；`--packed-color`／
`--packed-color16` 比较顶点 RGBA8／RGBA16_UNORM；`--discard-scene` 仅用于
开头完整不透明覆盖的合成场景，比较 LOAD／DONT_CARE。
这些都不是正式游戏的运行时回退路径。先运行 `--validate`，不通过像素门槛的候选不计时。

`--instanced-quads` 比较四顶点与 88 字节矩形实例，额外检查 CPU 旋转和五万实例，
保持原绘制次序、混合及 16 位索引拆分；四角染色／采样元数据须相同。
场景入口按已知用途收窄同步范围已进入 Vulkan 1.3 生产路径，
原 `--tight-scene-barriers` 实验开关已退役。实例实验仍互斥于其他实验，
正式库和 Android 不编译实例实验开关或 shader。

`--native-instances` 与 `--instanced-quads` 使用相同 shader／数据格式，但调用端
直接生成 88 字节矩形，避免四顶点临时整理，仅绘制主场景。
`--present-copy` 比较多一道最终同尺寸 SDR 采样 pass 与直接输出场景，
对比最终目标像素，包含背景独立场景；它不运行 WSI，不是完整直接呈现验收。

`--solid-fill` 在普通精灵场景末尾覆盖五档透明度的全屏白色矩形，比较现有纹理／
dual-source 混合与不采纹理的独立预乘 normal 混合；候选多一次 draw。
它只验证纯色路径，未接入手机。Vulkan 的未消费顶点输出 warning 保留在原始日志中。

`--separate-blend` 比较双源 normal/additive 合批与单输出预乘、按混合状态分批。
双方保留相同采样和预乘算术，Opaque 与通用图片路径不变，均使用四顶点／16 位索引。
六个场景覆盖小图／大面积重叠、每 1／16／256 张切换混合状态，记录实际 draw、
triangle、host Submit calls 和 GPU timestamp。先做像素和同步验证，再运行计时。
桌面计时若同时存在其他游戏进程，必须标记潜在干扰，不作为正式性能收益。

Android 可用默认关闭的 `PVZ_VULKAN_SEPARATE_BLEND_TEST` 编译开关生成独立测试 APK，
只改变 sprite 混合路径，不启用整个桌面实验库。打包需保留该次构建的 .so、PAK 和
`game_sprite_single_output_ps.spv`；随后关闭开关并重建正常输出。
换 shader 包须清除设备管线缓存；同场景对照还须记录性能模式、悬浮监控状态，
不能把监控引起的调度升频归因于代码改动。


## 当前Windows Vulkan Release复测

启动`tools/run-endless-modern.cmd`，然后运行`pwsh -File tools/measure-release-vulkan.ps1`。
不指定PID时自动匹配唯一的Vulkan Release进程，并核对亲和性0x4000。
在控制台按Enter开始，3秒后恢复战斗；默认没有180秒上限，也不会因再次Enter停止。
直到游戏Quit／进程退出才结束并写入汇总，采样器不会主动关闭游戏。
如需短诊断，必须显式传入`-Seconds`；这类记录不能当作完整一轮。
历史算法／固定Draw小指标继续保留用于3.0对照，整局新结果见docs/perf-fix-ledger.md。


单组当前记录复用`report-windows-game-perf.py <measurement目录> <证据目录> --single --label <图标题>`。
旧双组入口和新单组入口共用同一绘图函数；上图叠加FPS与CPU不同颜色曲线，下图CPU毫秒／绘制帧，非game样本留空。


## 当前生产微基准整组复测

`refresh-production-suite.py`先构建独立夹具，再串行运行生产查询、真实结构、帧缓存、轨道身份、粒子侧表、真实FastMath和固定DX12 CPU绘制，最后复测13项当前表声明与17项粒子身份契约。显式传入`--multiplier 11x`、原始`--archive`、`--emitter-fixture`与已校验的`--retail-probe`。输出目录必须不存在；不在测量时并行构建或渲染视频。

`reanim_identity_benchmark.cpp`对当前全部2641条合法轨道查找先穷举验证，再比较整数ID线性扫描与生产MPHF接口。它不是原版字符串API基准。植物分派模型桩适配当前三时钟参数，但保留同一输入和Mix工作，不将时间驱动业务算入该选择模型。
