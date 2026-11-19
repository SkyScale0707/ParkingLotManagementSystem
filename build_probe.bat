@echo off
setlocal
call "D:\Visual Studio 2022\Community 2022\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /std:c++20 /Zc:__cplusplus /EHsc /W3 /utf-8 /Fe:out\probe.exe /Fo:out\ /Fd:out\ prototype\probe_json.cpp
if errorlevel 1 exit /b 1
out\probe.exe
