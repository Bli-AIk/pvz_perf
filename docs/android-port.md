# Android 平台

Android 使用 NativeActivity、SurfaceView 和 Vulkan WSI。Activity 负责窗口、键盘及触点，
原生窗口适配层负责将窗口内坐标映射到游戏逻辑坐标；游戏逻辑不读取 Android 时间域或系统 UI 状态。

## 发行目标

| 包名 | 最低系统／CPU | 图形路径 |
| --- | --- | --- |
| `arm64-android14-vulkan1.3` | Android 14／Armv9-A，不要求 SVE／SVE2 | Vulkan 1.3，要求项目使用的 descriptor indexing 功能和 synchronization2 |
| `arm64-android7-vulkan1.0` | Android 7／Armv8-A，64位 | Vulkan 1.0 兼容提交与传统纹理绑定 |

旧包通过 `PVZ_VULKAN_LEGACY=ON` 构建，shader 目标为 Vulkan 1.0／SPIR-V 1.0。
兼容层把共用提交转换成 `vkQueueSubmit` 和 `vkCmdPipelineBarrier`；双槽帧资源、fence
检查及动态几何流与现代路径共用。旧包当前固定使用传统绑定，不是启动时自动切换 bindless。
现代包不在每个 draw 中执行旧路径的纹理拆批分支。管线缓存按 v10／v13 分开。

最低系统版本不代表所有该版本手机都可运行：需要厂商 Vulkan 驱动、相应 CPU 指令集，
以及所需纹理／渲染目标格式。当前不发布32位 APK。

## 构建与打包

需要 JDK、Android 命令行 SDK、NDK、CMake／Ninja；无需安装 Android Studio。
版本、工具路径及配置入口以项目根目录构建说明和 `tools/build-android-apk.ps1` 为准。
旧包 Java 胶水生成 Java 8 字节码，不要求手机安装桌面 JDK；宿主构建仍可使用 JDK 17。

原生库、shader 和 `main.pak64` 必须取自同一构建树：

- 现代原生中间产物：`out/android-arm64`。
- 兼容原生中间产物：`out/arm64-android7-vulkan1.0`。
- 可安装包：`out/arm64-android14-vulkan1.3` 或 `out/arm64-android7-vulkan1.0`，
  发行归档集中到 `out/distribution/<version>`，不在 `out` 根目录堆 APK。

已有原生构建完成后，用 `tools/build-android-apk.ps1 -BuildDirectory <tree> -Sign` 打包。
脚本执行16KiB对齐、签名和校验；`-Install` 会安装签名包。
`-release-unsigned.apk` 不能直接安装。APK 包含运行资源及附带存档，不需要另外拷贝数据包。
两个目标分别使用 Full LTO；不能混用 `.so` 和另一个目标的 PAK／shader。

## 设置、存档与退出

首选 `getExternalMediaDirs()` 提供的应用目录，通常为：

```text
/storage/emulated/0/Android/media/com.openpvz.perf/
```

游戏不请求额外存储权限；存在旧私有或 external-files 数据时按迁移代码处理。
目录的实际可见性由系统文件管理器决定，不能把 `Android/data` 当作当前首选目录。
首次创建配置时 `landscapeUI=true`、`autoDetectFrameRate=true`；已有配置按读取与钳制规则保留。
目标帧率最低30，自动检测和时钟规则见[运行时](runtime.md)。

Activity 不是独立的游戏逻辑更新线程。正常 Quit 完成存档、原生清理后结束 Activity 和进程，
不保留缓存游戏进程；安装时强退不会替尚未保存的关卡回写进度。

## 窗口和输入

- 使用 `sensorLandscape` 支持两个横向方向。Surface 的尺寸和旋转交由窗口与 Vulkan
  surface 变换配合处理，不额外对同一坐标重复旋转。
- SurfaceView 的窗口内位置在布局变化时更新。触点先减去此偏移，再映射到逻辑视口，
  小窗／分屏不沿用启动时的全屏尺寸。
- 多窗口保留系统标题栏；标题栏拖动从 DOWN 起归 Java 管理，不在进入游戏区域后改变归属。
  最大化、关闭和贴边按钮由设备窗口管理器提供。
- 全屏使用透明导航栏和 edge-to-edge 内容。系统支持的小窗能力不由 `screenMode` 保证，
  不支持多窗口的设备不能靠游戏配置强行获得该功能。
- 返回键按 Esc 处理。文本编辑使用可见输入条；检测到物理键盘时不主动弹出软键盘。
  游戏用户名称限制 ASCII，不自动把首字母改为大写。
- 多指、鼠标并用、共享选择、拾取和加农炮规则见[触控输入](touch-input.md)。

## 图形、HDR与验证

交换链采用600高的逻辑输出，SurfaceFlinger 负责缩放到物理显示；触点仍取实际窗口坐标。
`SUBOPTIMAL_KHR` 可继续呈现，只有失效表面／尺寸变化等条件触发重建，防止逐帧重建。
帧资源、纹理生命周期和各后端差异见[GPU提交](gpu-submission.md)，不在此重复维护性能数值。

Android HDR 当前禁用，设置项 disabled，旧配置的 `hdr=true` 静默归一为 false。
保留的 `PVZ_ANDROID_HDR_TEST` 仅供后续调查，不凭“支持HDR”的系统标志开启发行路径。
Windows 的 HDR 结论不外推到 Android。HLSL 缩放链不属于 Android 发行功能；
`scale.json` 仅保留当前支持的设置壳和后续扩展入口。

签名、ZIP／ELF16KiB对齐、库／PAK／shader 配对检查不等于实机验收。
准确范围记录于对应发行附件的 `build-verification.json`。
完整测试入口见[验证说明](validation.md)，性能原始数据与口径见[性能总账](perf-fix-ledger.md)。
