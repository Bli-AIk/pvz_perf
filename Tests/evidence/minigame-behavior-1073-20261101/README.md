# 1073 加速关卡、鱼缸与失败时钟核对

只读核对英文 GOTY 1.2.0.1073 发行 EXE，SHA256 为 `5f650f52d12751e08b63e590edb0d9b9f09d087cea900baa0eb39e4af8eda251`。地址为首选映像 VA；重构源码中的旧版本函数地址不作为证据。

- **Zombie Nimble Zombie Quick**：`004188DF` 先调用 `Board::UpdateGame`，`004188F0` 调用 `Challenge::Update`；后者在 `004273B1` 判断模式 29，并在 `004273BD` 再调用同一个 `004181E0`。所以原版是两次普通对象更新。独立特效调用在 `00418750`，水面计数加一在 `0041887D`，都位于外层，只执行一次。
- **进家失败**：`004160C0` 先写入场景 4，再调用 `WalkIntoHouse`。`004183B0` 的关卡主计数门控只接受场景 3（以及展示关卡例外），因此随后失败过场不再推进正常关卡计数。
- **Zombiquarium 输入**：`00424C2E` 比较点击值与 -1，右键分支只播放敲玻璃音效；非负左键在命中空场地时通过 `00424C5C` 调用喂食函数。
- **脑子限制与价格**：`0042AAB4` 计数类型 6 的脑子，`0042AACA` 检查小于 3；`0042AAD2` 传入价格 5，`0042AAD7` 调用扣阳光，成功后才在 `0042AAEC` 创建脑子。最多三个未吃掉的脑子、每个五阳光都是原版规则。

复现：安装 Python 的 `pefile`、`capstone` 后运行 `python extract.py > retail-disassembly.txt`。脚本校验 EXE 哈希，只解码已核实的局部指令范围，不运行或修改原程序。项目对应回归位于 `Tests/native/ImageRuntimeTests.cpp` 的 `MinigameBehaviorRuntime`，使用真实 Board、Challenge、Plant、Zombie 和特效代码。

额外核查三叶草吹走气球的掉落：通过真实 `BlowAwayFliers → Zombie::Update → DieWithLoot → DropLootPiece → CoinInitialize`，在泳池六行分别以30／100／240Hz运行。金币出生高度均与源僵尸行相符；例如100Hz六行的出生Y分别为72、155、240、323、409、495，只有最后一行的落地Y被钳制到521。此回归尚未复现用户所报“各行金币一出现都在最后一行”，不能据此宣称该现场问题已修复，也未为此更改生产掉落逻辑。
