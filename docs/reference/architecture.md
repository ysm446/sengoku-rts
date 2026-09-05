# 設計概要

作成日時: 2026-09-05 09:53
更新日時: 2026-09-05 10:13

[技術仕様 Draft 0.1](../戦国合戦シミュレーター%20技術仕様%20Draft%200.1.md)の要約。以下は主に設計上の構成を示す。現在の実装範囲は[開発手順](development.md)と[進捗](../plan/progress.md)を参照する。

## 技術と責務

| 領域 | 方針 |
| --- | --- |
| 実行環境 | Windows、C++、DirectX 12、HLSL。ゲームエンジンは使用しない |
| 地形 | 山・谷・地面・河川・道路を3Dで扱い、高低差・衝突・ナビゲーション用データを持つ |
| 描画 | 兵士、旗、木、建物、煙などの2D Spriteを3D空間に配置する |
| カメラ | 原則固定角度・回転なしのOrthographic Projection。Pan / Zoomに対応し、段階式Zoomは検討対象 |
| シミュレーション | Formation単位で位置・向き・兵力・士気・疲労・隊列などを管理する |
| シナリオ | 地形・軍勢・イベントを合戦別のデータとして管理する |
| 素材制作 | BlenderとPythonでモデルからSprite Sheetを生成するオフライン処理 |

## SimulationとRenderingの境界

Formationの状態をSoldier Spriteの配置・向き・アニメーションに反映する。表示上の兵士1人ずつを完全なGame Entityとしてシミュレーションしない。

技術仕様のFormation例は`position`、`direction`、`strength`、`morale`、`fatigue`、`cohesion`、`formationType`、`commander`を持つ。描画側のSoldierInstance例は位置とSprite・Animation・Frame・Direction・Teamの識別情報を持つ。正確な型や受け渡し方法は今後設計する。

描画はInstance BufferをGPUへ送り、`DrawIndexedInstanced`などを使用する方針。1,000体から検証を始め、5,000体・10,000体へ測定範囲を広げる。

## 戦闘

接敵 → 戦闘 → 損害 → Cohesion低下 → Morale低下 → Retreat / Fleeを基本の流れとする。Strength、Fatigue、Attack、Defense、Movement Speed、Terrain Bonusなども初期の検討対象。具体的な計算式や閾値は未定。

## 素材生成

3D Model → Rig → Animation → Orthographic Rendering → Low Resolution → Palette Reduction → Pixel Processing → Sprite Sheet。

最初は槍足軽1種類についてIdle / Walk / Spear Attackを生成する。32x32前後・8方向・少ないフレーム数を初期条件とし、小さくても動作を判別できるシルエットを優先する。一般的な解像度候補は24x24、24x32、32x32、32x40で、方向数の削減や左右反転も検討する。

ゲーム本体の60fps程度という目標と、Sprite Animationの低FPSは分けて扱う。仕様上の目安はWalkが6～10fps、Attackが6～12fps、Flagが6～10fps、Smokeが8～12fps。

Prototypeの描画はColor Spriteのみで始める。Depth、Normal、Character / Team Color / Equipment Maskは追加候補で、選択輪郭・陣営色・遮蔽・影などへの利用を後から検討する。

## データとディレクトリの初期案

仕様では`src/`配下に`core`、`renderer`、`simulation`、`battle`、`camera`、`input`、`audio`、`ui`を置き、`shaders/`、`assets/`、`data/battles/`、`tools/blender/`、`tools/sprite_pipeline/`を分ける案になっている。現在は最小構成として`src/`直下にファイル単位で責務を分離し、`shaders/`と`tools/build.ps1`を追加した。詳細なディレクトリ分割やシナリオ・素材パイプラインは今後実装する。

シナリオの例は`data/battles/sekigahara/`に`battle.json`、`terrain.json`、`armies.json`、`events.json`を置く構成。データスキーマやイベント実行方式は未定だが、関ヶ原固有のロジックをエンジンに直接書かない原則は維持する。

ドキュメントの配置はプロジェクトルールに従い、`docs/plan/`を進捗管理の入口、`docs/reference/`を設計資料の置き場とする。
