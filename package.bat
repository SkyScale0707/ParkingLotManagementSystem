@echo off
rem Package: Release build + windeployqt + zip
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\package.ps1" %*
