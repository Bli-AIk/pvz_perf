# v3.2.4现行微基准复测

2026-11-01，Windows LP14 / 0x4000。11x为用户控制条件，未独立采频。先完成构建与原生像素/同步验证，再串行采样；视频渲染在计时结束后启动。

- production：原版x86 EXE机器码与当前x64 Release/LTO，原版—当前—当前—原版；五项逐结果向量一致，另有两对象池查询；结构两进程十内核。ISA/ABI/编译器不同，差值不是纯算法倍率。
- table-models：13项当前表声明、17项粒子身份模型，两个进程。原始输入、共同Mix桩不变。植物分派已适配显式三时钟ABI，时间参数由共同桩忽略，不代表完整植物更新。初次旧桩编译失败后仅重测此表组，之前成功的生产计时不重跑；失败日志在Temp，最终14组全部通过。
- reanim_identity_benchmark：2641条合法查询穷举，重复ID取首条。当前整数ID线扫模型对实际MPHF API，两个进程、各六轮，AB/BA交替；不是原版字符串路径。
- reanim_frame_cache_benchmark：四种真实定义，查询与推进分别计时；含重复消费者、不同步长的独立组。frozen_v36仅是冻结算术模型。
- particle_life_side_cache_benchmark：完整渲染参数，冻结未缓存体对当前侧表；保留刷新成本与默认快路的退步。不是原版EXE。
- trig：真实FastMath翻译单元，两个进程、九轮中位数。只把共享范围归一化用于现行收益；sqrt、pow、CRT等保留诊断，不冒充已落地优化。
- frozen-draw：DX12、800×600、32植物40僵尸，128预热与640计时；只计CPU DrawScreen，flush/fence/readback在外。两进程像素相同，哈希与上次3.0归档相同，不声称与缺失1.0原图逐像素相同。

所有源、构建与程序哈希见manifest.json、builds和sha256.json；派生表见derived-summary.json。大体积EXE/PDB保留在Temp构建目录，不入Git。重跑入口为Tests/performance/refresh-production-suite.py，需要原始固定语料和已核验1073调用器。
