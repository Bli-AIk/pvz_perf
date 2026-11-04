# 文档索引

顶层维护当前接口、机制、构建使用与最终验证。原版行为、源素材和旧协议的调查证据集中在[research](research/README.md)；已发布版本说明放在releases。本地开发格式不等于已经发布，当前支持范围以存档协议和实现为准。

## 当前机制与使用

| 文档 | 内容 |
| --- | --- |
| [运行时](runtime.md) | 时钟域、根运动、发射、弹道、粒子与静态定义 |
| [存档转换](DAT-DAT64-CONVERSION.md) | 当前支持格式、历史转换边界和转换器行为 |
| [资源格式](RESOURCE-V5-FORMAT.md) | 静态定义、粒子记录、NRSC4、图集与纹理所有权 |
| [场景DSL](scene-atlas-dsl.md) | 完整sheet、局部view、作者语法和生成接口 |
| [资产与发行边界](ASSETS.md) | 编译输入、发行资源、素材来源和shader对应关系 |
| [Android适配](android-port.md) | 开发环境、构建打包、窗口和设备存储 |
| [触点输入](touch-input.md) | Android 多指、Win7/Win10 原生触控、鼠标并用、共享选择与拖放规则 |
| [GPU泳池](gpu-pool-production.md) | 静态网格、专用顶点路径和后端契约 |
| [GPU提交专项](android-gpu-submission.md) | 混合合批、边缘处理、索引与设备测量边界 |
| [裁剪与文字布局](clipping-and-text-layout.md) | 必要业务裁剪、纯文字测量和Widget遍历职责 |

## 修复、性能与验证

| 文档 | 内容 |
| --- | --- |
| [性能优化原理与基准](perf-fix-ledger.md) | 按原理聚类列出相关基准、精度成本和整局结果 |
| [修复总账](fix-ledger.md) | 本项目缺陷的根因、修复与证据；包括只读表存储期问题 |
| [验证说明](validation.md) | 当前检查覆盖、复现入口和平台限制 |
| [调查索引](research/README.md) | 值得保留的原版、源素材和冻结协议证据 |
| [v3.3.1发布说明](releases/v3.3.1.md) | 横板 UI、三类图集与 Android 修复 |
| [v3.3发布说明](releases/v3.3.md) | 原生多点触控、共享选中状态、输入坐标与图层修复 |
| [v3.2.4发布说明](releases/v3.2.4.md) | 三平台整包、原版Android图标、CPU绘制与现行微基准 |
| [v3.2.1发布说明](releases/v3.2.1.md) | 源码补丁、标题计时、水面冻结与末帧保持修复 |
| [v3.0发布说明](releases/v3.0.md) | 当前破坏性更新、v4存档和整包使用方法 |
| [v2.0发布说明](releases/v2.0.md) | 该发行的存档／PAK不兼容边界和使用方法 |
| [v1.3发布说明](releases/v1.3.md) | 该发行的资源、使用和验收边界 |

构建、运行、配置与打包入口见[项目说明](../README.md)。性能原始证据放在Tests/evidence，历史夹具放在Tests/reference；系统Temp中的诊断日志不作为长期文档依赖。不同后端、频率与测量范围不能混算收益。已撤销的实施方案不保留为当前指南。
