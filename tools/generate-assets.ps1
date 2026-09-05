param(
    [string]$BlenderPath = 'C:/Program Files/Blender Foundation/Blender 5.2/blender.exe',
    [switch]$AttackOnly
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$generatorPath = Join-Path $PSScriptRoot 'blender/generate_ashigaru.py'
$outputPath = Join-Path $repoRoot 'build/assets/ashigaru'
$assetPath = Join-Path $repoRoot 'assets/sprites'
if (-not (Test-Path -LiteralPath $BlenderPath)) { throw 'Blender executable not found. Set -BlenderPath.' }
if (-not $AttackOnly) {
    & $BlenderPath --background --factory-startup --python-exit-code 1 --python $generatorPath -- --output $outputPath
    if ($LASTEXITCODE -ne 0) { throw 'Blender asset generation failed.' }
}
& $BlenderPath --background --factory-startup --python-exit-code 1 --python $generatorPath -- --output $outputPath --attack-only
if ($LASTEXITCODE -ne 0) { throw 'Blender attack generation failed.' }
New-Item -ItemType Directory -Path $assetPath -Force | Out-Null
$outputNames = @('ashigaru_attack.png', 'ashigaru_attack.json')
if (-not $AttackOnly) { $outputNames += @('ashigaru_idle.png', 'ashigaru_idle.json', 'ashigaru_walk.png', 'ashigaru_walk.json') }
foreach ($outputName in $outputNames) {
    $sourcePath = Join-Path $outputPath $outputName
    $destinationPath = Join-Path $assetPath $outputName
    Copy-Item -LiteralPath $sourcePath -Destination $destinationPath
}
Write-Output "Generated assets: $assetPath"
