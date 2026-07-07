# わんぽ — 犬の散歩コース記録アプリ

散歩中の軌跡をリアルタイム表示し、距離・時間を自動保存して統計で振り返れるAndroidアプリ。
仕様は `../dog_walk_app_spec.md` を参照。

## セットアップ

1. **Android Studio で開く**
   Android Studio (Koala 以降推奨) で `DogWalkApp` フォルダを開く。
   Gradle wrapper の生成を促された場合はそのまま実行する(初回同期時に自動解決されます)。

2. **Google Maps API キーを設定する**
   - [Google Cloud Console](https://console.cloud.google.com/) でプロジェクトを作成し、**Maps SDK for Android** を有効化してAPIキーを取得
   - プロジェクトルートの `local.properties` に1行追加:

   ```
   MAPS_API_KEY=あなたのAPIキー
   ```

   ※ `local.properties` はGit管理外にすること(キーの流出防止)。

3. **実行**
   実機での動作確認を推奨(GPSを使うためエミュレータでは軌跡が取れない。エミュレータの場合は Extended Controls > Location でルート再生が可能)。

## 構成

| パッケージ | 内容 |
|---|---|
| `data` | Room (WalkRecord / DAO / DB / Repository) |
| `tracking` | Foreground Service によるGPS記録、セッション状態 (WalkSessionManager) |
| `ui` | Compose 4画面 (ホーム / 散歩中 / 結果 / 統計) + ViewModel |
| `util` | 期間計算(週は月曜始まり)・表示フォーマッタ |

## 主な設計ポイント

- 軌跡はメモリ上のみ保持し、終了時に距離・時間などの数値だけをRoomに自動保存
- 精度25m超の測位点と2m未満の微小移動は距離計算から除外(GPSノイズ対策)
- `ACCESS_BACKGROUND_LOCATION` は不使用。フォアグラウンド中に location タイプの
  Foreground Service を開始することで、画面消灯中も記録を継続
- Android 13+ では通知権限 (`POST_NOTIFICATIONS`) を散歩開始時にまとめてリクエスト
