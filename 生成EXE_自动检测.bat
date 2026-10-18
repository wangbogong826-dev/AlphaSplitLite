@echo off
setlocal EnableExtensions EnableDelayedExpansion
chcp 65001 >nul
cd /d "%~dp0"
set "LOG=%~dp0AlphaSplitLite_build_report.txt"
>"%LOG%" echo AlphaSplit Lite - Windows x64 automatic build report
>>"%LOG%" echo Time: %DATE% %TIME%
>>"%LOG%" echo Working directory: %CD%
>>"%LOG%" echo.

rem 1) Use an already initialized x64 MSVC environment when available.
where cl.exe >nul 2>&1
if not errorlevel 1 goto :compiler_ready

rem 2) Otherwise locate Visual Studio / Build Tools using vswhere.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" (
  >>"%LOG%" echo ERROR: vswhere.exe not found; Visual Studio Installer may not be installed.
  goto :no_compiler
)
set "VSINSTALL="
for /f "usebackq tokens=*" %%V in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%V"
if not defined VSINSTALL (
  >>"%LOG%" echo ERROR: Visual Studio C++ x64 build tools not found.
  goto :no_compiler
)
set "DEVCMD=!VSINSTALL!\Common7\Tools\VsDevCmd.bat"
if not exist "!DEVCMD!" (
  >>"%LOG%" echo ERROR: VsDevCmd.bat not found: !DEVCMD!
  goto :no_compiler
)
>>"%LOG%" echo Visual Studio installation: !VSINSTALL!
call "!DEVCMD!" -arch=x64 -host_arch=x64 >>"%LOG%" 2>&1
if errorlevel 1 (
  >>"%LOG%" echo ERROR: VsDevCmd.bat failed to initialize the x64 compiler environment.
  goto :no_compiler
)
where cl.exe >>"%LOG%" 2>&1
if errorlevel 1 goto :no_compiler

:compiler_ready
>>"%LOG%" echo Compiler:
where cl.exe >>"%LOG%" 2>&1
>>"%LOG%" echo.

rem 3) Locate the full NDI SDK (Runtime alone is not sufficient to compile).
set "SDK_FOUND="
if defined NDI_SDK_DIR if exist "%NDI_SDK_DIR%\Include\Processing.NDI.Lib.h" if exist "%NDI_SDK_DIR%\Lib\x64\Processing.NDI.Lib.x64.lib" set "SDK_FOUND=%NDI_SDK_DIR%"
if not defined SDK_FOUND for %%D in (
  "%ProgramFiles%\NDI\NDI 6 SDK"
  "%ProgramFiles%\NDI\NDI SDK"
  "%ProgramFiles%\NDI\NDI SDK for Windows"
  "%ProgramFiles%\NewTek\NDI SDK"
  "%ProgramFiles(x86)%\NDI\NDI 6 SDK"
) do (
  if not defined SDK_FOUND if exist "%%~D\Include\Processing.NDI.Lib.h" if exist "%%~D\Lib\x64\Processing.NDI.Lib.x64.lib" set "SDK_FOUND=%%~D"
)
if not defined SDK_FOUND (
  >>"%LOG%" echo ERROR: Full NDI SDK not found. NDI Runtime alone does not contain the compile-time header and import library.
  >>"%LOG%" echo Expected files: Include\Processing.NDI.Lib.h and Lib\x64\Processing.NDI.Lib.x64.lib
  >>"%LOG%" echo Official SDK download page: https://ndi.video/for-developers/ndi-sdk/download/
  goto :no_sdk
)
set "NDI_SDK_DIR=%SDK_FOUND%"
>>"%LOG%" echo NDI_SDK_DIR=%NDI_SDK_DIR%
>>"%LOG%" echo.

rem 4) Build and preserve the full compiler output in a report.
>>"%LOG%" echo ==== Build output ====
call "%~dp0build.bat" <nul >>"%LOG%" 2>&1
set "BUILD_RESULT=!ERRORLEVEL!"
>>"%LOG%" echo.
>>"%LOG%" echo ==== Final result ====
>>"%LOG%" echo build.bat exit code: !BUILD_RESULT!
if exist "%~dp0build\AlphaSplitLite.exe" (
  >>"%LOG%" echo EXE exists: %~dp0build\AlphaSplitLite.exe
) else (
  >>"%LOG%" echo EXE not found.
)
if exist "%~dp0build\Processing.NDI.Lib.x64.dll" >>"%LOG%" echo NDI Runtime DLL copied beside EXE.
if not exist "%~dp0build\Processing.NDI.Lib.x64.dll" >>"%LOG%" echo NDI runtime DLL not found beside EXE; launch may depend on system DLL search path.
type "%LOG%"
echo.
echo 诊断报告：%LOG%
echo 若编译失败，请把此报告发回继续排查。
pause
exit /b !BUILD_RESULT!

:no_compiler
>>"%LOG%" echo.
>>"%LOG%" echo RESULT: MSVC x64 compiler unavailable. Install Visual Studio Build Tools with the C++ desktop workload.
goto :show_failure

:no_sdk
>>"%LOG%" echo.
>>"%LOG%" echo RESULT: compiler is available, but the full NDI SDK is missing.
echo 未找到完整 NDI SDK。NDI Runtime 不能代替 SDK。
echo 请从官方页面下载 NDI SDK：
echo https://ndi.video/for-developers/ndi-sdk/download/
goto :show_failure

:show_failure
type "%LOG%"
echo.
echo 诊断报告：%LOG%
pause
exit /b 2
