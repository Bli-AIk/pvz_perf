# 15种生存模式的换轮与对象保留检查

`SurvivalRulesRuntime` 对GameMode 56..70分别运行100、30、240Hz及不规则纳秒步长，共60个真实Board场景。每种难度分别覆盖白天、夜晚、泳池、浓雾和屋顶。

已检查初始50阳光、普通每阶段10波／困难与无尽20波、正确背景与行数；全出怪表排除地形不兼容类型和十旗之前的红眼，旗帜僵尸按旗帜波特例处理。通过实际调度器生成第一波，再构造末波编号，调用实际末波生成和死亡回调进入换轮。

换轮流程使用真实`LawnApp::CheckForGameEnd → Board::InitSurvivalStage → SeedChooserScreen`，不只把scene直接改成选卡：

- 末波结束后保留五秒横幅等待，期间Board时间继续推进；等待结束才进入LEVEL_INTRO并停止Board时间。
- 保留原植物实例、动画、阳光和割草机。屋顶分别检查未购买清洁车时没有、购买后有五辆；复选卡不重建小车。
- 复选卡期间，Visual继续推进，保留植物的相位和攻击期限保持不变。
- 通过真实`ClickedSeedInChooser`开始卡片飞行动画，`Update`使卡片落入槽位，`CloseSeedChooser`回填选择和旧冷却起始时间，再调用实际`StartPlaying`恢复战斗。旧冷却不消耗选卡期间的墙钟时间。
- 恢复后Board时间和原动画继续，普通暂停再次冻结。
- 普通／困难显式构造零基阶段3、4，验证前一阶段仍复选卡，第五阶段分别累计五／十旗并发奖，不再复选卡。无尽显式构造阶段5，仍不是最终阶段，继续核对更后期出怪表。

1073的换轮场景写入与主计数门控沿用[原版只读反汇编证据](../survival-repick-clock-1073-20261030/retail-disassembly.txt)：`0x40D64F`切换场景，`0x4183B0`不在普通战斗场景增加关卡主计数。原始证据、现代纳秒测试和人工流程分别保留，不把其余规则声称为本次重新执行原版整局。

初始卡槽直接提供一组合法选卡，植物和屋顶花盆直接添加；中间战斗跳过，末波敌人用真实`DieNoLoot`清除，不声称自动打过五阶段。最后奖励检查不调用会销毁Board的最终应用跳转；第一轮实际CheckForGameEnd和持久化回调在Temp私有用户目录中执行，不覆盖玩家存档。

定向入口：`tools/test-image-runtime.ps1 -BuildDirectory out/x64-win32-vulkan -Case survival-rules -OutputDirectory $env:TEMP/pvz-survival-rules-runtime`。`capture.py <log> <test-exe> <pak>`只接受完整图像套件也通过的输出，核对60个唯一组合、背景、波数和终局类别，保存源码、exe、PAK哈希。

本次60项没有发现新的生产代码缺陷；不据此保证所有随机波表、每一种植物与地形组合、保存中断点或最终奖励界面。冒险、隐藏模式、花园和智慧树另行核对，本证据不产生性能结论。
