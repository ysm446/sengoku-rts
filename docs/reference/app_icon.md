# アプリ用アイコン

作成日時: 2026-09-05 11:16
更新日時: 2026-09-05 11:18

兜と赤・青の軍旗を使い、墨色・朱赤・金のレトロなPixel Art調で制作した。内蔵image_genを使用。Windowsの実行ファイルとウィンドウの大小アイコンへ組み込み済み。

- 採用画像: `assets/icons/sengoku-rts-v2.png`
- Windows形式: `assets/icons/sengoku-rts-v2.ico`（16 / 24 / 32 / 48 / 64 / 128 / 256ピクセル）
- v1は生成初稿。角に市松模様が描かれていたため、v2では不透明な墨色の背景へ修正した。
- ICOはPillowで形式変換し、収録サイズを再読み込みして確認した。

## 組み込みと検証

- `src/app.rc.in`からCMakeでResourceファイルを生成し、`src/resource.h`の`IDI_SENGOKU_APP`としてICOを埋め込む。ICO変更時もResourceを再ビルドする。
- `src/main.cpp`で埋め込みResourceを読み込み、ウィンドウクラスの`hIcon` / `hIconSm`へ設定する。実行時に外部ICOファイルは不要。
- Debug / Releaseのビルドと各4件のCTestが成功。起動時に大小アイコンの読み込み成功を確認する。
- Release実行ファイルから32pxアイコンを抽出し、生成した兜の図柄が表示されることを目視確認した。実際のタスクバー上の表示は未確認。

## 初回生成プロンプト

```text
Use case: logo-brand
Asset type: Windows desktop application icon for Sengoku RTS, a Japanese Sengoku battlefield simulator with retro pixel soldiers and Japanese battle-screen painting aesthetics.
Create one polished square app icon, 1024x1024. A bold front-facing stylized Japanese kabuto helmet with broad gold kuwagata crest, dark ink-black bowl and muted vermilion layered neck armor. Two simple small sashimono war banners rise behind it, one vermilion and one muted indigo, expressing opposing formations. Strong iconic silhouette and very few large readable shapes, centered and filling 80 percent of frame. Refined retro strategy-game painted pixel-art aesthetic: crisp stepped contours, limited palette, broad pixel clusters, subtly aged gold and lacquer surfaces, restrained texture, no tiny ornamental noise. A simple dark charcoal rounded-square tile with a muted antique-gold edge, genuinely transparent pixels outside its rounded corners. Head-on emblem composition, no human face or skull. Calm, dignified historical Japanese battle-screen mood. Strong contrast readable at 32x32. One finished icon only, no mockup, no grid, no lettering, no kanji, no watermark, no actual clan mon, no swords, no photorealistic 3D render, no neon, no surrounding scenery.
```

## 最終修正プロンプト

```text
Edit this app icon. Preserve the entire helmet, banners, gold border, all colors, pixel art, composition and proportions exactly. Change ONLY the white/light gray checkerboard areas outside the rounded gold border: replace those corners with a uniform solid ink charcoal color #101719, so the image is a full opaque square with dark corners. No checkerboard anywhere. No other changes. This is the final Windows application icon.
```
