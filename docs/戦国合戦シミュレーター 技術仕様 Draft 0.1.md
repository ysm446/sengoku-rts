# 戦国合戦シミュレーター 技術仕様 Draft 0.1

作成日時: 2026-09-05 09:53
更新日時: 2026-09-05 09:53

作成日時は文書管理上の初回記録日時。原資料の実際の作成日時は不明。設計本文は維持し、目的・計画・現状への案内を[ドキュメント入口](README.md)に整理した。

## 1. プロジェクト概要

戦国時代の合戦を再現する、俯瞰型リアルタイム戦闘シミュレーターを制作する。

第一弾のシナリオとして「関ヶ原の戦い」を実装するが、ゲームシステム自体は特定の合戦に依存させず、将来的に以下のような合戦を追加できる構造とする。

- 関ヶ原
- 長篠
- 川中島
- 桶狭間
- 大坂の陣
- その他の戦国時代の合戦

ゲームの中心的なビジュアルコンセプトは、

「山水図・合戦図屏風・古い日本画が動き出したような戦場」

とする。

リアルな3DCGではなく、意図的にレトロゲーム的な表現を狙う。

---

## 2. 基本技術

### 開発環境

- Windows
- Visual Studio Code
- Codex
- C++
- DirectX 12
- HLSL
- Python
- Git / GitHub

ゲームエンジンは使用しない。

DirectX 12による独自ゲームフレームワークとして実装する。

---

## 3. ビジュアル方式

基本構造は、

**3D Battlefield + 2D Sprite Characters**

とする。

### 3Dで扱うもの

- 地形
- 山
- 谷
- 地面
- 高低差
- 河川
- 道路
- 衝突判定
- ナビゲーション用データ

### 2D Spriteで扱うもの

- 足軽
- 武士
- 武将
- 騎馬
- 旗
- 木
- 建物
- 草木
- 煙
- 炎
- 砂埃
- 一部のエフェクト

内部的には3D空間上に2Dスプライトを配置する。

---

## 4. カメラ

RTS型の俯瞰カメラ。

基本的には平行投影を使用する。

### 初期方針

- Orthographic Projection
- カメラ角度は基本固定
- カメラ回転は原則行わない
- Pan可能
- Zoom可能
- Zoomは段階式を検討
- Pixel Spriteの見た目を崩さないZoom方式を検討する

ゲーム内の世界は3Dだが、画面としては2Dゲームに近く見えることを目標とする。

---

## 5. キャラクター制作方式

キャラクターは3Dモデルとして制作し、最終的には2D Spriteへ変換する。

ゲーム実行時には基本的にキャラクターの3Dモデルを使用しない。

制作パイプライン：

3D Model
↓
Rig
↓
Animation
↓
Orthographic Rendering
↓
Low Resolution
↓
Palette Reduction
↓
Pixel Processing
↓
Sprite Sheet

---

## 6. Blender

キャラクター素材制作のためBlenderを使用する。

Blenderはゲームエンジンとしてではなく、

**Character Asset Compiler**

として扱う。

BlenderのGUI操作への依存をできるだけ減らし、Pythonによる自動処理を目指す。

### Blenderで担当する処理

- Character Model読み込み
- Rig生成
- Bone設定
- Skinning
- Animation
- Weapon Attachment
- Equipment変更
- Camera Setup
- Lighting
- 方向別レンダリング
- Sprite Sheet生成用画像出力

---

## 7. Blender Automation

Blender Python APIを利用する。

例：

tools/blender/

- create_rig.py
- setup_character.py
- create_walk.py
- create_run.py
- create_spear_attack.py
- create_sword_attack.py
- create_gun_animation.py
- render_directions.py
- render_sprite_sheet.py

最終的にはコマンドラインからアセット生成できる構造を目指す。

例：

blender.exe --background character.blend --python render_sprites.py

---

## 8. Animation

最初に必要となる基本アニメーション：

### Foot Soldier

- Idle
- Walk
- Run
- Spear Idle
- Spear Attack
- Sword Attack
- Gun Fire
- Gun Reload
- Hit
- Death
- Flee

アニメーションはリアルさより、

「小さいスプライトでも動作が判別できる」

ことを優先する。

ポーズ・シルエット・動作を実際より誇張して構わない。

---

## 9. Animation FPS

ゲーム本体は60fps程度で動作させる。

Sprite Animationは意図的に低FPSとする。

目安：

- Walk: 6～10fps
- Attack: 6～12fps
- Flag: 6～10fps
- Smoke: 8～12fps

現代的な滑らかさではなく、レトロゲーム的なアニメーションを目指す。

---

## 10. Sprite Resolution

初期テストでは以下を候補とする。

- 24x24
- 24x32
- 32x32
- 32x40

騎馬や大型ユニットについては、それ以上のサイズも許可する。

最初のPrototypeでは32x32前後を基準とする。

---

## 11. Direction

キャラクターの方向は8方向を基本候補とする。

- N
- NE
- E
- SE
- S
- SW
- W
- NW

ただしカメラ固定であることを利用し、必要に応じて方向数を削減する。

左右反転可能な素材についてはMirrorを使用する。

---

## 12. Pixel Art Processing

Blenderからレンダリングした画像をそのまま使用せず、レトロ表現用の後処理を行う。

候補：

- Downsample
- Nearest Neighbor
- Limited Palette
- Color Quantization
- Dithering
- Hard Edge
- Anti-Alias Removal
- Contrast Adjustment
- Outline Processing

3Dレンダリングが「低解像度3DCG」に見えるのではなく、

「手描きのPixel Art」

に近づくことを目標とする。

---

## 13. Character Additional Buffers

必要に応じてColor以外の情報もSprite生成時に出力する。

候補：

- Color
- Depth
- Normal
- Character Mask
- Team Color Mask
- Equipment Mask

これらをDirectX 12側で利用し、以下の効果を実現できるようにする。

- Selection Outline
- Fog
- Fire Lighting
- Team Color
- Hit Flash
- Terrain Occlusion
- Shadow
- Weather Effects

ただしPrototype段階ではColor Spriteのみで開始する。

---

## 14. Soldier Rendering

大量の兵士を描画するため、SpriteはGPU Instancingを使用する。

各Soldierを個別のDraw Callにはしない。

基本データ例：

struct SoldierInstance
{
    float3 position;

    uint32_t spriteID;
    uint32_t animationID;
    uint32_t frame;
    uint32_t direction;

    uint32_t teamID;
};

Instance BufferをGPUへ送信し、DrawIndexedInstanced等で描画する。

目標：

- 1,000 soldiers
- 5,000 soldiers
- 10,000 soldiers

の順にPerformance Testを行う。

---

## 15. SimulationとRenderingの分離

非常に重要な設計原則とする。

兵士1人1人を完全なGame Entityとしてシミュレーションしない。

Simulation Unitは「部隊」とする。

例：

Formation
{
    position
    direction
    strength
    morale
    fatigue
    cohesion
    formationType
    commander
}

表示されるSoldier Spriteは、このFormationを視覚化するためのRepresentationとする。

例：

Simulation:

島左近隊
Strength = 1000

Rendering:

100～200 Soldier Sprites

Simulation上の人数と表示人数は一致しなくてもよい。

---

## 16. Battle Simulation

初期段階では以下を中心とする。

- Strength
- Morale
- Fatigue
- Cohesion
- Attack
- Defense
- Movement Speed
- Terrain Bonus

Battleの中心はHP削りではなく、

「隊列と士気の崩壊」

とする。

基本フロー：

接敵
↓
戦闘
↓
損害
↓
Cohesion低下
↓
Morale低下
↓
Retreat / Flee

---

## 17. Orders

Prototypeでは以下の命令を用意する。

- Move
- Hold
- Attack
- Charge
- Fire
- Retreat

高度な命令は後から追加する。

---

## 18. Scenario System

関ヶ原固有のロジックをゲームコードへ直接書かない。

Scenario Dataとして管理する。

例：

data/
  battles/
    sekigahara/
      battle.json
      terrain.json
      armies.json
      events.json

将来的に新しい合戦を追加可能にする。

---

## 19. Game Modes

将来的には二つのモードを検討する。

### Historical Mode

史実に沿って戦況が進行する観察・再現モード。

プレイヤーは合戦全体を観察する。

### Sandbox / RTS Mode

東軍または西軍を操作する。

史実とは異なる行動が可能。

例：

- 小早川が動かなかった場合
- 島津が積極攻勢した場合
- 西軍が別方向から展開した場合

---

## 20. Audio

Prototype段階ではPlaceholder Audioを使用する。

本制作では以下の音を重視する。

- 風
- 鳥
- 足音
- 軍勢のざわめき
- 鬨の声
- 陣太鼓
- 馬
- 刀
- 槍
- 鉄砲
- 鉄砲の一斉射撃
- 煙や火
- 遠距離の戦闘音

BGMはPrototype完成後に検討する。

環境音と戦闘効果音をBGM以上に重要な要素として扱う。

---

## 21. Repository Structure

初期案：

BattleSimulator/

src/
  core/
  renderer/
  simulation/
  battle/
  camera/
  input/
  audio/
  ui/

shaders/

assets/
  characters/
  terrain/
  sprites/
  effects/
  audio/

data/
  battles/

tools/
  blender/
  sprite_pipeline/

docs/
  spec.md
  architecture.md
  rendering.md
  simulation.md
  asset_pipeline.md

AGENTS.md

---

## 22. Codexの役割

Codexを単なるコード補完ではなく、開発エージェントとして使用する。

主な用途：

- Architecture Review
- C++実装
- DX12 Renderer実装
- Shader作成
- Debug
- Blender Python作成
- Asset Pipeline自動化
- Animation生成スクリプト
- JSON Format設計
- Test作成
- Documentation更新

---

## 23. Codexに対する基本ルール

AGENTS.mdには最低限以下を書く。

- This is a custom DirectX 12 engine.
- Do not introduce external game engines.
- Simulation and rendering must remain separated.
- Formation is the simulation unit.
- Individual soldiers are primarily visual representations.
- Battle scenarios must be data-driven.
- Do not hard-code Sekigahara-specific behavior into the engine.
- Prefer simple systems before adding abstractions.
- Asset generation should be automated where practical.
- Blender should be treated as an offline asset creation tool.
- Preserve the retro pixel-art visual direction.

---

## 24. First Prototype

最初から関ヶ原全体を作らない。

Prototype 01の目標：

「3D地形の上を、500人対500人のPixel Soldierが進軍して激突する」

必要機能：

- DX12 Window
- Orthographic Camera
- Simple 3D Terrain
- Sprite Renderer
- Instanced Sprite Drawing
- 1000 Soldier表示
- Two Formations
- Formation Movement
- Walk Animation
- Simple Collision / Contact
- Basic Combat
- Morale
- Retreat
- Placeholder Sprite

関ヶ原の実データはまだ使用しない。

---

## 25. First Character Prototype

最初のCharacter Assetは一種類だけ作る。

「槍足軽」

必要要素：

- Simple 3D model
- Basic Humanoid Rig
- Spear
- Idle
- Walk
- Spear Attack

これをBlenderからSpriteへ変換する。

初期条件：

- 32x32前後
- 8 directions
- Low frame count
- Pixel Art conversion

このテストによって、本ゲームのVisual Pipelineが成立するかを判断する。

---

## 26. Development Priority

Priority 1:
Visual Prototype

Priority 2:
Mass Soldier Rendering

Priority 3:
Formation Simulation

Priority 4:
Combat / Morale

Priority 5:
Blender Asset Pipeline

Priority 6:
Sekigahara Terrain

Priority 7:
Historical Scenario Data

Priority 8:
Effects / Audio / UI

最初の段階では歴史的正確性より、

「このゲームの画面が魅力的に見えるか」

を優先して検証する。
