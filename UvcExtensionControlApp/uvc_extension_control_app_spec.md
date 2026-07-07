# UVC Extension Unit 制御テストアプリ 実装指示書

## 目的

USB Video Class (UVC) 準拠のカメラデバイスに対して、Windows標準UVCドライバを使ったまま、
Extension Unit の Class-specific Control Request (SET_CUR / GET_CUR) を送受信できる
Windows デスクトップのテストアプリケーションを作成する。

対象コマンドの例（今回実装したいものの実体）:

```
[Send]
bmRequestType = 0x21  (Host->Device, Class, Interface)
bRequest      = 0x01  (SET_CUR)
wValue        = 0x0E00 (Control Selector = 0x0E, 予約バイト = 0x00)
wIndex        = 0x0400 (Entity ID = 0x04, Interface = 0)
wLength       = 0x0001
Data          = 01

[Recv]
bmRequestType = 0xA1  (Device->Host, Class, Interface)
bRequest      = 0x81  (GET_CUR)
wValue        = 0x0E00
wIndex        = 0x0400
wLength       = 0x0002
Data          = 64 00
```

このデバイスの Extension Unit GUID は以下で確定している。

```
EA94E4B5-D4E3-47C8-B8B3-4E5DC89DEBB2
```

## 技術方針（重要・必ず踏襲すること）

- **DirectShow の標準COM API (`IAMCameraControl` / `IAMVideoProcAmp` 等) は使用しない。**
  これらはUVC標準コントロールしか扱えず、Extension Unit には対応できない。
- 代わりに **`IKsControl` インターフェース経由の KSPROPERTY 要求**を使う。
  これはWindowsの標準UVCミニドライバ (`usbvideo.sys`) が Extension Unit への
  GET/SET要求をそのまま中継してくれる正規のルートであり、デバイスドライバの
  差し替えは不要。
- USB制御転送を直接送るための WinUSB/libusb 化は**行わない**（標準ドライバの
  カメラ機能を破壊するため、今回の要件には合わない）。

### KSPROPERTY へのマッピング

| UVCコントロール転送の要素 | KSPROPERTY側 |
|---|---|
| Entity ID (今回は 0x04) | `KSP_NODE.NodeId` |
| Control Selector (今回は 0x0E) | `KSP_NODE.Property.Id` |
| Extension Unit の GUID | `KSP_NODE.Property.Set` = `EA94E4B5-D4E3-47C8-B8B3-4E5DC89DEBB2` |
| SET_CUR | `IKsControl::KsProperty` を `KSPROPERTY_TYPE_SET` フラグで呼ぶ |
| GET_CUR | `IKsControl::KsProperty` を `KSPROPERTY_TYPE_GET` フラグで呼ぶ |
| Data | `KsProperty` のバッファ引数 |

## 実装言語・フレームワーク

- **言語**: C# (.NET 8, Windows専用)
- **UI**: WPF （最小構成のテストツールでよい。凝ったデザインは不要）
- **COMインターロップ**: `IKsControl` / `IBaseFilter` / `ICreateDevEnum` などは
  .NET標準ライブラリに存在しないため、**すべて手動でCOMインターフェース定義
  (`[ComImport][Guid(...)][InterfaceType(...)]`) を書くこと**。
  DirectShowLib 等の外部NuGetパッケージは、UVC拡張ユニットに関する部分の
  信頼性が低いため使用せず、自前でインターフェース定義する方針とする。

## 必要な機能要件

1. **デバイス列挙**
   - `ICreateDevEnum` を使って `CLSID_VideoInputDeviceCategory` のカメラデバイス一覧を取得し、
     UIのドロップダウンに表示・選択できるようにする。

2. **フィルタグラフへの追加と `IKsControl` 取得**
   - 選択したデバイスの `IBaseFilter` を取得し、`IGraphBuilder` に追加した上で
     `QueryInterface` により `IKsControl` を取得する。
   - `IKsControl` が取得できない場合は明確なエラーメッセージを表示する。

3. **Entity(Node) の確認（任意だが推奨）**
   - `IKsTopologyInfo` を使ってフィルタ内のノード一覧・GUIDを列挙し、
     指定した Entity ID (Node ID) が実際に Extension Unit として存在するかを
     確認できるデバッグ表示を用意する（トラブルシュート用）。

4. **SET_CUR 送信 UI**
   - 入力項目:
     - Node ID (Entity ID) … 16進数入力、デフォルト `0x04`
     - Control Selector (CS) … 16進数入力、デフォルト `0x0E`
     - 送信データ（バイト列） … 16進数文字列で入力（例: `01`）
   - 「送信 (SET_CUR)」ボタンで `KsProperty` を `KSPROPERTY_TYPE_SET` で実行する。
   - 実行結果（成功/失敗、HRESULT）をログ表示する。

5. **GET_CUR 受信 UI**
   - 入力項目:
     - Node ID (Entity ID)
     - Control Selector (CS)
     - 受信データ長（バイト数） … 例: `2`
   - 「受信 (GET_CUR)」ボタンで `KsProperty` を `KSPROPERTY_TYPE_GET` で実行し、
     取得したバイト列を16進数表示する。

6. **ログ／通信履歴表示**
   - 送受信内容（Node ID, CS, 方向, データHEX, HRESULT, タイムスタンプ）を
     リスト形式でUI上に蓄積表示する。コピー用にテキストとしてエクスポートできると良い。

7. **エラーハンドリング**
   - `IKsControl` 未取得、`KsProperty` の HRESULT 失敗、デバイスが物理的に
     Extension Unit を持たない場合など、各失敗ケースで例外を握りつぶさず
     HRESULTとメッセージをそのままログに出す。

## プロジェクト構成の指示

以下のような構成で新規ソリューションを作成すること。

```
UvcExtensionControlApp/
├── UvcExtensionControlApp.sln
├── UvcExtensionControlApp/              (WPFアプリ本体)
│   ├── App.xaml / App.xaml.cs
│   ├── MainWindow.xaml / MainWindow.xaml.cs
│   ├── Interop/
│   │   ├── DirectShowInterop.cs         (ICreateDevEnum, IBaseFilter, IGraphBuilder等の定義)
│   │   ├── KsControlInterop.cs          (IKsControl, KSPROPERTY, KSP_NODE等の定義)
│   │   └── KsTopologyInterop.cs         (IKsTopologyInfo等、任意)
│   ├── Services/
│   │   ├── DeviceEnumerationService.cs  (カメラデバイス列挙)
│   │   └── UvcExtensionUnitService.cs   (SET_CUR/GET_CURの実処理をラップ)
│   ├── ViewModels/
│   │   └── MainViewModel.cs
│   └── Models/
│       └── CommandLogEntry.cs
└── README.md                             (ビルド方法・使い方・GUID/EntityIDの調べ方を記載)
```

- `UvcExtensionUnitService` に、今回のGUID `EA94E4B5-D4E3-47C8-B8B3-4E5DC89DEBB2` を
  デフォルト値として持たせつつ、UI側から変更可能にしておくこと（他デバイス転用のため）。

## 実装時の注意点（Claude Codeへの技術的補足）

- `KSP_NODE` 構造体は `KSIDENTIFIER`(Property: `KSIDENTIFIER` = Set(GUID) + Id(uint) + Flags(uint)) の後に
  `NodeId(uint)` と `Reserved(uint)` を続けた構造。`KsProperty` 呼び出し時の
  `PropertyLength` は `sizeof(KSP_NODE)`、データバッファは別に確保して渡す。
- `IKsControl.KsProperty` のシグネチャは概ね以下（要検証・実装時に正確な定義を確認すること）:
  ```csharp
  [PreserveSig]
  int KsProperty(
      ref KSP_NODE property,
      int propertyLength,
      IntPtr propertyData,
      int dataLength,
      out int bytesReturned);
  ```
- SET/GET の区別は `KSIDENTIFIER.Flags` に `KSPROPERTY_TYPE_SET (0x2)` または
  `KSPROPERTY_TYPE_GET (0x1)` を設定することで行う（`TOPOLOGY` フラグ等は不要）。
- COMオブジェクトは使用後に確実に `Marshal.ReleaseComObject` すること。
- 管理者権限は基本不要だが、デバイスが他アプリ（カメラアプリ等）に排他的に
  掴まれていると `KsProperty` が失敗するので、テスト時は他のカメラアプリを
  終了させておく旨をREADMEに明記する。
- 実機での動作確認時、まず GET_CUR (CS=0x0E, NodeId=0x04) を送って
  レスポンス `64 00` が返ることを確認できる導線をUI上に用意すること
  （今回のやり取りにある実データでの動作確認用）。

## ビルド・実行環境

- Windows 10/11、.NET 8 SDK、Visual Studio 2019でビルド可能なこと。
- 実行には対象UVCカメラをUSB接続しておく必要がある。

## 完了条件（Definition of Done）

1. アプリを起動し、接続中のUVCカメラを一覧から選択できる。
2. Node ID=0x04, CS=0x0E, Data=`01` でSET_CURが送信でき、成功/失敗がログに表示される。
3. Node ID=0x04, CS=0x0E, 受信長=2 でGET_CURが送信でき、レスポンスのHEXデータがログに表示される。
4. すべての送受信ログが履歴として画面に残り、確認できる。
5. README.md に、ビルド方法・実行方法・GUID/EntityIDの調べ方（USBTreeView等の利用）が記載されている。
