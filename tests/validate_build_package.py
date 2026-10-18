#!/usr/bin/env python3
"""Dependency-free static checks for AlphaSplit Lite's Windows build package."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
checks = []

def check(description, condition):
    checks.append((description, bool(condition)))

workflow_path = ROOT / '.github' / 'workflows' / 'build-windows.yml'
workflow = workflow_path.read_text(encoding='utf-8') if workflow_path.exists() else ''
build = (ROOT / 'build.bat').read_text(encoding='utf-8')
cmake = (ROOT / 'CMakeLists.txt').read_text(encoding='utf-8')
source = (ROOT / 'src' / 'AlphaSplitLite.cpp').read_text(encoding='utf-8')
core = (ROOT / 'src' / 'AlphaSplitCore.cpp').read_text(encoding='utf-8')

check('Windows workflow exists', bool(workflow))
check('Windows x64 runner is configured', 'runs-on: windows-latest' in workflow)
check('MSVC x64 environment is initialized', 'ilammy/msvc-dev-cmd@v1' in workflow and 'arch: x64' in workflow)
check('NDI SDK download and diagnostic report are configured', 'NDI_SDK_setup_report.txt' in workflow and 'Invoke-WebRequest' in workflow)
check('NDI header and x64 import library are both checked', "Processing.NDI.Lib.h" in workflow and 'Processing.NDI.Lib.x64.lib' in workflow)
check('Windows build invokes build.bat', 'call build.bat' in workflow)
check('EXE artifact is staged', "build\\AlphaSplitLite.exe" in workflow and "artifact\\AlphaSplitLite.exe" in workflow)
check('NDI runtime DLL is explicitly excluded from artifact', "Remove-Item 'build\\Processing.NDI.Lib.x64.dll'" in workflow)
check('Workflow has a bounded Windows job timeout', 'timeout-minutes: 30' in workflow)
check('Build script verifies MSVC compiler', 'where cl.exe' in build)
check('Build script verifies NDI SDK header and import library', 'Processing.NDI.Lib.h' in build and 'Processing.NDI.Lib.x64.lib' in build)
check('Core processes BGRA/BGRX/RGBA/RGBX formats', all(x in core for x in ['PixelFormat::BGRA', 'PixelFormat::BGRX', 'PixelFormat::RGBA', 'PixelFormat::RGBX']))
check('Main program receives NDI video frames', 'NDIlib_recv_capture_v3' in source)
check('Main program exits on Escape', 'wParam == VK_ESCAPE' in source)
check('CMake rejects non-Windows builds', 'if(NOT WIN32)' in cmake)
check('Portable core tests exist', (ROOT / 'tests' / 'test_core.cpp').exists())

failed = [description for description, passed in checks if not passed]
for description, passed in checks:
    print(f"{'PASS' if passed else 'FAIL'}: {description}")
print(f"\nStatic package checks: {len(checks) - len(failed)}/{len(checks)} passed")
if failed:
    print('Failed checks: ' + '; '.join(failed), file=sys.stderr)
    sys.exit(1)
