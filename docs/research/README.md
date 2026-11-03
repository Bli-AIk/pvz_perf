# 调查报告索引

只保留仍可支撑兼容、行为核对或源资源追溯的调查。当前接口与最终实现放在[上层文档](../README.md)，报告中的旧构建结果仅证明当时状态。

| 报告 | 保留理由与边界 |
| --- | --- |
| [1073首次教学](1073-first-tutorial.md) | 零售EXE地址、首次触发与常驻提示的真实含义 |
| [1073磁铁起点](1073-magnet-coordinates.md) | 零售EXE坐标查询、道具偏移及反编译差异；历史修复检查不是最新轨迹证明 |
| [1073传送门](portal-relocation-1073.md) | 候选行限制、出生权重、原版地址与回归入口 |
| [ICE3／ICE4巨人动作中断](ice3-interruption.md) | 普通／红眼巨人投掷与锤击关键区间定身、漏事件后收手重试；Vulkan回归与精确时刻证据边界 |
| [粒子wire字段顺序](particle-wire-order.md) | 序列化顺序与成员顺序差异、独立decoder证据，防止迁移错误再次自证 |
| [原始拼图布局](original-atlas-audit.md) | 源sheet布局、格尺寸及当时消费端；去除已否决的拆图建议 |
| [TV 版选中框素材](tv-selection-frame.md) | 蓝色四角框已入图集；TV卡框与战场落点框的二进制引用、绘制层级核对 |
| [Windows Phone 触控与横板布局](windows-phone-touch-input.md) | 多触点 API 与单捕获业务的区别、拖放语义、侧栏布局及本项目接入范围 |
| [Android Free 触控二进制](android-free-touch-input.md) | Free 3.18.0 的触点标识、11槽操作上下文与独立光标，以及 HD 包的证据边界 |
| [根运动帧号方案](root-motion-frame-cursor.md) | 独立候选性能、所有Zombie的字段代价与跨步长精度；尚未接入生产 |
| [原版／v1到v2时间字段](dat64-v2-time-audit.md) | 冻结的时间字段偏移与单位，供离线转换追溯；不是当前运行时结构 |

磁铁指数曲线推导属于已撤销的解析运动方案，已删除。原版起点证据保留；当前吸附模拟及捕获规则以[运行时](../runtime.md)为准。
