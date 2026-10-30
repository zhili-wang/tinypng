# 安装包目录

安装包按版本存放，例如 `Installer/1.2.0/`。每个版本目录包含对应的 macOS DMG、Windows EXE/ZIP 和 `SHA256SUMS.txt`，历史版本分别保留。

构建脚本自动生成 `Installer/<版本号>/` 并更新该目录的校验文件。同一版本的不同平台包可以放在一起；从 CI 下载补充产物后，运行 `python3 scripts/package-checksums.py Installer/<版本号>` 更新完整校验文件。

已公开发布的安装包从 [GitHub Releases](https://github.com/zhili-wang/tinypng/releases) 下载。
