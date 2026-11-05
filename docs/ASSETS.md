# 资产与发行边界

本文维护资产来源、加载和发行边界。当前源码关卡格式为DAT64 v4（schema0x10744），运行时仅直接读取v4；转换器仅接受原DAT、v1、v2、v4，不支持v3系列。时间与运动见[运行时](runtime.md)，资源采样结构见[资源格式](resource-format.md)。

## 需要保存的输入

| 类别 | 内容与现状 |
| --- | --- |
| 游戏源码 | pvzPerf 下的入口、Lawn、Sexy.TodLib、SexyAppFramework，以及平台和图形后端 |
| 构建与依赖定义 | 根 CMake、tools 中现行构建/资源/转换/打包工具、vcpkg.json、overlay ports、平台启动脚本 |
| 定义编译器 | tools/resourceCompiler/src，C++23 编写；由主 CMake 构建 |
| 存档转换器 | tools/saveConverter/src，C++ 编写；和资源编译器分别使用独立构建子目录 |
| 定义源 | assets/definitions/reanim、particles、projectiles、ui；particle-reference 为现有粒子编译器需要的对照输入 |
| 图集搜索缓存 | assets/definitions/atlas-cache 中经当前几何、模型与求解器版本验证的完成记录；只作编译输入，不进入运行包 |
| 程序图片绑定 | Resources.h/.cpp 中 210 个命名 Image*，由实际 C++ 消费端确定加载闭包；没有全局图片编号表 |
| 定义编译输入 | 离线字体 DSL sidecar、reanim 类型/轨道映射；生产资源清单为 assets/definitions/resources.json |
| 字体 | assets/definitions/fonts 中的 20 份定义，以及 assets/runtime/data 中的字库图片；不再读取原版安装目录 |
| 运行资源 | 被定义和游戏代码引用的图片、音效、音乐、字体、UI 文本和其他实际读取的数据 |
| 内置 shader | HLSL，以及 Vulkan 当前使用的 GLSL .frag/.comp 和 include；输出 DXIL/SPIR-V 是可再生构建产物 |
| 应用标识与默认配置 | assets/branding、应用使用的光标、平台图标描述、根目录 scale.json；个人 settings 和个人存档不作为源资产 |
| 测试与文档 | Tests 中统一 DAT/DAT64、Tests/performance 中的性能工具、测试源码和夹具、docs、README；性能原始证据不能用未完成的新测量替代 |
| 来源和许可 | 根 LICENSE、第三方依赖与外部效果各自的来源、许可说明 |

程序使用 210 个命名 `Image*` 全局；本轮场景图集迁移后删除了 76 对
没有剩余 C++ 消费端的声明和定义。`ResourceId`、`ImageSymbolId` 和
运行时字体 SymbolId 均已退役，`ProgramImageSymbols.h/.cpp` 已删除。
资源流水线读取 `Resources.h` 的声明和实际 C++ 使用，生成对具体变量的
加载、直接指针赋值和对应释放代码；每张图片有具名 `SharedImageRef` owner，
不经过 slot、编号数组或指针反查。NRSC4 的绑定契约 CRC 覆盖组、步骤、
类型和具名目标，payload CRC 覆盖加载参数，程序与包须配套重建。
QuickPlay 的 12 张源图仍保存在 `assets/runtime/images/`，对应界面代码、
运行时绑定和包内图片均已移除。
旧 DAT / DAT64 v1、v2 的 ResourceId 是另一套历史编号，879 个槽位及其
顺序、图片类型和确证别名冻结在转换器专属 `FrozenResourceIds.h`。
游戏不再为了这些旧存档保留混合图片、字体、声音的 879 槽表或 typed-null 占位。
历史 2,287 项图片映射仅用于隔离的历史夹具和离线对照，不参与生产绑定。
此次运行时编号退役不改变历史存档编号，也不修改已发布版本的程序、包和 tag。
游戏代码的具名图片消费端也是资源闭包的根，不能只从 reanim/particle DSL 收集图片。
字体图片、UI 资源、遮罩/alpha 配图也必须计入依赖。

v6 中定义直接持有离线生成的图集，图集不占全局命名图片绑定；
定义专用源图片不再发行独立纹理。作者素材按名称和路径离线解析，完全不依赖程序图片编号；
全局图片、代码的直接消费以及字体路径共同保护独立资源。历史 ResourceId
仅用于离线旧存档转换，不参与运行时资源绑定。
仅供定义使用的图片即使原文件位于 `images/`，也不再保留独立纹理描述符或包内单图；
其作者源文件继续作为离线图集输入。代码与定义共用的图片仍保留独立绑定，
QuickPlay 源图的保存不构成发行引用根。
协议和加载边界见 [resource-format.md](resource-format.md)。
定义编译器不接受 `--image-registry`，不会生成程序图片符号代码。
其临时 `.source-record` 和 `image_sources.txt` 仅用于离线素材解析，
particle 的离线记录图片字段直接记录矩形；reanim 的共享行为保存
图片 slot，根定义中保存全部外观的矩形行。这三类离线记录随后生成完整
C++ 静态定义，不进入运行包。旧 `image_symbol_ids.bin`
仅保留给历史对照工具，不是生产构建输入。

普通运行期图片接口只接受 GPU `Image`；particle/reanim 根定义都保存
按外观索引的 GPU 指针及静态 `TextureRegion` 数组，不经过图片或图集包装，
不再保存图集尺寸数组。
共用图集的 reanim/particle 定义在加载期获得相同 GPU 指针；
reanim 共享关键帧只保存图片 slot，particle 发射器保存每行区域 offset/count；
绘制使用只含 GPU 指针和采样 flags 的临时 `TextureSource`。
`TextureRegion` 为公共 aggregate，保存 float `mU0/mV0/mU1/mV1` 和
int 逻辑 `mWidth/mHeight`；生成器在编译期按图集尺寸归一化，运行期直接采样。
loader 的预期 PNG 尺寸仅以字面量验证，不常驻在采样结构中。
旧图片及图集包装类型已删除；公共加载 registry 只记录解码和共享绑定，
renderer 直接登记 GPU 指针并负责释放，定义不指向 registry 条目。
加载线程只解码，主线程上传后统一给等待中的定义赋值，并释放临时绑定记录。
独立 CPU `MemoryImage` 与加载期
`ImageLib::Image` 不可隐式转换为绘制图片。图片不再携带 `ImageType`、
`mDrawn` 或文件路径。GPU 图片、图集和加载 payload 使用明确的裸指针
所有权，上传完成即释放全部解码数据；共享图集由 renderer 统一持有，
退出时在定义释放后、renderer 销毁前统一删除。具体边界见格式文档。
普通 `Image` 也直接保存 `GpuTexture*`，已合并绘制图片子类并删除独立
texture-data 包装、重复尺寸/采样状态及旧上传版本比较。需要图片加载语义
的程序图片使用 `Image`；纯纹理绑定直接使用 GPU 指针。
普通图片不保存总行数、总列数、动画元数据或帧矩形表。
UI、投掷物、场景、大头贴和定义的长期采样表保存 UV 与逻辑尺寸，
不从 UV 反推绘制宽高。通用按钮四态和 Coin 静态表同样保存 `TextureRegion`；
复选框未使用的切片字段与分支已删除，继续使用两张整图。
字体的基础字形、缩放字形和排序命令也保存 UV 区域；
加载／缩放时供 GPU 整数权重重采样使用的像素 Rect 为临时算子参数。
普通 `Image` 及后端纹理仍保留创建、离屏目标、复制和动态像素裁剪所需的尺寸。
动态 source crop 可在调用边界构造像素 Rect 并转换，不将所有 Rect 都解释为采样状态；
屏幕布局、clip、碰撞框和业务尺寸保持各自语义，浮点绘制偏移任务暂缓。
离线 JSON 资源清单保留原素材的行列布局，供作者工具识别和裁切；
NRSC4 加载计划不写入行列或图片动画参数，C++ 加载器也不读取它们。
CPU alpha 合成层和各 GPU 后端的 alpha 合成接口只接受同尺寸图片，不携带行列。
滤镜使用原纹理的采样标志，不再创建滤镜图片缓存或回源指针。
首次上传前完成透明度分析，避免延迟加载的字体被误标为不透明；
上传后的绘制只取已有 GPU 指针。临时 `TextureSource` 是按值绘制参数，
不持有 `Image*`，但 `GpuTexture*` 仍指向后端的纹理资源对象。
OpenGL/DX9 的纹理对象已直接包含原生纹理，不再间接持有 Storage。
提交批次记录原生纹理句柄；已销毁或替换但尚有排队绘制的对象由 renderer
暂存，Flush 发出这些绘制后释放，提交循环不再复制 shared_ptr。

reanim 离线中间记录使用 v6 外层 DEADFEDB、RAV7/IDT6：每个根类型只保存一次轨道、关键帧、字体、文本和
附件行为，实例只保存 `u32 mVariant`。外观是根定义中的纹理绑定数组、
`[variant][slot]` 的 UV／逻辑尺寸表和预计算切换表，不增加 ReanimationType
或独立 compiled 定义。图片 slot 离线按轨道、原图、原 cell 细分，保留
原图时沿用原 cell，替换图使用全图；`none` 只跳过 sprite，不隐藏附件。
运行期关键帧不再保存行列、矩形或 Image 覆写指针。
作者的 `variant-of` 按每条根轨道中原图首次出现的唯一顺序列出图片槽，
每条轨道的列表都要完整替换，`none` 显式表示空白。
compiler 生成 `ReanimVariantId.h`，其中
`enum class ReanimVariants::<Root>` 表示整套外观，可直接 `SelectVariant`；
`tools/generate_reanim_appearances.py` 则生成 `ReanimVariantOptions.h` 的
轨道选项、DSL、切换表和存档目录。`SetVariantOption` 根据当前
`mVariant` 选择 transition，保留其他轨道的外观选择；轨道选项不是整套
variant 编号。生产语料含 31 个外观家族、2,717 行（含各家族 base）、
166 个 action，transition 目标共 210,136 字节。这些数字描述结构和体积，
尚未登记帧时收益。`ResourceBuild.pvzbuild` 必须显式用 `Root_V*` 尾 glob
把外观纳入共享图集，不会按家族自动合图。
particle 的多格源图已拆为无损 RGBA PNG，定义只保存有序 `images`
列表（单图可用 `image` 简写），没有行列或源矩形。各帧独立装箱，
离线中间记录使用 v6 外层 DEADFEDC、PAV6/IDR6，先保存一次 emitter、
标量、轨道和 fields 行为，再在同文件尾保存全部外观的尺寸与四整数
矩形行。56 个外观汇入原版 106 个根，ParticleEffect 恢复原版顺序，
`ParticleVariantId.h` 为每个根生成 Default0 和按 component 字典序的枚举。
动态选帧保留整数帧号和逻辑帧数，运行期没有行列换算或相邻帧步长。
`particle-frame-images.json` 保留裁切来源及像素摘要，逐字节验证拆帧无损。
35 张废弃 PNG 已移出源资源：34 张仅供粒子旧格图使用的原图，
以及 GPU 固定路径不再使用的 `images/fog_software.png`。GPU 雾和跳跳僵尸
部件仍有 C++ 使用，保留其原图。移出的文件保存在仓库外的临时备份目录，
可恢复，未加入 PAK。拆帧测试对全部 213 张 PNG 检查预先记录的
完整 RGBA 摘要、尺寸和格位覆盖，对仍保留的原图另外逐像素比较裁切结果。
历史拆帧重跑须显式提供 `--original-root`，正常构建不依赖旧格图副本。
reanim 的 C++ 逐轨道 Image 覆写已删除，业务通过生成的外观选项切换索引；
普通图片的行列选格接口已删除。
particle 已删除图片覆写接口和实例中的 Image 指针，掉头、断臂、胡须及
僵王吐冰改为在出生时选择基础 effect 和内部 variant。七个原定义文件内的 `variant-of`
严格逐 emitter 一一替换 `images`：外层 emitter 数量和每个内层图片数量
都必须与父定义相等，不能声明 emitter 或覆盖行为。编译期可临时展开
继承内容做校验与装箱。父链只在编译期解析；运行时没有独立子定义、
递归加载或行为指针在根之间借用。一个根唯一拥有 emitter/轨道/fields
静态数组以及 `[variant][region]` 表，各外观只有 GPU 绑定和 UV／逻辑尺寸行，
不保存图集宽高数组。离线中间记录的整数矩形继续供装箱与像素验证使用。
程序静态定义包含 144 个 reanim 根和 106 个 particle 根，分别承载 2,830 和
162 行外观；不再为各外观重复编译行为。包含投掷物和 Zombatar 编辑器在内的
193 张运行期图集由配置明确控制。
无运行时 ID 的 `Zombie_dancer` 和 `Zombie_Jackson` 及其独占图集不进入运行包。
当前关卡存档为 v4，粒子记录偏移 0/4 分别保存 106 项基础 ID 和 variant，
原有空隙填入 variant，不增加记录长度。Reanimation 保存实例 variant，
逐轨道图片 ID 及其八字节指针保留区已删除。旧档通过离线转换器和冻结
外观目录显式输出为 dat64-v4，未知旧覆写拒绝转换。

`tools/test-image-runtime.ps1 -BuildDirectory out/x64-win32-vulkan`（或
`out/x64-win7-opengl`）复用该构建的生产对象文件，替换测试入口，
通过隐藏测试窗口验证 ImageFont 的延迟透明度分析、上传即弃、
纹理复用、独立复制及共享图片引用生命周期，不启动游戏或读取玩家存档。
`tools/test-static-definition-runtime.ps1 -BuildDirectory <build> -PakFile <main.pak64>`
另用生产对象验证完整静态定义生命周期：从原包只读生成临时 ZIP，
覆盖全部 144/106 根的 backing 数组与曲线节点保持不变、重复加载与两轮完整释放重载、
跨定义种类共享图集的独立解绑、工作线程等待绑定取消，以及缺失/损坏/尺寸错误 PNG
失败后的回滚和同进程恢复。全部证据只写系统 Temp，不修改原包或玩家存档。

## Shader 对应关系

| Vulkan 当前源码 | DX12 对应源码 |
| --- | --- |
| game_vs.hlsl、game_ps.frag | d3d12/shaders/game.hlsl 的 VSMain、PSMain |
| game_alpha_cs.comp | d3d12/shaders/game_alpha_cs.hlsl |
| game_color_cs.comp | d3d12/shaders/game_color_cs.hlsl |
| game_font_cs.comp | d3d12/shaders/game_font_cs.hlsl |
| game_caustic_cs.comp | d3d12/shaders/game_caustic_cs.hlsl |
| game_hdr_ps.hlsl | d3d12/shaders/game_hdr.hlsl 中的呈现逻辑 |
| game_coverage_ps.frag | DX12 主像素 shader 的 coverageInput 分支承担相关覆盖率处理；不是独立同名入口 |

这些运算可以用 HLSL 表达，DX12 的实现已经证明这一点。
Vulkan 当前使用 GLSL 的部分与其 glslc HLSL 前端的能力有关：
主像素路径需要非一致索引，计算路径需要明确的 storage image 格式。
项目的 ScaleEffectCompiler.cpp 已经使用 DXC 的 -spirv 路径，
并生成 vk::binding、vk::constant_id 和 vk::image_format 属性；
因此 HLSL 到 SPIR-V 的工具链在项目里已有使用依据。
统一到 HLSL 时需要选择能表达这些约定的 SPIR-V 编译路径，
并保持 descriptor set/binding、push constant、特化常量、顶点位置和混合语义。
现阶段不能只换后缀或直接拿 DX12 输出替换 SPIR-V；统一前保留正在使用的源文件。

Shader 源码按图形后端隔离，不按操作系统复制：Windows 的 Vulkan 与
Linux 的 Vulkan 共用 Vulkan shader；DX12 使用其对应 shader。
现有 graphics/vulkan/shaders 和 graphics/d3d12/shaders 已实现这一编译期边界，
无需为 shader 选择增加运行时分派。不能仅凭源语言或编译器名称判定运行性能，
需要比较编译结果和同条件测量。

## 发行输出

- main.pak64：字体 compiled、资源加载计划及资源引用闭包；reanim/particle/projectile 定义在程序内，不包含内置 DXIL/SPIR-V。
- shaders/：构建期生成的 DXIL/SPIR-V。当前保留目录方式；尚未实现 shaders.pak64 加载。
- effects/：外部缩放效果，发行时可携带，用户可从 Magpie 获取替换文件；不作为项目自有素材。
- 可执行文件、平台动态依赖、默认配置、图标和许可说明。构建缓存与个人数据不进入发行包。

## 构建流程

主 CMake 通过 CMake/Resources.cmake 构建 reanimc，再调用
tools/resourcePipeline/src/build_resources.py：

Python 构建环境需安装 Pillow，用于离线解码和打包图集；游戏运行时不依赖它。
精确布局还需 OR-Tools；依赖列表位于 `tools/resourcePipeline/requirements.txt`。
构建配置入口为 `assets/definitions/ResourceBuild.pvzbuild`，选择 reanim/particle
定义文件并声明同种或跨种共享图集组。原先两个总 root DSL 已删除，定义源通过明确
文件列表传入编译器；轨道前缀仍由 `tools/reanim_track_families.txt` 定义。

1. 从 Resources.h 的命名图片声明和实际 C++ 消费端确定有效绑定；reanim/particle 独立解析作者素材。退休图片不发行描述符；旧存档的 879 项顺序由转换器冻结边界单独验证，不再规定运行时资源槽。
2. 编译 DSL 和字体、生成数值资源清单，生成的 C++ 头文件与源文件写回 pvzPerf 的源码目录。
3. 验证数值定义和资源引用，收集图片及 alpha 配图、音效、音乐和 UI 数据。
4. 将引用闭包写入 out/<组合>/cache/resources/main.pak64，再部署到 Release/main.pak64。

`compiled/resources.compiled` 现在为 NRSC4 临时加载计划，当前源清单编译得到
210 个图片、168 个声音和 20 个字体，共 398 条显式装载步骤。生成的
`ResourceLoadBindings.inc` 对每个命名目标提供同类型 load/free，图片直接传入
目标指针及其具名 owner，组内逆序释放。计划头保存图片记录数、组数、
总记录数、绑定契约 CRC、payload CRC 和字节数，不包含资源或图片 SymbolId。
共享缓存仍按 `(物理路径, variant)` 复用；variant 仅区分同一路径的完整加载配方，
普通单配方路径为零，相同配方共享缓存条目，不承担图片身份或绘制查询。
pending plan 只解析一次，每组加载完成即销毁；字体同组引用在 plan 内解析为
稳定指针，真正字体与其共享 FontData 的生命周期独立于 plan。
失败回滚释放已经加载的目标，标题图早释放及退出清理均可重复调用。

资源编译器与存档转换器的构建输出分别位于
out/<组合>/tools/resourceCompiler/build 和 out/<组合>/tools/saveConverter/build，
不与工具源码混放。只更新资源也会触发部署，不依赖可执行文件重新链接。
输入未变化时不重复资源编译，生成源码内容未变化时不改写文件。
shader 继续由各后端构建规则单独生成，外部缩放 HLSL 继续在启动时编译。

## 场景图集与完整源 sheet

磁铁物件、Almanac 和成就页分别由 `assets/definitions/ui/` 下的
`Magnet.atlas.dsl`、`Almanac.atlas.dsl`、`Achievements.atlas.dsl` 描述。
FixedUI 直接使用 Python 中明确列出的 24 张源图：六张石质按钮状态、
十三张对话框部件和五张选卡按钮状态，不新增专用 DSL。

| 模块 | 逻辑 view | 完整源 sheet |
| --- | ---: | ---: |
| Magnet | 21 | 19 |
| Almanac | 19 | 19 |
| Achievements | 35 | 16 |
| FixedUI | 24 | 24 |
| 合计 | 99 | 78 |

这 78 张源 sheet 按原始逻辑 RGBA 尺寸完整装箱，保留内部所有像素、顺序、
空白和未被引用的区域。原有 sheet 不拆帧、不重排、不裁透明边，也不为
view 新建源 PNG。局部 view 的最终矩形仅为源 `rect` 加整张 sheet 的
装箱 origin，宽高不变；例如 Pogo 的三种状态仍共用完整 132×86 sheet，
二十个成就图标仍共用完整 512×256 sheet。

`SceneAtlasData.h/.cpp` 为 `MagnetAtlas`、`AlmanacAtlas`、
`AchievementsAtlas` 和 `FixedUIAtlas` 各生成一个直接 GPU 指针、
静态 `TextureRegion Images`、局部 `Sprite` 枚举及 `Load()` / `Free()`，
不公开图集 Width/Height 常量。`Load()` 使用预期尺寸字面量校验 PNG。
没有每个 view 的 `Image` 对象或运行时 ID 查找。磁铁模块另外生成实际使用的
`Scales`；成就模块按 `ICON_n` 的数字顺序生成独立 `Icons` 数组，
不依赖 DSL 声明顺序或枚举相邻。聚合函数负责四模块加载和逆序解绑。

场景图集与现有图集共用不旋转、两像素扩边和每单元 60 秒预算的装箱路径，
保留缓存复用和实际证明状态。未证明最优的结果不能标为最优；
本节不预记最终 atlas 尺寸、包条目、构建通过结论或性能收益。
`SceneAtlasBindings.h` 仅供离线检查整张源 sheet 的区域和 RGBA 摘要，
运行包没有场景行为 compiled。新增图片可直接由 DSL 引用路径，
无需新增 `resources.json` 标签或命名 `Image*` 全局；旧 `IMAGE_*` 标签
只作离线作者 alias。语法、配置和边界见 [scene-atlas-dsl.md](scene-atlas-dsl.md)。

## 投掷物图集与完整静态定义

`projectiles/Projectiles.projectile.dsl` 使用现有十四个 `ProjectileType` 名称，
明确给出 PNG 路径和缩放；昼夜阴影记录源裁剪矩形。C++ DSL 前端不读取
程序图片编号。公共流水线使用相同的非旋转装箱器和两像素扩边，写出
PRJ1 离线中间记录和投掷物图集。
程序保留一份 GPU 指针，以及静态存储的十四项 UV 区域/缩放和两个阴影 UV 区域；
投掷物图集结构不再保存 atlas width/height。采样边界与逻辑尺寸由编译器写入
C++ 初始化值，加载期按字面量验证 PNG 尺寸并绑定 GPU 指针，上传后立即释放 CPU 像素。
弹种、运动、碰撞和存档字段不变。
孢子的 DSL 缩放以原来的 0.3 为基准乘上年龄曲线，换大图时同样能调整显示大小。

`ResourceBuild.pvzbuild` 的 `definition_order(reanim ...)` 和
`definition_order(particle ...)` 显式保存现有枚举与定义名的对应顺序。
144 个运行时 reanim、106 个 particle 以及十四项投掷物外观完整生成到
`GeneratedDefinitions.h/.cpp`、`GeneratedDefinitionStorage.h` 和十六份数据分片。
根对象与可修改数组使用 `constinit`，只读 UV 区域等数组使用静态常量；
每个定义只保存实际的变体×图片槽位，不按全局最大尺寸补零。
生成代码逐项 `static_assert` 验证枚举值，离线构建还会核对 C++ 注册表的定义路径。
全部轨道、398381 个关键帧、5158 个粒子曲线节点、力场、外观选项和
689 段附件动作区间均离线解析。运行期不再读取 reanim/particle/projectile
`.compiled`，不分配或释放定义的嵌套数组，不填充默认值或解析父链。
`GeneratedDefinitionLoading.cpp` 生成一次性加载和对应解绑函数，通过公共
PNG/renderer 接口给 GPU 字段及少量字幕字体字段赋值。静态 backing 数组生命周期与程序相同。
运行包中这三类 compiled 均为零，字体 compiled 20 项，资源加载计划 1 项。
生成源码约 45 MB，是可再生构建产物；静态数据仍占用映射内存，不能把它全算成内存节省。
资源编译与当前DAT64 v4协议分别管理，资源改动不据此改变存档布局。

## Zombatar 编辑器与头像图集

`zombatar/Zombatar.zombatar.json` 显式保存 209 项编辑器、部件、遮罩和头像素材的
名称、PNG 路径及自动 alpha 策略，独立于程序命名图片绑定。配置使用
`sources(zombatar ...)` 和 `atlas(zombatar zombatar:Zombatar)`，与投掷物共用
无旋转、两像素扩边、每图集 60 秒的装箱路径。当前图集 1347×2208，
FEASIBLE，面积下界 2944583，gap 0.994998%，未证明最优；完成缓存保留在源树。

生成的 `ZombatarImage.h` 是模块内部的 16 位图片枚举，
`ZombatarAtlasData.cpp` 保存 constinit 图集对象、209 个 `TextureRegion`
及加载/解绑函数，图集对象不再保存 atlas width/height。
整个模块只持有一份 GPU 指针，子图不创建 Image、视图或独立纹理。
`ZombatarLoadResources()` 在 LawnApp 加载期调用，主线程屏障完成上传并释放 CPU 数据；
`ZombatarFreeResources()` 在共享图集销毁前解绑。页面隐藏不释放图集。
编辑器按钮使用只含实际布局、输入状态和图片枚举的专用数据类。
部件表、着色遮罩、静态头像与无损 PNG 导出均直接使用图集 UV 区域与逻辑尺寸。
战斗头部继续使用已有的 `REANIM_ZOMBATAR_HEAD` 定义图集；
预览地面借用 `IMAGE_ALMANAC_GROUNDDAY`，不由模块释放。
存档中的逻辑部件编号与颜色编号保持不变。
采样区域从像素矩形改为 UV 不改变离线装箱、PNG 像素、PAK 格式或存档布局。

## 已核实机制

音效有 SOUND_* 命名变量，经加载计划直接获得声音后端句柄；
音乐使用 MusicFile、MusicTune 等数值标识和装载计划，
不与图片、字体共享运行时资源编号。
生产 `SOUND_*` 全局保存音效后端的整数 handle，例如 Windows
`DSoundManager` 用它直接索引 `mSourceSounds`，并非把声音指针反查为 ResourceId。
Vulkan 的 `SlotOf` 直接读取 texture 对象中的 `slot` 并合并采样标志，
也不是指针到 ID 的哈希查找。`SharedImageMap` 是以 `(路径, variant)` 为键的
加载共享缓存，用于资源复用和生命周期管理；它不承担旧存档指针 ID 转换。
对应路径见 `platform/windows/audio/DSoundManager.cpp`、
`graphics/vulkan/VulkanRenderer.cpp` 与 `SexyAppBase.cpp`。
Windows 的 BASS 当前设备按线程选择。音乐加载线程在创建 MOD／stream
之前显式选择播放输出设备；音效解码单独使用无声设备 0，并恢复原设备。
PAK64 使用 Stored ZIP，不加密，不含 DXIL/SPIR-V 条目。
AU 解码和 MO3 播放是格式处理，不是包加密。
原生图片、音频与 UI 数据已收拢到 assets/runtime；编译器和 DSL 也已收拢，
正常主构建不依赖相邻 openPvz 或原版安装目录。
历史算法对拍脚本可以保留其明确指定的旧对照输入，不属于正常游戏构建依赖。

引用选择使用编译资源清单、字体图层和游戏源码中的路径，
图片依赖盘点覆盖两类 DSL 和代码；未选入项记录在缓存 manifest 中。
旧 build_numeric_runtime_pak64.py 的历史包转换模式仍按排除清单保留旧条目，
它不再充当主构建的资源选入策略。

### AI 成品与原生源资源分离

10 张已经确认来源的在用 AI 成品存放在 `assets/source-art/ai/runtime/`，
从 `assets/runtime/` 移出时逐文件 SHA-256 不变，没有重编码。
`assets/source-art/ai/runtime-overlay.json` 显式映射游戏虚拟路径到这些成品；
`tools/runtime_asset_sources.py` 统一解析原生输入和这份映射，重名或缺失会报错。
打包仍只选实际引用闭包，QuickPlay 源图不进入运行包；不会扫描并打包 AI 原件、
提示词、蒙版或试验图。AI 原始返回文件继续单独保存在 `assets/source-art/ai/`。
当前 aquarium1 和主界面原生拼合层没有误归类成 AI，不随这次迁移移动。

### QuickPlay 源素材保存与运行时退役

QuickPlay 界面代码及其 12 个图片绑定已删除。以下原始 PNG 继续保存在
`assets/runtime/images/`，供后续素材工作使用；它们没有运行时资源 slot，
不进入 NRSC4 加载计划或当前 PAK。作者清单保留源标签不会自动保留发行条目。
已发布版本的包与 tag 保持不变，历史策略以对应 tag 的文档和验证器为准。

| 保留的源图片 |
| --- |
| selector_morewaystoplay_background.png |
| quickplay_minigames_button.png |
| quickplay_minigames_button_highlight.png |
| quickplay_minigames_cloud.png |
| quickplay_puzzles_button.png |
| quickplay_puzzles_button_highlight.png |
| quickplay_puzzles_cloud.png |
| quickplay_survival_button.png |
| quickplay_survival_button_highlight.png |
| quickplay_survival_cloud.png |
| quickplay_back_button.png |
| quickplay_back_button_highlight.png |

构建后运行 `python -B tools/verify_runtime_resources.py <Release/main.pak64>`
验证 NRSC4 与当前具名绑定一致、所有数值记录及文件闭包、字体 glyph，
并检查上述 12 张源图存在且不在包内或加载描述符中。工具检查全部
程序图片、字体及图集的引用闭包和尺寸均与当前生成元数据一致。
当前验证器仅校验包与当前作者清单，不提供历史 `--predict-retirement`
分支；检查已发布 v1.3 时使用该 tag 自带的验证器。

Tests/reference/render-groups/source-baseline.tar.gz 是旧渲染分组审计的默认输入，
旧资源包位于 Tests/reference/legacy1073-resources；它们不进入当前发行包。
若撤除此历史审计，应同时移除或改造其入口，不能只删除夹具而留下失效工具。
总账实际引用的精选原始记录位于 Tests/evidence；根 log 为可清理的临时输出。

## 无尽基准来源

Tests/game3_13.dat 来自原版使用的
C:/ProgramData/PopCap Games/PlantsVsZombies/userdata/game3_13.dat，
经临时测试夹具复制后纳入 Tests，未改写游戏状态。
Tests/game3_58.dat64 由项目转换器生成。
当前为进行中状态，不是选卡检查点；用户已确认它是典型出怪组合，
可直接用于正式性能测试。每轮恢复基准副本，不使用试玩推进后的状态。
校验值和使用规则见 [Tests 说明](../Tests/README.md)。
