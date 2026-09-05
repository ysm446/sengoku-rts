# 戦国合戦シミュレーター

C++ / DirectX 12によるWindows向けの俯瞰型合戦シミュレーター。現在は、3D地形と仮の2D Spriteを表示するVisual Prototypeです。

## ビルド・起動

Visual Studioの「C++によるデスクトップ開発」、Windows SDK、CMakeを使用します。生成済みSpriteを同梱しているため、通常のビルド・起動にBlenderは不要です。リポジトリのルートでPowerShellから実行します。

```powershell
$buildScript = Join-Path (Get-Location).Path 'tools/build.ps1'
& $buildScript -Configuration Release -Test
$appPath = Join-Path (Get-Location).Path 'build/Release/sengoku_rts.exe'
& $appPath
```

WASD / 矢印で移動、ホイールで拡大縮小、Rでカメラを初期化、1 / 2 / 3で兵士数を1,000 / 5,000 / 10,000体に切り替えます。Escで終了します。

Q / Eを押している間、または中ボタンドラッグで、戦場をなめらかに回転できます。平行投影と見下ろす角度は維持します。兵士のSpriteは相対角度に応じた8方向切り替えです。

Spaceで2部隊の進軍・一時停止、Homeで部隊を初期位置に戻します。部隊は所定の位置で停止します。戦闘・士気・撤退は今後実装します。

兵士はBlenderで生成した8方向のIdle / Walk素材です。F2で素材確認画面、Vで旧素材との比較ができます。素材確認中はSpaceで歩行の再生・停止、Z / Cで兵士の向きを変更できます。

- [開発・起動・検証手順](docs/reference/development.md)
- [ビジュアルの方向性](docs/reference/visual_direction.md)
- [槍足軽の素材生成](docs/reference/asset_pipeline.md)
- [計画と資料の入口](docs/README.md)
