param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release'
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$executablePath = Join-Path $repoRoot "build/$Configuration/sengoku_rts.exe"
if (-not (Test-Path -LiteralPath $executablePath)) {
    throw "実行ファイルがありません。tools/build.ps1 -Configuration $Configuration を先に実行してください。"
}
# ユーザーが布陣を観察するための画面を開く。
Start-Process -FilePath $executablePath -ArgumentList '--sekigahara' -WorkingDirectory $repoRoot
