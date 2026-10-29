# 图片压缩软件
基于 TinyPNG 的批量图片压缩软件，支持 PNG、JPG/JPEG、WebP 和 SVG，扩展名大小写均可识别。


日常开发经常会用到[tinypng.com](https://tinypng.com/)图片批量压缩，TinyPNG平均压缩率能达到60%，肉眼能观察到的画质损失微乎其微，对页面加载提速及体验、节省用户流量、缩小程序包有很大帮助。


##### 下载：
- [Windows x64 安装包](https://github.com/zhili-wang/tinypng/releases/download/v1.2.0/TinyPNG-Windows-x64-1.2.0.exe)
- [Windows x64 免安装版](https://github.com/zhili-wang/tinypng/releases/download/v1.2.0/TinyPNG-Windows-x64-1.2.0.zip)
- [macOS 通用安装包（Intel / Apple Silicon）](https://github.com/zhili-wang/tinypng/releases/download/v1.2.0/TinyPNG-macOS-universal-1.2.0.dmg)
- [查看全部版本](https://github.com/zhili-wang/tinypng/releases)

### 新版功能

- **JPG/JPEG 与导入提示**：支持 `.jpg`、`.JPG`、`.jpeg` 等扩展名。队列上方显示当前大小范围，默认最大 `5120 KB`（`5 MB`，即 `5 × 1024 × 1024` 字节）；可在“设置 → 大小范围”中调整。超出范围、空文件、不支持的格式或无法读取的文件会显示跳过原因，并在活动记录中保留文件名、实际大小和限制。全部文件被拒绝时保留原队列。网页链接或混合拖入文件夹和图片也会给出提示。大小范围仅用于本地筛选，压缩服务可能另有大小限制。
- **SVG 官网压缩**：支持选择文件、拖拽和递归扫描文件夹，接入官网 `store → process → download` 压缩流程。SVG 始终使用网页接口，无需 API KEY；官方开发者 API 的格式列表目前未包含 SVG。
- **批量排队与自动重试**：每批最多 10 张并发，当前批次全部完成后再开始下一批。每张失败图片自动重试最多 2 次（共最多 3 次尝试），分别等待 1 秒、2 秒；等待重试时仍占用当前批次名额。暂停会取消等待中的重试，进度只统计最终结果。
- **重试失败项**：点击顶部按钮或按 `Ctrl+R`，一次重试所有失败文件。成功项与尚未开始的文件不参与这次重试；暂停后可点击“开始压缩”继续未完成任务。
- **界面与主题**：支持浅色、深色、跟随系统，偏好会保存，跟随系统时可自动切换。主题覆盖队列、状态标签、活动记录、拖拽区域和设置弹窗；大小范围使用输入框与 `− / +` 按钮。导入提示可以关闭，长提示内部滚动，空队列按可用空间自适应排版。

以下是新版界面的示例队列预览：

<img src="./assets/ui-preview.png" width="900px" />

单独添加图片时保存到原路径；添加文件夹时，可在设置中选择覆盖原文件或保存到同级 `_tiny` 目录。保存采用临时文件提交，写入失败时保留原文件，方便重试。

### 配置
默认使用免 KEY 网页模式。API KEY 模式只需填写你自己的 KEY，认证用户名固定为 `api`，不会自动填充账号或共享凭据。旧版本预置的凭据会在升级时清除，用户自行保存的 KEY 和其他偏好继续保留。

国内访问较慢时，可以配置 HTTP 代理。

### Usage
 There are two ways to add files:

 1.Click the button at the top-left to choose one or more files or a folder;  
 2.For a more convenient and efficient option, it is recommended to drag and drop the objects directly into the table area of the window;  
 3.Use shortcut keys associated with the buttons, for example: press Ctrl+O to add an object, Ctrl+S to start a task;  
 4.Double-click any row in the table to preview the picture associated with that row in the system's image previewer;    
 5.You can delete rows either by right-clicking on any of them or by selecting the rows and pressing the Del key;    
  
 
 Remark: If the object you add is a folder, the software will automatically list all the image files contained within that folder recursively.

##### 免责声明
该仓库仅用于学习，如有商业用途，请购买官方的 pro 版：https://tinify.com/checkout/web-pro

This Repo is only for study.





### 开发与验证

使用 Qt 5.14+ 或 Qt 6 SDK 构建桌面程序：

```sh
mkdir -p build
cd build
qmake ../tinypng/VsTinypng.pro
make
```

`tests/regression.pro` 包含 SVG 渲染对比、JPG/JPEG 大小写识别、格式过滤和重试筛选检查，需要 Qt SVG 模块。`tests/integration.pro` 通过本地 HTTP 测试服务验证真实上传、压缩、下载、每批 10 张屏障、最多 2 次自动重试、暂停取消、API KEY 认证和原文件保护，并检查扫描、拖拽、关闭提示和主题偏好，不依赖外部网络。`tests/ui-preview.pro` 使用实际主窗口生成浅色、深色队列、空队列提示、关闭提示及设置弹窗预览。

### 安装包构建

新版构建版本为 `1.2.0`。每次打包均使用新的构建目录，完整重新编译，不复用旧对象文件。GitHub Actions 中的 **Build installers** 可手动运行，生成 macOS 通用 DMG（Intel / Apple Silicon）和 Windows x64 安装 EXE、便携 ZIP；构建前会运行 SVG 渲染、失败项筛选和导入提示检查。构建产物保存在 Actions 的 Artifacts 中；已发布的 `1.2.0` 安装包可从上方下载链接或 [Releases](https://github.com/zhili-wang/tinypng/releases/tag/v1.2.0) 获取。

本地 macOS 构建需要完整 Qt SDK（含 Qt SVG 模块）：

```sh
bash scripts/build-macos.sh /path/to/Qt/6.8.3/macos
```

Windows 构建需要 Qt MSVC x64 SDK、Visual Studio x64 Developer PowerShell 和 Inno Setup 6：

```powershell
./scripts/build-windows.ps1 -QtRoot C:/Qt/6.8.3/msvc2022_64
```

macOS 脚本采用本地临时签名；公开分发时仍需使用开发者证书签名和 Apple 公证。Windows 安装程序为当前用户安装，并附带 Qt 和 MSVC 运行库。
