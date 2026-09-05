# 戦国合戦シミュレーター

C++ / DirectX 12によるWindows向けの俯瞰型合戦シミュレーター。関ヶ原の時系列デモと、2部隊の戦闘シミュレーションを実装しています。

## ビルド・起動

Visual Studioの「C++によるデスクトップ開発」、Windows SDK、CMakeを使用します。生成済みSpriteを同梱しているため、通常のビルド・起動にBlenderは不要です。リポジトリのルートでPowerShellから実行します。

```powershell
$buildScript = Join-Path (Get-Location).Path 'tools/build.ps1'
& $buildScript -Configuration Release -Test
$appPath = Join-Path (Get-Location).Path 'build/Release/sengoku_rts.exe'
& $appPath
```

通常起動は関ヶ原の史実再生デモです。再生ボタン、時間スライダー、右側の局面一覧で戦況を観察できます。模式地形・推定経路による初版で、史実の厳密再現ではありません。WASDで移動、ホイールで拡大縮小、Q/E・中ドラッグで回転。Spaceで再生・停止、←/→で10分移動、Homeで初期時刻、Rで全景、Escで終了します。[デモの仕様と出典](docs/reference/sekigahara_demo.md)を参照してください。

右上の「戦闘試作へ」、または起動引数 `--battle` で従来の戦闘試作を開きます。以下は戦闘試作での操作です。

WASD / 矢印で移動、ホイールで拡大縮小、Rでカメラを初期化、1 / 2 / 3で兵士数を1,000 / 5,000 / 10,000体に切り替えます。Escで終了します。

Q / Eを押している間、または中ボタンドラッグで、戦場をなめらかに回転できます。平行投影と見下ろす角度は維持します。兵士のSpriteは相対角度に応じた8方向切り替えです。

Spaceで2部隊の進軍・一時停止、Homeで部隊を初期位置に戻します。接敵すると前列が槍を突き、後列は待機・欠員補充します。損害・士気低下から撤退まで観察できます。

兵士はBlenderで生成した8方向のIdle / Walk / Attack素材です。F2で素材確認画面、Vで旧素材との比較ができます。素材確認中はF3で歩行／槍突きを切り替え、Spaceで再生・停止、Z / Cで兵士の向きを変更できます。

- [開発・起動・検証手順](docs/reference/development.md)
- [ビジュアルの方向性](docs/reference/visual_direction.md)
- [槍足軽の素材生成](docs/reference/asset_pipeline.md)
- [計画と資料の入口](docs/README.md)
