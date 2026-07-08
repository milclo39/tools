# CameraControlDirectShow

DirectShow を使った Windows カメラ制御確認アプリです。Visual Studio 2019 以降で `CameraControlDirectShow.sln` を開いてビルドできます。

## 主な機能

- 外付け/内蔵カメラの選択
- VGA / 720p / 1080p / 4K の解像度切替
- DirectShow の実機レンジ取得
  - `IAMCameraControl`: Zoom / Focus / Exposure
  - `IAMVideoProcAmp`: Brightness
- Focus の Auto / Manual 切替
- カメラプレビュー

## ビルド

1. Visual Studio 2019 以降で `CameraControlDirectShow.sln` を開く
2. 構成を `Debug x64` または `Release x64` にする
3. `ビルド > ソリューションのビルド`

## 注意

4K はカメラとドライバが対応している場合だけ有効です。アプリは指定解像度に合う DirectShow メディアタイプを探し、MJPG を優先して設定します。対応形式が無い場合はステータス欄に失敗を表示します。

カメラが他アプリで使用中の場合や、Windows のカメラプライバシー設定でデスクトップアプリが無効の場合、プレビューが開始できないことがあります。
