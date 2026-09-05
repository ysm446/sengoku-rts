# 開発・起動・検証手順

作成日時: 2026-09-05 10:13
更新日時: 2026-09-06 08:48

## 必要環境

- Windows、DirectX 12に対応するGPU。
- Visual Studioの「C++によるデスクトップ開発」、Windows SDK、CMake 3.24以上。
- PowerShell。`tools/build.ps1`はWindows PowerShellで日本語が誤解釈されないようUTF-8 BOM付き。

この作業環境ではVisual Studio Community 2026、MSVC 19.51、Windows SDK 10.0.26100.0、CMake 4.3.1を使用した。GPUは高性能優先で選択する。ソフトウェア描画の確認には`--warp`を使える。

ユーザーがBlender 5.2をインストール済み。`C:/Program Files/Blender Foundation/Blender 5.2/blender.exe`で5.2.1 LTSを確認し、槍足軽の8方向Sprite生成に使用している。[素材生成手順](asset_pipeline.md)を参照。生成済みPNGを同梱するため、通常のゲームビルド・起動にBlenderは不要。

## ビルド・起動

リポジトリのルートで実行する。

```powershell
$buildScript = Join-Path (Get-Location).Path 'tools/build.ps1'
& $buildScript -Configuration Release -Test
$appPath = Join-Path (Get-Location).Path 'build/Release/sengoku_rts.exe'
& $appPath
```

`Debug`も指定できる。スクリプトはCMakeでx64のVisual Studioプロジェクトを生成し、ビルド後にCTestを実行する。外部ライブラリのダウンロードは行わない。

## 関ヶ原デモ

引数なし、または `--sekigahara` で関ヶ原の史実再生デモを開く。タイムラインのドラッグ、局面クリック、再生・停止・速度変更、軍勢選択が可能。初期コンテンツ領域は1920×1080（枠・タイトルバーを除く）。WASD、ホイール、Q/E、中ドラッグでカメラを操作する。←/→は10分の時間移動。出典、模式化の範囲、全操作は[関ヶ原デモ](sekigahara_demo.md)を参照。

従来の戦闘試作は `--battle` または右上の「戦闘試作へ」。以下の操作一覧は戦闘試作のもの。

## 戦闘試作の操作

関ヶ原画面の「兵種を見る」から兵種確認へ移動できる。F4で槍足軽・刀武士・弓足軽・騎馬武者を切り替える。F3で移動／攻撃、Spaceで動作再生。直接起動は `--unit samurai` / `--unit archer` / `--unit cavalry`。新兵種は表示確認用で、戦闘ルールは未接続。

初期のコンテンツ領域は1920×1080ピクセル（16:9）。ウィンドウ枠・タイトルバーはこの外側に付く。モニターのDPIを考慮して枠サイズを計算し、起動時に実際のクライアント領域を検証する。通常のウィンドウサイズ変更にも対応する。

| 操作 | 内容 |
| --- | --- |
| WASD / 矢印 | カメラを画面の上下左右方向へ移動 |
| 左クリック | 戦場の部隊を選択（色を明るく表示）。空き地で選択解除 |
| 右クリック | 選択部隊の移動先を指定。一時停止中はSpaceで再開 |
| H | 選択部隊の移動命令を取り消し、その場で停止 |
| マウスホイール | 拡大・縮小 |
| R | 初期カメラへ戻す |
| F2 | 戦場と8方向の素材確認画面を切り替え |
| F3 | 素材確認画面で歩行／槍突きを切り替え |
| V | Blender素材と従来の仮素材を比較 |
| Q / E（長押し） | カメラを左右へ連続回転（毎秒90°） |
| マウス中ボタンドラッグ | カメラを水平に連続回転 |
| Space | 戦場では進軍・一時停止、素材確認では選択動作の再生・停止 |
| Home | 部隊位置とアニメーションを初期化 |
| Z / C | 素材確認画面で兵士の向きを変更 |
| 1 / 2 / 3 | 表示兵士数を1,000 / 5,000 / 10,000体へ変更 |
| ウィンドウのサイズ変更 | 描画領域と投影の縦横比を更新 |
| M | 音のON/OFF（一時停止・素材確認・非アクティブ時は消音） |
| F5 | 音量の調整対象を全体→環境→効果音へ切り替え |
| ＋／－（テンキーも可） | 選んだ音量を10％ずつ調整（0〜100％、タイトルに表示） |
| Esc | 終了 |

タイトルに素材の種類、表示モード、兵士数、ループのfps、操作案内を表示する。兵士数を増やすと同じ範囲に密集させるため、負荷確認用の配置になる。実際の部隊の兵力や隊列の仕様ではない。

Spaceで進軍すると接触戦闘が始まり、士気・隊列・兵力の低下から自動撤退へ進む。タイトルバーに両軍の状態と勝敗を表示する。撤退部隊は移動・停止命令を受け付けない。Homeでやり直せる。[戦闘の仮ルールと制限](combat_prototype.md)を参照。

## 仮効果音

Spaceで進行させると風と進軍・交戦の仮音が鳴る。Mで消音できる。F5で音量対象を選び、＋／－で調整する。音量変更時に実行ファイル隣のaudio-settings.txtへ保存し、次回起動で読み込む。Homeでは変更しない。保存不可の場合はタイトルに表示する。音声デバイスがなくてもゲームは続行し、タイトルに状態を表示する。詳しくは[音響方針](audio_assets.md)を参照。

## 自動検証と画像保存

- `unit_assets`：追加3兵種の全408コマの透過・画像端・動作差、表示寸法とPivotの切り替えを検証。
- `dx12_samurai` / `dx12_archer` / `dx12_cavalry`：各兵種の攻撃表示、F4による切り替え、GPU描画と画像保存を検証。

- `historical_scenario`：時間更新、一時停止、転進境界、経路の整合、巻き戻しの同一性、固定Sprite数を検証。
- `dx12_sekigahara`：7時点の再生画像、再生・停止・時刻ドラッグ・初期化・軍勢選択・矢印切り替えを検証。`build/sekigahara.bmp` と時刻別BMP、初期と12:15の `.ui.bmp` を保存する。`--sekigahara --smoke-test` で実行。

- `battle_audio`：波形とループ端、動作による音量、左右定位、Zoom、消音を検証。音声デバイスは無音で初期化し、利用可能か報告する。実際の試聴は含まない。

`scene_invariants`では個体ごとの追従・歩行時計、倒れた位置と姿勢の保持、一時停止、表示人数切り替え、初期化も確認する。倒れた兵士はHomeで初期化するまで保持される。[個体表示の実装範囲](individual_soldiers.md)を参照。

- `battle_simulation`：固定更新、接触戦闘、停止・離脱、士気・隊列の低下、対称条件の勝敗、撤退と初期化を確認。
- `dx12_combat`：0.1秒刻みで個体表示を更新し、80枚の描画で80秒の戦闘を進め、交戦・撤退・勝敗を検証。`build/combat.bmp.engaged.bmp`と`build/combat.bmp`へ交戦中と終了時を保存。`--combat-test`は`--soldiers 10000`・`--warp`とも併用可能（`--inspect`・`--motion-test`との併用は不可）。

- `scene_invariants`：表示人数、接地、全64歩行タイルの透過・画像端と各方向8フレームの差異、回転後の投影・移動、進軍・停止・目的地到達・無効入力、移動・停止命令と回転・ズーム・Pan後の地形クリックを確認。
- `dx12_smoke`：非表示ウィンドウで5フレーム描画し、描画領域の縮小・復元、Pan / Zoom、Scene再転送、GPUからの画像読み戻しを確認。戦場ではWin32メッセージによる部隊選択・移動・H停止・選択解除も確認。
- `dx12_inspect`：8方向×2陣営を表示し、`build/inspect.bmp`に保存。キー長押しの連続回転、左右同時押しの相殺、キー解放・フォーカス喪失時の停止、中ボタンドラッグも検証する。
- `dx12_attack`：8方向の槍突きを描画し、`build/attack.bmp`へ保存。`--inspect-attack`で確認画面を起動できる。
- `dx12_motion`：16フレームで進軍と8方位のカメラ回転を行い、歩行素材の読み込みと描画を確認して`build/motion.bmp`に保存。

`build/smoke.bmp`と`build/smoke.bmp.txt`に描画画像と結果を保存する。失敗時は、可能なら`build/smoke.bmp.error.log`へ理由を記録する。デバッグレイヤーが利用できる環境ではDX12の警告・エラーもテスト失敗として扱う。利用できない場合はレポートに明記する。

追加の確認例：

```powershell
$appPath = Join-Path (Get-Location).Path 'build/Release/sengoku_rts.exe'
$capturePath = Join-Path (Get-Location).Path 'build/smoke-10000.bmp'
& $appPath --smoke-test --soldiers 10000 --capture $capturePath
```

`--soldiers`は1000 / 5000 / 10000を受け付ける。`--capture`は通常起動にも指定でき、その場合は最初のフレームを保存してアプリを継続する。`--warp`でソフトウェア描画に切り替える。

`--march`は進軍（`--inspect`併用時は歩行再生）を開始する。`--yaw`は初期方位を度数で指定する。`--motion-test`は進軍を伴う自動検証で、`--soldiers 10000`や`--warp`と組み合わせられる。

## 実装の範囲と制限

- `src/main.cpp`：Win32ウィンドウ、入力、実行ループ、起動テスト。
- `src/camera.h`：俯角固定・水平360°回転のOrthographic Camera、画面基準のPan / Zoom、兵士の相対方向計算。
- `src/simulation.cpp`：2部隊の固定刻み更新、進軍・命令、接触戦闘・損害・士気・隊列・自動撤退・勝敗。表示兵士数から独立した状態更新。
- `src/scene.cpp`：地形・Atlas・各素材の配置、部隊位置とカメラ方位に応じた兵士位置・歩行フレームの更新。
- `src/soldier_visuals.cpp`：表示用IDに基づく兵士ごとの追従位置・動作時計・生存状態・死亡位置の保持。描画用の姿勢はSceneからInstanceへ渡す。
- `src/renderer.cpp`：DX12の初期化、深度バッファ、Atlas転送、Instancing、リサイズ、画像保存。
- `shaders/battlefield.hlsl`：地形とSpriteの描画。実行時にコンパイルし、ビルド時に実行ファイルの隣へコピーする。

座標系はY-upの左手系。仮素材は地面に立つ板、Blender製の兵士は画面に平行な板で表示し、透過部分を破棄して深度テストを行う。両者ともカメラ回転に追従する。木・旗・陣幕は1方向の仮素材のため、裏側の形状は再現しない。Atlasは64×64タイルを12列×17行に配置した768×1088。PNGの読み込みは`src/sprite_sheet.cpp`のWIC処理で行う。詳細は[素材生成手順](asset_pipeline.md)を参照。

通常描画では兵士のInstanceデータを毎フレーム更新し、地形やTextureは再転送しない。初期状態では既定の目的地へ進軍し、右クリックで選択部隊の移動先を変更できる。選択は各小組の現在中心±2.6を優先し、該当がなければ備中心±14で判定する。重なった場合は近い中心を選ぶ。選択した小組の所属・疲労・状態をタイトルに表示し、右クリックとHは備全体へ送る。地形外のクリックは移動命令を出さない。隊列が地形をはみ出さないよう目的地の中心を±60へ制限する。接触は半幅14の正方形同士で判定する。経路探索・障害物回避・多部隊の衝突解決・追撃は未実装。

最小構成では毎フレームFenceの完了を待つ。頂点・InstanceデータはUpload Heapに保持する。フレーム並列化、描画数削減、転送方式の最適化は性能測定後に行う。表示されるfpsはVSyncやデバッグレイヤーの影響を受けるため、正式なベンチマーク値ではない。

通常起動の起動失敗はダイアログと実行ファイル隣の`error.log`で確認する。配布・移動するときは実行ファイルと`shaders/`、`assets/`を一緒に置く。

DX12の初期化と同期は[Microsoftの基本構成資料](https://learn.microsoft.com/en-us/windows/win32/direct3d12/creating-a-basic-direct3d-12-component)を参照し、テクスチャの転送配置は[GetCopyableFootprints](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-getcopyablefootprints)に従っている。
