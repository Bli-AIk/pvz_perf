# 共享选择、拖放提示与通关奖励验证

本轮先完成本机验证，随后用户重新连接手机，已安装并启动本轮 APK。手机触控手感及新增功能的人工验收仍待用户测试。夹具链接 Windows Vulkan Release 的实际生产对象，读取匹配的 main.pak64；不是独立复制业务算法的测试。原始输出见 touch-input.log，基础回归见 native-tests.log，源文件及产物哈希见 manifest.json。

- 再点已选卡牌／铲子取消；轻点保留、失败拖放保留、再次点地种植仍然通过。植物和铲子共用一份触控选择，鼠标独立。
- 蓝色四角框来自未改像素的 TV PNG，以独立资源加载。800／1400 逻辑宽度下的卡片、铲子绘制均与独立整数像素裁切参考逐像素相等，并检查蓝色角可见。touch-frame-*.png 为实际 GPU 读回。
- 白色行／列宽带在平地、泳池、屋顶的预期格交叠，交叠区域更亮；触点取消后图像完全回到黑底。touch-guides-*.png 为隔离植物幻影后的实际读回，供检查宽带形状，不是游戏截图。
- 关卡铲子与花园浇水壶／手套／出售工具使用实际触点高亮植物；测试核对植物原有 additive 发亮标记，移开后清掉目标，不修改物理鼠标坐标。花园夹具为真实 Board／Plant／ZenGarden 对象的定向工具场景，不等于花园全流程验收。
- 两指独立拖动玉米炮；同炮争用、先后松手、取消、鼠标竞争、目标死亡、保存中断都通过。0／80 像素距离不发射，81 像素发射，60×60 对角拖动按直线距离发射；每次松手清掉目标，随后点地不发炮。鼠标原有防误触规则保留。
- 实际新植物奖励在按下时领取，钱袋在线段移动中领取，奖杯在抬指时领取；三者均调用真实 Coin::Collect 和 Board::FadeOutLevel，并核对已启动结算、全部触点结束、正在瞄准的炮未发射、迟到抬指无副作用。本夹具只推进到原有 FadeOutLevel 交接，不宣称逐一完整打通结算画面。
- CoinType 全集分别在普通／锤僵尸命中规则下对照原鼠标公式；可扫取范围是阳光、普通金钱和 IsLevelAward 奖励。普通掉落可用卡片、普通盆栽礼物不被此规则扫取。另覆盖跨过矩形而两端在外、只包围盒相交、排除右／下边界、死亡、正在收取、暂停和重复经过。
- 选卡 Menu 的 800／1400 宽度触控／鼠标与实际对话框回调，原有 DAT64 读写／回调内保存、按钮捕获、用户列表和鼠标隔离仍通过。

四项 CTest 为 pvz_game_viewport、pvz_time_counter_save、pvz_v7_save_schema、pvz_android_touch_input。触控状态不进存档，格式仍是 DAT64 v4。本轮没有正式性能倍率或手机实机结论。

## 宽带来源调查

WP 参考提交 26cbb77df48c674dca6cb6b1fa9ad6783903a5c4 的 CursorObject.DrawGroundLayer 遍历目标行／列，Board.DrawCelHighlight 调用 FillRect，没有宽带图片。Free 3.18.0 的 Board::DrawCelHighlight 位于 0x1a0eed8..0x1a0f52c：设置纯白颜色，调用 PolyFill，并恢复混合模式；该函数没有纹理绘制调用。指令保存在 free-DrawCelHighlight.asm.txt，源文件 URL 和哈希在 highlight-sources.json。这里核对的是上述样本，不把 WP 后续 MonoGame 移植代码等同于发行 DLL 的逐方法复核。

本项目宽带采用普通半透明白色矩形；没有声称与 WP／Free 的混合参数逐像素一致。当前蓝色框仍是独立图片，未合进图集。

## 后续装机

已在 FY241471004E（nubia Z60 Ultra / NX769J）上使用 `adb install -r` 更新并通过 PerfActivity 启动，安装时游戏进程未运行。换包前后外部 media 目录的 17 个文件 SHA256 集合完全一致，未覆盖设置或存档。原生库、PAK 和全部 SPIR-V 已核对与本次 Android 构建逐字节相同；APK 通过 v2/v3 签名验证及 16KB 页对齐检查。观察到进程 PID 7308 持续运行且游戏画面正常，不把此启动检查当作新增触控功能已经人工验收。APK 路径和哈希见 manifest.json。
