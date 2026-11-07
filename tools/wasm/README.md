# v3.3.5 网页版：WebGPU / WebGL 2 / Wasm

独立 CMake 工程 `game/` 编译当前 pvz_perf 游戏逻辑，不替换为上游游戏。
发行包包含 WebGPU 和 WebGL 2；启动时默认优先 WebGPU，不可用时使用 WebGL 2，也可手动选择。
两个运行时共用资源和存档，浏览器只下载所选后端的 Wasm；切换接口重新加载启动页。
两者是平级构建后端，浏览器窗口使用 SDL2 的 Emscripten 端口。
OpenGL / WebGL 共享 `graphics/gl` 的绘制实现，各自的上下文、API 差异由编译期别名选择。
浏览器调度、加载、文件路径和音频实现放在 `platform/web`，由 CMake 选入；
公共游戏代码不包含 `__EMSCRIPTEN__` 分支，原生目标不引入 SDL 或浏览器运行库。

## 构建和预览

本机已安装 Emscripten 6.0.13。需要 libspng 0.7.4 源码和 nlohmann-json 头文件，
可通过脚本参数指定；CMake 不会把 Windows 的库文件链接进 Wasm。
SDL2、zlib、libjpeg 由 Emscripten ports 为 wasm32 编译。

```powershell
pwsh -File tools/build-web-toy.ps1
python -m http.server 18089 --bind 127.0.0.1 --directory out/wasm-webgpu/toy
```

浏览器打开 `http://127.0.0.1:18089/`，等待加载后点击“开始游戏”。不能用 file://
直接打开，需要 HTTP。包位于 `out/wasm-webgpu/pvz-perf-toy.zip`，
GitHub 附件名为 `wasm32-web-v3.3.3.zip`；入口及资源 URL 均使用相对路径。
源码对照构建可显式使用 `-GraphicsBackend WEBGL`，输出至原 `out/wasm/` 树。
ZIP 内 `web-build-manifest.json` 记录版本、Web 补充源码提交和每个资源的 SHA256。
v3.3.3 标签包含首发原生和 Web 源码；后续替换的网页附件以包内 `source_commit` 记录的补充提交为准。

`package_toy.py` 只打包 HTML、JS、预加载资源数据和 gzip 后的 Wasm，
不打包原始 Wasm、调试符号、日志和验证截图。页面用浏览器 DecompressionStream
解压 Wasm，再通过 instantiateWasm 交给 Emscripten。两种体积（文件总和、上传 ZIP）
都会打印，并分别标记总量的 50,000,000 字节／50MiB 和 ZIP 的 50,000,000 字节阈值。
默认直接使用 Release 的 `main.pak64`，不再默认采用 compact 资源；需要时可通过
`-PakFile` 或 `PVZ_WEB_PAK` 指定其他包。普通包与 compact 包分别约 45.25MB、37.49MB。

## 平台边界

- 手机 UA 默认建议 60 FPS，可手动修改；手机 B 站包不检测刷新率，不自动跟随。桌面行为保留。
- Web 触控以画布所属窗口路由，不要求 SDL 键盘焦点已恢复；焦点变化只入队，在正常事件阶段处理，不重入游戏更新。

- WebGPU 启动时申请可用的双源混合；WebGL 2 路径 normal/additive 分开提交。
  两者均为 SDR，不支持原生 HDR/超分管线。
- BGRA 上传在 Web 平台转换成 RGBA；WebGPU 使用 WGSL，WebGL 对照将 GLSL 转为 ES 3.00。
- WebGPU 的普通贴图使用 Vulkan 1.3 同类路径的单次硬件采样，颜色过滤采用保持色相的闭式公式；需逐 texel 变换时才使用手工采样。目标预乘状态用管线常量区分，覆盖率文字使用独立入口。主帧复用绑定组和顶点缓冲，场景与呈现记录到同一命令编码器再提交，呈现使用全屏三角形。
- 使用 Asyncify 让主循环及模态等待交还浏览器。加载暂时在主线程执行，直接完成纹理
  队列与图集上传，不等待被挂起的外层循环。没有引入 SharedArrayBuffer、COOP/COEP 或 pthread 要求。
- 主循环按 requestAnimationFrame 让出执行权；两套动态顶点流各自跟踪完成状态。
  WebGPU 由队列完成回调标记，WebGL 使用 fence 零等待轮询；忙时保留绘制脏标记并重试。
  纹理与场景目标共用，正常主帧不等待整个队列清空。
- 音乐定位后先解码并保留一个采样帧，使首行命令先执行，再恢复控制器的 BPM／speed；
  读取这两个属性时等待音频线程确认，避免读取异步旧值。读档及暂停恢复共用此路径，
  不丢弃这个预解码采样，不改动原生 BASS。
- Web 专用 BASS 调用适配层以 libopenmpt 0.8.10 解码 MO3，AudioWorklet 输出 PCM；
  非安全上下文退回 ScriptProcessor。普通 OGG 音效由随 Wasm 编译的 stb_vorbis 在加载期解码为 AudioBuffer，不调用浏览器的 OGG 解码器，
  两个 AU 音效按 µ-law 解码；播放时直接启动缓冲源，支持声像、实时音量和暂停。
  credits OGG 使用相同的 Wasm 解码器，再由 Web Audio 播放 PCM。Windows／Android 仍使用原有 BASS，不加载 Web 解码器。
- `/persist` 挂载 IndexedDB；首次创建用户，正常 Quit 后同步存档。
  不接触桌面/Android 的 data、settings 和现有存档。未承诺跨平台存档互读。
- 浏览器输入透过独立 NativeWindow 实现进入现有鼠标/触控事件路径；不会把触控改成全局鼠标位置。
- Wasm 没有可写 FTZ/DAZ 控制寄存器，不能沿用原生开关。

v3.3.3 提供双后端网页 ZIP；构建脚本不登录或上传 Bilibili。
浏览器与宿主能力仍需实际检测，不能把桌面 Edge 验证当作所有手机 WebView 均可运行。

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
Custom Scale 在 Web 禁用。游戏 Quit 完成存档同步后直接返回启动设置，复用当前 Wasm 实例和已挂载的 IndexedDB 文件系统，
不用点击额外的“重新开始”按钮，下次只需一次“开始游戏”。
网页每秒读取一次游戏实际绘制计数来显示 FPS，不提前启用 C++ 调试字体。开发档独立放在 IndexedDB 的 `/persist/development-v331`，
不覆盖早期 `/persist` 下的试玩档。离线配置首次复制发行的完美存档与无尽基准，之后保留游玩进度；
B 站不自动部署完美用户，可在“用户进度”页新增用户并编辑进度。

局域网部署可用 `python -m http.server 18089 --bind 0.0.0.0 --directory out/wasm-webgpu/toy`，
手机访问电脑 LAN IP（需要同网和防火墙允许入站），Wasm 在手机本地执行，非桌面串流。
WebGPU 在手机普通 HTTP LAN IP 下通常不可用；跨设备试玩应部署 HTTPS。
WebGL 源码对照可以使用普通 HTTP；AudioWorklet 同样需要安全上下文。
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
不能承诺完整无尽战场跨设备恢复；需更紧凑的快照或另配后端。关卡 DAT64 不上传云端，仅通过本地导入／导出备份。

用户进度通过 Toy SDK 云存储备份；不接入额外云服务器。生存模式限制尚未启用。

## 相同包体的两种配置

`build-web-toy.ps1` 编译一次、打包两次。两个 ZIP 的运行库和资源相同；
`web-config.js` 中的 `bypassBilibiliChecks` 以及反映该配置的校验清单不同：

- GitHub／离线：`out/wasm-webgpu/pvz-perf-toy.zip`，值为 `true`，不会加载 B 站 SDK。
- B 站：`out/wasm-webgpu/pvz-perf-toy-bilibili.zip`，值为 `false`。

两者均是多文件站点，不将二进制转换成 HTML 内嵌编码。解压后用 `start-local.cmd`
或 `sh start-local.sh` 开启 localhost；需要 Python 3，不需要联网或第三方 Python 包。
不能直接通过 `file://` 打开 `index.html`。同一浏览器的存档按访问地址／端口隔离。

源码配置为 `tools/wasm/game/web-config.json`；成品配置为 `web-config.js`。
“自定义存档”入口检查：给 `rewardVideoBvid` 指定的视频投满两个币，或当前用户为作者本人。
当前配置为 `BV1hV4P6PEDc`。仅关注、一个币、错误 BV、未登录或失败响应都不放行。
使用 SDK 的 `getAuthorRelation().data.isAuthor` 和
`getVideoUserActions({videos:[{bvid}]})` 中同 BV、成功项的 `coinCount >= 2`。
只在打开自定义存档时验证。弹窗打开后，生成、部署、提取与导出均不重复查询；关闭后重新打开才再次验证。

预置档以 `save-bundle.json` 随包分发，不依赖宿主单独分发 `.dat64` 后缀文件。
预置档只用于开发起点与离线构造模板。没有整批覆盖用户的领取按钮，新增、删除与编辑均针对所选用户；设置保留。

网页提供音效音量、音乐音量、启动时独占全屏配置。未启用独占全屏时只铺满网页。
浏览器／WebView 可拒绝 Fullscreen API，网页不能保证系统允许独占全屏。

### 自定义无尽与 DAT64

- 默认已完成 **128 旗**（每轮两旗，填写偶数），随机种子 **114514**。
- 场景：白天、黑夜、泳池、浓雾、屋顶、月夜；在泳池无尽入口读取实际场景。
- 默认阵型来自现有无尽基准；支持添加自己的 Toolkit 1.22 阵型码。
- 阵型码只记录布阵、叠层、睡眠／模仿者、梯子与钉耙等，不保存血量。
- 生成使用正常植物构造，满血重建；Continue 后通过原有无尽换轮流程进入选卡。
- 无尽生成档保存选卡曲的正确 MO3 起点，使用解码器实际读取的 BPM／speed；开始战斗后由正常游戏流程按场景切曲，不写死构造默认速度。
- 默认部署 `game1_58.dat64`，可以指定其他已存在用户和模式。生成冒险档时同步该用户的当前冒险关卡。
- 在自定义弹窗中选择浏览器内部的当前格式 DAT64 提取阵型码，也可直接下载所选完整 DAT64。
- 创建、部署、提取和导出只在游戏未运行时执行；游戏内请先保存并 Quit 返回设置页。

离线工具是临时、同源 `srcdoc` iframe 内的独立 Wasm 实例，复用同一份已下载的游戏二进制与资源；
不导航到第二个 HTML 路由，iframe 沙箱禁止顶层导航，避免 Toy 的 HTML 页面壳接管工具地址。
只初始化构造／序列化所需资源，不进入游戏更新和绘制循环，不挂载 IDBFS、不写用户存储。
生成使用新 Board，提取使用独立读档对象；完成后销毁整个工具实例。
代码位于 `tools/wasm/game/web/SaveTool.cpp`，正常游戏 `Board::LoadGame` 无导入钩子。
工具资源请求交给主页面上下文执行；Wasm 同时接受 gzip 文件和宿主已自动解压的响应，避免重复解压。
错误分别标出投币查询、资源文件／HTTP 状态、阵型解压，不把它们都显示成裸 `Failed to fetch`。
Toolkit 阵型格式参考 [pvztoolkit 的 lineup.cpp](https://github.com/lmintlcx/pvztoolkit/blob/master/src/lineup.cpp)。

当前原始基准与自定义满血重建档用途不同：前者保留原参数供性能复测，后者可修改场景／旗数／种子。
相同种子只在同实现、同初始条件与同随机调用序列下可重现，不承诺跨游戏版本出怪一致。

### 浏览器保存位置与刷新节奏

游戏虚拟目录 `/persist/development-v331/` 持久化到 IndexedDB，数据库 `/persist`、对象存储 `FILE_DATA`。
它属于当前站点／嵌入上下文的浏览器存储，不是 B 站账号云存档；换浏览器、设备或访问地址不共享。
可在开发者工具 Application → IndexedDB 查看。手机 App 内置浏览器通常放在其私有数据中，
不是文件管理器可直接浏览的一组 DAT64；需要使用网页导出按钮。

运行时从真正等待的 requestAnimationFrame 回调观测节奏，用两段各 12 个间隔确认稳定变化。
只在 Web 原生事件阶段应用自动目标刷新率；保留墙钟欠账和逻辑时间余量，不用帧数冒充时间。
主循环已有实际墙钟纳秒步长，这次不修改共享主循环。宿主仍可在无操作时限帧、隐藏页时暂停回调；
网页不能保证物理面板刷新率，也不伪造触摸事件强行保活。

GitHub Release 提供离线配置的 WebGPU ZIP，不自动更新 Toy。SDK 的真实权限和手机 WebView 下载／嵌套工具
仍需在实际 Toy 宿主验证；本地浏览器模拟覆盖两个币／作者与拒绝路径。

## WebGPU 后端

`pwsh -File tools/build-web-toy.ps1 -GraphicsBackend WEBGPU` 输出至 `out/wasm-webgpu/`，
保留原 `out/wasm/` WebGL 构建。编译与链接使用 Emscripten 官方 `--use-port=emdawnwebgpu`，
绘制、纹理、绑定组、命令录制和队列提交均由 C++ 后端负责；WGSL 源码位于 `.cpp`。
公共路径只扩展编译期后端选择和已有能力分支，不增加每次绘制的运行期接口分发。

Web UI 在运行游戏前检测 adapter、device、可选特性和限制。发行包只包含 WebGPU，
检测失败时明确显示不能启动，不携带或下载 WebGL 回退运行库。原 WebGL 包单独保留作对照。
WebGPU 需要安全上下文；
本机 localhost 可用，手机通过普通 HTTP 的局域网 IP 访问通常不开放 WebGPU，需要 HTTPS。
B 站内置浏览器是否开放由实际内核、设备和宿主策略决定，不能按 UA 推定。

- 继续使用两套动态顶点缓冲，各自跟踪队列完成；第三拍复用第一套之前检查完成状态。
  未完成仅延期绘制，不跳过逻辑。纹理、图集和静态 16 位四边形索引共用，不复制纹理。
- 缓存纹理／采样器绑定组、目标参数及呈现绑定组；相邻批次复用管线、绑定组和裁剪状态。
  每批上传动态顶点，保留 65536 顶点容量，满时提交并继续，不扩页。
- 检测并显式申请 `dual-source-blending`。支持时普通精灵的 normal/additive 可合批，
  不支持时保留各自混合批次。`?dualSource=off` 仅供检查分开提交路径。
- 没有 Vulkan descriptor indexing 式 bindless；缓存绑定组不称为 bindless。
  GPU 不计算 reanim 或 particle 行为，存档格式不因切换后端变化。
- 字体使用原有整数权重生成 RGBA16F，普通纹理／主场景使用 RGBA8；透明与生成图像走
  对应采样路径。显式读回可以等待，正常主帧不调用全队列同步等待。

验证脚本：`verify_webgpu_shaders.mjs` 检查 WGSL；`test_graphics.mjs` 检查能力选择／失败回退；
`verify_webgpu.mjs --endless` 检查真实游戏。独立 `Tests/native/WebGpuRuntimeProbe.cpp`
链接实际 WebGPU 后端，覆盖上传读回、局部复制、alpha、字体、混合、coverage、容量边界和帧准入；
它不编入游戏。性能是否改善须用相同场景对照，不把 API 可用或满上限当作提速证明。

自定义无尽的默认阵型为97无炮、14格、24炮、[终极战术]五角星无炮流；
用户自存阵型单独保留。生成器接受初始阳光（0～9990，默认9990），选卡阶段恢复，
不生成小推车。领取完美档附带的原始无尽基准不受自定义生成规则影响。

## v3.3.4beta 兼容性修复

- 资源编译统一限制纹理宽高不超过 4096，字体长条离线无损折行。WebGL 启动页读取并显示实际纹理上限，低于资源要求时不提供该后端。
- `EncodingError: Decoding failed` 原先可能来自浏览器 OGG 解码。音效与 credits OGG 统一由 Wasm 解码，MO3 仍使用 libopenmpt；播放热路径不增加判断。
- 启动等待与音频初始化有超时提示，错误面板包含启动阶段、图形能力和浏览器信息；WebGL 上下文丢失显式报告。它们不能替代发生黑屏的实体设备复测。
- 默认使用 `out/x64-win32-vulkan/cache/resources/main-compact.pak64`。轻有损只量化图集 RGB，alpha、尺寸不变；字体与普通图片保持像素无损。
- `test_audio.mjs` 验证 PCM 传递和 AU 解码；`verify_mobile_compat.mjs` 将纹理上限约束为 4096、禁止浏览器 OGG 解码，再验证加载及图鉴。

同一页面内连续进入／Quit 不重载网页，不重新下载 Wasm 或资源，不重复执行 preRun／挂载存档。每轮正常销毁游戏实例的 GPU、音效和资源包映射，下一轮重新创建游戏实例，避免保留失效资源指针；设置与存档由同一个 saveRoot 和浏览器存储键管理。主动切换图形接口会加载另一个 Wasm，依然读取同一份设置和存档，不创建后端专属配置。
`verify_session_reuse.mjs` 分别连续进入和退出两种后端三次，并验证跨后端读取相同配置。

## 用户进度与关卡文件的持久化

- `/persist` 挂载关闭自动持久化。原版语义会在读取后移除关卡文件；这一步不再自动删除 IndexedDB 中的旧检查点。正常 Quit 后才显式同步新状态。同步失败会保留 MEMFS 内容并显示错误，提供重试与 DAT64 导出，不再打印后直接重载。
- 用户列表与 `user<ID>.dat64` 在写入事件和 Quit 时备份至独立的浏览器数据库；不逐帧扫描。较新的独立备份可修复主挂载丢失或未成功提交的用户资料。
- B 站配置通过 `getCloudStorage`／`setCloudStorage` 备份同一组用户资料。使用 gzip、Base64、分片与 SHA-256，数据先写入另一槽，再提交索引。中断提交不覆盖上一份完整进度；相同进度不重复上传，忙时仅保留最新待写快照。云端冲突由用户选择上传本地或恢复云端，不静默覆盖。
- 不上传 `game<ID>_<mode>.dat64`、游戏设置或资源。关卡导入／导出统一放在投币解锁的“自定义存档”中，打开后不重复查询。导入只在游戏关闭时执行，先在隔离的离线工具实例中完整试读，再写入用户指定槽位；现有目标用户必须存在，文件字节不重写。
- 云端能力与账户命名空间最终由 Toy 宿主决定。离线包不连接 B 站 SDK；云端错误只影响云同步，本地游戏仍可使用。真实账户往返需在试验版内联调；单元测试不等同于线上账户验证。

### 可视化存档修改器

网页顶部提供开始游戏、上传云端、从云端恢复；实际上传期间显示红色提醒。存档只保留一个授权入口“自定义存档”，用户进度是首页，可增删用户、修改 ID 与进度；另有逐格布阵、六场景参数、DAT64 导入／导出和文件名对照。

界面源码位于独立仓库 PvzDevTools 的 `saveEditor/`，`tools/bin/web/save-editor.html` 是生成产物，旁边的 manifest 记录源提交和 SHA256。集成层 `save-editor-host.js` 校验同源 iframe、当前授权、游戏已关闭与串行任务状态。它不修改共享游戏代码或存档格式。正式版和试验版继续共用现有来源下的本地存档；云端按宿主 Toy 隔离。

打包时将编辑器文档生成为 `save-editor-document.js`，授权后通过模块加载，并在继承当前来源的 `srcdoc` 中打开。不要请求第二个 `.html` 页面：Toy 会对 HTML 请求返回自己的查看器／密码页，`fetch` 后再嵌入也会把密码页当成编辑器；主页面已经通过的访问校验不能代替这次请求。此改动不改变投币检查时机和存档目录。


用户编辑使用 PvzDevTools 生成的 `save-codec.js`，不启动游戏对象、不改存档格式。修改 ID 会迁移该用户的关卡文件，未编辑的花园和大头贴字节保留；删除仅作用于选定用户。整批领取完美存档入口已移除。生成文件名默认为 `game1_58.dat64`，支持其他用户／模式；模式 0 另传 1～50 的冒险关卡。文件名对照按实际菜单入口将隐藏无尽放在 limbo 折叠项中。

阵型库使用 PvzDevTools 生成的 `lineup-library.js` 读写 JSON：`{format:1,lineups:[{name,code}]}`。以阵型码标识条目，不分配整数 ID，不依赖 SQLite；支持整个阵型库的 JSON 导入／导出，原本两份本地列表会迁移。列表位置不再作为选中条目的身份，删除后保留未保存草稿，清空列表也不会恢复默认阵型。

Toy 云端单独使用 `pvz_lineups_v1` 索引和字符串分片保存压缩 JSON 快照；分片序号只是传输格式，不是阵型 ID。新增、删除、JSON 导入完成后同步，未修改不重传，不逐帧查询。数据先写另一槽，索引最后提交，空列表也是有效快照，防止中断上传或恢复时复活已删除阵型。顶部“上传云端／从云端恢复”同时处理用户进度和阵型库；冲突需明确选择方向，关卡 DAT64 仍不上传。SDK 模拟测试不能替代真实账户联调。

图形接口、画面比例和显示帧率放在默认折叠的“高级”。“重试本地保存”仅在 IndexedDB 写入失败时出现，成功后隐藏；上传云端期间红字提示保持不变。

启动前分别对可用后端执行一次 RGBA16F 混合、线性采样至 RGBA8、像素回读验证；仅创建上下文或完整 framebuffer 不足以判定绘制可用。验证资源随后释放，不进入正常游戏帧。启动监控等待资源完成标志和其后一次绘制，不能因装载画面已有绘制计数就提前撤销。启动期间显示当前阶段，超时报告包含后端、浏览器、音频状态和画布尺寸；后台停留不计超时。AudioWorklet 在等就绪消息前接入输出图，初始化有时限；音乐节点失败后断开并保留音效，正常退出后可重试。桌面 Chromium 回归不等于 iOS WebKit 真机验证。

音乐工作线程不可用时只停用 BGM，并显示非致命提示，保留 WebAudio 音效。解码器下载、Worklet 初始化、命令／渲染异常及状态屏障超时都结束挂起的同步请求，不把 Promise 拒绝传到 Wasm 主循环；保留最初错误及命令信息。普通宿主暂停不判定为音乐崩溃。失效节点会断开，之后不重复等待；正常 Quit 清理后新一轮可重试。音乐禁用时 credits 的虚拟音频时钟仍推进，避免片尾被音轨位置卡住。正常 AudioWorklet 与同步 ScriptProcessor 路径保持原来的 MO3 解码和播放，后者持有自己的 `mixer`，不等待异步回包。

修改器内确认操作使用 HTML dialog，不能依赖 Toy 祖先 iframe 可能禁止的原生 `confirm()`。`verify_editor_sandbox.mjs` 覆盖没有 `allow-modals` 的两层 iframe，验证取消、删除和刷新后的列表持久化；`verify_audio_failure.mjs` 注入丢失回包、命令失败与模块失败并验证游戏继续绘制，同时验证 ScriptProcessor 正常同步路径。

开始前设置页使用文档原生滚动，面板处于正常文档流，不再将长表单放进“根页面禁止滚动＋绝对定位容器滚动”的结构。进入游戏后才锁定根页面并禁用画布手势，Quit 时恢复文档滚动。启动尺寸由可见窗口扣除页眉页脚计算，不使用长表单撑高后的容器高度；这些操作只发生在 Web 布局和模式切换。`verify_launcher_scroll.mjs` 使用原生触控滚动手势覆盖直接打开和 Toy 式 iframe 的设置页上下滚动。
