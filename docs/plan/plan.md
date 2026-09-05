# plan — 実装方針と優先順位

作成日時: 2026-09-05 09:53
更新日時: 2026-09-05 11:12

## 計画の位置づけ

[技術仕様 Draft 0.1](../戦国合戦シミュレーター%20技術仕様%20Draft%200.1.md)の開発優先順位を、着手順と確認項目に整理したもの。以下は実装予定であり、完了状況は[progress.md](progress.md)に記録する。日程と各段階の詳細な合格基準は未確定。

## 実装順序

| 優先度 | 段階 | 主な作業 | 確認すること |
| --- | --- | --- | --- |
| 1 | Visual Prototype | DX12 Window、俯角固定・水平回転可能なOrthographic Camera、Pan / Zoom、簡単な3D地形、Color Sprite表示 | 3D地形と2D Spriteの組み合わせが、目指すレトロな戦場表現に見えるか |
| 2 | Mass Soldier Rendering | Instance BufferとGPU Instancingによる大量描画 | まず1,000体、続いて5,000体・10,000体で性能を測定する。兵士ごとのDraw Callを避ける |
| 3 | Formation Simulation | 2部隊の状態管理、移動、向き、表示用兵士の配置、Walk Animation | 部隊の状態から表示を生成でき、兵力と描画数を独立して扱えるか |
| 4 | Combat / Morale | 簡易接触判定、戦闘、損害、Cohesion / Morale低下、撤退 | 500対500の交戦から士気低下・撤退までを確認し、Prototype 01の到達点を満たす |
| 5 | Blender Asset Pipeline | 槍足軽1種類のモデル・Rig・槍・Idle / Walk / Spear Attack、方向別レンダリング、Pixel Art変換 | 32x32前後・8方向・少ないフレーム数の素材で動作が判別でき、生成を自動化できるか |
| 6 | Sekigahara Terrain | 関ヶ原の地形データと読み込み | 共通の仕組みで合戦の地形を扱えるか |
| 7 | Historical Scenario Data | 軍勢・イベントなどのデータ形式と関ヶ原シナリオ | 合戦固有の挙動をScenario Dataで表現できるか |
| 8 | Effects / Audio / UI | 演出、環境音、戦闘効果音、操作・観察UIの充実 | 戦況の読み取りやすさと戦場の雰囲気を高められるか |

優先度8は本格的な充実の段階とする。Prototypeに必要な最小限の入力や表示は、それ以前の検証に含める。Prototypeの音はPlaceholder Audioを使用する方針。

[音響方針と必要素材リスト](../reference/audio_assets.md)に制作分担と優先順位を整理した。最初は風・足音・装備音の仮素材から開始できる。自然な人声などの本素材は録音・外部素材で補い、BGMは後回しにする。音声制作と再生処理は未着手。

## Prototypeの範囲

最初はPlaceholderのColor Spriteのみで開始する。Depth / Normal / 各種Mask、天候、複雑な照明などは後続の検討とし、関ヶ原全体の再現を先行させない。

命令の候補はMove / Hold / Attack / Charge / Fire / Retreat。技術仕様のPrototype向け命令一覧と、Prototype 01の最小機能一覧には粒度の差があるため、各命令をどの段階で実装するかは着手時に具体化する。

キャラクター素材はまず槍足軽1種類で制作方式を検証する。基本アニメーション全種や騎馬・武将・追加装備への拡張は、その結果を踏まえて進める。

## 今回具体化した方針

- C++20 / CMake / MSVC / Windows SDKを採用し、外部ライブラリなしでDX12描画を開始した。[ビルド・検証手順](../reference/development.md)を参照。
- 俯瞰カメラ、起伏のある地形、仮Sprite、GPU Instancingを実装し、1,000 / 5,000 / 10,000体で起動を確認した。正式な性能測定は未実施。
- ユーザーのAge of Empires風という希望と参考画像を[ビジュアル方針](../reference/visual_direction.md)に整理した。
- 槍足軽の簡易Rig、剛体ウェイト、Idleと8フレームWalkの8方向生成を実装した。[素材生成手順](../reference/asset_pipeline.md)を参照。部隊単位の進軍・一時停止・到着時停止まで動作する。左クリックの部隊選択・右クリックの移動命令・Hの停止命令まで実装した。次は接敵・戦闘・士気へ進む。
- ユーザーの希望により、Draftの「原則カメラ回転なし」を変更し、俯角固定・水平360度回転に対応した。兵士は世界での向きとカメラ方位の差からSpriteを選ぶ。
- ユーザー指定に合わせてコンテンツ領域を1920×1080とし、通常の戦場と素材確認画面を用意した。

## 今後具体化する項目

- ビルド環境の他のPCでの再現性と配布手順。
- 性能測定に使うハードウェア、画面解像度、測定条件。
- 現在のY-up左手系・立ち板Sprite・深度テストを土台に、移動時の遮蔽とPixel Spriteが崩れないZoom方式を評価する。
- Formationの更新方式、移動・接触判定、戦闘と士気の最小ルール。

候補を検証し、採用した判断と理由を`docs/reference/`に記録する。実装が進んだら進捗と未決事項を更新する。
