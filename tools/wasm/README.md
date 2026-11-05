# HTML / Wasm Toy 试验目标

独立 CMake 工程 `game/` 编译当前 pvz_perf 游戏逻辑，不替换为上游游戏。
WebGL2 与 OpenGL、Vulkan 是平级构建后端，浏览器窗口使用 SDL2 的 Emscripten 端口。
OpenGL / WebGL 共享 `graphics/gl` 的绘制实现，各自的上下文、API 差异由编译期别名选择。
浏览器调度、加载、文件路径和音频实现放在 `platform/web`，由 CMake 选入；
公共游戏代码不包含 `__EMSCRIPTEN__` 分支，原生目标不引入 SDL 或浏览器运行库。

## 构建和预览

本机已安装 Emscripten 6.0.13。需要 libspng 0.7.4 源码和 nlohmann-json 头文件，
可通过脚本参数指定；CMake 不会把 Windows 的库文件链接进 Wasm。
SDL2、zlib、libjpeg 由 Emscripten ports 为 wasm32 编译。

```powershell
pwsh -File tools/build-web-toy.ps1
python -m http.server 18088 --bind 127.0.0.1 --directory out/wasm/toy
```

浏览器打开 `http://127.0.0.1:18088/`，等待加载后点击“开始游戏”。不能用 file://
直接打开，需要 HTTP。上传包在 `out/wasm/pvz-perf-toy.zip`；入口及所有 URL 均使用相对路径。

`package_toy.py` 只打包 HTML、JS、预加载资源数据和 gzip 后的 Wasm，
不打包原始 Wasm、调试符号、日志和验证截图。页面用浏览器 DecompressionStream
解压 Wasm，再通过 instantiateWasm 交给 Emscripten。两种体积（文件总和、上传 ZIP）
都会打印，并分别标记总量的 50,000,000 字节／50MiB 和 ZIP 的 50,000,000 字节阈值。
默认直接使用 Release 的 `main.pak64`，不再默认采用 compact 资源；需要时可通过
`-PakFile` 或 `PVZ_WEB_PAK` 指定其他包。普通包与 compact 包分别约 45.25MB、37.49MB。

## 平台边界

- WebGL2 不使用桌面双源混合；normal/additive 分开提交。SDR 输出，不支持原生 HDR/超分管线。
- BGRA 上传在 Web 平台转换成 RGBA；GLSL 1.30 编译前转换为 GLSL ES 3.00。
- 使用 Asyncify 让主循环及模态等待交还浏览器。加载暂时在主线程执行，直接完成纹理
  队列与图集上传，不等待被挂起的外层循环。没有引入 SharedArrayBuffer、COOP/COEP 或 pthread 要求。
- 主循环按 requestAnimationFrame 让出执行权；两套动态顶点流使用 WebGL2 fence
  零等待轮询，忙时保留绘制脏标记并重试。纹理与场景目标共用，不执行主画面每帧 glFinish。
- 音乐定位后先解码并保留一个采样帧，使首行命令先执行，再恢复控制器的 BPM／speed；
  读取这两个属性时等待音频线程确认，避免读取异步旧值。读档及暂停恢复共用此路径，
  不丢弃这个预解码采样，不改动原生 BASS。
- Web 专用 BASS 调用适配层以 libopenmpt 0.8.10 解码 MO3，AudioWorklet 输出 PCM；
  非安全上下文退回 ScriptProcessor。普通 OGG 音效在加载期解码为 AudioBuffer，
  两个 AU 音效按 µ-law 解码；播放时直接启动缓冲源，支持声像、实时音量和暂停。
  credits OGG 使用 Web Audio。Windows／Android 仍使用原有 BASS，不加载 Web 解码器。
- `/persist` 挂载 IndexedDB；首次创建用户，正常 Quit 后同步存档。
  不接触桌面/Android 的 data、settings 和现有存档。未承诺跨平台存档互读。
- 浏览器输入透过独立 NativeWindow 实现进入现有鼠标/触控事件路径；不会把触控改成全局鼠标位置。
- Wasm 没有可写 FTZ/DAZ 控制寄存器，不能沿用原生开关。

该目录是试验移植，不是正式发行平台。只有通过实际关卡、保存/重载和目标浏览器测试后，
才能视为可以公开发布的 Toy。构建脚本不登录或上传 Bilibili。

本机 Edge 已验证：加载、标题、创建用户、主菜单、进入首关战斗、选择卡牌种植、
空格暂停/恢复、用户表同步到 IndexedDB；最终一轮控制台无错误。
已验证发行无尽基准读取、Quit 后设置落盘，以及实际音频图非静音、音乐暂停／恢复。
移动浏览器、实体设备音频延迟和完整关卡通关仍需人工测试。
compact 资源版文件合计约 43.0MB、ZIP 约 39.2MB；普通资源版预计总量约 50.8MB、
ZIP 约 47MB（十进制；构建时输出实际值）。上传额度的压缩／解压口径不能混用。
网页启动器管理目标帧率、自动估算刷新率、画面比例、横板 UI、音效音量和浏览器全屏。
设置在 main 执行前写入浏览器 settings.json。ResizeObserver 只在布局变化时通知窗口调整，
保持 600 逻辑高度与 800～1400 宽度，超出比例留黑边，不拉伸游戏。
原生 HWND/浏览器 DOM 全屏分别归各自平台管理；HTML/CSS/JS 不接管游戏内 UI。

## 验证

`verify_toy.mjs` 使用 playwright-core + Edge 的无头模式验证本地页面，保存截图和控制台记录至
`out/wasm/verification`。可通过 `PVZ_PLAYWRIGHT_MODULE` 指定已安装模块的 file: URL。
不操作用户正在运行的浏览器窗口。

## 开发入口与测试档

开始游戏后使用 CSS 铺满当前网页视口，隐藏页面页头／页脚，不调用 Fullscreen API，
默认保留浏览器标签栏和地址栏；游戏内 Full Screen 选项请求浏览器全屏。
Custom Scale 在 Web 禁用。游戏 Quit 完成存档同步后自动重载回启动设置，
不用点击额外的“重新开始”按钮，下次只需一次“开始游戏”。
网页每秒读取一次游戏实际绘制计数来显示 FPS，不提前启用 C++ 调试字体。开发档独立放在 IndexedDB 的 `/persist/development-v331`，
不覆盖早期 `/persist` 下的试玩档。首次复制发行的完美存档与无尽基准，之后保留游玩进度，
设置页提供显式重置按钮。

局域网试玩可用 `python -m http.server 18088 --bind 0.0.0.0 --directory out/wasm/toy`，
手机访问电脑 LAN IP（需要同网和防火墙允许入站），Wasm 在手机本地执行，非桌面串流。
不需要 HTTPS 的基本试玩可用；未来 AudioWorklet 等功能还需安全上下文。
浏览器不提供线程 CPU 亲和性接口；本项目不尝试对 Wasm 锁核。
触屏设备按长边／短边计算画面比例，竖持时仅在网页内旋转游戏区域，
不依赖浏览器的屏幕方向锁。PointerEvent 坐标逆变换后进入 SDL 事件队列，
鼠标和触控保持独立，不使用 SDL 对旋转 CSS 的默认坐标换算。
PointerEvent 的默认行为被抑制后，按下画布会显式恢复 iframe/window 和 canvas 焦点。
失焦、页面隐藏、pointercancel 和 lostpointercapture 清理尚未完成的手势；
不依赖 Toy 的专用暂停接口。`verify_iframe_focus.mjs` 通过宿主侧栏模拟失焦、
缩窄 iframe，再验证恢复按钮和空格切换。

Web 手机上的 EditWidget 复用 NativeTabletCaret 接口，提供可见 HTML 文本输入条。
平台判断使用 Android/iOS UA、iPad 桌面 UA 特征和 coarse pointer；普通游戏帧没有编辑器时
不访问输入邮箱。DOM 只提交 ASCII 文本与选区，在正常事件阶段交给游戏原有 KeyChar／KeyDown
处理，保留长度、宽度和字符校验；不从 DOM 回调中修改游戏对象。软键盘期间保留游戏尺寸，
输入条跟随 visualViewport。已验证真实输入框获焦、非 ASCII 拒绝以及新用户资料写入；
实体设备的 OS 软键盘自动弹出仍需人工验证。
完美进度由测试档生成器生成，包含坚果包扎术；已有 IndexedDB 档不自动覆盖。

## libopenmpt 替代调查（2026-11-05）

官方 libopenmpt 0.8.10 提供 Wasm 包：
https://lib.openmpt.org/libopenmpt/download/

`probe_openmpt.cjs` 使用这个官方包，实际加载项目的 `mainmusic.mo3` 与
`mainmusic_hihats.mo3`，按 48 kHz 解码各 10 秒，检查非静音、30 通道、236 个 order、
order/row 定位以及 interactive 扩展的通道音量和 tempo factor。测试通过。

它可以替代模块音乐解码、分轨控制和定位的基础功能，但不是 BASS 的二进制替代品。
当前 Music.cpp 直接依赖 BassMusicInfo、gBass、BASS flags 和 packed order，这些
调用已在 Web 专用层适配。libopenmpt 输出 PCM，浏览器设备输出另用 Web Audio；
音效、credits 音乐和实时混音不能只靠 MO3 解码库解决，因此不替换原生平台的 BASS。

打包依赖官方 Web bundle，解压至 `out/dependencies/libopenmpt-web-0.8.10`。
下载地址为 `https://lib.openmpt.org/files/libopenmpt/dev/libopenmpt-0.8.10+release.dev.js.tar.gz`，
SHA256 为 `71f930d56141cef1b45d4ecee423617a5daaf9068bb5e2bddd1ce4c3044071a9`。
分发包包含库及其附带依赖的许可证。

## Web 模态窗口调度

商店的 `Dialog::WaitForResult` 逐阶段调用 `UpdateAppStep`，普通主循环则使用
`UpdateApp`。Web 的 `HostYield` 仅在完成逻辑更新、进入绘制阶段时让出执行权，
避免每个消息／更新／绘制阶段都调用 `emscripten_sleep(0)`，叠加浏览器定时器等待。
不改变原生平台的调度，不增加游戏对象或存档字段。
同一无头 Edge、240FPS 设置下的定位短测：商店约 64.6→178.4FPS；
同期菜单约 192.9→191.4FPS，无尽约 147.6→143.5FPS。不是锁频正式基准。

## Toy 账号存档调查（2026-11-05）

官方 SDK 文档：https://www.bilibili.com/toy/publish/sdk
官方调用节奏说明：https://github.com/bilibili/toy/blob/main/skills/toy/references/content-checklist.md

`getCloudStorage` / `setCloudStorage` / `removeCloudStorage` 按登录用户与 Toy 隔离，
不必为云存档读取 UID 或弹出资料授权；可选的 `getUserProfile` 才用于昵称、头像和
当前 Toy 内的假名标识。云存储是键值托管，不是任意 SQL 数据库。
当前上限 128 个键，每值 UTF-8 字符串 1024 字节。保存应批量在退出／过关等事件发生时做，
不轮询、不逐帧请求，限流 307044 需退避。SDK 只能在实际 Toy 宿主中联调。

完美进度 `user1.dat64` 为 894 字节，可以压缩编码分片后托管。
本轮无尽档 673781 字节：gzip 后 140974 字节，Brotli quality 11 后 124680 字节，
后者 Base64 166240 字节，需要 163 个满片，尚未计索引与用户进度，超过平台配额。
不能承诺完整无尽战场跨设备恢复；需更紧凑的快照或另配后端。当前仍仅本地 IndexedDB。

不接入云服务器，进度仅保存在本地 IndexedDB；生存模式限制尚未启用。

## 相同包体的两种配置

`build-web-toy.ps1` 编译一次、打包两次。两个 ZIP 的文件名集合与内容完全一致，
唯一例外是 `web-config.js` 中的 `bypassBilibiliChecks`：

- GitHub／离线：`out/wasm/pvz-perf-toy.zip`，值为 `true`，不会加载 B 站 SDK。
- B 站：`out/wasm/pvz-perf-toy-bilibili.zip`，值为 `false`。

两者均是多文件站点，不将二进制转换成 HTML 内嵌编码。解压后用 `start-local.cmd`
或 `sh start-local.sh` 开启 localhost；需要 Python 3，不需要联网或第三方 Python 包。
不能直接通过 `file://` 打开 `index.html`。同一浏览器的存档按访问地址／端口隔离。

源码配置为 `tools/wasm/game/web-config.json`；成品配置为 `web-config.js`。
两个领取入口默认均检查关注状态，作者本人也可通过；只有完整成功响应中的
`isFollowing` 或 `isAuthor` 为真才通过。修改成品配置后刷新即可，不需要重新编译。
将 `perfectSaveRequirement` 改为 `coin` 时才启用 `rewardVideoBvids`：任意一个指定
BV 已投币即可领取完美档；空列表不视为通过投币验证。

领取按钮使用官方 SDK 文档中的 `getAuthorRelation()`（`isFollowing`／`isAuthor`）与
`getVideoUserActions({videos:[{bvid:...}]})`（逐视频 `coinCount`）。仅在点击时查询，
成功后把随包发布的档复制到 IndexedDB；不在游戏循环中判断账号状态。
`unsupported`／未登录／授权未开启不能当作已满足条件；外部手机浏览器不支持视频互动查询。
接口拒绝、部分成功、零投币、未关注、离线零调用已通过单元与浏览器模拟测试，
但真实 B 站宿主查询尚未验证，也尚未指定正式 BV 号。

离线开发首次安装完美进度和无尽基准；B 站配置首次不导入，保留游戏正常建档流程。
导入完美档会替换 PerfTest 用户资料；导入无尽还会替换其泳池无尽关卡档。
两者都包含坚果包扎术。当前随包只有泳池无尽基准，其他经典阵型待提供兼容存档后加入。
存档与程序同包分发，领取按钮属于便利入口，不是加密或防提取机制。
