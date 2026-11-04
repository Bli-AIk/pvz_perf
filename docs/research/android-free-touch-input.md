# Android Free 触控二进制调查

调查日期：2026-11-03。只读分析本地 APK、DEX 和 ELF，没有安装、执行样本或修改当前游戏。

Free 可以直接从 APK 调查，不依赖完整反编译工程。已在 Free 3.18.0 样本的原生库确认：
Board 按触点标识维护多个 TouchInfo，每个槽分别持有 CursorObject 与 CursorPreview。
这份战场实现比 WP 参考代码的单操作状态更适合独立多点接入。

## 样本与方法

| 样本 | 身份与用途 |
| --- | --- |
| Free 3.18.0 | 本地 base.apk 与 arm64 split；libpvz.so 保留大量 C++ 动态符号，适合追踪 Board 和框架输入 |
| Free HD | 用户提供的 Plants vs. Zombies HD_2026.10.6.apk，实际 Manifest 版本 26.10.7；有 Fairy 封装，不能把全部代码当作未修改的 EA 发行版 |

Free 3.18.0 原生库为 50,117,680 字节，SHA-256
`079a0c71a67ae61fa98cd797dbb527e1c16fa0e409fe7f980c459afe1488dff5`。
HD APK SHA-256 为
`e61750680bfdc10b3f8a2f88e3c6851367d568e8af4b7a28292eabd3982fbedd`。
完整身份与函数地址见[来源清单](../../Tests/evidence/android-free-touch-20261103/source-manifest.json)。

用 NDK llvm-nm 核对符号、llvm-objdump 核对 AArch64 指令，用 Android SDK dexdump 检查 Java 方法声明。
这里确认的是二进制控制流与数据访问，不把函数名或字符串单独作为行为证明，也没有恢复完整 C++ 工程。

## 已确认的原生战场多点

| Free 3.18.0 函数 | 虚拟地址 | 已核对指令行为 |
| --- | --- | --- |
| PvZApplication::OnPointerDown | 0x161c794 | 接收 unsigned int 指针标识，写入 Sexy::Touch；检查多点开关，未开启时对已有触点作限制 |
| Board::GameModeSupportsMultiTouch | 0x19e8100 | 根据模式返回支持与否；该样本模式值 19、20、24、27 返回 false，未用其他版本枚举猜测名字 |
| Board::EnableMutiTouch | 0x19e81a4 | 结合模式支持结果调用 SetMultiTouchEnabled；拼写 Muti 是原符号 |
| Board::InitTouches | 0x19e731c | 初始化 11 个 TouchInfo 槽，每槽步幅 40 字节 |
| TouchInfo::Init | 0x19e5054 | 各槽独立准备 CursorObject 和 CursorPreview，指针已存在时复用 |
| Board::NewTouchInfo | 0x19fe13c | 扫描空闲槽，把传入 Touch 的标识写入该槽 |
| Board::GetTouchInfo | 0x19fe1fc | 将传入标识与各槽标识比较，返回匹配上下文 |
| Board::TouchBegan | 0x19fe3ac | 建立触点上下文后处理资源拾取、种子与工具等操作 |
| Board::TouchMoved | 0x1a00ac8 | 按标识选中对应上下文，只更新该上下文的操作位置与相关状态 |
| Board::TouchEnded | 0x19ff6d4 | 进入独立的 ProcessTouchEnded 操作提交路径 |
| Board::RemoveTouch | 0x19fe2a0 | 按触点清理状态 |
| Board::RemoveAllTouches | 0x19fe344 | 清理全部触点 |

关键证据：
[按标识建立槽](../../Tests/evidence/android-free-touch-20261103/Board_NewTouchInfo.asm.txt)、
[按标识查询槽](../../Tests/evidence/android-free-touch-20261103/Board_GetTouchInfo.asm.txt)、
[独立光标与预览初始化](../../Tests/evidence/android-free-touch-20261103/TouchInfo_Init.asm.txt)、
[移动时选择上下文](../../Tests/evidence/android-free-touch-20261103/Board_TouchMoved.asm.txt)。

40 字节 TouchInfo 的指令访问可辨认出标志、操作坐标、两个对象指针和标识。Board 保存当前操作上下文指针，
收到某触点事件时先选择该上下文，再复用操作逻辑。无需把全部操作状态退化为一个全局鼠标位置。
11 个内部槽不等于设备保证支持 11 根手指，也不证明每个模式都允许全部槽同时使用。

Sexy::WidgetManager::TouchBegan 仍保留单个捕获控件，并向它分发 Touch 指针。
Free 的战场利用自己的 Touch 覆写处理多点，因此不能把控件树的单捕获直接等同于战场只有单指，
也不能反过来把战场的多点支持推广成所有 UI 控件都可独立多点。

## HD 封装层线索

HD DEX 中 `com.ea.blast.TouchSurfaceAndroid` 提供：

- `IsTouchScreenMultiTouch()`：实际调用 PackageManager.hasSystemFeature，检测 touchscreen.multitouch。
- `NativeOnPointerEvent(IIIFF)V`：保留三个整数与两个浮点参数；参数含义需结合调用方与 JNI 核对。
- Down、Move、Up、Cancel 四类事件 ID 的原生入口。

对应[DEX 反汇编](../../Tests/evidence/android-free-touch-20261103/freehd-TouchSurfaceAndroid-dexdump.txt)。
这些证明封装有多点相关接口，不能单独证明 HD 内部 Board 与 3.18.0 完全相同。

## 布局的进一步入口

Free 3.18.0 仍保留 `Board::GetSeedPacketPositionY`、`SeedBank::Draw`、`SeedBank::MouseHitTest` 等符号，
可以继续定位横卡纵排、命中范围、工具坐标、选卡飞入以及传送带的具体实现。

现有 FREE_UI_Integration_960 包提供卡面、阳光、工具、传送带和面板素材，但来源是独立解包模板，
并非与 HD APK 逐项相同的运行时布局证据。资源名、源档位与图片尺寸不能直接当作逻辑坐标或触控热区。

## 本项目的直接参考价值

沿用每触点操作上下文的思路，平台事件保留稳定 ID，战场按 ID 选择自己的卡片、工具、位置与预览。
鼠标可以作为另一个操作来源，但要保留鼠标按钮、悬停和取消等既有语义。
普通控件的触控捕获与战场上下文分开管理，不照抄 Free 的单捕获限制。

目前已确认多点实现的结构，尚未把各特殊模式的冲突规则、同卡竞争与取消行为全部逆向或实机验证。
本次不改变当前存档、输入或 HUD 实现。

## 场上掉落卡牌

追加核对仍针对同一 Free 3.18.0 ELF，未安装其他样本或修改运行代码。卡牌沿用 `COIN_USABLE_SEED_PACKET`，不是阳光式移动扫取，也没有在这条领取路径中转入卡槽托盘。

- `Coin::Collect` 的落卡分支只记录当前操作光标的植物类型、来源 CoinID 和光标类型，原 Coin 仍是来源对象。
- `Coin::MouseHitTest` 对 `IsPlantsFromCoinsLevel()` 放开已选掉落卡牌及手持卡牌时的命中，使已选卡和别的卡仍可点到。
- `Board::TouchBegan` 先处理 Challenge；命中卡牌后，在已有场上卡选择时比较来源对象：点同一张调用 `DroppedUsableSeed`、清光标并忽略抬指；点另一张先归还旧卡，再对新卡调用 MouseDown。手持卡槽植物时则先归还卡槽选择。
- 拿起卡牌后设置本触点的忽略抬指标记；`TouchMoved` 对落卡光标解除该标记，随后更新操作位置。没有移动的首次点击不会直接把卡种在卡片掉落处，拖动后才走抬指种植。
- `DroppedUsableSeed` 清除已选状态、处理原有消失计时并增加归还次数，没有创建另一个卡牌对象。

地址、指令与来源哈希见 [field-card-manifest.json](../../Tests/evidence/android-free-touch-20261103/field-card-manifest.json)。这是二进制控制流结论，未把所有重叠卡片或拖放边界做 Free 实机验收。

WP 参考代码同样专门保护落卡：`Coin.Collect` 设置 `mIgnoreNextMouseUpSeedPacket`；`Board.MouseDown` 在拿卡后忽略抬指，`MouseDrag` 对落卡解除这一步限制；`MouseUp` 还会检查是否仍在卡牌上，防止选卡点击顺带种植。`Coin.Draw` 将已选来源卡画灰，来源卡仍保留在场上。WP 这份代码包含后续移植修改，以上只归于固定参考提交；没有据此宣称它的切换、取消和重叠规则与 Free 相同。

当前项目需要补的是场上卡牌的手势，而非收集规则：`Board::TouchEnded` 对任意植物光标直接尝试种植，轻点落在草坪内的掉落卡会尝试原地种植；`IsSelectedControl` 仅支持卡槽和铲子，持有植物时 `TouchBegan` 也不会再分发普通拿卡调用，因此缺少场上同卡取消和直接换卡。后续已用既有 CoinID 接入单一共享选择，补上轻点／拖放区分、同卡取消、直接换卡和罐子手势隔离，保留普通鼠标路径。实现与本机验证见 [触控说明](../touch-input.md#场上掉落卡牌)。

## 种植横竖白条的显示条件（2026-11-04 补核）

同一 3.18.0 原生库的 `CursorObject::DrawGroundLayer`（0x1a743c4）对植物光标值 1／2，先计算种植格，再调用 `Board::CanPlantAt`。0x1a744c4 调用之后，非零结果经 0x1a744d0 直接跳到函数收尾 0x1a74718，因此整组行列提示都不绘制。通过检查后才进入行列绘制循环（调用点 0x1a7462c、0x1a746ac），并非只把不能种植的单格从条带里扣掉。铲子另有目标命中及忽略抬起状态检查。

见[原始指令](../../Tests/evidence/android-free-touch-20261103/CursorObject_DrawGroundLayer.asm.txt)和[核对清单](../../Tests/evidence/android-free-touch-20261103/placement-guides-manifest.json)。本项目后续已接入种植合法性过滤，直接复用 CursorPreview 更新结果；按用户要求，铲子只保留目标植物发亮，不绘制白条。

## 禅境花园工具

仍以同一个 Free 3.18.0 arm64 ELF 为依据，以下是静态控制流结论。

- `Board::TouchBegan` 在花园模式下扫描已有触点光标，将光标类型映射为工具，取得其工具栏矩形；再次按下该原图标会调用 `ClearCursor(false)`，直接结束这次按下处理。
- `Board::ProcessTouchEnded` 从当前触点取得坐标，检查是否仍位于所持工具的原图标。留在图标内直接返回，保留选择；离开图标后调用 `MouseDownWithTool`。因此既支持轻点工具后点目标，也支持从图标拖到目标后松开使用，不在经过每棵植物时反复施用。
- `ZenGarden::MouseDownWithTool` 将水壶、肥料、杀虫剂、唱片机、巧克力送入喂养路径；手套记录目标植物 ID，并切换到 `PLANT_FROM_GLOVE`，后续再放置。出售、空手推车装入和有货手推车放置分别处理。
- 使用喂养工具后会清光标；不能把保留选择理解成工具永久保持激活。

Free 的花园取消分支会扫描多个触点光标，不能据此认定其工具选择严格全局唯一。目前没有完成所有多工具冲突的逆向，也没有确认它使用哪张选中边框图片。本项目按用户指定采用触控全局唯一选择和 TV 蓝色四角框，不将这两项宣称为 Free 原样行为。

地址、原始指令与样本哈希见 [garden-manifest.json](../../Tests/evidence/android-free-touch-20261103/garden-manifest.json)。本机实现与回归见 [花园触控](../touch-input.md#花园工具)。


## 选卡滚动控件与模仿者（2026-11-04 补核）

3.18.0 的 `SeedChooserScreen::GetSeedPositionInChooser`（0x1c377a4）对种子 48 单独从模仿者 GameButton 读取 X/Y；其他种子委派给 `SeedPacketsWidget::GetSeedPosition`（0x1c366f4），后者由列数、牌宽高和间隔计算行列位置。因此模仿者不是普通滚动牌表的最后一项。本地 Free HD 资源另外提供 142×112 的模仿者扩展底板、108×70 的模仿者卡面和 494 宽的选卡面板。当前项目采用独立右侧扩展区；本项目坐标属于适配值，没有将其冒充为 Free 各屏幕档位的原始坐标。

原图像素与逻辑尺寸需要区别：本资源包的卡面为 108×70。本项目为满足 800×600 下一点对一点，改用 WP 移植包内原生 81×53 图像，而不缩放这份 Free 960 资源。不能仅从原图尺寸推断 Free 的逻辑高度。
