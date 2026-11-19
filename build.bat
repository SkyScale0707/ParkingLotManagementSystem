@echo off
rem Build ParkingLotManagementSystem (Debug by default)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\build.ps1" %*
