# 文档索引

这里只维护现行机制、可复现验证与仍有价值的原版调查。临时排障记录、候选方案和安装过程
不属于公开文档；原始性能数据保留在其证据目录，不在指南中复制多份结论。

## 当前机制

| 文档 | 内容 |
| --- | --- |
| [运行时](runtime.md) | 时钟、事件、根运动、粒子与运动求值 |
| [存档](DAT-DAT64-CONVERSION.md) | 当前格式、支持的转换边界和工具 |
| [资源格式](resource-format.md) | 静态定义、PTC1、NRSC4、离线协议及纹理所有权 |
| [场景图集DSL](scene-atlas-dsl.md) | sheet、view、作者语法与生成接口 |
| [资产与发行边界](ASSETS.md) | 必须保存的编译输入、资源来源和发行过滤 |
| [GPU提交](gpu-submission.md) | 采样、合批、索引、各后端帧资源与同步 |
| [泳池绘制](gpu-pool-production.md) | 波纹、焦散、裁剪与像素参照 |
| [裁剪与文字布局](clipping-and-text-layout.md) | 必要业务裁剪与无绘制文字测量 |
| [触控](touch-input.md) | 原生触点、鼠标并用、共享选择与拖放规则 |
| [Android](android-port.md) | 平台边界、构建打包、窗口和存档目录 |
| [Web试验目标](../tools/wasm/README.md) | 本地站点、WebGL、音频、输入与领取配置；本次不发Web包 |

## 性能、缺陷与证据

| 文档 | 内容 |
| --- | --- |
| [性能优化原理与基准](perf-fix-ledger.md) | 按原理列出有效测量；保留仍有比较意义的1.0数据 |
| [修复总账](fix-ledger.md) | 按根因归类的有效修复，不保存工作过程 |
| [验证说明](validation.md) | 测试入口、覆盖范围和平台限制 |
| [原版调查](research/README.md) | 1073、WP、Free及素材／协议证据 |
| [最新发布说明](releases/v3.3.2.md) | v3.3.2的内容、格式和附件 |
| [历史发行](https://github.com/YuHao2006/pvz_perf/releases) | 每个已发布版本的冻结说明与原包 |

构建及配置入口见[项目说明](../README.md)。测试工具见[性能采集工具](../Tests/performance/README.md)。
历史记录须按其构建哈希理解，不能把某次检查通过当成所有后续版本或硬件的验收证明。
