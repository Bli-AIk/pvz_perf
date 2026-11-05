# 资源包重压缩

常规 `main.pak64` 使用 ZIP_STORED。游戏直接映射条目数据，当前不接受
ZIP Deflate 条目。压缩选项只改变 PNG 内部编码，不改变这一运行时契约。

完整资源编译可追加：

```powershell
python tools/resourcePipeline/src/build_resources.py --compiler <资源编译器路径> --cache <缓存目录> --compact lossless
# 或 --compact light-lossy；可指定 --compact-output 和 --compact-workers（默认 2）
```

也可以复用已经编好的包，无需重新装箱、求解和生成 C++：

```powershell
python tools/compact_runtime_package.py out/x64-win32-vulkan/Release/main.pak64 --mode lossless
python tools/compact_runtime_package.py out/x64-win32-vulkan/Release/main.pak64 --mode light-lossy
```

默认输出同目录的 `main-compact.pak64` 和 `main-compact.report.json`。
原包、源图片、生成的 C++、权威 manifest 均不修改。重复执行会替换这两个试验输出。

- `lossless`：尝试更高强度的 zlib 编码和 RGB/RGBA PNG 重新滤波，逐图检查解码像素不变。
- `light-lossy`：在无损候选基础上，仅对 `compiled/**/*.atlas.png` 的 RGB
  以步长 4 四舍五入；每个颜色分量最大误差 2/255。透明度完全不变。
  独立图片、字体、透明蒙版、带 tRNS 色键的 PNG 均保持无损。
- 保持 PNG 文件格式、尺寸、IHDR 和全部非 IDAT 元数据；不转换调色板，
  不修改 UV，不缩放，不转码 JPEG/GIF/音频，不删除任何资源。
- 每个候选只有比原文件小时才采用；APNG 原样保留。
- 报告记录每项前后大小、SHA256、实际 RGB 最大误差和均方根误差。
  ZIP 路径集合、CRC、非 PNG 内容和透明度均在生成时检查。

无损包可使用 `tools/verify_runtime_resources.py` 做完整源码像素核验。
轻有损包会有意不符合图集源像素 SHA256，因此不能把该校验的像素差异报错
解释为缺资源。它使用重压缩器的原包逐项比较和误差上限检查；画面观感仍需人工确认，
尤其是平滑渐变。当前不默认用于发行，也不自动替换游戏读取的 `main.pak64`。
