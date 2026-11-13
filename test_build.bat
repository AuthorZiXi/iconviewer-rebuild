@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cd /d "%~dp0"
if not exist build\tests mkdir build\tests
cl /nologo /EHsc /std:c++20 /W4 /O1 /utf-8 /DUNICODE /D_UNICODE /Ires /Isrc /Fo:build\tests\ tests\test_load.cpp src\common.cpp src\bitmap.cpp src\icofile.cpp src\strings.cpp /Fe:build\test_load.exe /link /SUBSYSTEM:CONSOLE user32.lib gdi32.lib shlwapi.lib ole32.lib