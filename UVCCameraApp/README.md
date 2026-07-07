# UVC Camera App (MVP)

USB接続のUVCカメラをプレビュー表示し、UVCコマンドでパラメータ操作できるAndroidアプリ。
仕様は `../android_uvc_camera_spec.md` 参照。

## 機能

- USBカメラ選択(製品名表示、複数接続時の切替、初回接続時にUSB権限ダイアログ)
- 解像度切替(VGA / 720p / 1080p / 4K のうちデバイスが対応するもののみ表示)
- Zoom / Focus (Auto・Manual) / Exposure / Brightness の調整
  (いずれも GET_MIN / GET_MAX / GET_DEF / GET_CUR をスライダー範囲・初期値に反映。非対応コントロールは無効表示)
- Exposure は内部でAEモードをManualへ切り替えてから値を送信(専用トグルなし)

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
