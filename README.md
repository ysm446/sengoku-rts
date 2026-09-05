# 戦国合戦シミュレーター

C++ / DirectX 12によるWindows向けの俯瞰型合戦シミュレーター。現在は、3D地形と仮の2D Spriteを表示するVisual Prototypeです。

## ビルド・起動

Visual Studioの「C++によるデスクトップ開発」、Windows SDK、CMakeを使用します。Blenderや外部素材はまだ不要です。リポジトリのルートでPowerShellから実行します。

```powershell
$buildScript = Join-Path (Get-Location).Path 'tools/build.ps1'
& $buildScript -Configuration Release -Test
$appPath = Join-Path (Get-Location).Path 'build/Release/sengoku_rts.exe'
& $appPath
```

WASD / 矢印で移動、ホイールで拡大縮小、Rでカメラを初期化、1 / 2 / 3で兵士数を1,000 / 5,000 / 10,000体に切り替えます。Escで終了します。

兵士は静止した仮素材です。部隊の移動・戦闘・士気・撤退は今後実装します。

- [開発・起動・検証手順](docs/reference/development.md)
- [ビジュアルの方向性](docs/reference/visual_direction.md)
- [計画と資料の入口](docs/README.md)
