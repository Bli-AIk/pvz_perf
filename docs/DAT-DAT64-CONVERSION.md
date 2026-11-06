# 存档转换与兼容性

当前关卡格式为 **DAT64 v4：major 4 / minor 0 / schema `0x00010745`**。游戏只直接读写这一格式；独立转换器只支持原版1073 DAT、DAT64 v1、v2、v4。**整个v3系列不再作为输入或输出格式**，包括曾用于本地开发的v3.2～v3.6。不能通过改头部版本或扩展名混读。

`users.dat` 用户摘要与 `PVZUSR64` 版本13的玩家详情是独立协议，不随关卡v4改号。已发布包的历史要求见[发布说明](https://github.com/YuHao2006/pvz_perf/releases/tag/v2.0)，不能据此推断当前源码的输入范围。

## 支持范围与命令

| 输入／输出 | 转换器 | 当前游戏 |
| --- | --- | --- |
| 原版1073 `game*.dat` | 支持读写 | 须先转换为v4 |
| DAT64 v1 | 支持读写 | 须先转换为v4 |
| DAT64 v2 | 支持读写 | 须先转换为v4 |
| DAT64 v3及所有minor／schema组合 | 拒绝，不创建或改动输出 | 拒绝 |
| DAT64 v4 | 支持读写 | 直接读写 |

原版DAT省略目标时默认生成v4；DAT64省略目标时默认导出原版DAT。建议明确指定新输出路径并保留输入：

```powershell
.\pvz_dat64_converter.exe .\copies\game3_13.dat .\converted\game1_58.dat64 --target dat64-v4
.\pvz_dat64_converter.exe .\copies\old-v2.dat64 .\converted\game1_58.dat64 --target dat64-v4
.\pvz_dat64_converter.exe .\copies\game1_58.dat64 .\converted\game3_13.dat --target legacy
```

支持的目标仅为 `legacy|dat64-v1|dat64-v2|dat64-v4`。所有 `dat64-v3*` 目标明确拒绝。单文件模式不删除输入；输入与输出不能相同。无参数批量模式扫描转换器自身目录，成功后删除该批输入、失败保留，属于CLI显式批处理行为。

导出原版或v1时，默认要求时间可由原单位（10ms、毫秒或秒）精确表达；非整单位拒绝。`--round-time nearest` 显式启用最接近舍入，恰半时远离零，超出原32位范围始终拒绝。v2与v4之间的纳秒值不经旧tick量化。音乐属性在v4中为float，旧整数格式导出需按目标规则转换。

## 文件与数据分工

| 文件 | 内容 |
| --- | --- |
| `users.dat` | 原版用户摘要表 |
| `user<ID>.dat` | 原版版本12的玩家详情 |
| `user<ID>.dat64` | `PVZUSR64`、版本13的玩家详情 |
| `game<ID>_<mode>.dat` | 原版1073关卡中断存档 |
| `game<ID>_<mode>.dat64` | `PVZDAT64`、major4/minor0的当前关卡快照 |

用户名可见不等于详情和关卡快照已恢复。原版直接保存部分对象与池布局，指针宽度、枚举顺序和现代运行时不同；不能机械扩展四字节字段，也不能把 `sizeof(T)` 当磁盘协议。GPU句柄、原生指针和派生绘制缓存不入档。

当前11池顺序为 Zombie、Plant、Projectile、Coin、LawnMower、GridItem、ParticleSystem、ParticleEmitter、Particle、Reanimation、Attachment。非空旧Trail池没有运行时对应物，转换器拒绝。v1到v2的冻结时间字段与单位证据见[时间审计](research/dat64-v2-time-audit.md)。

## v4帧状态与根运动

`Reanimation` 统一持有动画速率、当前关键帧号与该关键帧的开始时间。关键帧整数时长存于 `mFramePeriodNs`，每帧不足一纳秒的时长固定舍弃；实际更新越过关键帧边界的整数余量保留。绘制帧选择与根运动读取同一份帧状态，不各自从完整周期相位重复定位关键帧。

旧 `mPhaseOriginNs` 已删除。事件判断仍需要前一次处理区间的相位和周期数，不能删除为一般绘制缓存。帧时长倒数不缓存；帧内年龄与帧时长均为整数纳秒，到绘制／位移处才临时相除。根运动几何换算系数为运行期派生值，读档后重建，不写磁盘。

v4在冻结规范记录之后、DataArray ID之前保存显式附加状态，按字段序列化，不复制C++填充字节：

| 对象 | 附加状态字节数 | v4变化 |
| --- | ---: | --- |
| Reanimation | 72 | 当前帧号与关键帧时间，替代旧相位原点；保留事件处理历史 |
| Zombie | 105 | 保留位置与世界坐标绑定截距；删除已消费周期和坐标系数缓存 |
| Plant | 156 | 沿用已有状态，保龄球直接共享reanim帧缓存 |
| Projectile | 8 | 出生时间 |
| ParticleSystem／Emitter／Particle | 4／44／20 | 当前CPU粒子及随机状态，帧缓存变更不增加粒子类型 |

Reanimation附加段（偏移相对附加段起点）：

| 偏移 | 字段 | 宽度 |
| --- | --- | ---: |
| 0 | 时间域 | 4 |
| 4 | 当前关键帧开始时间 `mStartTimeNs` | 8 |
| 12 | 当前关键帧 `mCurrentFrame` | 4 |
| 16 | 关键帧整数时长 `mFramePeriodNs` | 8 |
| 24 | 完整周期 `mPeriodNs` | 8 |
| 32 | 本次更新区间回绕数 `mCycleCrossings` | 8 |
| 40 | 上一次处理区间相位 `mLastFrameNs` | 8 |
| 48 | 已完成根运动基础位移 `mRootDisplacement`（double） | 8 |
| 56 | 历史有效标记 | 1 |
| 60 | 播放代次 | 4 |
| 64 | 上次更新时间 | 8 |

Reanimation在回绕时直接累加已完成的根运动基础位移，包含根轨道归一化系数，不包含角色缩放；32处仅记录本次更新区间的回绕事件，不保存累计周期。Zombie不再保存周期或坐标系数，绑定截距加上Reanimation求得的位移产生当前位置。旧格式导入时根位移从0开始，并根据旧存档坐标首次绑定截距，避免重复应用历史位移。附加段长度与运行期 `sizeof` 不同。

## 头部与结构拒绝

原DAT头为长度前缀12、magic `0xFEEDDEAD`、版本2及构建指纹；接受已核实的1073零售指纹 `0xAC9B80AC` 和早期项目指纹 `0xB7EFF6A8`。DAT64头部固定48字节，小端：

| 偏移 | 字段 | 当前值 |
| --- | --- | --- |
| `0x00` | magic | `PVZDAT64` |
| `0x08` | major | 4 |
| `0x0C` | 头长 | 48 |
| `0x10` | 权威构建 | 1073 |
| `0x14` | flags | 3（小端、纳秒） |
| `0x18` | 载荷字节数 | 与实际一致 |
| `0x20` | 原CRC32槽 | 写0，读取忽略任意旧值 |
| `0x24` | schema | `0x00010745` |
| `0x28` | minor | 0 |
| `0x2C` | 保留 | 0 |

载荷结束标记为 `0xB4A9ABBEBBA5A9AF`。版本、长度、schema、边界、结束标记、池代次、拥有关系及跨对象引用仍需有效。**不校验内容CRC或哈希**，修改合法字段不需要重新计算摘要。v1导出仍写其历史CRC以兼容旧程序，但当前转换器读取时忽略。

转换流程为完整解析、规范化字段与引用、映射身份、序列化目标、重新解析并比较规范语义，最后原子写出。错误在打开目标输出之前拒绝，不以默认状态替代未知数据。

## 模式、外观与旧图片身份

模式编号来自 `game<profileId>_<mode>` 文件名，不是Board载荷。自动输出名使用现有模式映射；显式输出名由调用者负责采用目标模式编号。玩家详情v12旧槽序与v13直接GameMode下标逐字段映射，不能字节搜索替换。

v4粒子系统在固定记录0/4分别保存原版106个基础ID与内部variant；不写发射器图片覆写ID。Reanimation固定记录12保存variant，根type仍表示行为定义；逐轨图片指针保留区与旧ResourceId补充项已经删除。未知base、variant、旧覆写或未声明组合拒绝，不用默认外观代替。

转换器专属 `FrozenResourceIds.h` 冻结879项历史资源身份，`ResourceIdRemap.h` 和生成的外观目录完成原版／v1／v2到当前外观的双向映射。原版PartnerLogo与Blank的确证2→0别名保留。运行时没有ResourceId／ImageSymbolId，也不按当前素材是否加载来判断旧存档图像是否合法。

旧粒子162项规范身份仅用于离线比较与映射，不是另一个受支持的文件版本。旧轨道每条减少的图片区域与外观编号规则继续有效；v4的变化在精确帧状态和根运动状态。

## 验证与实现入口

头及字段布局：`pvzPerf/Lawn/System/LevelSaveFormat.h`、`V7SaveSchema.h`、`SaveRecordMapping.h`；游戏读写：`SaveGame.cpp`；转换器：[PvzDevTools 的转换器](https://github.com/YuHao2006/PvzDevTools/tree/main/saveConverter/src)。状态重建和动作切换的当前机制见[运行时](runtime.md)。

转换测试覆盖原DAT、v1、v2、v4规范往返，全部旧粒子／reanim外观、无CRC读取、非法schema/minor和整个v3系列拒绝且不改动输出。真实游戏回归需加载转换副本、保存、重载并比较对象与外观快照；具体本轮通过状态见[验证说明](validation.md)，独立转换通过不代替完整游戏验收。
