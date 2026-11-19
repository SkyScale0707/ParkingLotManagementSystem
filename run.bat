@echo off
rem Run the packaged application (auto-package when missing)
setlocal
cd /d "%~dp0"
if not exist "dist\ParkingLotManagementSystem\parking_lot.exe" (
    echo Packaged application not found, packaging now...
    call "%~dp0package.bat"
    if errorlevel 1 exit /b 1
)
start "" "dist\ParkingLotManagementSystem\parking_lot.exe"
