# Android Vulkan port

The Android target uses the NDK native app glue and the Vulkan Android WSI. The
activity hosts the game in a SurfaceView and forwards its holder callbacks to
NativeActivity, leaving the main Window surface available for Android's caption.
`APP_CMD_INIT_WINDOW` attaches the SurfaceView's `ANativeWindow` to the game
window adapter and `APP_CMD_TERM_WINDOW` detaches it. Vulkan creates the surface
with `vkCreateAndroidSurfaceKHR`, and the drawable size is read from
`ANativeWindow_getWidth/Height`. Android currently requests a 600-pixel-high
swapchain at the drawable aspect ratio (subject to surface limits), leaving
physical touch coordinates unchanged. SurfaceFlinger scales the presented layer.

## HDR output and scaling

Android HDR is not exposed in v3.3. The Options row is disabled and labeled
HDR (disabled); a retained hdr=true setting is silently normalized to false.
The release uses its validated SDR swapchain. Device support for FP16 scRGB was
confirmed, but screen grayness/brightness calibration did not pass visual
validation; no experimental paper-white or normalization path is shipped.
Windows Vulkan/DX12 HDR uses linear RGBA16F, with output math and desktop WSI
validated. The retired PQ equations remain only in test reference probes.

Android currently supports built-in presentation resizing only. HLSL/compute
scale-effect chains are deliberately unsupported; nonempty effects arrays are
rejected by the Android configuration reader. Do not equate ordinary bilinear
presentation/system scaling with an additional super-resolution effect chain.
The APK includes an empty scale.json template (multiplier 1, no effects). On
first launch it is created beside settings.json in the selected user-data
directory, normally Android/media/com.openpvz.perf; existing files are retained.
The native reader uses this external file. No load-time super-resolution, disk
scale cache or native high-resolution assets are implemented here; the template
preserves the configuration boundary for that future work.

Android Studio is not required. Install the Android command-line tools, an NDK,
the SDK platform and CMake, then configure with the NDK toolchain:

```text
cmake -S . -B out/android-arm64 -G Ninja ^
  -DCMAKE_TOOLCHAIN_FILE=%ANDROID_NDK%/build/cmake/android.toolchain.cmake ^
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 ^
  -DPVZ_WINDOW_BACKEND=ANDROID -DPVZ_RENDER_BACKEND=VULKAN ^
  -DPVZ_BUILD_LEGACY_GAME=ON -DCMAKE_BUILD_TYPE=Release
cmake --build out/android-arm64
```

The local SDK/NDK and `adb` are installed under `C:/UserProgram/DevTools`.
Arm64 APK builds have been installed and tested on a nubia Z60 Ultra (Android
14). The APK includes `main.pak64`; no manual data-package copy is required.
Always package the native library, resource archive and SPIR-V from the same
build tree. Shader changes also require clearing the application's pipeline
cache before testing.

## Rendering and submission

Reanimation interpolation/transforms and particle simulation/render parameters
are evaluated on the CPU. The GPU receives ordinary textured triangles. The
resident reanimation and particle paths, generated GPU behavior records and
associated shader stages have been removed, including their experimental build
options.

Vulkan textures and samplers remain bindless, but a pipeline/blend/scissor change
can still end a batch. On devices supporting `dualSrcBlend`, straight-alpha
sprites use one dual-source pipeline for normal and additive blending. It
preserves paint order and destination alpha; it does not reorder translucent
draws. Devices without that feature retain their separate ordinary blend
pipelines.

Plain program images repair fully transparent RGB edges once before upload.
Visible RGB and all alpha bytes stay unchanged. Only alpha-bearing neighbors
in the immediately adjacent rows contribute, matching legacy sanding. The
in-place operation needs no additional pixel buffer; loading pixels are still
released after upload. Deferred alpha compositions keep their existing shader
sampling because their final pixels do not exist at that preparation point.

## GPU concurrency measurement

The current runtime uses vertex and fragment shaders, without resident animation
jobs or animation compute dispatches. Fragment invocation count depends on
covered samples and overdraw, while vertex work depends on submitted triangle
vertices and caching. Hardware concurrency is not a fixed API constant: it
varies with shader resources, occupancy and GPU scheduling.

GPU elapsed timestamps include pipeline stalls and do not directly measure
energy use. Phone power comparisons require a controlled scene and refresh rate,
with device power counters or external power measurement where available.

For presentation measurements, poll SurfaceFlinger latency history more often
than the buffer can wrap. Its finite history must not be sampled every four
seconds at 100 FPS: missing entries create artificial large frame intervals.
Current diagnostic collection polls every half second and retains unique
presentation timestamps. A fixed paused fixture must also match triangle count
and screenshot before runs can be compared. Automatic fixture loading alone
does not guarantee that the user has not subsequently resumed the scene.

## Android 目标帧率与外部设置（2026-10-27）

设置与存档优先使用可写的 `getExternalMediaDirs()` 目录，游戏不请求额外存储权限。
本测试机的目标位置是 `/storage/emulated/0/Android/media/com.openpvz.perf/`。
Media 不可用时回退 `getExternalFilesDir(null)`，再回退 Activity 的内部 files 目录。单实例锁
固定使用内部运行目录，避免 Media 的共享存储文件系统不支持 flock 导致启动失败。资源包、shader 和运行期缓存
始终使用内部目录。Android/data 的存在不保证第三方文件管理器能看到它；
本机用有效文件描述符实测 Media 的 flock 返回 ENOSYS（Function not implemented），
应用内部目录的相同操作成功；用户数据可写不代表该文件系统支持进程锁。
本机通过 ADB 确认当前用户为 0，上述目录实际包含 settings.json 和 DAT64 存档。
公共路径解析器不再猜测 `/storage/emulated/0` 或回退旧 `com.openpvz.modern` 包名。
首次使用新目录时优先复制旧 external-files 中的设置、用户档与关卡档，再补内部目录
缺少的文件；已有目标文件不覆盖，源文件不删除。完成后写入隐藏迁移标记，避免后续
启动时把已读取并消费的关卡档从旧目录重新拷回。用户索引在对应记录复制后发布。
GPU 固定场景仅隔离关卡／用户档，设置不再跟随 fixture 复制到内部 session，
通过现有 `PVZ_SETTINGS_FILE` 路径覆盖统一读写正常外部设置。

Android 提供持久化启动参数，便于受 scoped storage 限制的设备配置：

```text
adb shell am start -n com.openpvz.perf/.PerfActivity --ei targetFps 240
```

该参数在新进程初始化前更新正常 settings.json 的 targetFps，保留其余设置，
然后由统一设置加载器读取；不是初始化后强改字段而遗漏全局时间步进。
应用已运行时，应先正常保存退出再重新启动以应用参数；不要在战斗中强制结束进程。
无参数启动继续读取已保存设置。此参数不改变屏幕支持的物理刷新率。
240FPS 对应目标等待间隔 4166666ns，并非固定逻辑步长。逻辑按实际墙钟纳秒推进，超出目标间隔时处理完整实际步长，节拍余量留给下一次；不再按10ms拆分补拍，逾期不额外等待。最终绘制率受硬件、GPU同步和呈现模式影响。

性能采样需区分：GPU timestamp 记录 GPU 命令区间；gpu-workload 的 renderFps
记录每 120 次呈现提交完成的墙钟区间（首批无前界故记 0，需丢弃首批）；
SurfaceFlinger --latency 的实际呈现时间去重后反映上屏节奏，受物理刷新率约束。
不能将这些指标互相替代，手机叠加层显示的 FPS 口径也不能未经核实视为渲染提交率。

实机注意：固定无尽后续 30 秒采样约 100FPS，柱子／后续界面则记录到连续约
239～241FPS 的内部提交；没有统一写死的 100FPS 上限。屏幕实际呈现最高约 120，
而钱袋炫白仍可造成明显下降。配置读取成功与各场景持续性能是独立验收条件。

Android 包名自本次打包起为 `com.openpvz.perf`，显示名为 `pvz_perf`。旧包 `com.openpvz.modern` 的数据不会由系统自动迁移；换包前须备份和显式迁移，不能将新包空数据视为迁移完成。`Android/data` 可能被系统文件管理器隐藏，外部应用专属目录不等于任意文件管理器都可访问。


## APK output directory

Ready-to-install APKs and their `.apk.idsig` sidecars live in `out/arm64-android14-vulkan1.3/`,
not directly in `out/`. The current signed package is
`out/arm64-android14-vulkan1.3/pvz_perf-arm64-android14-vulkan1.3.apk`. Native build intermediates remain in
`out/android-arm64/`.

`tools/build-android-apk.ps1` copies Gradle output into this directory by default;
`-OutputDirectory` can select another package directory. Unsigned Gradle Release
output keeps the `-release-unsigned.apk` suffix and does not replace the current
signed package. Sign and align it before installation.

The distribution identifier is `arm64-android14-vulkan1.3`; it names the Android
14 test target, not a change to Gradle's minimum supported API. The current CPU
baseline remains Armv9-A without SVE/SVE2. Native intermediates and layout caches
remain in the single active `out/android-arm64` tree.

The activity uses `sensorLandscape` to support both landscape orientations.
Native resize events carry the current window extent and update the cached
NativeWindow width/height before viewport and pointer mapping. Touch coordinates
remain window-local in split/freeform mode, rather than using startup fullscreen
dimensions after the window has changed size.
The SurfaceView's location within the Window is published on layout changes and
subtracted from native motion coordinates. Gesture ownership is fixed on DOWN:
caption drags stay with Java until release even when they cross the game area.
The activity explicitly supports resizing and clears fullscreen/immersive flags
in multi-window mode. Actual caption buttons and side docking are supplied by
the device's window manager; they are not game-drawn controls.
Fullscreen uses edge-to-edge game content beneath transparent navigation buttons,
with navigation-bar contrast enforcement disabled. Multi-window restores the
system's caption/content layout rather than retaining fullscreen layout flags.
Configuration and native-window resize commands invalidate the swapchain even
when a 180-degree flip leaves the width and height unchanged. Soft input starts
hidden and does not resize the render surface; only `EditWidget` focus requests
the keyboard, using an explicit request rather than forcing it for every widget.
Tapping an already focused editor requests it again after manual dismissal.
`PerfActivity` makes NativeActivity's content view focusable in touch mode and
reapplies the soft-input mode after `super.onCreate`, which otherwise replaces
the manifest setting. Android Back uses the existing Escape mapping; non-text
keys now reach that mapping instead of being returned to the Android framework.
Enter, Backspace and arrow keys use the same repaired input route. Volume keys
remain with the system. Legacy Back dispatch is explicitly retained instead of
registering a predictive-back callback that would exit the activity.

The initial focus repair was installed and confirmed interactively on the
Android 14 Z60 Ultra: keyboard opening, Back/Escape and 180-degree rotation work.

Text entry now uses a bottom Android Dialog with an EditText and OK action.
It owns a separate surface, leaving Vulkan's game SurfaceView at its original size.
The input window uses resize adjustment to stay above the keyboard. Text changes
cross JNI through a mutex-protected latest-value mailbox; only the game thread
accesses EditWidget. Session numbers reject callbacks from closed or destroyed
widgets. Java explicitly loads `pvz_perf` through its class loader: NativeActivity's
native dlopen alone does not make exported JNI methods resolvable by ART.
Both Java's input filter and the native mailbox consumer allow only printable
ASCII (32..126), including pasted/IME text. The consumer applies an edit through
the existing KeyDown/KeyChar handlers rather than inserting UTF-8 bytes directly;
this preserves character checks and length/pixel-width limits. LawnEditWidget's
one-time first-letter capitalization field and KeyChar override have been removed;
both desktop and Android preserve the case entered by the user.
Without that boundary, non-ASCII bytes reached missing bitmap-font glyphs and
crashed in ImageFont::CharWidthKern during EnforceMaxPixels.
The actual Java filter was extracted and executed with ASCII, mixed Chinese,
emoji, control-character, source-range and deletion cases; all passed.
OK submits the existing Return action; dismissing the input window keeps edits.
Game-side text changes are mirrored back while editing. Full Unicode rendering
in the game's bitmap fonts is outside this input bridge.

A real alphabetic InputDevice or an available QWERTY hardware configuration
suppresses automatic soft input. Virtual keyboards and game controllers do not
count. Detection happens at input requests and device/configuration events, not
per frame; InputDeviceListener handles hotplug. The owned input window's focus
transfer is not treated as leaving the app, while onPause still forwards loss
of focus and closes the editor when another app takes over.
