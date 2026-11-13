@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (echo vcvars failed & exit /b 1)
cd /d "%~dp0"
if not exist build mkdir build
rc /nologo /I src /I res /fo build\app.res res\app.rc
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++20 /W4 /O2 /MT /utf-8 /DUNICODE /D_UNICODE /Ires /Fo:build\ src\common.cpp src\bitmap.cpp src\icofile.cpp src\settings.cpp src\strings.cpp src\dataobject.cpp src\preview.cpp src\main.cpp build\app.res /Fe:build\IconViewerRebuild.exe /link /SUBSYSTEM:WINDOWS