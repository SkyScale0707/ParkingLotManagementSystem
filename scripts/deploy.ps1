# 部署脚本：把 Qt 运行库/插件、offscreen 插件与 VC++ 运行库放到目标目录
# 供 build.ps1 与 package.ps1 共用，保证目标目录自包含。
param(
    [Parameter(Mandatory = $true)][string]$Qt,
    [ValidateSet('Debug', 'Release')][string]$Config = 'Release',
    [Parameter(Mandatory = $true)][string]$TargetDir,
    # 构建目录额外复制 Qt6Test，方便不配置 PATH 也能直接跑 ctest
    [switch]$IncludeTestLib
)

$ErrorActionPreference = 'Stop'
$TargetDir = [System.IO.Path]::GetFullPath($TargetDir)
$exe = Join-Path $TargetDir 'parking_lot.exe'
if (-not (Test-Path $exe)) { throw "未找到可执行文件：$exe" }

# ---------------------------------------------------------------------------
# 1) Qt 运行库与插件
# ---------------------------------------------------------------------------
$windeployqt = Join-Path $Qt 'bin\windeployqt.exe'
if (-not (Test-Path $windeployqt)) { throw "未找到 windeployqt：$windeployqt" }

$mode = if ($Config -eq 'Debug') { '--debug' } else { '--release' }
& $windeployqt $mode --no-opengl-sw --no-system-d3d-compiler --compiler-runtime `
    --dir $TargetDir $exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# windeployqt 默认只带 qwindows；补 offscreen，便于无显示器环境验证
$pluginName = if ($Config -eq 'Debug') { 'qoffscreend.dll' } else { 'qoffscreen.dll' }
$offscreen = Join-Path $Qt (Join-Path 'plugins\platforms' $pluginName)
if (Test-Path $offscreen) {
    Copy-Item $offscreen (Join-Path $TargetDir 'platforms') -Force
}

# ---------------------------------------------------------------------------
# 2) 可选：Qt Test 运行库（仅构建目录需要，便于直接 ctest）
# ---------------------------------------------------------------------------
if ($IncludeTestLib) {
    $testName = if ($Config -eq 'Debug') { 'Qt6Testd.dll' } else { 'Qt6Test.dll' }
    $testLib = Join-Path $Qt (Join-Path 'bin' $testName)
    if (Test-Path $testLib) {
        Copy-Item $testLib $TargetDir -Force
    }
}

# ---------------------------------------------------------------------------
# 3) VC++ 运行库（--compiler-runtime 在部分环境不会复制，显式补齐）
# ---------------------------------------------------------------------------
$crtNames = if ($Config -eq 'Debug') {
    @('concrt140d.dll', 'msvcp140d.dll', 'msvcp140_1d.dll', 'vcruntime140d.dll',
      'vcruntime140_1d.dll')
} else {
    @('concrt140.dll', 'msvcp140.dll', 'msvcp140_1.dll', 'msvcp140_2.dll',
      'msvcp140_atomic_wait.dll', 'vcruntime140.dll', 'vcruntime140_1.dll')
}

$crtDirs = @()
if ($Config -eq 'Release') {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -products * -property installationPath
        if ($vsPath) {
            $redist = Join-Path $vsPath 'VC\Redist\MSVC'
            if (Test-Path $redist) {
                $crtDirs += Get-ChildItem $redist -Directory |
                    Where-Object { $_.Name -match '^\d+(\.\d+)+$' } |
                    Sort-Object Name -Descending |
                    ForEach-Object { Join-Path $_.FullName 'x64\Microsoft.VC143.CRT' } |
                    Where-Object { Test-Path $_ }
            }
        }
    }
}
$crtDirs += 'C:\Windows\System32'

foreach ($name in $crtNames) {
    if (Test-Path (Join-Path $TargetDir $name)) { continue }
    foreach ($dir in $crtDirs) {
        $candidate = Join-Path $dir $name
        if (Test-Path $candidate) {
            Copy-Item $candidate $TargetDir
            break
        }
    }
}

# Release 包缺少关键运行库直接报错，避免发出跑不起来的包
if ($Config -eq 'Release') {
    $required = @(
        'parking_lot.exe', 'Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll',
        'platforms\qwindows.dll', 'vcruntime140.dll', 'msvcp140.dll'
    )
    $missing = $required | Where-Object { -not (Test-Path (Join-Path $TargetDir $_)) }
    if ($missing) {
        throw "部署缺少必要文件：$($missing -join ', ')"
    }
}

Write-Host "Qt/VC 运行库已部署到：$TargetDir"
exit 0
