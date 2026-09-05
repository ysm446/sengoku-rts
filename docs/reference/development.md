# 開発・起動・検証手順

作成日時: 2026-09-05 10:13
更新日時: 2026-09-05 10:21

## 必要環境

- Windows、DirectX 12に対応するGPU。
- Visual Studioの「C++によるデスクトップ開発」、Windows SDK、CMake 3.24以上。
- PowerShell。`tools/build.ps1`はWindows PowerShellで日本語が誤解釈されないようUTF-8 BOM付き。

この作業環境ではVisual Studio Community 2026、MSVC 19.51、Windows SDK 10.0.26100.0、CMake 4.3.1を使用した。GPUは高性能優先で選択する。ソフトウェア描画の確認には`--warp`を使える。

ユーザーがBlender 5.2をインストール済み。`C:/Program Files/Blender Foundation/Blender 5.2/blender.exe`で5.2.1 LTSを確認し、バックグラウンドでのPython実行も成功した。現在の仮素材はC++で生成するため、ゲームのビルド・起動にBlenderは不要。槍足軽の素材実験から使用する。

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

初期の描画領域は1600×1200ピクセル（4:3）。ウィンドウ枠・タイトルバーはこの外側に付く。通常のウィンドウサイズ変更にも対応する。

| 操作 | 内容 |
| --- | --- |
| WASD / 矢印 | カメラを画面の上下左右方向へ移動 |
| マウスホイール | 拡大・縮小 |
| R | 初期カメラへ戻す |
| 1 / 2 / 3 | 表示兵士数を1,000 / 5,000 / 10,000体へ変更 |
| ウィンドウのサイズ変更 | 描画領域と投影の縦横比を更新 |
| Esc | 終了 |

タイトルに兵士数、ループのfps、使用GPU、操作案内を表示する。兵士数を増やすと同じ範囲に密集させるため、負荷確認用の配置になる。実際の部隊の兵力や隊列の仕様ではない。

## 自動検証と画像保存

- `scene_invariants`：各兵士数での表示人数、地形との接地、Atlas範囲・透過、無効入力、カメラ範囲・投影を確認。
- `dx12_smoke`：非表示ウィンドウで5フレーム描画し、描画領域の縮小・復元、Pan / Zoom、Scene再転送、GPUからの画像読み戻しを確認。

`build/smoke.bmp`と`build/smoke.bmp.txt`に描画画像と結果を保存する。失敗時は、可能なら`build/smoke.bmp.error.log`へ理由を記録する。デバッグレイヤーが利用できる環境ではDX12の警告・エラーもテスト失敗として扱う。利用できない場合はレポートに明記する。

追加の確認例：

```powershell
$appPath = Join-Path (Get-Location).Path 'build/Release/sengoku_rts.exe'
$capturePath = Join-Path (Get-Location).Path 'build/smoke-10000.bmp'
& $appPath --smoke-test --soldiers 10000 --capture $capturePath
```

`--soldiers`は1000 / 5000 / 10000を受け付ける。`--capture`は通常起動にも指定でき、その場合は最初のフレームを保存してアプリを継続する。`--warp`でソフトウェア描画に切り替える。

## 実装の範囲と制限

- `src/main.cpp`：Win32ウィンドウ、入力、実行ループ、起動テスト。
- `src/camera.h`：固定角度のOrthographic Camera、Pan / Zoom。
- `src/scene.cpp`：起伏のある地形、仮Atlas、静的な兵士・旗・松・陣幕の配置。
- `src/renderer.cpp`：DX12の初期化、深度バッファ、Atlas転送、Instancing、リサイズ、画像保存。
- `shaders/battlefield.hlsl`：地形とSpriteの描画。実行時にコンパイルし、ビルド時に実行ファイルの隣へコピーする。

座標系はY-upの左手系。Spriteは地面に立つ板として固定カメラの横方向に揃え、透過部分を破棄して深度テストを行う。仮Atlasは32x48の4タイルで、本素材の解像度を決めたものではない。

最小構成では毎フレームFenceの完了を待つ。頂点・InstanceデータはUpload Heapに保持する。フレーム並列化、描画数削減、転送方式の最適化は性能測定後に行う。表示されるfpsはVSyncやデバッグレイヤーの影響を受けるため、正式なベンチマーク値ではない。

通常起動の起動失敗はダイアログと実行ファイル隣の`error.log`で確認する。配布・移動するときは実行ファイルと`shaders/`を一緒に置く。

DX12の初期化と同期は[Microsoftの基本構成資料](https://learn.microsoft.com/en-us/windows/win32/direct3d12/creating-a-basic-direct3d-12-component)を参照し、テクスチャの転送配置は[GetCopyableFootprints](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-getcopyablefootprints)に従っている。
