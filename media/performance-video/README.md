# 性能讲解视频源码

`storyboard.json`是逐段讲解与伪代码，`tables.json`是由已核验原始数据生成的表格。正式性能文档不包含横向作品比较，视频单独保留已授权的比较段；两轮是同阵型、相似出怪，不是完全一致的战斗。

使用Python/Pillow生成1920×1080画面，Windows本机Microsoft Huihui Desktop离线合成普通中文配音，ffmpeg生成H.264/AAC MP4。没有模仿特定人物声音，没有向外部语音服务上传内容。

```powershell
python media/performance-video/build_video.py --tables media/performance-video/tables.json --output out/media/performance-v3.2.4
```

需要Pillow、ffmpeg/ffprobe、Windows中文TTS和System.Speech。`--slides-only`可先检查排版，不生成音频/视频。`make_tables.py`从微基准、整局汇总和横向比较记录生成表格；当前tables.json本身已足够重建视频，不依赖本机Temp目录。

交付文件：带配音MP4、无配音MP4、连续narration-full.wav、逐句captions.srt、narration.txt与timeline.json。无配音版仍保留画面内字幕，便于自行录音；如需改讲解内容，在storyboard.json修改后重新构建。

先按每个源片段实际帧数拼接无声视频，再将PCM配音补齐对应时长、连续编码一次，避免逐段AAC编码延迟造成接缝DTS错误。最终视频的音视频从零起始，字幕按实际片段帧边界重新定位。

测试表展示局部范围，不把原版x86／当前x64差值全部归因算法，不把draw次数变少当成GPU时间倍率，不把整机电池功耗当成应用独占功耗。视频中的比较只使用有效单核结果，作废双核数据不出现。
