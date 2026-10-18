@echo off
setlocal
chcp 65001 >nul
cd /d "%~dp0\.."
where cl.exe >nul 2>&1
if errorlevel 1 (
  echo 找不到 cl.exe。
  echo 请打开 Visual Studio 的“x64 Native Tools Command Prompt”，再运行本脚本。
  pause
  exit /b 2
)
if not exist build mkdir build
cl.exe /nologo /std:c++17 /utf-8 /EHsc /O2 /W4 /I"src" src\AlphaSplitCore.cpp tests\benchmark_core.cpp /Fe:build\AlphaSplitCoreBenchmark.exe
if errorlevel 1 (
  echo 性能测试程序编译失败。
  pause
  exit /b 3
)
echo.
build\AlphaSplitCoreBenchmark.exe
echo.
echo 这是图像核心性能，不代表完整 NDI/HDMI 程序的实际帧率。
pause
