# 3.3 beta 与 v3.2.4 整局观察对比

本轮按 Enter 开始、Quit 结束。Vulkan、LP14（0x4000）、240FPS 上限、用户设置11x；实际CPU/GPU频率没有独立记录。起点状态、EXE哈希与配置分别见 checkpoint.json、measurement.json、settings.json。只选择 valid=1、segment=game 的前台战斗样本。

| 指标 | v3.2.4（2026-11-01） | 3.3 beta 原生触控（本轮） |
|---|---:|---:|
| 有效时间 | 359.000秒 | 359.001秒 |
| 绘制FPS | 196.736 | 175.504 |
| CPU，单核=100% | 89.550% | 90.364% |
| CPU ms/绘制帧 | 4.55180 | 5.14882 |

帧率观测下降10.79%，CPU ms/帧增加13.12%。该CPU指标是整个进程累计CPU时间/绘制帧数，不是主线程阶段计时，更不是单帧延迟。

comparison.png/svg 按 Board 时间相对共同起点2291960000000ns对齐，无平滑。comparison.json 包含按波段加权结果，重建命令：

```powershell
python Tests/performance/compare_endless_runs.py Tests/evidence/windows-vulkan-lp14-240-20261101 Tests/evidence/windows-vulkan-lp14-240-native-touch-20261103 Tests/evidence/windows-vulkan-lp14-240-native-touch-20261103
```

## 已排除与未定案部分

- 上轮带侧键模拟182.710FPS、5.06109ms；本轮取消侧键模拟后175.504FPS、5.14882ms。没有观察到取消模拟带来的整局恢复，不能继续将此前差额主要归因于侧键。两轮战斗不一致，也不能据此说原生触控使性能进一步下降。
- 本轮前0–9波159秒，对照170秒；10–19波146秒，对照132秒；第20波54秒，对照57秒。不是整局统一加速，且波次边界已不同。相同起点不保证对象数量、手动操作、随机数消费与步长轨迹完全相同。
- 相比tag v3.2.4，Reanimator、TodParticle、PoolEffect、Vulkan渲染代码与主循环SexyAppBase.cpp没有源码差异；RenderInterface新增透明边缘修补只在加载上传期运行。不能据此归罪于新shader或每帧边缘处理。
- Board新增的触控光标更新/预览绘制在无活动触点时提前返回；高亮仍复用原有植物遍历，没有新增无条件全僵尸/粒子扫描。选中蓝框会增加四角绘制，依赖选择状态。
- GameButton每次更新增加父节点绝对坐标换算，无触点时跳过触点表。Windows输入增加原生触控路由、鼠标/触控归属与坐标缓存。这些是新增固定工作，但静态审计尚无证据表明能解释约0.597ms/帧。
- v3.2.4至v3.2.5已有射手首次发射、僵尸定身后移动重绑、出生时钟绑定等行为修复；其中部分会改变实际战斗负载。不能把全部版本差值视为触控成本。

两次11月3日整局观察均慢于11月1日，下降覆盖多个战斗阶段，值得调查；目前证据不足以判为稳定代码回归或纯偶然。下一步应使用完全相同冻结对象、相同运行环境对两个版本作交替A/B，并单独量更新/CPU绘制。现有整局曲线不支持给某个函数分摊成本。

## 被测程序与后续ICE3修复

本轮EXE SHA256为402127cc99f7ee5f1fcf752a278c0608dfd69ebb17de552b3c69132fdb49a75c。采集后Release已构建ICE3/ICE4事件时序修复，当前磁盘EXE不再等于被测EXE；本轮数据不包含该修复。ICE3/ICE4各24项专项及完整僵尸回归通过，详见 docs/research/ice3-interruption.md。未修改存档结构。

本记录为调查证据，暂不改正式性能收益总账。
