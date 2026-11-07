pvz_perf Web

Extract the whole ZIP. Windows: start-local.cmd (Python 3 required).
Linux/macOS: sh start-local.sh. Do not open index.html with file://.
LAN: start-local.cmd --host 0.0.0.0, then visit the computer LAN IP.

Offline and Bilibili packages share game files; web-config.js and its manifest differ:
  bypassBilibiliChecks: true = offline, no Bilibili API calls
  bypassBilibiliChecks: false = require 2 coins on rewardVideoBvid OR author
  rewardVideoBvid: "BV1hV4P6PEDc"
Following alone and one coin do not grant saves. This client-side convenience
check is not encryption; the template saves are included in save-bundle.json.

Graphics defaults to WebGPU when available, otherwise WebGL 2. Select either
backend on the setup screen; changing it reloads setup and keeps browser saves.
All bundled APIs are checked before populating the selector, including WebGL 2
RGBA16F render targets, blending, linear sampling and pixel readback. Unavailable backends are omitted; an unavailable saved
choice is reset to auto. An error is shown if no bundled backend is usable.
If actual game initialization
fails after the probe, the page shows the error, browser details and a WebGL retry
button when available. Copy that error report when asking for help with a black screen.
The startup status remains visible until resource initialization and a later game
frame complete. Its foreground-only watchdog and graphics readbacks stop before
normal play. AudioWorklet initialization connects the graph before waiting for
readiness, and failed initialization can be retried after reporting an error.

Custom Save opens on user progress. Add/delete users or edit IDs, names and progress without running gameplay. Garden and Zombatar payloads are retained. No perfect-profile claim action is exposed.
The editor document is packaged as save-editor-document.js and loaded into
srcdoc; no second HTML navigation or Toy password page is required.
Settings are retained. Custom endless uses Toolkit lineup codes and supports
6 scenes, completed flags (default 128), RNG seed (default 114514), and sun (0..9990, default 9990).
Generated checkpoints use full-health plants, have no lawn mowers, and resume into seed selection.
Inside Custom Save, select an internal browser DAT64 to extract its lineup
or export the full file. The coin-video link is on the settings page.
Generate, deploy, extract lineup and export DAT64 only before the game starts.
Quit normally to save current progress and return to the Web settings screen.

Browser storage: IndexedDB /persist, FILE_DATA; virtual game directory
/persist/development-v331/. Different origins/browsers/devices do not share it.
Use Export DAT64 to obtain an ordinary file. Mobile WebView downloads and
Bilibili host permissions require actual-device verification.
Music decoder licenses: audio/licenses/.

WebGPU packages detect capabilities before starting the game.
The combined release includes WebGL 2 for devices without WebGPU.
Each backend keeps two dynamic frame buffers; textures are shared across frames.
WebGPU needs HTTPS or localhost. Plain HTTP LAN IPs generally cannot use it.
Build source and file hashes: web-build-manifest.json.

同一页面会话中 Quit 后直接回设置，Wasm 与存档文件系统复用，不重复准备。WebGL／WebGPU 共用同一份设置与存档；主动切换接口会加载相应 Wasm。

Custom Save is the single gated entry for visual lineups, endless settings, user progress editing, and DAT64 import/export. Authorization is checked once when opening it. The HTML editor is built from PvzDevTools; see save-editor-manifest.json. Upload-in-progress is shown in red.

Custom Save opens on Users: add/delete users, change IDs/names and progress without running gameplay. ID changes also rename the user's checkpoints. Perfect-profile replacement was removed. Generated filenames default to game1_58.dat64; the filename guide lists current project mode IDs, with Limbo and four hidden endless entries collapsed. Advanced graphics/aspect/FPS options are collapsed. Local save retry appears only after persistence failure.
