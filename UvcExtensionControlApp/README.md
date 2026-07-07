# UVC Extension Unit 制御テストツール

USB Video Class (UVC) カメラの Extension Unit に対して、Windows 標準 UVC ドライバ
(`usbvideo.sys`) を使ったまま SET_CUR / GET_CUR を送受信するテストアプリです。
DirectShow の `IKsControl` インターフェース経由で KSPROPERTY 要求を発行します
(WinUSB/libusb によるドライバ差し替えは行いません)。

## 動作環境

- Windows 10 / 11
- .NET Framework 4.8 (Windows 10 1903 以降は標準搭載)
- Visual Studio 2019 以降 (「.NET デスクトップ開発」ワークロードのみで可。.NET SDK は不要)

## ビルド方法

### Visual Studio

1. `UvcExtensionControlApp.sln` を Visual Studio 2019 以降で開く
2. ビルド → ソリューションのビルド (Ctrl+Shift+B)

### コマンドライン (Developer Command Prompt)

```
msbuild UvcExtensionControlApp.sln /p:Configuration=Release
```

出力先: `UvcExtensionControlApp\bin\Release\UvcExtensionControlApp.exe`

## 使い方

1. 対象の UVC カメラを USB 接続する
2. **他のカメラアプリ (Windows カメラ、Teams、Zoom 等) をすべて終了する**
   — デバイスが他アプリに掴まれていると `KsProperty` が失敗します
3. アプリを起動し、ドロップダウンからカメラを選択して「接続」を押す
   - 成功すると「接続済み (IKsControl 取得済み)」と表示される
4. 「ノード一覧」で Extension Unit (KSNODETYPE_DEV_SPECIFIC) の Node ID を確認できる
5. パラメータを設定してコマンドを実行する

| 項目 | デフォルト値 | 対応する UVC 要素 |
|---|---|---|
| XU GUID | `EA94E4B5-D4E3-47C8-B8B3-4E5DC89DEBB2` | Extension Unit の guidExtensionCode |
| Node ID | `0x04` | Entity ID (wIndex 上位バイト) |
| CS | `0x0E` | Control Selector (wValue 上位バイト) |

### SET_CUR (送信)

送信データを HEX 文字列で入力 (例: `01`、複数バイトは `0A FF 03`) し、
「送信 (SET_CUR)」を押す。結果 (HRESULT) がログに表示されます。

### GET_CUR (受信)

受信データ長 (バイト数、10進) を入力し「受信 (GET_CUR)」を押す。
受信バイト列が HEX でログに表示されます。

### クイックテスト

「クイックテスト」ボタンは Node=0x04, CS=0x0E, 受信長=2 の GET_CUR を
実行し、期待レスポンス `64 00` との一致を自動判定します (実機動作確認用)。

### ログ

すべての送受信 (時刻・種別・Node・CS・データHEX・HRESULT・メッセージ) が
画面下部に蓄積されます。「コピー」でクリップボードへ、「エクスポート」で
タブ区切りテキストとして保存できます。

## UVC コントロール転送と KSPROPERTY の対応

| UVC 制御転送の要素 | KSPROPERTY 側 |
|---|---|
| Entity ID | `KSP_NODE.NodeId` |
| Control Selector | `KSP_NODE.Property.Id` |
| Extension Unit GUID | `KSP_NODE.Property.Set` |
| SET_CUR | `KsProperty` + `KSPROPERTY_TYPE_SET` |
| GET_CUR | `KsProperty` + `KSPROPERTY_TYPE_GET` |
| Data | `KsProperty` のデータバッファ |

※ 一部環境・デバイスでは `KSPROPERTY_TYPE_TOPOLOGY` フラグの併用が必要な
場合があります。コマンドが失敗する場合は「TOPOLOGYフラグ付与」に
チェックを入れて再試行してください。

## GUID / Entity ID の調べ方

対象カメラの Extension Unit GUID と Entity ID は USB ディスクリプタから確認できます。

1. **USB Device Tree Viewer** (USBTreeView) をダウンロードして起動
   <https://www.uwe-sieber.de/usbtreeview_e.html>
2. 対象カメラをツリーから選択し、ディスクリプタ表示の
   **Video Control Interface Descriptor** 内の **Extension Unit Descriptor** を探す
   - `guidExtensionCode` → 本アプリの「XU GUID」
   - `bUnitID` → 本アプリの「Node ID (Entity ID)」
3. Control Selector (CS) と データ内容はデバイスベンダーの仕様書に従う

このほか、本アプリの「ノード一覧」ボタンでも、接続中デバイスの
Extension Unit ノードの有無と Node ID を確認できます
(DirectShow の Node ID は通常 UVC の Entity ID と対応しますが、
一致しない実装もあるため、失敗時はノード一覧の ID を試してください)。

## トラブルシューティング

- **「IKsControl を取得できませんでした」**
  → デバイスが標準 UVC ドライバで動作しているか確認 (デバイスマネージャーで
  ドライバが `usbvideo.sys` か確認)。仮想カメラでは取得できません。
- **KsProperty が失敗する (HRESULT 0x8007001F 等)**
  → 他のカメラアプリを終了する / TOPOLOGYフラグを試す /
  Node ID・CS・データ長がデバイス仕様と一致しているか確認。
- **デバイスが一覧に出ない**
  → USB 接続とデバイスマネージャーの「カメラ」カテゴリを確認。
  「更新」ボタンで再列挙できます。

## プロジェクト構成

```
UvcExtensionControlApp/
├── UvcExtensionControlApp.sln
├── UvcExtensionControlApp/
│   ├── App.xaml / App.xaml.cs / App.config
│   ├── MainWindow.xaml / MainWindow.xaml.cs
│   ├── Properties/AssemblyInfo.cs
│   ├── Interop/
│   │   ├── DirectShowInterop.cs   … ICreateDevEnum / IBaseFilter / IGraphBuilder 等
│   │   ├── KsControlInterop.cs    … IKsControl / KSP_NODE / フラグ定義
│   │   └── KsTopologyInterop.cs   … IKsTopologyInfo (ノード列挙デバッグ用)
│   ├── Services/
│   │   ├── DeviceEnumerationService.cs … カメラデバイス列挙
│   │   └── UvcExtensionUnitService.cs  … SET_CUR / GET_CUR 実処理
│   ├── ViewModels/
│   │   └── MainViewModel.cs
│   └── Models/
│       └── CommandLogEntry.cs
└── README.md
```
