# TV 版选中框素材调查

调查日期：2026-11-03。来源为本机 TV_HD_1.3.4_NoBushes 解包素材。调查阶段仅保留源素材；后续按用户选择接入蓝色版本，实施状态见下方。

| 原资源 ID | 文件 | 像素尺寸 | 形状与用途证据 |
| --- | --- | --- | --- |
| GAMEPAD_CURSOR_FRAME | board_selector.png | 104×144 | 四个分离的拐角，颜色不统一；中心 alpha=0 |
| GAMEPAD_CURSOR_FRAME_BLUE | board_selector_BLUE.png | 104×144 | 四个统一蓝／青色拐角与黑描边，中心 alpha=0 |
| SEED_SELECTOR | seed_selector.png | 60×80 | 黄色完整矩形卡片边框，中心透明 |
| SEED_SELECTOR_BLUE | seed_selector_BLUE.png | 60×80 | 蓝色完整矩形卡片边框，中心透明 |

资源声明来自 properties/resources.xml 第194、195、206、207行。文件及声明来源 SHA256 保存在 [素材证据](../../Tests/evidence/tv-selection-frame-20261103/manifest.json)。这些是原始解包图片，没有 AI 重画，也没有修改像素。

桌面布局采用蓝色，不做绿色着色；横板布局后来采用 WP 黄色选框，现行规则见[触控说明](../touch-input.md)。`board_selector_BLUE.png` 原样保存到 `assets/runtime/images/touch_selection_frame_blue.png`；最初独立加载，现位于 CommonUIAtlas，四角UV由资源编译器生成。四个角的源像素范围分别是 (0,0,42,36)、(62,0,42,36)、(0,108,42,36)、(62,108,42,36)，各自以 21×18 逻辑像素绘制，位置依据实际卡槽和铲子按钮计算。

已经验证 800／1400 逻辑宽度下卡牌与铲子框的绘制，与独立像素裁切参考逐像素一致。参见 [触控说明](../touch-input.md) 和 [本机验证](../../Tests/evidence/touch-selection-rewards-20261103/README.md)。

## 绘制层级核对（2026-11-04）

读取本机 TV HD 1.3.4 无草丛和 TV 1.1.1 英语优化版 APK 的 armeabi/libGameMain.so，以 NDK llvm-nm 查动态符号、llvm-objdump 强制 Thumb 模式反汇编，并由 ELF32 PT_LOAD/GOT 实际解析图片引用。样本没有安装或执行；两者都带修改版名称，不据此覆盖所有TV发行版。

- 两个样本的 `IMAGE_SEED_SELECTOR`、`IMAGE_SEED_SELECTOR_BLUE` 都在 `SeedPacket::DrawBackground` 中被使用。卡框是卡牌自身绘制的一部分，随后还绘制卡面与覆盖内容，并非在全部战场对象之后统一补画。
- `Board::DrawUIBottom` 调用 `SeedBank::Draw`。1.3.4在0x160358/0x160490调用0x1772ec；1.1.1在0x174a7c/0x174bb4调用0x18ba14。卡牌随种子栏进入相应UI渲染层。
- 我们选用的四角素材是另一套 `IMAGE_GAMEPAD_CURSOR_FRAME_BLUE`：TV版中它由 `BaseGamepadControls::DrawFrame` 绘制，包含战场网格位置计算。`Board::DrawGameObjects` 在两个渲染项分支中以FrameRenderPhase=0/1调用它，不能将该落点光标与卡牌选中框视为同一个层级规则。
- 修复前本项目将适配后的四角卡框统一放在 `Board::DrawUITop` 末尾，而种子栏在 `DrawUIBottom`；这解释了卡牌被巨人覆盖时，蓝框仍浮在巨人上面的现象。该顶层接法来自本项目，并不是已经核实的TV卡框行为。桌面布局已将卡槽／铲子／花园工具四角框移到 `DrawUIBottom`，场上掉落卡牌的框在其 `Coin::Draw` 内绘制；它们随所属对象一起被后续对象遮挡，不增加独立渲染项或全池扫描。拖动准心仍在顶层。

原生库SHA256、函数地址、GOT解析结果及相关反汇编保存在 [绘制顺序证据](../../Tests/evidence/tv-selection-frame-20261103/draw-order-20261104.json)。若调整，应让卡牌/工具选中框跟随所属对象所在的绘制层；保持现有图集四角绘制即可，不需要额外全池扫描、独立排序项或触控线程。
