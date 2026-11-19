# 打包脚本：Release 构建 + windeployqt 部署 + VC 运行库 + 压缩
# 用法：.\scripts\package.ps1
param(
    [string]$Qt = 'D:/Qt/6.10.3/msvc2022_64',
    [string]$Config = 'Release'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

& (Join-Path $PSScriptRoot 'build.ps1') -Config $Config -Qt $Qt -Deploy:$false
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$dist = Join-Path $root 'dist\ParkingLotManagementSystem'
Remove-Item -Recurse -Force $dist -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $dist | Out-Null

$exe = Join-Path $root "build\$Config\parking_lot.exe"
if (-not (Test-Path $exe)) { throw "未找到可执行文件：$exe" }
Copy-Item $exe $dist

# 部署 Qt 运行库、offscreen 插件与 VC++ 运行库（与 build.ps1 共用同一脚本）
& (Join-Path $PSScriptRoot 'deploy.ps1') -Qt $Qt -Config $Config -TargetDir $dist
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# ---------------------------------------------------------------------------
# 启动脚本与文档
# ---------------------------------------------------------------------------
$runBat = @"
@echo off
cd /d "%~dp0"
start "" parking_lot.exe
"@
Set-Content -Path (Join-Path $dist 'run.bat') -Value $runBat -Encoding ascii

Copy-Item 'README.md' $dist
Copy-Item -Recurse 'docs\screenshots' (Join-Path $dist 'screenshots')

# ---------------------------------------------------------------------------
# 4) 压缩（保留顶层文件夹，解压后是完整目录而不是散落一堆文件）
# ---------------------------------------------------------------------------
$zip = Join-Path $root "dist\ParkingLotManagementSystem-$Config.zip"
Remove-Item -Force $zip -ErrorAction SilentlyContinue
Compress-Archive -Path $dist -DestinationPath $zip

Write-Host "打包完成：$zip"
exit 0
