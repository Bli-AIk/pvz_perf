# Windows Phone 触控与横板布局调查

调查日期：2026-11-03。

Windows Phone 参考实现通过 XNA 读取多触点集合，但没有将触点身份保留到控件和战场。
它可以作为单指拖放与左侧卡槽的参考，不能直接作为独立多点触控的实现。本项目在调查起点的
Android 输入同样按单指鼠标处理；当前已接入独立触点事件，实施状态见 [触点输入](../touch-input.md)。

## 证据范围

核对 [PvZ Windows Phone decomp](https://github.com/IsraelDXPP/PvZ-Windows-Phone-decomp/tree/26cbb77df48c674dca6cb6b1fa9ad6783903a5c4)，
固定提交 `26cbb77df48c674dca6cb6b1fa9ad6783903a5c4`。
其 [README](https://github.com/IsraelDXPP/PvZ-Windows-Phone-decomp/blob/26cbb77df48c674dca6cb6b1fa9ad6783903a5c4/README.md)
说明源码来自 WP7/8 的托管 LAWN.dll，随后移植到现代 .NET/MonoGame，窗口缩放和鼠标模拟触控等包含后续修改。
以下结论针对这份反编译参考链路；没有对原 WP 发行 DLL 的 IL 逐方法复核，不能把新增移植行为归于原版。

另核对本地 PlantsVsZombies.NET。其 Board 已增加专门的 Touch 覆写，不能拿它代替这份 WP
参考版的 Board 路径。本次保留的远端文件哈希见
[来源清单](../../Tests/evidence/windows-phone-touch-20261103/source-manifest.json)。

## 输入链路与多点支持

链路为 `TouchPanel.GetState → Main.HandleInput → SexyAppBase.Touch* → WidgetManager.Touch* → Widget → Board`。

| 层级 | 已核对行为 | 含义 |
| --- | --- | --- |
| XNA 读取 | `HandleInput` 遍历整个 `TouchCollection`；`InputController.TestTouchCaps` 读取 `MaximumTouchCount` | 输入 API 有多点能力，不能据此认定业务已支持独立多点 |
| 触点事件 | `_Touch` 只有当前位置、前一位置、点击次数、时间和 phase，没有 ID；构造时不传 `TouchLocation.Id` | 触点进入游戏后丢失身份 |
| 按下分发 | 每次 `HandleInput` 通过局部 flag 最多发送一个新 Pressed；Moved 和 Released 仍遍历下发 | 不是各触点各自完整的事件序列 |
| 控件捕获 | 只有一个 `mLastDownWidget`，复用鼠标按键位、鼠标位置和 `mIsDown` | 没有逐触点 capture；多指可能修改同一状态 |
| 取消 | 参考版 `TouchesCanceled` 为空 | 不能直接沿用为本项目的中断清理 |
| 战场 | 一套 `CursorObject`、工具位置与忽略松手标志 | 没有多根手指独立拿卡、拿工具的操作上下文 |

代码定位：
[Main.HandleInput](https://github.com/IsraelDXPP/PvZ-Windows-Phone-decomp/blob/26cbb77df48c674dca6cb6b1fa9ad6783903a5c4/Sexy/Main.cs#L439)、
[_Touch](https://github.com/IsraelDXPP/PvZ-Windows-Phone-decomp/blob/26cbb77df48c674dca6cb6b1fa9ad6783903a5c4/Sexy/_Touch.cs#L3)、
[WidgetManager 触控分发](https://github.com/IsraelDXPP/PvZ-Windows-Phone-decomp/blob/26cbb77df48c674dca6cb6b1fa9ad6783903a5c4/Sexy/WidgetManager.cs#L604)、
[触控能力读取](https://github.com/IsraelDXPP/PvZ-Windows-Phone-decomp/blob/26cbb77df48c674dca6cb6b1fa9ad6783903a5c4/Sexy/InputController.cs#L10)。

因此，参考实现能够收到多点数据，但业务依然基本按单触点编写。第二根手指的事件被读取，
不等于两根手指能独立完成拖放；仅把每个原生触点循环转发为 MouseDown/Drag/Up 也不能补齐。

## 单指操作语义

`Widget.TouchBegan/Moved/Ended` 默认分别转为 `MouseDown/MouseDrag/MouseUp`。
这份参考版 Board 没有覆写 Touch 方法，其鼠标处理函数本身已经适配触摸：

- 按下卡片时选择植物，拖动时更新工具位置与种植预览，松手后执行种植。
- 资源拾取在按下阶段处理，并通过忽略松手状态避免随后误种植物。
- 铲子与玉米炮瞄准按各自的松手规则执行，复用同一套工具位置。
- 原生坐标先换算为游戏坐标，再换算为捕获控件的局部坐标；拖动不会因移出起始控件而失去目标。

对应
[Widget 默认触控适配](https://github.com/IsraelDXPP/PvZ-Windows-Phone-decomp/blob/26cbb77df48c674dca6cb6b1fa9ad6783903a5c4/Sexy/Widget.cs#L353)、
[Board.MouseDown](https://github.com/IsraelDXPP/PvZ-Windows-Phone-decomp/blob/26cbb77df48c674dca6cb6b1fa9ad6783903a5c4/Lawn/Board.cs#L2217)、
[Board.MouseUp](https://github.com/IsraelDXPP/PvZ-Windows-Phone-decomp/blob/26cbb77df48c674dca6cb6b1fa9ad6783903a5c4/Lawn/Board.cs#L2326)。

这与本项目 PC 版按下时种植的部分路径不同。菜单按钮可以继续共享命令，战场手势需要独立
选择提交时机，不能只增加平台 MouseUp 就宣称完成触控。

## 种植横竖白条的显示条件（2026-11-04 补核）

固定参考提交的 `CursorObject.DrawGroundLayer` 先检查光标。对 `CURSOR_TYPE_PLANT_FROM_BANK`，调用种植坐标换算及 `CanPlantAt`；结果不是 `PLANTING_OK` 时立即返回，整组横竖提示都不显示。通过后才遍历整行与整列绘制高亮。该条件只写在卡槽光标分支中，其他来源的植物不能直接据此认定相同。铲子则在忽略抬起或 `ToolHitTest` 无目标时返回。

源码：[CursorObject.cs](https://github.com/IsraelDXPP/PvZ-Windows-Phone-decomp/blob/26cbb77df48c674dca6cb6b1fa9ad6783903a5c4/Lawn/CursorObject.cs)。本地源码哈希已与既有来源清单一致；这里仍是反编译移植代码结论，未追加原 WP 发行 DLL 的 IL 核对。本项目后续已复用现有种植预览结果过滤非法目标；按用户要求，铲子只保留目标植物发亮，不绘制白条。

## 横板布局

WP 参考版使用左侧竖排卡槽，而非把 PC 顶部整条种子栏旋转：

- SeedBank 高度随可绘制高度；卡片沿 Y 排列，间距按卡片数量调整。
- 传送带沿 Y 推进，绘制与裁剪区域也改为纵向。
- 阳光数和铲子使用独立 HUD 坐标；卡槽、HUD 与战场分别处理画面偏移。
- SeedBank 命中先限制侧栏范围，再按 Y 选择最近卡片，交互范围不只是贴图矩形。
- 参考数组是 9 个卡槽，本项目需要保留现有 10 槽规则，不能照抄容量与间距。

对应
[SeedBank 尺寸与绘制](https://github.com/IsraelDXPP/PvZ-Windows-Phone-decomp/blob/26cbb77df48c674dca6cb6b1fa9ad6783903a5c4/Lawn/SeedBank.cs#L17)、
[侧栏命中](https://github.com/IsraelDXPP/PvZ-Windows-Phone-decomp/blob/26cbb77df48c674dca6cb6b1fa9ad6783903a5c4/Lawn/SeedBank.cs#L162)、
[卡片纵向位置](https://github.com/IsraelDXPP/PvZ-Windows-Phone-decomp/blob/26cbb77df48c674dca6cb6b1fa9ad6783903a5c4/Lawn/Board.cs#L4806)、
[HUD 坐标](https://github.com/IsraelDXPP/PvZ-Windows-Phone-decomp/blob/26cbb77df48c674dca6cb6b1fa9ad6783903a5c4/Sexy/Constants.cs#L1839)。

本项目可按之前确定的方向，使用实际左侧可用宽度决定是否切换侧栏，窄布局继续使用顶部种子栏。
阈值需要按 10 槽、阳光、工具和最小触控范围计算，不能只判断是否手机或屏幕是否横向。
绘制、命中、选卡飞入落点、冷却遮罩、传送带、提示箭头与资源收集终点必须使用同一份布局结果。
战场逻辑坐标保持原状，布局不应修改植物和僵尸的位置。

## 本项目接入范围

当前 AndroidWindow 只读 `AMotionEvent_getX/Y(event, 0)`，用 pointer index 0 决定按下／松开，
维护一个 `gPointer`，再发送 MouseButton。触点数组下标不是稳定 ID，其他手指抬起后仍可能重新编号。
[Android 官方多点说明](https://developer.android.com/develop/ui/views/touch-and-input/gestures/multi)
明确区分 pointer index 和整个手势内稳定的 pointer ID。

NativeWindowEvent 目前没有触点 ID、来源和触控阶段；WidgetManager 使用一套鼠标捕获；Board 使用一套
CursorObject/Preview。这三层均需调整，不能仅改 Android 回调。

接入分为三层：

1. 平台事件保留来源、设备与稳定触点 ID，显式区分 Down/Move/Up/Cancel。Android 使用 pointer ID，
   Windows 独立接收触控消息，并消除同一触摸再被系统提升为鼠标事件造成的重复操作。保留物理鼠标。
2. 控件管理器按触点保存捕获目标和坐标。移出控件继续向原目标分发；窗口失焦、切后台、弹窗、
   控件移除与旋转／布局切换必须取消相关手势。连续 Move 可按 ID 合并，但不能打乱 Down/Up/Cancel。
3. 战场按操作来源保存选卡、工具与预览上下文。多触点不能共用一个 CursorObject 随意覆盖。
   所有输入仍在游戏线程顺序执行，提交时重新检查阳光、冷却与目标有效性，避免两个操作消费同一资源。

Windows 需要继续支持 Win7，可使用 WM_TOUCH 作为共同入口，而非无条件依赖 Win8 才有的指针 API。
注册和读取要求见
[RegisterTouchWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registertouchwindow)、
[GetTouchInputInfo](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-gettouchinputinfo)。
实现还必须正确关闭触控输入句柄，并在只处理触摸提升消息时过滤重复鼠标消息。

触点、capture、拖动预览和 HUD 布局属于瞬时运行状态，可以放在管理器或运行时侧表。接入方案不要求
修改 DAT64 v4 的序列化字段；若实际改动触及任何已序列化对象布局，仍须单独核对转换器与版本。
新增数据直接 public，事件及大结构按 const& 传入，函数实现放 .cpp。

上述内容保留原始调查结论和接入建议；已落地的原生触控、全局唯一选择、花园交互和横板布局以[当前实现](../touch-input.md)为准。


## 原生横卡与阳光栏尺寸（2026-11-04 补核）

`AtlasResources_480x800.cs` 的 SeedAtlas 区域表给出卡面总图 324×689、4 列 13 行，即每张原始图像为 81×53。`Constants.cs` 中 SMALL_SEEDPACKET_HEIGHT=52 是布局常量，不等于图像像素高度。原生阳光栏为 143×44，价格底片 33×20；因此在本项目 600 高内，可按阳光栏 44＋顶部间隔 2＋十卡 530＋九个间隔 18＝594 排下，不需要缩放。

对应原始 PNG 已在本机 `wp移植版0.0.7.1.zip/Assets.zip/Content/images/480x800/SeedAtlas.png` 核对；这份 PNG 为 996×814。按原始区域裁出后逐像素保存，不使用旧精华 1.5.3 的混淆 XNB 像素，也不把 Free 960 的 108×70 卡面缩成 81×53。可复核来源与区域见[清单](../../assets/source-art/landscape-ui/native-source.json)。
