# 资产、构建输入与发行边界

本文只维护必须保存的输入、当前运行包组成和许可边界。运行结构见[资源格式](resource-format.md)，作者语法见[场景图集 DSL](scene-atlas-dsl.md)，性能结果不在此重复。

## 必须保存的输入

| 输入 | 用途 |
| --- | --- |
| `assets/definitions/ResourceBuild.pvzbuild` | 枚举顺序、定义文件、共享图集与装箱策略 |
| `assets/definitions/` 中的 reanim、particle、projectile、场景 DSL | 动作、粒子、投掷物及图集作者数据 |
| `assets/runtime/` | 原生图片、字体、音效和音乐源资源 |
| `assets/source-art/ai/` 及其中的 `runtime-overlay.json` | AI 原始成品与在用成品的显式路径映射；不与原生源图混放 |
| `assets/definitions/atlas-cache/` | 已完成布局搜索结果；不是运行时资源 |
| `pvzPerf/Sexy.TodLib/generated/` | 可复现生成的 C++ 数值、枚举、UV 和绑定代码 |
| `Tests/manifest.json` 及其引用档案 | 原版 DAT 和项目无尽基准的来源与哈希 |

`tools/runtime_asset_sources.py` 解析原生资源与 AI 映射，重名、缺失或未满足引用会报错。打包只取实际引用闭包，不扫描整份素材目录作为发行清单。

QuickPlay 源 PNG 继续保存在项目中，界面代码、运行时绑定及包内条目已退役。AI 提示词、蒙版、原始返回文件和未引用图片不因留在仓库就进入 PAK。

## 定义与图集

reanim、projectile 行为使用生成的静态 C++；particle 的完整 CPU 定义从 `compiled/particle-runtime/` 的 PTC1 读取。原 reanim、PAV6、PRJ1 等编译产物中，仍被生成器使用的部分属于离线中间格式，不能统称为游戏启动时需要加载的行为文件。

图像采样区域存储最终 UV 和逻辑绘制宽高，不保存行列信息。定义直接持有按外观索引的 GPU 指针；轨道保存图片槽号，不创建每张子图的 Image 包装。程序具名图片、定义和场景图集可以共用 GPU 资源；具体所有权与失败回滚见[资源格式](resource-format.md)。CPU 解码像素上传后释放。

场景资源按用途拆分：

| 图集 | 加载策略 |
| --- | --- |
| Magnet、Almanac、Achievements | 现有作者 DSL 描述对应图片和 view |
| CommonUI | 两种布局共用的按钮、对话框及其他公共 UI |
| DesktopUI | 桌面布局专用；横板布局不加载 |
| LandscapeUI | 横板布局专用；桌面布局不加载 |

启动时选定布局，加载 CommonUI 和对应专用图集。没有运行时 FixedUI 总图集。图集 view 数量、源图数量和尺寸以当前生成头文件、绑定清单及包内容为准，不在多篇指南复制易过期的数量。

原始 sheet 在场景装箱中保持完整，不按子区域拆开，不旋转或缩放。已有图集的局部区域仅转换成 UV。特例的作者像素合成和面板平铺按生成器规则执行，不冒充源图片逐字节不变。

## 构建与部署

主 CMake 构建资源工具，再运行 `tools/resourcePipeline/src/build_resources.py`。Python 图像步骤依赖 Pillow，装箱求解依赖 OR-Tools，依赖清单位于 `tools/resourcePipeline/requirements.txt`；游戏运行时不依赖 Python。

流水线解析作者定义、生成 C++、验证引用和纹理区域，再写入 `out/<组合>/cache/resources/main.pak64` 并部署到该树的 Release。输入和输出内容不变时不重复改写生成源码。shader 按后端单独编译。

NRSC4 是临时加载计划，给具名目标提供装载／释放步骤，不是运行时资源 ID 表。各组加载完成后销毁计划；字体和共享图片保留自身所需的生命周期状态。历史 ResourceId／ImageSymbolId 仅由独立转换器冻结映射，不恢复到生产绘制路径。

## 发行文件

- 主程序／原生库、`main.pak64`、对应后端的 shader、默认配置、平台依赖与许可。
- `main.pak64` 包含有效图片／图集、字体、音频、NRSC4 和 PTC1；不包含内置 DXIL／SPIR-V。
- `effects/` 是外部缩放效果及其许可，不表示每个后端都支持同一 HLSL／compute 链。
- 附带存档仅使用完美档和已核对的统一无尽基准，不包含开发者个人进度或设置。
- 编译缓存、测试程序、日志和临时候选包不进入游戏发行目录。图集缓存可作为单独发行附件提供。

EXE／原生库、PAK、shader 和生成绑定必须来自相配构建；不能把一个平台的 `.so` 与另一棵树的资源包混装。当前发布的实际哈希和验证范围以 Release 附件 `manifest.json`、`build-verification.json` 为准。

## 平台数据与依赖

Windows 存档位于程序目录 `data/`；Android 首选 `Android/media/com.openpvz.perf/`，详见[Android](android-port.md)。原生 Linux 依次使用 `PVZ_APPDATA_DIR`、XDG 数据目录或 `$HOME/.local/share/PlantsVsZombiesModern`，保留旧名称以保留进度。

Linux 源码目标依赖对应的系统窗口、Vulkan loader、音频和 C++ 运行库。`tools/package-linux-runtime.py` 可收集可信 ELF 的依赖闭包，但驱动、动态插件和系统数据不因此变成可携带文件；编译通过不保证任意发行版直接运行。

Windows 音效使用 DirectSound；Linux 输出使用 OpenAL Soft，BASS 负责其模块解码。Web 的 libopenmpt 只属于 Web 平台，不能把它写成所有平台已经替换 BASS。

## 来源与许可

项目源码基于 PvZ-Portable 继续修改。游戏原生素材、PopCap 框架、第三方库、Magpie 等外部效果和 AI 成品分别保留来源与许可；根目录 LICENSE 不代替其他权利人的条款。

图像采样参考实现的许可证随相关源码保留。Web 音乐解码器的许可放在其分发目录。正式包应附平台依赖清单，不把本机 SDK、NDK、工具安装路径或调试文件当作游戏资源。

原版无尽基准的原始档及转换档由 [Tests](../Tests/README.md) 管理，来源和校验值以 `Tests/manifest.json` 为准；不在文档中写开发者机器的个人存档绝对路径。
