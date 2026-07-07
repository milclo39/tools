using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows;
using System.Windows.Input;
using Microsoft.Win32;
using UvcExtensionControlApp.Models;
using UvcExtensionControlApp.Services;

namespace UvcExtensionControlApp.ViewModels
{
    /// <summary>
    /// 汎用 RelayCommand。
    /// </summary>
    public class RelayCommand : ICommand
    {
        private readonly Action _execute;
        private readonly Func<bool> _canExecute;

        public RelayCommand(Action execute, Func<bool> canExecute = null)
        {
            _execute = execute;
            _canExecute = canExecute;
        }

        public event EventHandler CanExecuteChanged
        {
            add { CommandManager.RequerySuggested += value; }
            remove { CommandManager.RequerySuggested -= value; }
        }

        public bool CanExecute(object parameter)
        {
            return _canExecute == null || _canExecute();
        }

        public void Execute(object parameter)
        {
            _execute();
        }
    }

    public class MainViewModel : INotifyPropertyChanged, IDisposable
    {
        private readonly DeviceEnumerationService _enumService = new DeviceEnumerationService();
        private readonly UvcExtensionUnitService _xuService = new UvcExtensionUnitService();

        public MainViewModel()
        {
            Devices = new ObservableCollection<CameraDeviceInfo>();
            LogEntries = new ObservableCollection<CommandLogEntry>();

            RefreshDevicesCommand = new RelayCommand(RefreshDevices);
            ConnectCommand = new RelayCommand(Connect, () => SelectedDevice != null);
            SetCurCommand = new RelayCommand(ExecuteSetCur, () => IsConnected);
            GetCurCommand = new RelayCommand(ExecuteGetCur, () => IsConnected);
            QuickTestCommand = new RelayCommand(ExecuteQuickTest, () => IsConnected);
            EnumerateNodesCommand = new RelayCommand(ExecuteEnumerateNodes, () => IsConnected);
            ExportLogCommand = new RelayCommand(ExportLog, () => LogEntries.Count > 0);
            CopyLogCommand = new RelayCommand(CopyLog, () => LogEntries.Count > 0);
            ClearLogCommand = new RelayCommand(() => LogEntries.Clear(), () => LogEntries.Count > 0);

            RefreshDevices();
        }

        #region Bindable properties

        public ObservableCollection<CameraDeviceInfo> Devices { get; private set; }
        public ObservableCollection<CommandLogEntry> LogEntries { get; private set; }

        private CameraDeviceInfo _selectedDevice;
        public CameraDeviceInfo SelectedDevice
        {
            get { return _selectedDevice; }
            set { _selectedDevice = value; OnPropertyChanged("SelectedDevice"); }
        }

        private bool _isConnected;
        public bool IsConnected
        {
            get { return _isConnected; }
            private set
            {
                _isConnected = value;
                OnPropertyChanged("IsConnected");
                OnPropertyChanged("ConnectionStatus");
            }
        }

        public string ConnectionStatus
        {
            get { return IsConnected ? "接続済み (IKsControl 取得済み)" : "未接続"; }
        }

        private string _extensionGuidText = UvcExtensionUnitService.DefaultExtensionGuid.ToString().ToUpperInvariant();
        public string ExtensionGuidText
        {
            get { return _extensionGuidText; }
            set { _extensionGuidText = value; OnPropertyChanged("ExtensionGuidText"); }
        }

        private string _nodeIdText = "0x04";
        public string NodeIdText
        {
            get { return _nodeIdText; }
            set { _nodeIdText = value; OnPropertyChanged("NodeIdText"); }
        }

        private string _controlSelectorText = "0x0E";
        public string ControlSelectorText
        {
            get { return _controlSelectorText; }
            set { _controlSelectorText = value; OnPropertyChanged("ControlSelectorText"); }
        }

        private string _setDataText = "01";
        public string SetDataText
        {
            get { return _setDataText; }
            set { _setDataText = value; OnPropertyChanged("SetDataText"); }
        }

        private string _getLengthText = "2";
        public string GetLengthText
        {
            get { return _getLengthText; }
            set { _getLengthText = value; OnPropertyChanged("GetLengthText"); }
        }

        private bool _useTopologyFlag;
        public bool UseTopologyFlag
        {
            get { return _useTopologyFlag; }
            set { _useTopologyFlag = value; OnPropertyChanged("UseTopologyFlag"); }
        }

        #endregion

        #region Commands

        public ICommand RefreshDevicesCommand { get; private set; }
        public ICommand ConnectCommand { get; private set; }
        public ICommand SetCurCommand { get; private set; }
        public ICommand GetCurCommand { get; private set; }
        public ICommand QuickTestCommand { get; private set; }
        public ICommand EnumerateNodesCommand { get; private set; }
        public ICommand ExportLogCommand { get; private set; }
        public ICommand CopyLogCommand { get; private set; }
        public ICommand ClearLogCommand { get; private set; }

        #endregion

        #region Command implementations

        private void RefreshDevices()
        {
            try
            {
                IsConnected = false;
                _xuService.Close();

                foreach (var d in Devices)
                {
                    d.Dispose();
                }
                Devices.Clear();

                var list = _enumService.EnumerateVideoInputDevices();
                foreach (var d in list)
                {
                    Devices.Add(d);
                }

                if (Devices.Count > 0)
                {
                    SelectedDevice = Devices[0];
                    AddInfoLog(string.Format("カメラデバイスを {0} 台検出しました。", Devices.Count));
                }
                else
                {
                    SelectedDevice = null;
                    AddInfoLog("カメラデバイスが見つかりませんでした。USB接続を確認してください。");
                }
            }
            catch (Exception ex)
            {
                AddErrorLog("デバイス列挙に失敗: " + ex.Message);
            }
        }

        private void Connect()
        {
            try
            {
                _xuService.Open(SelectedDevice);
                IsConnected = true;
                AddInfoLog(string.Format("「{0}」に接続し IKsControl を取得しました。", SelectedDevice.FriendlyName));
            }
            catch (Exception ex)
            {
                IsConnected = false;
                AddErrorLog("接続失敗: " + FormatException(ex));
            }
        }

        private void ExecuteSetCur()
        {
            Guid guid;
            uint nodeId, cs;
            byte[] data;
            if (!TryParseCommonInputs(out guid, out nodeId, out cs)) return;
            if (!TryParseHexBytes(SetDataText, out data))
            {
                AddErrorLog("送信データのHEX文字列が不正です (例: \"01\" や \"0A FF 03\")。");
                return;
            }

            try
            {
                int hr = _xuService.SetCur(guid, nodeId, cs, data, UseTopologyFlag);
                AddCommandLog("SET_CUR", nodeId, cs, BytesToHex(data), hr,
                    hr >= 0 ? "送信成功" : HResultMessage(hr));
            }
            catch (Exception ex)
            {
                AddErrorLog("SET_CUR 実行時例外: " + FormatException(ex));
            }
        }

        private void ExecuteGetCur()
        {
            Guid guid;
            uint nodeId, cs;
            if (!TryParseCommonInputs(out guid, out nodeId, out cs)) return;

            int length;
            if (!int.TryParse(GetLengthText.Trim(), out length) || length <= 0)
            {
                AddErrorLog("受信データ長が不正です (1以上の10進数を指定)。");
                return;
            }

            try
            {
                byte[] received;
                int hr = _xuService.GetCur(guid, nodeId, cs, length, UseTopologyFlag, out received);
                AddCommandLog("GET_CUR", nodeId, cs,
                    received != null ? BytesToHex(received) : "-",
                    hr,
                    hr >= 0 ? string.Format("受信成功 ({0} bytes)", received != null ? received.Length : 0)
                            : HResultMessage(hr));
            }
            catch (Exception ex)
            {
                AddErrorLog("GET_CUR 実行時例外: " + FormatException(ex));
            }
        }

        /// <summary>
        /// 動作確認用クイックテスト: Node=0x04, CS=0x0E, 受信長=2 の GET_CUR。
        /// 期待レスポンス: 64 00
        /// </summary>
        private void ExecuteQuickTest()
        {
            Guid guid;
            if (!Guid.TryParse(ExtensionGuidText.Trim(), out guid))
            {
                AddErrorLog("Extension Unit GUID の形式が不正です。");
                return;
            }

            try
            {
                byte[] received;
                int hr = _xuService.GetCur(guid, 0x04, 0x0E, 2, UseTopologyFlag, out received);
                string hex = received != null ? BytesToHex(received) : "-";
                string msg;
                if (hr >= 0)
                {
                    msg = hex == "64 00"
                        ? "クイックテスト成功: 期待値 64 00 と一致"
                        : string.Format("受信成功 (期待値 64 00 / 実際 {0})", hex);
                }
                else
                {
                    msg = "クイックテスト失敗: " + HResultMessage(hr);
                }
                AddCommandLog("GET_CUR", 0x04, 0x0E, hex, hr, msg);
            }
            catch (Exception ex)
            {
                AddErrorLog("クイックテスト実行時例外: " + FormatException(ex));
            }
        }

        private void ExecuteEnumerateNodes()
        {
            try
            {
                var nodes = _xuService.EnumerateNodes();
                AddInfoLog(string.Format("ノード数: {0}", nodes.Count));
                foreach (var n in nodes)
                {
                    AddInfoLog(string.Format("  Node {0}: {1}{2} Type={3}",
                        "0x" + n.NodeId.ToString("X2"),
                        string.IsNullOrEmpty(n.NodeName) ? "(名称なし)" : n.NodeName,
                        n.IsExtensionUnit ? " [Extension Unit]" : "",
                        n.NodeType.ToString().ToUpperInvariant()));
                }
                if (!nodes.Any(n => n.IsExtensionUnit))
                {
                    AddInfoLog("※ Extension Unit (KSNODETYPE_DEV_SPECIFIC) ノードが見つかりませんでした。");
                }
            }
            catch (Exception ex)
            {
                AddErrorLog("ノード列挙失敗: " + FormatException(ex));
            }
        }

        private void ExportLog()
        {
            var dialog = new SaveFileDialog
            {
                Filter = "テキストファイル (*.txt)|*.txt|すべてのファイル (*.*)|*.*",
                FileName = "uvc_xu_log_" + DateTime.Now.ToString("yyyyMMdd_HHmmss") + ".txt"
            };
            if (dialog.ShowDialog() != true) return;

            try
            {
                File.WriteAllText(dialog.FileName, BuildLogText(), Encoding.UTF8);
                AddInfoLog("ログをエクスポートしました: " + dialog.FileName);
            }
            catch (Exception ex)
            {
                AddErrorLog("ログ保存失敗: " + ex.Message);
            }
        }

        private void CopyLog()
        {
            try
            {
                Clipboard.SetText(BuildLogText());
                AddInfoLog("ログをクリップボードにコピーしました。");
            }
            catch (Exception ex)
            {
                AddErrorLog("クリップボードコピー失敗: " + ex.Message);
            }
        }

        private string BuildLogText()
        {
            var sb = new StringBuilder();
            sb.AppendLine("Timestamp\tDirection\tNodeId\tCS\tData\tResult\tMessage");
            foreach (var e in LogEntries)
            {
                sb.AppendLine(e.ToExportLine());
            }
            return sb.ToString();
        }

        #endregion

        #region Helpers

        private bool TryParseCommonInputs(out Guid guid, out uint nodeId, out uint cs)
        {
            guid = Guid.Empty;
            nodeId = 0;
            cs = 0;

            if (!Guid.TryParse(ExtensionGuidText.Trim(), out guid))
            {
                AddErrorLog("Extension Unit GUID の形式が不正です。");
                return false;
            }
            if (!TryParseHexUInt(NodeIdText, out nodeId))
            {
                AddErrorLog("Node ID の16進数指定が不正です (例: 0x04)。");
                return false;
            }
            if (!TryParseHexUInt(ControlSelectorText, out cs))
            {
                AddErrorLog("Control Selector の16進数指定が不正です (例: 0x0E)。");
                return false;
            }
            return true;
        }

        internal static bool TryParseHexUInt(string text, out uint value)
        {
            value = 0;
            if (string.IsNullOrWhiteSpace(text)) return false;
            string s = text.Trim();
            if (s.StartsWith("0x", StringComparison.OrdinalIgnoreCase))
            {
                s = s.Substring(2);
            }
            return uint.TryParse(s, NumberStyles.HexNumber, CultureInfo.InvariantCulture, out value);
        }

        internal static bool TryParseHexBytes(string text, out byte[] bytes)
        {
            bytes = null;
            if (string.IsNullOrWhiteSpace(text)) return false;

            // 区切り文字 (空白, カンマ, ハイフン) を除去
            string s = text.Replace(" ", "").Replace(",", "").Replace("-", "").Replace("\t", "");
            if (s.Length == 0 || s.Length % 2 != 0) return false;

            var result = new byte[s.Length / 2];
            for (int i = 0; i < result.Length; i++)
            {
                byte b;
                if (!byte.TryParse(s.Substring(i * 2, 2), NumberStyles.HexNumber, CultureInfo.InvariantCulture, out b))
                {
                    return false;
                }
                result[i] = b;
            }
            bytes = result;
            return true;
        }

        internal static string BytesToHex(byte[] bytes)
        {
            if (bytes == null || bytes.Length == 0) return "";
            return string.Join(" ", bytes.Select(b => b.ToString("X2")));
        }

        private static string HResultMessage(int hr)
        {
            try
            {
                var ex = Marshal.GetExceptionForHR(hr);
                return ex != null ? ex.Message : "不明なエラー";
            }
            catch
            {
                return "不明なエラー";
            }
        }

        private static string FormatException(Exception ex)
        {
            var comEx = ex as COMException;
            if (comEx != null)
            {
                return string.Format("{0} (HRESULT=0x{1:X8})", comEx.Message, comEx.ErrorCode);
            }
            return ex.Message;
        }

        private void AddCommandLog(string direction, uint nodeId, uint cs, string dataHex, int hr, string message)
        {
            LogEntries.Add(new CommandLogEntry
            {
                Timestamp = DateTime.Now,
                Direction = direction,
                NodeId = "0x" + nodeId.ToString("X2"),
                ControlSelector = "0x" + cs.ToString("X2"),
                DataHex = dataHex,
                Result = (hr >= 0 ? "OK" : "NG") + string.Format(" (0x{0:X8})", hr),
                Message = message
            });
        }

        private void AddInfoLog(string message)
        {
            LogEntries.Add(new CommandLogEntry
            {
                Timestamp = DateTime.Now,
                Direction = "INFO",
                NodeId = "-",
                ControlSelector = "-",
                DataHex = "-",
                Result = "-",
                Message = message
            });
        }

        private void AddErrorLog(string message)
        {
            LogEntries.Add(new CommandLogEntry
            {
                Timestamp = DateTime.Now,
                Direction = "ERROR",
                NodeId = "-",
                ControlSelector = "-",
                DataHex = "-",
                Result = "-",
                Message = message
            });
        }

        #endregion

        public event PropertyChangedEventHandler PropertyChanged;

        private void OnPropertyChanged(string name)
        {
            var handler = PropertyChanged;
            if (handler != null)
            {
                handler(this, new PropertyChangedEventArgs(name));
            }
        }

        public void Dispose()
        {
            _xuService.Dispose();
            foreach (var d in Devices)
            {
                d.Dispose();
            }
        }
    }
}
