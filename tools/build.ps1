param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [switch]$Test
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot 'build'
$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmakeCommand) {
    $cmakePath = $cmakeCommand.Source
} else {
    $vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswherePath)) { throw 'Visual Studio C++環境とCMakeが必要です。docs/reference/development.mdを参照してください。' }
    $vsPath = & $vswherePath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsPath) { throw 'Visual StudioのC++ツールが見つかりません。' }
    $cmakePath = Join-Path $vsPath 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
}
$ctestPath = Join-Path (Split-Path -Parent $cmakePath) 'ctest.exe'
& $cmakePath -S $repoRoot -B $buildDir -A x64
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
& $cmakePath --build $buildDir --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
if ($Test) {
    & $ctestPath --test-dir $buildDir -C $Configuration --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed. Check build/smoke.bmp.error.log for DX12 errors.' }
}
