# 性能讲解视频源码

当前预览使用[motion前端动画](motion/README.md)，由Canvas按时间绘制角色、查询过程、绘制批次与原始测量曲线。先检查网页，再确认配音和视频导出；不自动生成MP4。

`storyboard.json`是逐段讲解与伪代码，`voiceover-zh.txt`是供豆包网页试听的纯配音文本，`tables.json`是由已核验原始数据生成的表格。字幕位于`motion/film.js`，修改文案时保持二者同步。配音中的“子弹类型”与“低功耗核心的十一倍频”使用自然语言，具体LP14及条件仍在画面和证据中保留。仓库链接由画面展示，无需朗读。配音保留“行”字，行索引、当前行及选行中的“行”须读háng；不要改写为“横排”。

正式性能文档不包含横向作品比较，视频单独保留已授权的比较段；两轮是同阵型、相似出怪，不是完全一致的战斗。

以下为早期静态画面工具的使用说明，保留用于重建历史产物，当前动画预览不走此流程。

使用Python/Pillow生成1920×1080画面，Windows本机Microsoft Huihui Desktop离线合成普通中文配音，ffmpeg生成H.264/AAC MP4。没有模仿特定人物声音，没有向外部语音服务上传内容。

```powershell
python media/performance-video/build_video.py --tables media/performance-video/tables.json --output out/media/performance-v3.2.4
```

需要Pillow、ffmpeg/ffprobe、Windows中文TTS和System.Speech。`--slides-only`可先检查排版，不生成音频/视频。`make_tables.py`从微基准、整局汇总和横向比较记录生成表格；当前tables.json本身已足够重建视频，不依赖本机Temp目录。

交付文件：带配音MP4、无配音MP4、连续narration-full.wav、逐句captions.srt、narration.txt与timeline.json。无配音版仍保留画面内字幕，便于自行录音；如需改讲解内容，在storyboard.json修改后重新构建。

先按每个源片段实际帧数拼接无声视频，再将PCM配音补齐对应时长、连续编码一次，避免逐段AAC编码延迟造成接缝DTS错误。最终视频的音视频从零起始，字幕按实际片段帧边界重新定位。

测试表展示局部范围，不把原版x86／当前x64差值全部归因算法，不把draw次数变少当成GPU时间倍率，不把整机电池功耗当成应用独占功耗。视频中的比较只使用有效单核结果，作废双核数据不出现。
