# 槍足軽の素材生成

作成日時: 2026-09-05 10:35
更新日時: 2026-09-05 12:09

## 現在の到達点

Blender 5.2.1 LTSのPythonで槍足軽のモデルと6本のBoneを持つ簡易Rigを作り、Idleと8フレームWalkを各8方向のPNGに生成してDX12に表示できる。各パーツを1本のBoneへ剛体ウェイトで割り当て、脚と左腕を動かす。槍を水平に構えて突く8フレームAttackも各8方向で生成する。史実考証や自然な歩容を完成させた素材ではない。

## 再生成

リポジトリのルートで実行する。

```powershell
$assetScript = Join-Path (Get-Location).Path 'tools/generate-assets.ps1'
& $assetScript
$buildScript = Join-Path (Get-Location).Path 'tools/build.ps1'
& $buildScript -Configuration Release -Test
```

攻撃素材だけ生成する場合はスクリプトへ`-AttackOnly`を渡す。

Blenderの場所が異なる場合は`-BlenderPath`で実行ファイルを指定する。`--background --factory-startup`で専用プロセスを起動し、ユーザーが開いているSceneや設定は変更しない。

## ファイル

| 場所 | 内容 |
| --- | --- |
| `tools/blender/generate_ashigaru.py` | モデル・カメラ・照明、方向別レンダリング、減色、PNG出力 |
| `tools/generate-assets.ps1` | Blender実行と配布用PNG・JSONのコピー |
| `assets/sprites/ashigaru_idle.png` | ゲーム用512×64 PNG。64×64の8方向を横に配置 |
| `assets/sprites/ashigaru_idle.json` | 生成条件、方向順序、Pivot、画素数 |
| `assets/sprites/ashigaru_walk.png` | 512×512 PNG。列が8方向、行が8フレーム。ゲームでは8fpsで再生 |
| `assets/sprites/ashigaru_attack.png` | 512×512 PNG。8方向×8フレームの槍突き |
| `assets/sprites/ashigaru_attack.json` | Attackの生成条件、Pivot、FPS |
| `assets/sprites/ashigaru_walk.json` | Walkの生成条件、Bone名、FPS、フレーム数 |
| `build/assets/ashigaru/ashigaru.blend` | 再生成可能なモデル・カメラ・照明。Git管理対象外 |
| `build/assets/ashigaru/preview.png` | 8方向の最近傍拡大比較画像 |
| `build/assets/ashigaru/raw_*.png` / `idle_*.png` | 減色前後の方向別画像 |

生成スクリプトが現在の素材の編集元。再実行すると出力を更新するため、`.blend`を手動で編集する場合は別名で保存して使う。通常のゲームビルドは生成済みPNGをコピーするだけで、Blenderを必要としない。

## 画像と座標の取り決め

- BlenderはZ-up、正面は-Y。画像0が正面で、次の画像ごとにモデルをZ軸周りに+45度回す。
- 生成カメラはOrthographic、俯角約40.316度、Idle / Walkは`ortho_scale = 3.4`、Attackは5.0。ゲーム側も同じ俯角を保ち、水平方位のみ回転させる。
- 足元原点のPivotは画像左上基準で`(0.5, 0.88)`。ゲームでは幅・高さ3.4の画面に平行な板へ貼り、同じPivotを地形へ接地させる。AttackはPivot `(0.5, 0.75)`、幅・高さ5.0。
- 64×64で描画後、RGB各チャンネルを16段階に減色し、Alphaを0または255にする。全方向で画像端に不透明画素がないことを生成時に検証する。
- 鎧と笠の中立的な灰色をゲーム側で陣営色に変える。独立したTeam Color Maskはまだ使わない。
- ゲーム内Atlasは768×1088（64×64の12列×17行）。先頭行の0～3は仮素材、4～11はIdle。後続8行の列4～11はWalk、さらに8行はAttack。仮素材は元の32×48から最近傍で展開する。

JSONは生成記録として使用する。実行時はIdleが512×64、Walk / Attackが512×512であることを検証する。解像度・Pivot・縮尺を変更するときは生成スクリプト、Scene、Shaderを合わせて更新する。`.blend`ではフレーム0をIdle、1～8をWalkとし、9にはループ先頭と同じポーズを置く。

実行ファイルの隣の`assets/sprites/`からIdle / Walk / AttackをWICで読む。Attackがない場合は前後動で代用する。Idleがない場合は仮素材、Walkだけがない場合はIdleのみで表示する。存在するPNGが破損やサイズ不一致の場合はエラーにする。`--motion-test`では生成済みの両PNGを必須として配布漏れも検証する。

## 見た目の確認

- F2：戦場と素材確認画面を切り替える。素材確認は8方向×2陣営の16体。
- V：Blender素材と従来の仮素材を切り替える。
- Q / E：長押しでカメラを連続回転。中ボタンドラッグも連続回転。兵士のSpriteはカメラとの相対角度に応じて8方向から選ぶ。
- Z / C：素材確認画面で兵士の世界方位を変更する。
- F3：素材確認画面でWalk / Attackを切り替える。
- Space：選択動作の再生・一時停止。Homeで動作時計を初期化。
- `--inspect`：素材確認画面で起動。`--inspect-attack`：槍突きを再生開始。`--placeholder`：仮素材で起動。

CTestの`dx12_inspect`は素材確認画面を`build/inspect.bmp`へ保存し、`dx12_motion`はカメラ回転と歩行を検証する。`dx12_attack`は槍突きを`build/attack.bmp`へ保存する。全攻撃画像の透過・端・各方向4種類以上のポーズ差も検証する。全64Walk画像の透過・画像端・欠落と、8フレームが異なるポーズであることを単体テストでも確認する。

参照した公式API：[Render Operators](https://docs.blender.org/api/5.2/bpy.ops.render.html)、[RenderSettings](https://docs.blender.org/api/5.2/bpy.types.RenderSettings.html)、[world_to_camera_view](https://docs.blender.org/api/5.2/bpy_extras.object_utils.html)。
