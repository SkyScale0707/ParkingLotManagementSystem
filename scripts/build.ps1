# 构建脚本：配置 + 编译（默认 Debug）
# 用法：
#   .\scripts\build.ps1              # Debug
#   .\scripts\build.ps1 -Config Release
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Config = 'Debug',
    [string]$Qt = 'D:/Qt/6.10.3/msvc2022_64',
    [string]$BuildDir = 'build',
    [bool]$Deploy = $true
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

function Find-CMake {
    $cmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -products * -property installationPath
        if ($vsPath) {
            $candidate = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
            if (Test-Path $candidate) { return $candidate }
        }
    }
    throw '未找到 cmake，请安装 CMake 或 Visual Studio 的 CMake 组件。'
}

if (-not (Test-Path (Join-Path $Qt 'lib/cmake/Qt6/Qt6Config.cmake'))) {
    throw "Qt 路径无效：$Qt（应指向 msvc2022_64 目录）"
}

$cmake = Find-CMake
Write-Host "CMake : $cmake"
Write-Host "Qt    : $Qt"
Write-Host "配置  : $Config"

& $cmake -S . -B $BuildDir -G 'Visual Studio 17 2022' -A x64 "-DCMAKE_PREFIX_PATH=$Qt"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $cmake --build $BuildDir --config $Config --parallel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# 把 Qt 运行库与插件部署到输出目录，让 build\<Config>\parking_lot.exe 可以独立运行
$outDir = Join-Path $root (Join-Path $BuildDir $Config)
$exe = Join-Path $outDir 'parking_lot.exe'
if ($Deploy -and (Test-Path $exe)) {
    & (Join-Path $PSScriptRoot 'deploy.ps1') -Qt $Qt -Config $Config -TargetDir $outDir -IncludeTestLib
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Write-Host "构建完成：$BuildDir\$Config\parking_lot.exe"
