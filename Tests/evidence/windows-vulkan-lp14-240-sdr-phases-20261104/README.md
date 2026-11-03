# SDR整局复测与分相定位

2026-11-04，LP14 / 0x4000，用户设定11x，240FPS上限，VSync关闭。当前代码含原生触控、所有僵尸统一动作事件顺序；本轮开启临时CPU阶段与GPU时间戳插桩。未独立采集CPU/GPU频率，不作为干净发行版最终性能总账。

起点经采集程序验证：DAT64关卡时间2291960000000ns、wave0、20波、9990阳光、暂停状态，PerfTest用户的泳池无尽槽位。进程退出后观察结束，metadata为complete/game-exited。有效前台战斗363.000秒。

| 整局指标 | v3.2.4，11月1日 | 3.3 beta上轮，11月3日 | 当前SDR插桩版 |
|---|---:|---:|---:|
| 有效秒数 | 359.000 | 359.001 | 363.000 |
| 平均FPS | 196.736 | 175.504 | 188.898 |
| 进程CPU，单核=100% | 89.550% | 90.364% | 90.121% |
| 进程CPU ms/绘制帧 | 4.55180 | 5.14882 | 4.77090 |

当前比上轮FPS高7.63%、CPU ms/帧低7.34%；比3.2.4的FPS低3.98%、CPU ms/帧高4.81%。当前全程实际hdr=0、960x600场景、960x600交换链、scale=0、MAILBOX。上轮没有记录实际HDR/视口状态；且两轮间还统一了僵尸事件顺序并增加插桩，战斗操作与对象轨迹并不完全相同，不能把全部回升归因为关闭HDR。

## 分相

按QPC与外部前台game区间对齐，排除暂停、菜单和有超过0.1%区间不在有效战斗内的边界行，选择362行、362.409秒。只保留完整汇总行，不把部分菜单/暂停行的阶段成本按比例摊给战斗。

| 主机阶段 | 平均墙钟耗时/调用 |
|---|---:|
| LawnApp::UpdateFrames | 0.90388ms |
| DrawDirtyStuff，含下列绘制/提交入口 | 3.65651ms |
| WidgetManager::DrawScreen，含绘制准入检查 | 2.62218ms |
| reset fence + vkQueueSubmit2 | 0.21048ms |
| vkQueuePresentKHR | 0.73790ms |
| 零超时vkAcquireNextImageKHR | 0.00709ms |
| vkGetFenceStatus | 0.01567ms |

阶段具有包含关系，不能全部相加。这里是主机墙钟时间，不能把呈现调用的0.738ms全叫作GPU执行或CPU计算。

GPU完成帧回收68430个时间戳结果，场景平均设备区间1.29189ms，最终输出平均0.05307ms，合计约1.34497ms。设备时间区间可能含等待/气泡；结果在资源槽复用时回收，通常晚两个提交帧。它不构成纯shader忙碌周期测量。

68510次绘制尝试中，81次因fence未完成跳过，约0.118%；交换链取图失败0次，显式同步WaitForFrame在选择区间调用0次。因此本轮没有支持“主循环每拍阻塞等待fence”或“GPU持续跟不上绘制”的证据；偶发GPU迟到仍存在。正常逻辑更新未因跳帧取消。

本轮主要可见成本是主机绘制与驱动调用，CPU绘制入口2.62ms高于逻辑更新0.90ms，GPU平均耗时明显低于平均5.29ms帧间隔。但尚未把CPU绘制进一步分成reanim插值、三角函数、顶点生成和绘制遍历，也不能由这轮反推旧版本各阶段差额。

## 数据与重建

- `samples.csv`、`measurement.json`：外部观察及QPC起点。
- `frame-phases.csv`：全部内部阶段汇总；`selected-frame-phases.csv`为严格对齐后选择。
- `phase-summary.json`：加权统计与实际图形状态。
- `conditions.json`：被测EXE/PAK及源码哈希；`settings-used.json`、`scale-used.json`为使用配置。
- `sdr-comparison.png/svg`：按Board关卡时间对齐的三轮曲线、当前主机阶段与GPU时间。

```powershell
python Tests/performance/analyze_frame_diagnostics.py Tests/evidence/windows-vulkan-lp14-240-sdr-phases-20261104 --baseline Tests/evidence/windows-vulkan-lp14-240-20261101 --previous Tests/evidence/windows-vulkan-lp14-240-native-touch-20261103
```

若要验证HDR的因果作用，应保持当前EXE、插桩、分辨率和起点不变，仅开启HDR再测；启动脚本已支持`-ColorMode HDR`，无需重编译。当前SDR结果保留，不能在对照完成前覆盖。临时插桩仍由默认关闭的CMake开关管理，未提交或推送。
