@echo off
setlocal EnableExtensions
chcp 65001 >nul
cd /d "%~dp0"
set "LOG=%~dp0AlphaSplitLite_build_report.txt"
>"%LOG%" echo AlphaSplit Lite Windows x64 build report
>>"%LOG%" echo Time: %DATE% %TIME%
>>"%LOG%" echo Working directory: %CD%
>>"%LOG%" echo.
>>"%LOG%" echo ==== Compiler ====
where cl.exe >>"%LOG%" 2>&1
if errorlevel 1 >>"%LOG%" echo RESULT: cl.exe not found. Open x64 Native Tools Command Prompt for Visual Studio, then run this script from that window.
>>"%LOG%" echo.
>>"%LOG%" echo ==== NDI_SDK_DIR environment ====
if defined NDI_SDK_DIR (>>"%LOG%" echo NDI_SDK_DIR=%NDI_SDK_DIR%) else (>>"%LOG%" echo NDI_SDK_DIR is not set; build.bat will check common install folders.)
>>"%LOG%" echo.
>>"%LOG%" echo ==== Build output ====
call "%~dp0build.bat" <nul >>"%LOG%" 2>&1
set "BUILD_RESULT=%ERRORLEVEL%"
>>"%LOG%" echo.
>>"%LOG%" echo ==== Final result ====
>>"%LOG%" echo build.bat exit code: %BUILD_RESULT%
if exist "%~dp0build\AlphaSplitLite.exe" >>"%LOG%" echo EXE exists: %~dp0build\AlphaSplitLite.exe
if not exist "%~dp0build\AlphaSplitLite.exe" >>"%LOG%" echo EXE not found.
if exist "%~dp0build\Processing.NDI.Lib.x64.dll" >>"%LOG%" echo NDI runtime DLL copied beside EXE.
if not exist "%~dp0build\Processing.NDI.Lib.x64.dll" >>"%LOG%" echo NDI runtime DLL not found beside EXE; launch may depend on system DLL search path.
type "%LOG%"
echo.
echo 诊断日志已保存到：%LOG%
echo 如果没有生成 EXE，请把 AlphaSplitLite_build_report.txt 发回给我。
pause
exit /b %BUILD_RESULT%
