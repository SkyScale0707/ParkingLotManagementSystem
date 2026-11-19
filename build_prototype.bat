@echo off
setlocal
chcp 65001 >nul
call "D:\Visual Studio 2022\Community 2022\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
if errorlevel 1 (echo [ERR] vcvars failed & exit /b 1)
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /std:c++20 /Zc:__cplusplus /EHsc /W3 /utf-8 /O2 /Fe:out\prototype.exe /Fo:out\ /Fd:out\ prototype\main.cpp prototype\parking.cpp
if errorlevel 1 exit /b 1
out\prototype.exe
