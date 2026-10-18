@echo off
setlocal
chcp 65001 >nul
rem Always build relative to this script, even when launched from another folder.
cd /d "%~dp0"
if "%NDI_SDK_DIR%"=="" (
  for %%D in ("%ProgramFiles%\NDI\NDI SDK" "%ProgramFiles%\NDI\NDI SDK for Windows" "%ProgramFiles%\NDI\NDI 6 SDK" "%ProgramFiles%\NewTek\NDI SDK") do (
    if exist "%%~D\Include\Processing.NDI.Lib.h" set "NDI_SDK_DIR=%%~D"
  )
)
if "%NDI_SDK_DIR%"=="" (
  echo 未自动找到完整 NDI SDK。
  echo 你已安装的 NDI Runtime 不能代替 SDK；SDK 需要包含 Include 和 Lib\x64。
  echo 如果 SDK 已解压到其他目录，请先执行：set "NDI_SDK_DIR=SDK完整目录"
  echo 然后重新运行 build.bat。
  pause
  exit /b 2
)
if not exist "%NDI_SDK_DIR%\Include\Processing.NDI.Lib.h" (
  echo 找不到 NDI SDK 头文件：%NDI_SDK_DIR%\Include\Processing.NDI.Lib.h
  pause
  exit /b 3
)
if not exist "%NDI_SDK_DIR%\Lib\x64\Processing.NDI.Lib.x64.lib" (
  echo 找不到 NDI x64 导入库，请检查 SDK 目录结构。
  pause
  exit /b 4
)
where cl.exe >nul 2>&1
if errorlevel 1 (
  echo 找不到 cl.exe。请从 Visual Studio 的“x64 Native Tools Command Prompt”运行。
  pause
  exit /b 5
)
if not exist build mkdir build
cl.exe /nologo /std:c++17 /utf-8 /EHsc /O2 /W4 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX /I"%NDI_SDK_DIR%\Include" src\AlphaSplitLite.cpp src\AlphaSplitCore.cpp /Fe:build\AlphaSplitLite.exe /link /SUBSYSTEM:CONSOLE /LIBPATH:"%NDI_SDK_DIR%\Lib\x64" Processing.NDI.Lib.x64.lib User32.lib Gdi32.lib Shell32.lib
if errorlevel 1 (
  echo 编译失败。请保存完整错误信息以便排查。
  pause
  exit /b 10
)
echo.
echo 编译完成：%CD%\build\AlphaSplitLite.exe
rem The import library links to Processing.NDI.Lib.x64.dll. A runtime installed
rem under Program Files is not guaranteed to be on the Windows DLL search path,
rem so copy a locally installed x64 runtime DLL beside this EXE when available.
set "NDI_RUNTIME_DLL="
for %%D in ("%ProgramFiles%\NDI\NDI 6 Runtime\v6" "%ProgramFiles%\NDI\NDI 5 Runtime\v5" "%ProgramFiles%\NDI\NDI Runtime\v4" "%NDI_SDK_DIR%\Bin\x64" "%NDI_SDK_DIR%\Bin") do (
  if not defined NDI_RUNTIME_DLL if exist "%%~D\Processing.NDI.Lib.x64.dll" set "NDI_RUNTIME_DLL=%%~D\Processing.NDI.Lib.x64.dll"
)
if defined NDI_RUNTIME_DLL (
  copy /Y "%NDI_RUNTIME_DLL%" "build\Processing.NDI.Lib.x64.dll" >nul
  if errorlevel 1 (
    echo 警告：找到 NDI Runtime DLL，但复制到 build 目录失败：%NDI_RUNTIME_DLL%
  ) else (
    echo 已将本机 NDI Runtime DLL 复制到 build 目录，降低启动时找不到 DLL 的风险。
    echo 来源：%NDI_RUNTIME_DLL%
  )
) else (
  echo 警告：未在常见位置找到 Processing.NDI.Lib.x64.dll。
  echo EXE 启动时可能报告缺少该 DLL；请安装兼容的 NDI Runtime，或将 DLL 放在 EXE 同目录。
)
echo 运行电脑必须安装与程序位数匹配、且符合 NDI 许可要求的 NDI Runtime。
echo 此 EXE 仍需在真实 WeMix NDI 与 HDMI 分离盒上验收。
pause
