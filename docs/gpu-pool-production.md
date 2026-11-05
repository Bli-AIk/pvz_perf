# 泳池波纹与焦散绘制

Vulkan泳池三层波纹由CPU计算最终网格，专用顶点着色器只投影；其他后端仍由顶点着色器计算波纹。普通图片、reanimation和particle的格式不变。业务入口是 `PoolEffect::PoolEffectDraw`，通过 `RenderInterface::DrawPool` 提交给各后端的 `SubmitPool`。

## 数据与执行

`graphics/GpuPool.h` 定义一份编译期静态网格：96 个顶点、450 个索引，对应 150 个三角形。拓扑与索引共享；Vulkan每次绘制将96个最终顶点追加到当前帧流，避免三层或多次绘制互相覆盖，其他后端复用静态网格。

水面相位为已发布采样时刻减出生时间，保留原有取模规则；冻结时保持旧采样，恢复时平移出生时间，不补播冻结间隙。Vulkan按行列复用正弦结果并在CPU生成最终位置、UV和透明度；其他后端仍用GPU计算几何。焦散纹理由GPU根据源纹理生成，不读回CPU。

纹理采样直接使用归一化 UV，没有增加像素矩形采样转换层。网格的目标位置仍使用逻辑像素坐标。三层绘制沿用现有片元着色器、混合和纹理采样标志。

## 保留的行为

- 保留原版三层波形、边界顶点、昼夜透明度和提交顺序。
- 焦散层保留原公式两次叠加基础 UV 的行为及 repeat 采样。
- 裁剪仍先将未平移的目标顶点截断为整数，再判断右、下边界排除；通过顶点 alpha 实现。
- 普通绘制与波纹交错时先提交待处理批次，波纹之后恢复或失效相应绘制状态。
- 渲染目标自采样使用已有快照机制，快照包含此前待提交的普通绘制。
- 不改变关卡时钟、存档字段或存档格式。

## 后端

Vulkan、OpenGL、Direct3D 9、Direct3D 12 和 Metal 均有独立的泳池顶点入口，复用各后端已有的片元采样路径。Direct3D 9 在执行 `sincos` 前将角度折叠到要求的范围。生产绘制不要求 timer query，也不依赖 compute shader 或 bindless。

Vulkan 的 `game_pool_vs.spv`、Direct3D 9 的 `pool_vs` 和 Direct3D 12 的泳池 shader 纳入各自已有的离线 shader 构建。OpenGL 与 Metal 使用各自已有的 shader 装载方式。

## 验证

`Tests/native/PoolGpuChecks.h` 使用独立保留的旧 CPU 网格公式作图像参照，覆盖昼夜、三种混合、nearest/linear/repeat、相位边界、大计数值、整数截断裁剪、分数平移、普通绘制交错和浮点渲染目标反馈。Vulkan 在实际游戏链接的 ImageRuntimeTests 中运行；OpenGL、Direct3D 9 在后端套件中运行。

nearest 采样存在纹素边界不连续：极小 UV 舍入差异可能选择相邻纹素。测试保留原最大误差及均值限制；仅在 nearest 场景恰好出现一个异常像素时，额外使用独立 CPU 参照验证相邻纹素候选。候选每次只微调一层的一个 UV 轴，异常像素的 RGBA 与浮点结果必须同时匹配同一个候选。其他像素仍执行原限制，均值仍包含原始异常误差；linear、裁剪及反馈测试不适用此例外。生产 UV 不作偏置。

Direct3D 12 的原生测试额外覆盖三层、昼夜、主目标与离屏目标的组合，以及几何、透明度、平移和普通绘制状态恢复。Metal 在当前 Windows 环境中尚未编译或执行，需要在 macOS 上补验。

当前发行的实际构建、资源哈希及测试状态由发行附件的build-verification.json和manifest.json记录，
不在机制文档中复制某次旧PAK哈希或安装状态。复现入口见[验证说明](validation.md)。
当前Windows主机的验证不能视为Win7旧显卡、Android或Metal实机认证；图像一致性
通过也不能推导为所有硬件上的整帧性能收益。
