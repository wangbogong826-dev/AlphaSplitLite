@echo off
chcp 65001 >nul
echo ===== AlphaSplit Lite Windows 构建环境检查 =====
echo.
echo [1] 操作系统与架构：
echo PROCESSOR_ARCHITECTURE=%PROCESSOR_ARCHITECTURE%
echo PROCESSOR_IDENTIFIER=%PROCESSOR_IDENTIFIER%
echo.
echo [2] MSVC 编译器：
where cl.exe 2>nul
if errorlevel 1 echo 未找到 cl.exe；请安装 Visual Studio C++ 桌面开发组件并使用 x64 Native Tools 命令提示符。
echo.
echo [3] CMake：
where cmake.exe 2>nul
if errorlevel 1 echo 未找到 CMake（使用 build.bat 不强制需要 CMake）。
echo.
echo [4] NDI SDK 目录：
if "%NDI_SDK_DIR%"=="" (
 echo NDI_SDK_DIR 尚未设置。
) else (
 echo NDI_SDK_DIR=%NDI_SDK_DIR%
 if exist "%NDI_SDK_DIR%\Include\Processing.NDI.Lib.h" (echo 找到 NDI SDK 头文件。) else (echo 未找到 NDI SDK 头文件。)
 if exist "%NDI_SDK_DIR%\Lib\x64\Processing.NDI.Lib.x64.lib" (echo 找到 NDI x64 导入库。) else (echo 未找到 NDI x64 导入库。)
)
echo.
echo [5] 常见 NDI Runtime 位置：
if exist "%ProgramFiles%\NDI\NDI 6 Runtime\v6\Processing.NDI.Lib.x64.dll" echo 找到 NDI 6 Runtime x64 DLL。
if exist "%ProgramFiles%\NDI\NDI 5 Runtime\v5\Processing.NDI.Lib.x64.dll" echo 找到 NDI 5 Runtime x64 DLL。
echo.
echo 此检查只读取环境信息，不会安装或修改任何内容。
pause
