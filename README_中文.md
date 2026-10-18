# AlphaSplit Lite — 开发检查点 14（显示模式兼容性修正）

## 本次实际改动

1. 核心处理器显式拒绝未知像素格式枚举，避免未知输入被静默当成 RGBX 处理。
2. NDI 主程序遇到负 stride 时直接丢弃异常帧，不再错误地把负 stride 当成默认正 stride。当前接收器请求非翻转 BGRX/BGRA，因此负 stride 不应作为正常输入。
3. 新增未知像素格式的单元测试；核心测试共 9 项，包含 300 组随机帧逐像素校验。


1. 加入 300 组可重复的随机输入帧测试，覆盖四种像素格式、不同分辨率、输出缩放与带填充 stride；测试逐像素核对 Fill 颜色、Key 灰度和不透明输出 Alpha。
2. 核心测试脚本启用 AddressSanitizer 与 UndefinedBehaviorSanitizer，帮助发现越界访问和未定义行为。
3. Windows 主程序启动时启用 DPI awareness，降低 Windows 显示缩放导致全屏输出坐标/尺寸不匹配的风险。


1. 收到第一帧时记录实际视频分辨率、像素格式及是否带 Alpha，方便现场排查 WeMix 输出格式。
2. 区分“不支持的像素格式”和“帧参数不合规”：只有真正遇到未知 FourCC 时才显示对应警告，避免把 stride 或帧尺寸异常误报成像素格式问题。
3. NDI 信号名称按 UTF-8 转换后统一用宽字符控制台输出，避免中英文/混合流输出造成名称显示异常。
4. 扩展核心单元测试到 7 项，新增带行填充的 stride、垂直缩放、RGBX 通道顺序及零尺寸输入检查。

1. 将 Fill/Key 像素转换从 Win32/NDI 主程序中拆分为独立的 `AlphaSplitCore.cpp/.h`，便于单元测试和后续复用。
2. 增加输入尺寸、输出尺寸、像素步幅（stride）校验，避免不合规帧导致越界读取。
3. 加入窗口消息合并：接收线程不会为每一帧无节制地向 UI 消息队列投递刷新消息，降低高帧率时消息积压风险。
4. 检查输出显示器当前分辨率；不是 2560×720 时会在控制台明确警告画面可能被拉伸。
5. 如果 NDI 实际返回 BGRX/RGBX（没有 Alpha），会提示 Key 将全白，避免误以为透明通道正常。
6. 增加跨平台的核心单元测试，覆盖 BGRA Alpha 转 Key、RGBA/RGBX 通道顺序、无 Alpha 输入、无效 stride/奇数输出宽度、带填充 stride、垂直缩放、零尺寸输入及 300 组随机帧逐像素校验。

## 测试结果

已在当前开发环境用 `g++` 编译并运行独立像素处理核心：**9/9 项测试通过，其中包含 300 组随机帧逐像素校验**。

这只验证了独立像素处理核心，不代表 Windows NDI 主程序已编译，也不代表已经连通 WeMix 或实体 HDMI 分离盒。

## 当前仍然不是正式 EXE

当前环境没有 Windows 编译器和完整 NDI SDK，Windows 主程序仍未完成实际编译与运行验证。请勿将源码包当作已经验证可替代 RGB Alpha Splitter 的正式成品。

## Windows 构建要求

- Windows 10 x64。
- Visual Studio C++ 桌面开发工具（包含 `cl.exe`）及 Windows SDK。
- 完整 NDI SDK（NDI Tools / NDI Runtime 本身不等于 SDK）。
- `NDI_SDK_DIR` 指向 SDK 根目录，且根目录下存在：
  - `Include\Processing.NDI.Lib.h`
  - `Lib\x64\Processing.NDI.Lib.x64.lib`

在 Visual Studio 的“x64 Native Tools Command Prompt for VS”中进入本目录，执行：

```bat
set NDI_SDK_DIR=C:\NDI SDK\NDI SDK
build.bat
```

具体 SDK 路径请以实际安装位置为准，并遵守 NDI SDK 许可条款。本项目不随包分发 NDI Runtime 或 SDK 二进制。

## 当前程序行为

1. 搜索局域网 NDI 源，在控制台列出信号并让用户输入编号。
2. 请求 NDI 接收器使用 `NDIlib_recv_color_format_BGRX_BGRA`。
3. 仅在检测到 Windows 扩展桌面的第二台显示器时输出，避免意外覆盖主屏。
4. 固定输出画布 2560×720：左半边是不透明 Fill，右半边是 Alpha 灰度 Key。
5. 当前仅支持 BGRA/BGRX/RGBA/RGBX；若实际 WeMix NDI 使用 UYVA 等其他格式，需增加转换逻辑。
6. 按 Esc 退出输出。

## 下一步工作

1. 在 Windows x64 + NDI SDK 环境中编译，修正任何编译/API 兼容问题。
2. 连接真实 WeMix NDI，确认收到的 FourCC 和 Alpha 是否正确。
3. 验证 2560×720 HDMI 输出、Fill/Key 左右顺序、颜色/灰度及分离盒识别。
4. 在用户的 8GB 内存笔记本上测 CPU、GPU、内存和持续运行稳定性，并与原 RGB Alpha Splitter 对照。
5. 再做 GUI、错误日志、运行时检测、自动重连和可交付打包。

## 参考

- NDI 接收： https://docs.ndi.video/all/developing-with-ndi/sdk/ndi-recv
- NDI 帧类型： https://docs.ndi.video/all/developing-with-ndi/sdk/frame-types


## 检查点 8 更新
- 启动时枚举所有显示器，显示设备名、分辨率和主/扩展状态。
- 可以明确选择字幕输出显示器；输入 0 使用默认的第一台扩展显示器，避免多显示器环境下误选。
- 增加窗口类注册失败检查，并对已注册类进行兼容处理。
- 仍需在 Windows 10 x64 与完整 NDI SDK 环境中编译验证；本包不是已验证的正式 EXE。


## 检查点 9：更明确的 Windows 构建路径

本包新增 `CMakeLists.txt` 和 `check_windows_env.bat`。如果你或后续构建人员在 Windows 10 x64 上编译，可先在“x64 Native Tools Command Prompt for VS”运行 `check_windows_env.bat`，再按原方式设置 `NDI_SDK_DIR` 并运行 `build.bat`。也可以用 CMake 配置：

```bat
cmake -S . -B build-cmake -A x64 -DNDI_SDK_DIR="C:/NDI SDK/NDI SDK"
cmake --build build-cmake --config Release
```

请注意：NDI Runtime 与 NDI SDK 不同。构建需要 SDK 的头文件和 x64 导入库；运行时需要符合 NDI 许可要求的 NDI Runtime。当前开发容器不具备 Windows 编译器和 NDI SDK，因而本包没有声称包含经过验证的 EXE。


## 检查点 10：本次实际改动

- 输出窗口在收到第一帧之前显示“黑色 Fill + 白色 Key”测试底色，避免启动等待期间两半全黑造成误判；收到第一帧后会被真实 NDI 图像覆盖。
- NDI 接收错误改为只提示一次，避免连接异常时控制台每隔一会儿重复刷屏。
- 调整 WM_PAINT 中的锁作用域，确保 EndPaint 在帧缓冲互斥锁释放后执行。
- 重新运行跨平台核心测试，确认 Fill/Key 像素处理没有回归。

注意：这些改动仍未替代 Windows x64 + 完整 NDI SDK 的实际编译，也未完成实体设备验收。


## 今晚优先编译的最快路径

1. 解压整个开发包。
2. 在装有 Visual Studio C++ 工具和完整 NDI SDK 的 Windows 电脑上，打开“x64 Native Tools Command Prompt”。
3. 进入解压目录，运行 `build.bat`。脚本会尝试在常见安装目录自动查找 NDI SDK；若找不到，再按提示设置 `NDI_SDK_DIR`。
4. 成功后程序位于 `build\AlphaSplitLite.exe`。首次运行选择 WeMix NDI 源和输出显示器。

注意：这份代码尚未在本开发环境中用 Windows/MSVC/NDI SDK 完整编译，也尚未连接实际 HDMI 分离盒验收。请把编译窗口中的完整报错发回，才能针对真实错误修复。

## 第十二版新增：快速诊断并编译

请优先从 Visual Studio 的“x64 Native Tools Command Prompt”中运行 `快速诊断并编译.bat`。脚本会尝试调用 `build.bat` 并保存完整输出到 `AlphaSplitLite_build_report.txt`。如果未生成 EXE，把日志发回用于定位实际编译错误。

注意：本版本仍是源码包，不包含经 Windows 编译与硬件验收的 EXE。核心测试通过不等于 Windows 主程序已编译通过。


## 第十三版更新
- 新增目标显示器分辨率处理：若 Windows 为所选显示器提供 2560×720 模式，程序会尝试临时切换；退出时尝试恢复原分辨率。
- 若模式不存在或切换失败，程序继续运行并提示会按当前显示器尺寸拉伸。
- 该行为尚未在用户的真实 Windows/HDMI 分离盒环境中验证；首次测试时请确认输出显示器选择正确。


## 检查点 14：显示模式兼容性修正

- 复查显示器模式切换代码后，修正一个兼容性问题：之前枚举到可用的 2560×720 模式后，又强制把色深设为 32 bpp；如果驱动只公布 24 bpp 模式，就可能导致本来可用的模式切换失败。现在保留 Windows 枚举到的模式色深。
- 已重新运行跨平台 Fill/Key 核心测试，结果仍为 9/9 通过（包含 300 组随机帧）。
- 本次修正尚未在真实 Windows 显示驱动上验证。Windows EXE 仍未在当前开发环境中生成。


## 检查点 15 补充
- `build.bat` 会先切换到脚本所在目录，避免从其他路径启动时相对路径失效。
- CMake/MSVC 构建启用 `/utf-8`，与批处理构建保持一致。
- Linux 上的图像核心测试通过，但完整 Windows EXE 仍未在 MSVC + 完整 NDI SDK 环境中编译验证。


## 开发检查点 16（2026-10-18）

本次代码审查修正：输出窗口显示后，显式尝试将其设为前台并取得键盘焦点，减少控制台仍占据焦点、导致按 Esc 无法退出输出窗口的情况。Windows 仍可能基于前台窗口规则拒绝焦点切换，因此需在目标电脑实测。

验证边界：跨平台 Fill/Key 核心测试通过；本环境没有 Windows x64 MSVC、Windows SDK 和完整 NDI SDK，尚未编译 Windows EXE，也未连接 WeMix / HDMI 分离盒实测。


## 开发检查点 17：NDI 视频帧状态监测

- 输出窗口增加每秒一次的帧状态检查：启动 5 秒仍没有可处理的视频帧时提示检查 NDI 输出、网络和像素格式。
- 已经收到视频后，连续 3 秒没有新帧会提示信号可能中断；帧恢复后会重新允许后续中断告警。
- 该检查只提示，不会自动重启接收器，也不会改变 Fill/Key 像素处理逻辑。
- Linux 核心测试可验证像素处理模块，但无法验证 Windows 定时器、MSVC 编译或 NDI 实机连接。


## 检查点 18：核心边界测试补强
新增 Alpha 边界值测试（0、1、127、128、254、255）以及空指针、源行跨度整数溢出测试。当前可移植核心测试为 11 项，包含 300 组随机帧逐像素校验。Windows 主程序仍需使用 MSVC 和完整 NDI SDK 实际编译，并在 WeMix/HDMI 分离盒链路上验收。


## 检查点 19：实际目标尺寸回归测试
本检查点新增 1920×1080 BGRA 输入到 2560×720 左 Fill/右 Key 输出的核心回归测试，检查多个输出位置的颜色通道、Alpha 灰度及不透明输出 Alpha。此项测试不等同于 Windows NDI 主程序编译或硬件实测。


## 开发检查点 20：缩短画面锁定时间

- NDI 接收线程现在先把 Fill/Key 帧转换到线程本地的备用缓冲区，不再在完整像素转换循环期间持有 UI 画面互斥锁。
- 只有整帧转换成功后，才在短临界区内交换前后缓冲区，降低 WM_PAINT 与图像处理之间的锁竞争风险。
- 该改动面向 i5-7200U 等低功耗 CPU 的实时输出稳定性；尚未在 Windows/MSVC 与真实 NDI 帧率下进行性能实测。
- Linux 核心测试用于确认像素处理逻辑不回归；不代表 Windows 主程序已经编译或验收。


## 开发检查点 21：NDI 实时处理帧率诊断

- 输出窗口运行期间，每约 5 秒在控制台报告一次成功处理的 NDI 帧率（fps）和累计处理帧数。
- 该数值统计的是已完成 Fill/Key 转换并提交到输出缓冲区的帧，不是 NDI 网络层的原始到达率；可用于现场判断处理是否接近 25/30 fps，以及信号是否明显变慢。
- 帧率报告与原有 1 秒信号状态检查共用窗口定时器，不在每帧输出日志，避免逐帧控制台打印干扰性能。
- 当前 Linux 环境无法编译验证 Windows 主程序；该诊断功能仍需在 Windows 实机确认。


## 开发检查点 22：运行时 DLL 部署检查

- `build.bat` 编译成功后，会在常见的 NDI 5/6 Runtime 和 SDK Bin 目录中查找 `Processing.NDI.Lib.x64.dll`。
- 找到后，将本机已安装的 x64 DLL 复制到 `build` 目录，与 EXE 放在一起，降低 Windows 启动时无法定位导入 DLL 的风险。
- 找不到 DLL 时会明确警告；这并不代表编译失败。
- `快速诊断并编译.bat` 会把 DLL 是否位于 EXE 同目录写入构建报告。
- 此改动尚未在 Windows 上实测；本开发环境仍缺少 MSVC 和完整 NDI SDK。不要将它视为已完成的 EXE 构建或实机验收。


## 开发检查点 23：稳定保存所选 NDI 源描述符

- 修复潜在生命周期风险：NDI Finder 返回的 `NDIlib_source_t` 字符串指针由 Finder 管理；直接复制结构体后销毁 Finder，可能使接收器持有的名称/URL 指针失效。
- 现在把所选 NDI 名称与 URL 复制到 `main()` 作用域中的 `std::string`，并让描述符指向这些稳定存储，确保其生命周期覆盖接收器生命周期。
- 这是针对 Windows/NDI 主程序的源码健壮性修正；当前环境没有 Windows SDK/MSVC/完整 NDI SDK，尚不能进行完整主程序编译或实机验证。


## 开发检查点 24：缩短输出绘制持锁时间

本次调整 `WM_PAINT`：绘制线程在 `g_frameMutex` 内只复制一份一致的帧快照，随后释放互斥锁，再调用 `StretchDIBits`。这样，较慢的 GDI 绘制不再占用共享帧锁，NDI 接收线程可更早发布下一帧。快照使用 UI 线程局部缓冲区复用，避免每次重绘都重新分配内存。

验证范围：可移植图像处理核心自动化测试；Windows 窗口代码尚未在 MSVC/NDI SDK 环境下编译或实机验证。

## 开发检查点 25：独立图像核心性能基准

新增 `tests/benchmark_core.cpp` 与 `tests/运行图像核心性能测试.bat`。在 Windows x64 的 Visual Studio x64 Native Tools 命令提示符中运行该批处理，可在**不需要 NDI SDK**的情况下编译并测试图像核心，把 1920×1080 BGRA 输入转换为 2560×720 Fill/Key 输出，并报告帧率与每帧耗时。该结果仅代表像素转换核心，不包含 NDI 接收、画面缓冲复制、GDI 绘制或 HDMI 输出，不能替代完整程序实测。


## 第26版：一键检测 Windows 编译环境

新增 `生成EXE_自动检测.bat`：优先使用当前 x64 MSVC 环境；否则尝试通过 Visual Studio Installer 的 `vswhere.exe` 找到 Visual Studio / Build Tools 并初始化 x64 编译环境；随后查找完整 NDI SDK、调用现有构建脚本，并保存完整日志到 `AlphaSplitLite_build_report.txt`。

注意：该脚本不会自动安装 Visual Studio 或 NDI SDK，也不会绕过 NDI 许可。NDI Runtime 与 NDI SDK 不同；若缺少完整 SDK，脚本会提示官方 SDK 下载页。此开发环境没有 Windows x64 编译器、Windows SDK 或完整 NDI SDK，因此这里仍不能生成或验证最终 EXE。

## 开发检查点 27：增加 Windows 云端构建工作流

新增 `.github/workflows/build-windows.yml`：使用 GitHub Actions 的 Windows x64 runner 初始化 MSVC，尝试下载并静默安装 NDI 6 SDK，随后运行现有 `build.bat` 并上传 EXE 构建产物。核心图像测试在 Ubuntu runner 上独立执行。工作流不会把 NDI Runtime DLL 上传为构建产物。

首次运行说明见 `云端生成EXE_操作指南.md`。该工作流文件尚未在真实 GitHub Actions runner 上执行，因此 SDK 安装参数、官方下载可达性以及最终 MSVC 编译结果仍待 CI 首次运行确认；不能把它视为已生成的 EXE。


## 第二十八版构建流程修正

- Windows 云端任务增加 30 分钟超时。
- 新增 `NDI_SDK_setup_report.txt`，记录 SDK 下载体积、签名状态、安装退出码、SDK 头文件与 x64 导入库定位结果。
- SDK 安装失败时，收尾步骤仍会尝试上传诊断报告，减少“失败了却没有日志”的情况。
- 当前仍未实际执行 GitHub Actions，因此尚未宣称生成 EXE。

### 构建流程静态检查
运行 `python3 tests/validate_build_package.py` 可检查 Windows 工作流与本地构建脚本的关键连接点；此检查不需要第三方 Python 包，但不能替代 Windows x64 的真实编译与硬件验收。
