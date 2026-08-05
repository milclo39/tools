# UVC Camera App (MVP)

USB接続のUVCカメラをプレビュー表示し、UVCコマンドでパラメータ操作できるAndroidアプリ。
仕様は `../android_uvc_camera_spec.md` 参照。

## 機能

- USBカメラ選択(製品名表示、複数接続時の切替、初回接続時にUSB権限ダイアログ)
- 解像度切替(VGA / 720p / 1080p / 4K のうちデバイスが対応するもののみ表示)
- Zoom / Focus (Auto・Manual) / Exposure / Brightness の調整
  (いずれも GET_MIN / GET_MAX / GET_DEF / GET_CUR をスライダー範囲・初期値に反映。非対応コントロールは無効表示)
- Exposure は内部でAEモードをManualへ切り替えてから値を送信(専用トグルなし)
- 音声付きMP4録画 (H.264映像 + AAC-LC 48 kHz/128 kbps モノラル音声)
  - 録画中はカメラ・解像度の切替をロックし、停止完了後にMP4を確定
  - Android 10以降は `Movies/UVC Camera` に保存

## 構成

- 単一Activity + XMLレイアウト(SurfaceView系)、Compose不使用
- `MainActivity` … UI制御
- `MainViewModel` … 状態保持 (LiveData)
- `UvcCameraController` … UVCカメラ操作をまとめたControllerクラス
- ライブラリ: [UVCAndroid 1.0.13](https://github.com/shiyinghan/UVCAndroid) (saki4510t/UVCCamera の保守フォーク、Maven Central配布)

## ビルド

- Android Studio でこのフォルダ (`UVCCameraApp`) を開いて Sync → Run
- compileSdk / targetSdk 36 (Android 16)、minSdk 24
- AGP 8.10.1 / Gradle 8.11.1 / Kotlin 2.0.21

### 注意: gradle-wrapper.jar について

ネットワーク制約により `gradle/wrapper/gradle-wrapper.jar` と `gradlew` スクリプトは同梱していません。
Android Studio は `gradle-wrapper.properties` の distributionUrl から自動でGradleを取得するため、IDEでのビルドには影響ありません。
コマンドラインビルドが必要な場合は、Gradleをインストール済みの環境で一度 `gradle wrapper` を実行してください。

## 動作要件

- USB Host対応のAndroid端末 (USB OTG)
- UVC準拠のUSBカメラ
- マイク権限 (`RECORD_AUDIO`。初回起動時にカメラ権限と合わせて確認)

## 録画の確認

1. カメラがプレビューできる状態で、`録画開始` を押す。
2. 表示が `● 録画中` になったことを確認してから講義を開始する。
3. 終了時は `録画停止` を押し、`録画を保存しました` が表示されるまでアプリを終了・カメラを取り外さない。

音声と映像は同一の録画セッションでAAC/H.264としてMP4へ多重化する。講義利用では、開始時と終了時に拍手など明確な同期音を入れ、90分録画後に再生して開始・終了の両方でずれが2〜3秒以内であることを実機ごとに確認する。
