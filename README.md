# pvz_perf

以零售 1073 的游戏行为为基准，优化 CPU 热路径、资源装载和原生呈现。
文档维护当前有效机制与修复；现行实测和历史算法证据分开标注，不按日期追加开发过程。

发行下载：[GitHub Releases](https://github.com/YuHao2006/pvz_perf/releases)。
**当前版本 v3.3.1，沿用 v3.0 引入的 DAT64 v4：v1.x/v2.x 的旧存档与旧 PAK64 不能直接混用。**
v3.3.1 提供Win10 Vulkan 1.3、Win7 OpenGL 3.3、Android 14 Vulkan 1.3及Android 7 Vulkan 1.0整包，以及完美用户档与统一无尽基准。
完整解压到新的可写目录后运行 `pvz_perf.exe`。
游戏只读取 **DAT64 v4 / schema 0x10745**；原版 DAT、DAT64 v1/v2 需使用附带转换器升级。
未发布的 v3 系列中间存档不受支持。用户摘要 `users.dat` 和玩家详情 `PVZUSR64` v13 独立管理。
资源采用当前静态定义和离线图集，EXE、shaders 与 PAK64 必须成套更新。
本次修复见 [v3.3.1 发布说明](docs/releases/v3.3.1.md)，存档迁移背景见 [v3.0 发布说明](docs/releases/v3.0.md)。
生成的 PerfTest 完美用户档包含全解锁进度，DAT64 v4 无尽档来自仓库原版基准；不包含个人进度或设置。
普通生存完成记录为 5 旗，困难生存为 10 旗。
**Android v3.3.1 暂不开放 HDR；Win7 统一 SDR，不支持 HDR；DX9/OpenGL 的默认配置不启用自定义缩放效果。**
Windows 游戏使用 `/MD`，发行包不携带 CRT DLL，运行机器需安装兼容的 VC++ x64 运行库；
存档转换器使用 `/MT`。
Linux 使用通用 x86-64 构建基线，实际 glibc、GLIBCXX、窗口及音频依赖要求
见各发行包 README 和依赖清单，不承诺在所有 Linux 发行版直接运行。
Linux 依赖与外部效果许可随包提供，系统图形、窗口、音频运行库由用户安装。
贡献通过 fork 和 Pull Request 提交，由维护者审查，见 [参与开发](CONTRIBUTING.md)。

- [文档索引](docs/README.md)：当前架构、验证、原版调查与历史发布说明。
- [运行时](docs/runtime.md)：时间、运动、曲线、粒子及静态定义。
- [验证说明](docs/validation.md)：验收入口、实际覆盖及平台限制。
- [性能总账](docs/perf-fix-ledger.md)：受控测量、落地机制与收益范围。
- [修复总账](docs/fix-ledger.md)：触发条件、根因、修复和验证。
- [存档转换](docs/DAT-DAT64-CONVERSION.md)：DAT/DAT64、模式编号和图像 ID。
- [无尽性能基准](Tests/README.md)：原版 DAT 与项目 DAT64 的同状态存档、校验值及测试使用规则。
- [资产与发行边界](docs/ASSETS.md)：源码、编译输入、资源、外部效果及现有工具链缺口。

## 当前源码的存档协议

当前源码只直接读写 **DAT64 v4（major4/minor0/schema0x10745）**。独立转换器只支持原版1073 DAT、DAT64 v1、v2和v4；整个v3系列的输入与输出均已删除。原版／v1／v2先用 `--target dat64-v4` 转到新文件，不能改头部或扩展名混读。历史v2.0包的协议见其独立发布说明，详见[存档转换](docs/DAT-DAT64-CONVERSION.md)。

## 构建与运行

每个 CPU、窗口、图形 API 组合只使用一个构建目录，正式配置为 Release/LTO。
游戏输出名为 `pvz_perf.exe` 或 `pvz_perf`，内部 CMake 目标仍是 `LawnProject`。

| 组合 | 构建入口 | 程序目录 |
| --- | --- | --- |
| Windows 7 x64 / Win32 / DX9 | `tools/build-win7-opengl.ps1 -RenderBackend D3D9` | `out/x64-win7-d3d9/Release/` |
| Windows 7 x64 / Win32 / OpenGL 3.3 | `tools/build-win7-opengl.ps1` | `out/x64-win7-opengl/Release/` |
| Windows x64 / Win32 / Vulkan（推荐） | CMake：`PVZ_RENDER_BACKEND=VULKAN` | `out/x64-win32-vulkan/Release/` |
| Windows x64 / Win32 / DX12 | `tools/build-d3d12-game.ps1` | `out/x64-win32-dx12/Release/` |
| Linux x64 / X11 / Vulkan | `sh tools/build-linux-game.sh x11` | `out/x64-x11-vulkan/Release/` |
| Linux x64 / Wayland / Vulkan | `sh tools/build-linux-game.sh wayland` | `out/x64-wayland-vulkan/Release/` |

Linux 构建脚本按本机架构选择 x64 或 arm64；依赖 vcpkg、编译器、CMake、
Ninja、Vulkan shader 工具以及对应窗口开发库。跨架构不能复用 CMakeCache。
Windows 发行优先推荐 Vulkan，保留 DX12；没有为 DX12 Ultimate 单独创建后端。

当前开发中的 Vulkan 后端要求 **Vulkan 1.3**、Synchronization2 和所需的
descriptor indexing 功能／限额，旧 Vulkan 兼容路径尚未实现；这不改变已发行 v3.0 包的要求。
内置 shader 按 Vulkan 1.3 编译并校验。屏障与队列提交使用 Synchronization2，
主帧轮换两套命令／同步及动态几何资源；复用槽位前非阻塞检查fence，未完成时跳过本次绘制，逻辑继续更新。素材纹理与图集不复制。

Android 的 APK ABI 为 `arm64-v8a`，PAK64 内置于 APK，无需另外复制数据包或安装 Java。
新包默认以 Armv9-A 为指令集基线，使用 `-march=armv9-a+nosve+nosve2`，
不要求手机内核暴露 SVE/SVE2；保留 NEON。NDK r27 Clang 在关闭 SVE2 后仍定义其功能宏，
构建同时取消这个错误宏，避免依赖库误选 SVE2 路径。Armv9 包的 ABI 目录仍是 `arm64-v8a`，
不能据此判断其指令集基线，也不保证 Armv8 设备能够运行。
旧包可用 `-DPVZ_ANDROID_CPU_BASELINE=armv8-a` 单独重建；当前 Vulkan 后端仍要求 1.3，
这个 CPU 配置不代表旧 Vulkan 降级路径已经完成。已有 Android 构建树切换方式：

```powershell
cmake -S . -B out/android-arm64 -DPVZ_ANDROID_CPU_BASELINE=armv9-a
cmake --build out/android-arm64 --config Release
```

当前 manifest 的最低版本为 API 26，但现有原生构建树使用 `android-29`，
因此当前产物按 **Android 10 / API 29 及以上**看待，尚未验证 Android 8/9。
电脑可使用 GraalVM JDK 25 配合 Gradle 9.2.1 构建；这里使用其 JDK，
不使用 Native Image 替代 Android DEX 或 NDK C++ 编译。
原生 Release 构建完成后，运行：

```powershell
./tools/build-android-apk.ps1 -JavaHome 'C:\Program Files\Java\graalvm-jdk-25.0.3+9.1'
```

APK 输出到 `out/arm64-android14-vulkan1.3/`；未配置发行签名时输出文件标记为 `-unsigned.apk`，
需签名后安装。脚本仅在子构建期间设置 JDK 和短 AF_UNIX 临时目录，结束后恢复环境变量。
测试 APK 内置完美用户档和由原版转换的统一泳池无尽基准，首次启动且不存在用户档时
自动初始化。更新安装保留已有进度，不重新补入已消费的关卡存档。

源码目录可以命名为 `pvzPerf`，文件夹名称不参与应用身份。
改名或搬迁后重新配置并构建，不复用旧路径的 CMakeCache、Ninja 文件和 PDB。
`out/` 与本机 vcpkg 安装目录不进入版本库。
正常构建的编译器、DSL、字体、图片和音频输入均位于本仓库，
不再要求相邻 openPvz 或原版安装目录。

游戏源码位于 `pvzPerf/`；公共声明通过 CMake 的 `pvz_source_headers`
接口目标共享，不增加 DLL 或运行时分派。资源编译器位于
`tools/resourceCompiler/src/`，存档转换器位于 `tools/saveConverter/src/`。
主 CMake 自动生成源码及数值定义，PAK64 先写入
`out/<组合>/cache/resources/`，再部署到对应 `Release/`。
性能工具集中在 `Tests/performance/`，生产编译和打包工具留在 `tools/`。
离线图集的已完成搜索缓存保存在 `assets/definitions/atlas-cache/`，
按图片尺寸、边距、模型及求解器版本验证后复用；中断检查点不会当作已完成结果。
当前资源配置给每张图集 60 秒求解预算，超时保留最佳合法排布并如实记录未证明最优。
主项目同时构建本平台的存档转换器：Windows 生成 `data/pvz_dat64_converter.exe`，
Linux 生成 `data/pvz_dat64_converter`。每次构建都检查并部署转换器，
即使游戏没有重新链接也会更新；安装目标同样放入 `data/`。

同机原生 Ubuntu 可直接读取本工作区并复制到 Linux 文件系统构建，不必先上
GitHub。源码还存在未提交改动时，仅克隆旧提交不能取得全部当前工作。
WSL 可以验证编译与无窗口测试，不能替代原生 Ubuntu 的 GPU/桌面性能验收。

Windows 的 `start-lp14.cmd` 以亲和性 `0x4000` 启动游戏。
Linux 构建目录里的 `start.cmd` 是 Windows 调用 WSL 的入口，原生 Ubuntu
直接运行 ELF。应用图标取自原版多尺寸图标；Linux 应用分组使用
`pvz_perf.desktop`、同名图标及 `pvz_perf` app_id/WM_CLASS。
desktop 文件安装后要求程序或启动器位于 PATH。

## 资源与配置

发行运行目录包含主程序、`main.pak64`、`effects/`、
`scale.json`、本平台运行库及图标。正式分发使用干净 staging，
仅附指定测试存档，不把个人存档、旧对照程序、烟测程序和历史日志混入发行包。
Vulkan、DX12 和 DX9 的内置 shader 位于 `shaders/`；OpenGL 内置 GLSL 随 EXE 分发。

| 资源 | 位置与规则 |
| --- | --- |
| reanim / particle / font 数值定义 | pak 内 `compiled/<类别>/<名称>.compiled` |
| 数值资源清单 | pak 内 `compiled/resources.compiled` |
| 内置 DXIL/SPIR-V | 构建期生成，包外 `shaders/` |
| 用户缩放 HLSL/HLSLI/DDS | 包外 `effects/`，启动加载时编译 |

根目录 `scale.json` 是默认值，构建只在程序目录缺失该文件时补入，不覆盖
用户配置，游戏不回写它。窗口为一次 Anime4K 2 倍放大；全屏为两次 2 倍
放大后经 SSimDownscaler 按 `fit` 保持比例输出，`overSharp=1`。
自定义缩放仍由游戏选项开关控制。Linux HDR 尚未实现，默认关闭。
Win7 组合使用独立的空效果链默认值；随包 effects 用于保留外部资源和来源说明，
不表示 DX9/OpenGL 已支持 Magpie 的 compute 效果。
`settings.json` 按游戏设置写回；改名不迁移现有存档目录。

原生 Linux 的数据目录依次由 `PVZ_APPDATA_DIR`、XDG 数据目录或
`$HOME/.local/share/PlantsVsZombiesModern` 决定，保留旧名称以保留进度。
显式 `PVZ_SETTINGS_FILE`、`PVZ_LOG_DIR`、`PVZ_SCALE_CONFIG_FILE`
可用于隔离测试。不要把 WSL 的 `./data` 启动约定当作原生 Linux 默认值。
v1.1 Linux 发行包的 `start.sh` 显式选择随包 `data/`，可直接使用 PerfTest 和无尽基准，
并尊重用户提供的 `PVZ_APPDATA_DIR`；直接运行 ELF 时仍采用上述原生目录规则。

## Linux 运行库

当前直接依赖包括 glibc、C++ 运行库、Vulkan loader、OpenAL Soft、DXC/DXIL、
BASS，以及 X11 或 Wayland/xkbcommon/libdecor 客户端库。
Linux 桌面错误对话框使用系统的 `zenity`（GNOME）或 `kdialog`（KDE）；
启动失败、缺少缩放效果或资源错误会显示诊断，不依赖游戏窗口或 Vulkan 初始化成功。
没有显示会话或两项工具均不可用时，仍保留 stderr/日志与失败退出码。
BASS 只负责解码，Linux 声音输出由 OpenAL Soft 完成；Windows 音效使用
DirectSound，平台代码分离。

`tools/package-linux-runtime.py CLEAN_RELEASE NEW_OUTPUT` 在 Linux 上将可信
ELF 的递归依赖收集到 `env/`，使用配套 loader/libc 启动，生成哈希和来源清单。
输入需要主程序、BASS、资源包和 shader，输出必须是新目录。
`env/shaders -> ../shaders` 保持显式 loader 启动后的资源定位。

该工具的结果仍为候选环境：动态音频后端、libdecor 插件、键盘布局等数据
不在 ELF 闭包内；Vulkan ICD 和显卡驱动来自目标机器。
`-march=native` 也保留构建机指令集要求，因此不能承诺任意 Linux/任意 CPU。
打包回归为 `python3 tools/test-package-linux-runtime.py`。

## 诊断与性能采样

Linux 使用 `Ctrl+Shift+D` 切换诊断模式，避开桌面环境的 `Ctrl+Alt+D` 显示桌面绑定；
Windows/macOS 保留原版 `Ctrl+Alt+D`。Linux 开关每次实际按下只切换一次，
按住不会反复切换，组合键也不会继续传入游戏操作或文本输入。
启用后按 `F3` 显示帧率，`Shift+F3` 切换指标。

游戏内 Smoke 入口、逐帧计时、提交计数、临时日志和 shadow 对拍代码已从生产源码移除。
独立回归测试与外部只读观测工具保留在测试目录，不向游戏注入探针。
原版 TodLog、错误/崩溃处理和官方诊断功能保留。

Windows 实玩对照双方为原版 1073 与当前项目，均用 **core LP14（`0x4000`）**；
**11x 用户实机确认，25x 尚待切换核实**，不换算具体频率。
亲和性只选择 CPU；观察者避开被测核，不与游戏抢同一个核。
credits 对齐播放区间，无尽使用同源存档、阵型、波次、尺寸和缩放设置，
预热后 ABBA，正式区间至少 120 秒，不并行构建或运行别的基准。
25x 的 credits 与统一无尽 CPU/FPS、平均每帧 CPU 成本已记录在
[性能总账](docs/perf-fix-ledger.md)，包含数据口径、证据和限制；11x 实玩测试已取消。
现行固定 Draw 仅列当前项目值；其他未重测子项明确保留为历史证据、当前值待更新，
不以现代项目冻结旧版作为现行基线。原版算法到已落地算法的确切历史收益仍保留。

| 工具 | 数据口径 |
| --- | --- |
| `Tests/performance/monitor_cpu_native.c` | QPC 实际间隔内的进程 user+kernel CPU 时间，单核满载为 100% |
| `Tests/performance/fps_probe.cpp` | 窗口变化帧及桌面呈现采样，不等于游戏精确 FPS/update/s；窗口必须前台 |
| `Tests/performance/measure-linux-process.py` | 原生 Linux 进程 CPU，逐线程核对亲和性，明确拒绝 WSL |

采样采用起点加绝对截止时间，迟到不累积漂移、不补发假样本。
唤醒仍有调度抖动，报告使用真实间隔。Linux CPU 编号先按拓扑核实，
不能假设 Linux CPU14 自动等于 Windows LP14。

```sh
taskset -c N ./out/x64-x11-vulkan/Release/pvz_perf
python3 Tests/performance/measure-linux-process.py --pid PID --cpu N \
  --scene credits --seconds 120 --output /tmp/pvz-credits-run1
```

无尽改为 `--scene endless`，Wayland 改用其构建目录；两种窗口系统串行测量。
该采样器不推算 FPS，帧率须由同轮独立采集或人工诊断显示补充。
原生 Ubuntu Wayland/Vulkan 已完成 credits 与统一无尽的 FPS/CPU 独立采集，
结果及时间曲线见[性能总账](docs/perf-fix-ledger.md)。新入口为
`Tests/performance/run-linux-game-perf.py`，配套构建与运行说明见
[性能工具](Tests/performance/README.md)。本轮锁 25x 最高倍频上限（用户确认），X11 尚未实测；不能将其直接当作
与 Windows 同核同频的性能对照，也不能把编译通过或 WSL 数据写成实机性能通过。

总账直接引用的精选原始记录位于 `Tests/evidence/`，历史对照夹具位于
`Tests/reference/`；两者均不作为当前发行包或现代项目冻结性能基线。
根目录 `log/` 只用于临时输出，可以整目录清理；新测试需要时会重新创建。

## 来源

This product includes portions of the PopCap Games Framework,
(c) 2005-2009 PopCap Games, Inc. All rights reserved.
(http://popcapframework.sourceforge.net/).

框架许可见 `pvzPerf/SexyAppFramework/PopCap Framework License.txt`；
游戏素材、依赖和外部缩放效果保留各自的权利及许可，根目录 LICENSE 不代替它们。
