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

通常起動は戦闘シミュレーションです。Spaceで槍足軽の進軍を開始し、F7で刀武士同士の戦闘へ切り替えます。F2で兵種素材、素材確認からF6で隊列確認を開けます。現在は兵種・隊列・衝突・士気による自律戦闘を優先して開発しています。

関ヶ原の時系列再生デモ（シアターモード）は起動引数 `--sekigahara` で開きます。既存デモを維持し、再生演出や地形の精密化は後順位にしています。模式地形・推定経路による初版で、史実の厳密再現ではありません。[デモの仕様と出典](docs/reference/sekigahara_demo.md)を参照してください。

右上の「戦闘試作へ」、または起動引数 `--battle` で従来の戦闘試作を開きます。以下は戦闘試作での操作です。

「兵種を見る」では槍足軽・刀武士・弓足軽・騎馬武者を確認できます。F4で兵種、F3で移動／攻撃を切り替え、Spaceで再生します。直接開く場合は `--unit samurai` / `--unit archer` / `--unit cavalry`。追加兵種は表示素材の初版で、固有の射撃・突撃などの戦闘計算は今後実装します。

素材確認でF6を押すと、24体の[隊列確認](docs/reference/formation_drill.md)へ切り替わります。右クリックで移動先を指定し、Spaceで再生。歩兵と騎馬の間隔・加速・旋回・再整列を比較できます。直接起動は `--formation --unit cavalry`。

素材確認・隊列確認・戦闘試作でF7を押すと、刀武士同士の近接戦闘へ移ります。Spaceで進軍開始。直接起動は `--sword-battle`。[刀の間合いと実装範囲](docs/reference/melee_profiles.md)を参照してください。

WASD / 矢印で移動、ホイールで拡大縮小、Rでカメラを初期化、1 / 2 / 3で兵士数を1,000 / 5,000 / 10,000体に切り替えます。Escで終了します。

Q / Eを押している間、または中ボタンドラッグで、戦場をなめらかに回転できます。平行投影と見下ろす角度は維持します。兵士のSpriteは相対角度に応じた8方向切り替えです。

Spaceで2部隊の進軍・一時停止、Homeで部隊を初期位置に戻します。接敵すると前列が槍を突き、後列は待機・欠員補充します。損害・士気低下から撤退まで観察できます。

兵士はBlenderで生成した8方向のIdle / Walk / Attack素材です。F2で素材確認画面、Vで旧素材との比較ができます。素材確認中はF3で歩行／槍突きを切り替え、Spaceで再生・停止、Z / Cで兵士の向きを変更できます。

- [開発・起動・検証手順](docs/reference/development.md)
- [ビジュアルの方向性](docs/reference/visual_direction.md)
- [槍足軽の素材生成](docs/reference/asset_pipeline.md)
- [計画と資料の入口](docs/README.md)
