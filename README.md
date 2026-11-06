# pvz_perf

基于 [PvZ-Portable](https://github.com/wszqkzqk/PvZ-Portable) 修改，继续优化《植物大战僵尸》的 CPU 热路径、资源装载、图形提交和跨平台输入。游戏行为以零售 1073 为主要核对依据。

**当前发行版：[v3.3.3](https://github.com/YuHao2006/pvz_perf/releases/tag/v3.3.3)。**

版本号采用三段规则：**第一个数表示破坏性更新，第二个数表示功能更新，第三个数表示 bug 修复**。兼容性以各版发行说明为准。

## 下载与运行

| 发行包 | 系统和图形要求 |
| --- | --- |
| `x64-win10-vulkan1.3` | Windows 10 x64、Vulkan 1.3 及所需 descriptor indexing 能力 |
| `x64-win7-openGL3.3` | Windows 7 x64、OpenGL 3.3 |
| `arm64-android14-vulkan1.3` | Android 14、Armv9-A、Vulkan 1.3 及所需 descriptor indexing 能力；不要求 SVE/SVE2 |
| `arm64-android7-vulkan1.0` | Android 7、Armv8-A、Vulkan 1.0；传统纹理绑定 |
| `wasm32-web` | 支持 WebAssembly 和 WebGL 2／WebGPU 的现代浏览器；默认优先 WebGPU，可选择 WebGL 2 |

下载入口：[GitHub Releases](https://github.com/YuHao2006/pvz_perf/releases)。Windows ZIP 完整解压到可写目录后运行 `pvz_perf.exe`；需要兼容的 VC++ x64 运行库。Android APK 已包含资源，不需要另拷数据包或在手机安装 Java。设备满足系统版本还不够，CPU、驱动和所需图形格式也必须满足对应要求。

发行附件同时提供完美用户档、统一无尽基准、图集布局缓存和校验清单。完美档包含坚果包扎术。不要用个人进度覆盖仓库的性能基准。

**v3.x 对旧版本有资源和存档兼容边界：**游戏只直接读取 DAT64 v4／schema `0x10745`。原版 DAT、DAT64 v1/v2 使用附带转换器升级；未发布的 v3 中间格式不支持。程序、shader、PAK64 必须成套更新。见[存档转换](docs/DAT-DAT64-CONVERSION.md)与[发行说明](docs/releases/v3.3.3.md)。

## 设置与存档

- Windows：程序目录的 `data/`。
- Android：通常为 `Android/media/com.openpvz.perf/`，无需额外存储权限。
- 网页：当前网站来源的 IndexedDB，游戏虚拟目录 `/persist/development-v331/`；不是系统文件夹。设置页部分选项和自定义阵型列表另存 localStorage。不同浏览器／网站不共享，清除网站数据会删除进度；可在“自定义无尽”中导出 DAT64。
- Linux 源码目标：遵循 `PVZ_APPDATA_DIR` 或 XDG 数据目录，详见[资产与运行边界](docs/ASSETS.md)。

`settings.json` 首次创建时自动跟随刷新率；Windows 默认桌面 UI，Android 默认横板 UI。已有配置保留，目标帧率最低 30。横板 UI 在画幅不足时回退桌面布局。音量按滑条值保存并钳制在 0～100。退出时通过游戏菜单保存，安装覆盖或强退不会替未保存的关卡回写进度。

Android HDR 当前禁用，设置项保留但不可启用；Win7 使用 SDR。桌面缩放配置由 `scale.json` 管理，各后端支持范围见[GPU 提交](docs/gpu-submission.md)。

## 性能与实现

公开整局对照采用 **Windows 低功耗核心 LP14、用户控制 11x 倍频**。以下是 v3.2.4 对应测量构建的完整无尽结果，不把后续未复测版本自动标成同一成绩：

| 程序 | 绘制 FPS | CPU 时间／绘制帧 |
| --- | ---: | ---: |
| 项目 Vulkan，240 FPS 上限 | 196.736 | 4.552 ms |
| 原版 1073，100 Hz 逻辑 | 43.951 | 20.676 ms |

两边使用同源阵型起点，但渲染器、步长及战斗演化不同；这些是整局观测，不是某个函数的提速倍率。逐秒曲线、原始样本、构建哈希、微基准及手机测量口径集中在[性能总账](docs/perf-fix-ledger.md)，不在首页维护第二套数字。

当前主要机制包括：按类型和动作身份组织的段表、行／格索引、reanim 整数纳秒相位和共享帧缓存、CPU 粒子模拟、离线图集及 UV、上传后释放 CPU 图片、各后端的帧资源复用。已撤回的 GPU 动画求值和线程流水线不属于现行路径。

## 构建入口

正式配置使用 Release／LTO；`out/`、本机依赖、个人存档和临时日志不进入源码仓库。

| 目标 | 入口 |
| --- | --- |
| Windows Vulkan | CMake：`PVZ_RENDER_BACKEND=VULKAN`，输出 `out/x64-win32-vulkan/Release/` |
| Windows 7 OpenGL | `tools/build-win7-opengl.ps1` |
| Windows DX12 | `tools/build-d3d12-game.ps1` |
| Windows 7 DX9 | `tools/build-win7-opengl.ps1 -RenderBackend D3D9` |
| Android | NDK 原生构建后用 `tools/build-android-apk.ps1 -BuildDirectory <tree> -Sign` 打包，详见[Android](docs/android-port.md) |
| Linux Vulkan 源码目标 | `sh tools/build-linux-game.sh x11` 或 `wayland`；不是本次四个发行附件之一 |
| 网页版（WebGPU／WebGL 2） | `tools/build-web-toy.ps1`；[构建与本地运行说明](tools/wasm/README.md)，v3.3.3 提供双后端 ZIP |

Android 不需要安装 Android Studio；可使用命令行 SDK、NDK、JDK、CMake 和 Ninja。兼容包使用 `PVZ_VULKAN_LEGACY=ON`，必须配套 Android API 24 原生构建；现代包与兼容包不得交叉混用 `.so`、PAK 和 shader。

资源入口为 `assets/definitions/ResourceBuild.pvzbuild`。图集已完成布局缓存位于 `assets/definitions/atlas-cache/`；未证明最优的限时排布如实记录。资源构建链和存档转换器的源码迁至独立开源仓库 [PvzDevTools](https://github.com/YuHao2006/PvzDevTools)，本仓库只附带 `tools/bin/windows-x64/` 中的预编译工具。主 CMake 直接运行 C++ 工具，不通过 Python 构建资源；其他主机可自行构建工具并设置 `PVZ_DEVTOOLS_BIN_DIR`。测试与性能采集脚本继续保留，性能采集位于 `Tests/performance/`。源码构建不要求相邻的原版安装目录。

## 文档与贡献

- [文档索引](docs/README.md)：现行机制和平台说明。
- [性能总账](docs/perf-fix-ledger.md)：优化原理、有效基准与复现边界；保留仍有对照意义的 1.0 微基准。
- [原版调查](docs/research/README.md)：1073、Windows Phone、Free 与资源证据。
- [验证说明](docs/validation.md)、[无尽基准](Tests/README.md)、[性能工具](Tests/performance/README.md)。
- [资产与许可边界](docs/ASSETS.md)、[参与开发](CONTRIBUTING.md)。

调查报告不替代当前代码，历史测试不替代新版本验收。临时排障、候选优化和未验证结论不进入公开机制文档。

This product includes portions of the PopCap Games Framework, (c) 2005-2009 PopCap Games, Inc. All rights reserved.
框架许可见 [PopCap Framework License](pvzPerf/SexyAppFramework/PopCap%20Framework%20License.txt)。游戏素材、依赖和外部效果保留各自权利与许可；根目录 LICENSE 不代替它们。
