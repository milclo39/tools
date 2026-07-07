using System;

namespace UvcExtensionControlApp.Models
{
    /// <summary>
    /// 送受信ログの 1 エントリ。
    /// </summary>
    public class CommandLogEntry
    {
        public DateTime Timestamp { get; set; }

        /// <summary>SET_CUR / GET_CUR / INFO / ERROR など</summary>
        public string Direction { get; set; }

        public string NodeId { get; set; }

        public string ControlSelector { get; set; }

        /// <summary>送信または受信データの HEX 表現</summary>
        public string DataHex { get; set; }

        /// <summary>HRESULT (成功時 "OK (0x00000000)" 等)</summary>
        public string Result { get; set; }

        public string Message { get; set; }

        public string TimestampText
        {
            get { return Timestamp.ToString("HH:mm:ss.fff"); }
        }

        public string ToExportLine()
        {
            return string.Format("{0}\t{1}\t{2}\t{3}\t{4}\t{5}\t{6}",
                Timestamp.ToString("yyyy-MM-dd HH:mm:ss.fff"),
                Direction, NodeId, ControlSelector, DataHex, Result, Message);
        }
    }
}
