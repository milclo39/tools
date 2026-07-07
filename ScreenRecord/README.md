# ScreenRecord

解説動画用の画面キャプチャアプリ。常に最前面の小型ツールバーから、画面録画 (MP4) と静止画キャプチャ (PNG) を行います。

## 機能

- 録画トグルボタン (● / ■) を持つフローティングツールバー (ドラッグで移動可)
- 画面録画: H.264 (libx264) + Opus (libopus) の MP4 出力
- 静止画キャプチャ: PNG 出力
- 設定: 録画モニター / マイク (dshow デバイス) / フレームレート (15・30・60fps)
- 保存先はアプリ (exe) と同じフォルダ。ファイル名は `yyyyMMdd_HHmmss.mp4(.png)`、重複時は `_2` 以降を付与
- ツールバー自体は録画・スクリーンショットに写り込みません (Windows 10 2004 以降、`SetWindowDisplayAffinity` 使用)

## 必要なもの

1. **Qt 6.2.4** (MSVC 2019 キット) + Qt Creator
2. **ffmpeg.exe** — 録画エンジンとしてサブプロセス起動します
   - <https://www.gyan.dev/ffmpeg/builds/> の「release essentials」ビルドを推奨 (libx264 / libopus 同梱)
   - zip 内 `bin/ffmpeg.exe` を **ビルドした ScreenRecord.exe と同じフォルダ** に置いてください

## ビルド

Qt Creator で `ScreenRecord.pro` を開き、MSVC 2019 (Qt 6.2.4) キットでビルドするだけです。
実行ファイルはビルドフォルダに生成されるので、そこに `ffmpeg.exe` をコピーしてください。
録画・PNG・`settings.ini` はすべて exe と同じフォルダに生成されます。

## 内部動作 (録画時の ffmpeg コマンド)

```
ffmpeg -f gdigrab -framerate 30 -offset_x X -offset_y Y -video_size WxH -i desktop
       -f dshow -i audio="マイク名"
       -c:v libx264 -preset veryfast -crf 23 -pix_fmt yuv420p
       -c:a libopus -b:a 128k -ar 48000
       -strict -2 -movflags +faststart 20260707_123456.mp4
```

停止時は ffmpeg の標準入力に `q` を送り、moov アトムを確実に書き込んで正常終了させます。

## ライセンスに関する注記

- **Opus**: ロイヤリティフリー。ライセンス面で最も安全な音声コーデックです。
- **H.264**: コーデック自体に特許プールがありますが、ffmpeg.exe を別プロセスとして利用する構成のため、本アプリのコードには GPL の libx264 がリンクされません。ffmpeg.exe は各自でダウンロードして配置する形を想定しています。
- 特許リスクを完全に避けたい場合は、`recorder.cpp` のエンコード指定を `libvpx-vp9` + `libopus` (WebM) に変えるのが最も確実です。

## 既知の注意点

- **Opus in MP4** は ffmpeg 上 experimental 扱いのため `-strict -2` を付けています。VLC / Chrome / Edge では再生できますが、古いプレイヤー (旧 Windows Media Player 等) では音が出ないことがあります。互換性重視なら `recorder.cpp` の `libopus` を `aac` に変更してください。
- モニターごとに拡大率が異なるマルチモニター環境 (混在 DPI) では、キャプチャ範囲の座標がずれる場合があります。その場合は Windows の設定で拡大率を揃えてください。
- マイク一覧は `ffmpeg -list_devices` (DirectShow) から取得します。一覧が空の場合は設定ダイアログの「マイク一覧を更新」を押してください。
