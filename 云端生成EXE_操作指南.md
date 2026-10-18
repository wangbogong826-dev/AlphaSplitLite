# AlphaSplit Lite：云端生成 Windows EXE

这个流程用于当前开发环境无法直接调用 Windows/MSVC 编译器的情况。GitHub Actions 会在微软提供的 Windows x64 runner 上编译主程序，并把 EXE 作为构建产物提供下载。

## 你需要做一次的操作

1. 登录 GitHub 并新建一个**私有仓库**（建议名称 `AlphaSplitLite`）。
2. 将本开发包中的所有文件和文件夹上传到仓库根目录，务必保留 `.github/workflows/build-windows.yml`。GitHub 网页上传时，如果看不到 `.github` 文件夹，可以先在网页创建文件 `.github/workflows/build-windows.yml`，再粘贴本文件的同名内容；也可以使用 GitHub Desktop 上传整个解压目录。
3. 提交到 `main` 或 `master` 分支。
4. 打开仓库的 **Actions** 页面，选择 **Build AlphaSplit Lite (Windows x64)**，点击 **Run workflow**。首次提交到默认分支后也会自动触发。
5. 等待 `core-tests` 和 `windows-exe` 两个任务结束。成功后，在该次运行页面底部的 **Artifacts** 下载 `AlphaSplitLite-Windows-x64`。
6. 解压产物，检查 `AlphaSplitLite.exe`。运行电脑仍须安装兼容的 NDI Runtime；本构建流程刻意不把 NDI DLL 打包进产物。

## 失败时

如果 `windows-exe` 任务失败，请在该次 Actions 页面下载 `AlphaSplitLite_build_report.txt` 或 `NDI_SDK_setup_report.txt`（如果已生成），或打开失败步骤，把完整错误文本发回继续排查。SDK 安装步骤现在会记录下载体积、签名状态、安装退出码以及头文件/导入库查找结果。

## 本版构建诊断改进

- Windows 构建任务增加 30 分钟超时，避免云端任务无限等待。
- SDK 下载/安装/定位过程会写入 `NDI_SDK_setup_report.txt`，即使 SDK 步骤失败，收尾步骤也会尽量将诊断报告上传为 Artifact。
- 安装程序签名状态会记录到报告；若签名状态不是 `Valid` 或 `NotSigned`，流程会停止并提示。

## 重要限制

- 该流程会从 NDI 官方下载地址获取 SDK，并由 GitHub 的 Windows runner 进行编译；首次运行需要网络可访问该地址。
- 构建产物目前只是**已编译的程序**，尚未经过你的 WeMix NDI、2560×720 HDMI 和分离盒的实机验收。
- 不要把 NDI Runtime DLL 随意重新分发；你的电脑已经安装 Runtime 时，让 EXE 使用本机安装的版本即可。

### 本版本额外的自动检查
Ubuntu 检查任务现在除了运行图像核心单元测试，还会执行 `tests/validate_build_package.py`。它会静态核对 Windows 工作流的关键步骤是否仍然存在，避免后续改动意外删掉 SDK 检查、EXE 归档或报告收集步骤。该检查不替代真实 Windows 编译。
