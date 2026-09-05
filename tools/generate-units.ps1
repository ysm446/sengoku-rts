param(
    [string]$BlenderPath = 'C:/Program Files/Blender Foundation/Blender 5.2/blender.exe',
    [ValidateSet('samurai', 'archer', 'cavalry')]
    [string[]]$Unit = @('samurai', 'archer', 'cavalry')
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$generatorPath = Join-Path $PSScriptRoot 'blender/generate_units.py'
$assetPath = Join-Path $repoRoot 'assets/sprites'
foreach ($kind in $Unit) {
    $outputPath = Join-Path $repoRoot "build/assets/$kind"
    & $BlenderPath --background --factory-startup --python-exit-code 1 --python $generatorPath -- --output $outputPath --unit $kind
    if ($LASTEXITCODE -ne 0) { throw "Unit generation failed: $kind" }
    foreach ($animation in @('idle','walk','attack')) {
        foreach ($extension in @('png','json')) {
            $sourcePath = Join-Path $outputPath "${kind}_${animation}.$extension"
            $destinationPath = Join-Path $assetPath "${kind}_${animation}.$extension"
            Copy-Item -LiteralPath $sourcePath -Destination $destinationPath
        }
    }
}
