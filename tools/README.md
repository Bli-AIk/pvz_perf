# 工具与测试

资源构建链、DSL 编译器和 DAT64 转换器的源码已迁往独立开源仓库 [PvzDevTools](https://github.com/YuHao2006/PvzDevTools)。本仓库只保留 `bin/windows-x64/` 中的预编译 Windows 工具；工具 Release 也在该仓库发布。

主 CMake 导入这些主机程序，不编译工具源码，不运行 Python 资源流水线。其他平台自行构建工具后设置 `PVZ_DEVTOOLS_BIN_DIR`；Android／Web 交叉编译仍使用主机工具，不把它们带入 APK 或网页资源包。

`reanimc.exe` 编译定义 DSL、字符串和轨道标识；`pvz-resource-build.exe` 处理配置、字体、CP-SAT 图集、PNG 合成、静态 C++ 输出、资源加载绑定和 PAK64。源资源、枚举顺序和图集缓存继续保存在游戏的 `assets/`；编译器使用的字体标签、轨道族元数据位于 `assets/definitions/toolchain/`。

`pvz_dat64_converter.exe` 支持原版 1073 DAT、DAT64 v1／v2／当前 v4，以及用户资料转换。未发布的 v3 中间格式不支持。冻结资源映射、格式快照和转换器回归测试随源码迁往 PvzDevTools。

本目录中的发行打包、平台启动、检查脚本和 `native-tests/` 不属于游戏正式源码，继续保留 Python／PowerShell。性能工具在 `Tests/performance/`。资源工具的 Python 旧实现只在 PvzDevTools 的 `tests/reference/python` 用于对拍；游戏侧工具源代码审计测试通过 `PVZ_DEVTOOLS_SOURCE_DIR` 指定独立 checkout，主 CMake 会传入必要的测试环境。

`publish-github-release.py --source-only --tag <标签> --commit <提交> --notes <说明文件>` 仅发布源码归档，不接受 `--asset`，不覆盖已有附件。
