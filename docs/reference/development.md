# 開発・起動・検証手順

作成日時: 2026-09-05 10:13
更新日時: 2026-09-05 11:12

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

## 操作

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
| V | Blender素材と従来の仮素材を比較 |
| Q / E（長押し） | カメラを左右へ連続回転（毎秒90°） |
| マウス中ボタンドラッグ | カメラを水平に連続回転 |
| Space | 戦場では進軍・一時停止、素材確認では歩行再生・停止 |
| Home | 部隊位置とアニメーションを初期化 |
| Z / C | 素材確認画面で兵士の向きを変更 |
| 1 / 2 / 3 | 表示兵士数を1,000 / 5,000 / 10,000体へ変更 |
| ウィンドウのサイズ変更 | 描画領域と投影の縦横比を更新 |
| Esc | 終了 |

タイトルに素材の種類、表示モード、兵士数、ループのfps、操作案内を表示する。兵士数を増やすと同じ範囲に密集させるため、負荷確認用の配置になる。実際の部隊の兵力や隊列の仕様ではない。

## 自動検証と画像保存

- `scene_invariants`：表示人数、接地、全64歩行タイルの透過・画像端と各方向8フレームの差異、回転後の投影・移動、進軍・停止・目的地到達・無効入力、移動・停止命令と回転・ズーム・Pan後の地形クリックを確認。
- `dx12_smoke`：非表示ウィンドウで5フレーム描画し、描画領域の縮小・復元、Pan / Zoom、Scene再転送、GPUからの画像読み戻しを確認。戦場ではWin32メッセージによる部隊選択・移動・H停止・選択解除も確認。
- `dx12_inspect`：8方向×2陣営を表示し、`build/inspect.bmp`に保存。キー長押しの連続回転、左右同時押しの相殺、キー解放・フォーカス喪失時の停止、中ボタンドラッグも検証する。
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
- `src/simulation.cpp`：2部隊の進軍、一時停止、移動・停止命令、目的地での停止。表示兵士数から独立した状態更新。
- `src/scene.cpp`：地形・Atlas・各素材の配置、部隊位置とカメラ方位に応じた兵士位置・歩行フレームの更新。
- `src/renderer.cpp`：DX12の初期化、深度バッファ、Atlas転送、Instancing、リサイズ、画像保存。
- `shaders/battlefield.hlsl`：地形とSpriteの描画。実行時にコンパイルし、ビルド時に実行ファイルの隣へコピーする。

座標系はY-upの左手系。仮素材は地面に立つ板、Blender製の兵士は画面に平行な板で表示し、透過部分を破棄して深度テストを行う。両者ともカメラ回転に追従する。木・旗・陣幕は1方向の仮素材のため、裏側の形状は再現しない。Atlasは64×64タイルを12列×9行に配置した768×576。PNGの読み込みは`src/sprite_sheet.cpp`のWIC処理で行う。詳細は[素材生成手順](asset_pipeline.md)を参照。

通常描画では兵士のInstanceデータを毎フレーム更新し、地形やTextureは再転送しない。初期状態では既定の目的地へ進軍し、右クリックで選択部隊の移動先を変更できる。選択は地形上のクリック点と部隊中心の±14の範囲で判定し、重なった場合は近い中心を選ぶ。地形外のクリックは移動命令を出さない。隊列が地形をはみ出さないよう目的地の中心を±60へ制限する。経路探索・部隊間衝突・接触戦闘は未実装。

最小構成では毎フレームFenceの完了を待つ。頂点・InstanceデータはUpload Heapに保持する。フレーム並列化、描画数削減、転送方式の最適化は性能測定後に行う。表示されるfpsはVSyncやデバッグレイヤーの影響を受けるため、正式なベンチマーク値ではない。

通常起動の起動失敗はダイアログと実行ファイル隣の`error.log`で確認する。配布・移動するときは実行ファイルと`shaders/`、`assets/`を一緒に置く。

DX12の初期化と同期は[Microsoftの基本構成資料](https://learn.microsoft.com/en-us/windows/win32/direct3d12/creating-a-basic-direct3d-12-component)を参照し、テクスチャの転送配置は[GetCopyableFootprints](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-getcopyablefootprints)に従っている。
