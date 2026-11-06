# 预编译开发工具

源码与 Windows 工具 Release：[YuHao2006/PvzDevTools](https://github.com/YuHao2006/PvzDevTools)。本目录不包含编译器、构建器或转换器源码。

`windows-x64` 中的 `pvz-resource-build.exe`、`reanimc.exe` 在开发主机运行。图集求解器的 DLL 仅为开发工具依赖，不随游戏、APK 或网页资源包部署。`pvz_dat64_converter.exe` 还会复制到桌面游戏的 `data/` 供玩家转换存档，其发行构建使用经过 Win7 导入检查的旧工具链。

二进制版本、字节数和 SHA-256 见同目录 `manifest.json`。其他主机从独立源码仓库构建后，可用 `PVZ_DEVTOOLS_BIN_DIR` 指向工具目录。无需把该源码复制回本仓库。
