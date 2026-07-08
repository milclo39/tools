using System;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Windows.Forms;
using System.Xml;

using Microsoft.Win32.SafeHandles;

namespace USB_IR_sample
{
    public partial class Form1 : Form
    {
        private const int SlotCount = 32;
        private const int RemoteColumns = 4;
        private const int RemoteRows = 8;
        private const uint DefaultFrequency = 38000;
        private readonly IrSlot[] irSlots = new IrSlot[SlotCount];
        private readonly Button[] remoteButtons = new Button[SlotCount];
        private ToolTip slotToolTip;
        private bool learningMode = false;

        [DllImport("USB_IR_Library.dll")]
        public static extern SafeFileHandle openUSBIR(IntPtr hRecipient);
        [DllImport("USB_IR_Library.dll")]
        public static extern int closeUSBIR(SafeFileHandle HandleToUSBDevice);
        [DllImport("USB_IR_Library.dll")]
        public static extern int writeUSBIRData2(SafeFileHandle HandleToUSBDevice, uint freq, byte[] data, uint bit_len);
        [DllImport("USB_IR_Library.dll")]
        public static extern int recUSBIRData_Start(SafeFileHandle HandleToUSBDevice, uint freq);
        [DllImport("USB_IR_Library.dll")]
        public static extern int recUSBIRData_Stop(SafeFileHandle HandleToUSBDevice);
        [DllImport("USB_IR_Library.dll")]
        public static extern int readUSBIRData(SafeFileHandle HandleToUSBDevice, ref IntPtr data, ref uint data_len, ref uint bit_len);
        [DllImport("USB_IR_Library.dll")]
        public static extern int freeMem(ref IntPtr ptr);

        public Form1()
        {
            InitializeComponent();

            if (IntPtr.Size == 4)
            {
                this.Text += " 32ビット";
            }
            else if (IntPtr.Size == 8)
            {
                this.Text += " 64ビット";
            }

            slotToolTip = new ToolTip(this.components);
            InitializeIrSlots();
            BuildRemoteButtons();
            LoadIrSlots();
            RefreshRemoteButtons();
            UpdateModeView();
        }

        private class IrSlot
        {
            public int Index;
            public string Name;
            public uint Frequency;
            public uint DataLen;
            public uint BitLen;
            public byte[] Data;
            public string UpdatedAt;

            public bool HasData
            {
                get { return Data != null && DataLen > 0 && BitLen > 0; }
            }
        }

        private string SlotFilePath
        {
            get { return Path.Combine(Application.StartupPath, "ir_slots.json"); }
        }

        private string LegacySlotFilePath
        {
            get { return Path.Combine(Application.StartupPath, "ir_slots.xml"); }
        }

        private void InitializeIrSlots()
        {
            for (int i = 0; i < SlotCount; i++)
            {
                irSlots[i] = new IrSlot();
                irSlots[i].Index = i + 1;
                irSlots[i].Name = "";
                irSlots[i].Frequency = DefaultFrequency;
                irSlots[i].DataLen = 0;
                irSlots[i].BitLen = 0;
                irSlots[i].Data = new byte[0];
                irSlots[i].UpdatedAt = "";
            }
        }

        private void BuildRemoteButtons()
        {
            int buttonWidth = 56;
            int buttonHeight = 48;
            int gap = 8;

            pnl_remote.Controls.Clear();
            for (int i = 0; i < SlotCount; i++)
            {
                int row = i / RemoteColumns;
                int col = i % RemoteColumns;

                Button button = new Button();
                button.Name = "btn_remote_" + (i + 1).ToString();
                button.Tag = i;
                button.Location = new Point(col * (buttonWidth + gap), row * (buttonHeight + gap));
                button.Size = new Size(buttonWidth, buttonHeight);
                button.Font = new Font("Segoe UI", 16F, FontStyle.Bold, GraphicsUnit.Point);
                button.FlatStyle = FlatStyle.Standard;
                button.Click += new EventHandler(remoteButton_Click);
                remoteButtons[i] = button;
                pnl_remote.Controls.Add(button);
            }
        }

        private void RefreshRemoteButtons()
        {
            for (int i = 0; i < SlotCount; i++)
            {
                IrSlot slot = irSlots[i];
                Button button = remoteButtons[i];
                button.Text = slot.HasData ? GetFirstTextElement(slot.Name) : "";
                button.BackColor = slot.HasData ? SystemColors.Control : Color.WhiteSmoke;
                slotToolTip.SetToolTip(button, slot.HasData ? slot.Name : "");
            }
        }

        private static string GetFirstTextElement(string text)
        {
            if (text == null || text.Length == 0)
            {
                return "";
            }

            TextElementEnumerator enumerator = StringInfo.GetTextElementEnumerator(text);
            if (enumerator.MoveNext())
            {
                return enumerator.GetTextElement();
            }

            return "";
        }

        private void UpdateModeView()
        {
            btn_mode.Text = learningMode ? "学習モード" : "実行モード";
            btn_mode.BackColor = learningMode ? Color.Khaki : SystemColors.Control;
            lbl_status.Text = learningMode ? "学習モード: ボタンを押すと学習します" : "実行モード: ボタンを押すと送信します";
        }

        private void LoadIrSlots()
        {
            if (!File.Exists(SlotFilePath))
            {
                if (File.Exists(LegacySlotFilePath))
                {
                    LoadLegacyXmlSlots();
                    SaveIrSlots();
                }
                return;
            }

            try
            {
                string json = File.ReadAllText(SlotFilePath, Encoding.UTF8);
                string[] objects = SplitSlotObjects(json);
                for (int i = 0; i < objects.Length; i++)
                {
                    string item = objects[i];
                    int index = ReadJsonInt(item, "index", 0);
                    if (index < 1 || index > SlotCount)
                    {
                        continue;
                    }

                    IrSlot slot = irSlots[index - 1];
                    slot.Name = ReadJsonString(item, "name", "");
                    slot.Frequency = ReadJsonUInt(item, "frequency", DefaultFrequency);
                    slot.DataLen = ReadJsonUInt(item, "dataLen", 0);
                    slot.BitLen = ReadJsonUInt(item, "bitLen", 0);
                    slot.UpdatedAt = ReadJsonString(item, "updatedAt", "");

                    string dataText = ReadJsonString(item, "data", "");
                    if (dataText.Length > 0)
                    {
                        slot.Data = Convert.FromBase64String(dataText);
                        if (slot.DataLen == 0)
                        {
                            slot.DataLen = (uint)slot.Data.Length;
                        }
                    }
                    else
                    {
                        slot.Data = new byte[0];
                    }
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show("スロットファイルの読み込みに失敗しました。\n" + ex.Message, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void LoadLegacyXmlSlots()
        {
            try
            {
                XmlDocument doc = new XmlDocument();
                doc.Load(LegacySlotFilePath);
                XmlNodeList nodes = doc.SelectNodes("/IrSlots/Slot");

                foreach (XmlNode node in nodes)
                {
                    int index = ToInt(GetAttributeValue(node, "index"), 0);
                    if (index < 1 || index > SlotCount)
                    {
                        continue;
                    }

                    IrSlot slot = irSlots[index - 1];
                    slot.Name = GetNodeText(node, "Name", "");
                    slot.Frequency = ToUInt(GetNodeText(node, "Frequency", DefaultFrequency.ToString()), DefaultFrequency);
                    slot.DataLen = ToUInt(GetNodeText(node, "DataLen", "0"), 0);
                    slot.BitLen = ToUInt(GetNodeText(node, "BitLen", "0"), 0);
                    slot.UpdatedAt = GetNodeText(node, "UpdatedAt", "");

                    string dataText = GetNodeText(node, "Data", "");
                    if (dataText.Length > 0)
                    {
                        slot.Data = Convert.FromBase64String(dataText);
                        if (slot.DataLen == 0)
                        {
                            slot.DataLen = (uint)slot.Data.Length;
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show("旧スロットファイルの移行に失敗しました。\n" + ex.Message, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private static string GetAttributeValue(XmlNode node, string name)
        {
            if (node.Attributes == null || node.Attributes[name] == null)
            {
                return "";
            }
            return node.Attributes[name].Value;
        }

        private static string GetNodeText(XmlNode node, string name, string defaultValue)
        {
            XmlNode child = node.SelectSingleNode(name);
            return child == null ? defaultValue : child.InnerText;
        }

        private static uint ToUInt(string text, uint defaultValue)
        {
            try
            {
                return Convert.ToUInt32(text);
            }
            catch
            {
                return defaultValue;
            }
        }

        private static int ToInt(string text, int defaultValue)
        {
            try
            {
                return Convert.ToInt32(text);
            }
            catch
            {
                return defaultValue;
            }
        }

        private void SaveIrSlots()
        {
            StringBuilder sb = new StringBuilder();
            sb.AppendLine("{");
            sb.AppendLine("  \"version\": 1,");
            sb.AppendLine("  \"slots\": [");

            for (int i = 0; i < SlotCount; i++)
            {
                IrSlot slot = irSlots[i];
                int saveDataLen = slot.HasData ? Math.Min((int)slot.DataLen, slot.Data.Length) : 0;
                string dataText = saveDataLen > 0 ? Convert.ToBase64String(slot.Data, 0, saveDataLen) : "";

                sb.AppendLine("    {");
                sb.AppendLine("      \"index\": " + slot.Index.ToString() + ",");
                sb.AppendLine("      \"name\": \"" + EscapeJsonString(slot.Name) + "\",");
                sb.AppendLine("      \"frequency\": " + slot.Frequency.ToString() + ",");
                sb.AppendLine("      \"dataLen\": " + slot.DataLen.ToString() + ",");
                sb.AppendLine("      \"bitLen\": " + slot.BitLen.ToString() + ",");
                sb.AppendLine("      \"updatedAt\": \"" + EscapeJsonString(slot.UpdatedAt) + "\",");
                sb.AppendLine("      \"data\": \"" + dataText + "\"");
                sb.Append("    }");
                if (i < SlotCount - 1)
                {
                    sb.Append(",");
                }
                sb.AppendLine();
            }

            sb.AppendLine("  ]");
            sb.AppendLine("}");
            File.WriteAllText(SlotFilePath, sb.ToString(), Encoding.UTF8);
        }

        private static string EscapeJsonString(string text)
        {
            if (text == null)
            {
                return "";
            }

            StringBuilder sb = new StringBuilder();
            for (int i = 0; i < text.Length; i++)
            {
                char c = text[i];
                switch (c)
                {
                    case '\\':
                        sb.Append("\\\\");
                        break;
                    case '"':
                        sb.Append("\\\"");
                        break;
                    case '\b':
                        sb.Append("\\b");
                        break;
                    case '\f':
                        sb.Append("\\f");
                        break;
                    case '\n':
                        sb.Append("\\n");
                        break;
                    case '\r':
                        sb.Append("\\r");
                        break;
                    case '\t':
                        sb.Append("\\t");
                        break;
                    default:
                        if (c < 32)
                        {
                            sb.Append("\\u");
                            sb.Append(((int)c).ToString("x4"));
                        }
                        else
                        {
                            sb.Append(c);
                        }
                        break;
                }
            }

            return sb.ToString();
        }

        private static string UnescapeJsonString(string text)
        {
            StringBuilder sb = new StringBuilder();
            for (int i = 0; i < text.Length; i++)
            {
                char c = text[i];
                if (c != '\\' || i + 1 >= text.Length)
                {
                    sb.Append(c);
                    continue;
                }

                i++;
                char escaped = text[i];
                switch (escaped)
                {
                    case '"':
                    case '\\':
                    case '/':
                        sb.Append(escaped);
                        break;
                    case 'b':
                        sb.Append('\b');
                        break;
                    case 'f':
                        sb.Append('\f');
                        break;
                    case 'n':
                        sb.Append('\n');
                        break;
                    case 'r':
                        sb.Append('\r');
                        break;
                    case 't':
                        sb.Append('\t');
                        break;
                    case 'u':
                        if (i + 4 < text.Length)
                        {
                            string hex = text.Substring(i + 1, 4);
                            sb.Append((char)Convert.ToInt32(hex, 16));
                            i += 4;
                        }
                        break;
                    default:
                        sb.Append(escaped);
                        break;
                }
            }

            return sb.ToString();
        }

        private static string[] SplitSlotObjects(string json)
        {
            int slotsIndex = json.IndexOf("\"slots\"");
            if (slotsIndex < 0)
            {
                return new string[0];
            }

            int arrayStart = json.IndexOf('[', slotsIndex);
            if (arrayStart < 0)
            {
                return new string[0];
            }

            System.Collections.ArrayList objects = new System.Collections.ArrayList();
            bool inString = false;
            bool escaped = false;
            int depth = 0;
            int objectStart = -1;

            for (int i = arrayStart + 1; i < json.Length; i++)
            {
                char c = json[i];
                if (inString)
                {
                    if (escaped)
                    {
                        escaped = false;
                    }
                    else if (c == '\\')
                    {
                        escaped = true;
                    }
                    else if (c == '"')
                    {
                        inString = false;
                    }
                    continue;
                }

                if (c == '"')
                {
                    inString = true;
                }
                else if (c == '{')
                {
                    if (depth == 0)
                    {
                        objectStart = i;
                    }
                    depth++;
                }
                else if (c == '}')
                {
                    depth--;
                    if (depth == 0 && objectStart >= 0)
                    {
                        objects.Add(json.Substring(objectStart, i - objectStart + 1));
                        objectStart = -1;
                    }
                }
                else if (c == ']' && depth == 0)
                {
                    break;
                }
            }

            return (string[])objects.ToArray(typeof(string));
        }

        private static string ReadJsonString(string jsonObject, string propertyName, string defaultValue)
        {
            int valueStart = FindJsonValueStart(jsonObject, propertyName);
            if (valueStart < 0 || valueStart >= jsonObject.Length || jsonObject[valueStart] != '"')
            {
                return defaultValue;
            }

            StringBuilder raw = new StringBuilder();
            bool escaped = false;
            for (int i = valueStart + 1; i < jsonObject.Length; i++)
            {
                char c = jsonObject[i];
                if (escaped)
                {
                    raw.Append('\\');
                    raw.Append(c);
                    escaped = false;
                }
                else if (c == '\\')
                {
                    escaped = true;
                }
                else if (c == '"')
                {
                    return UnescapeJsonString(raw.ToString());
                }
                else
                {
                    raw.Append(c);
                }
            }

            return defaultValue;
        }

        private static int ReadJsonInt(string jsonObject, string propertyName, int defaultValue)
        {
            string number = ReadJsonNumberText(jsonObject, propertyName);
            if (number.Length == 0)
            {
                return defaultValue;
            }

            try
            {
                return Convert.ToInt32(number);
            }
            catch
            {
                return defaultValue;
            }
        }

        private static uint ReadJsonUInt(string jsonObject, string propertyName, uint defaultValue)
        {
            string number = ReadJsonNumberText(jsonObject, propertyName);
            if (number.Length == 0)
            {
                return defaultValue;
            }

            try
            {
                return Convert.ToUInt32(number);
            }
            catch
            {
                return defaultValue;
            }
        }

        private static string ReadJsonNumberText(string jsonObject, string propertyName)
        {
            int valueStart = FindJsonValueStart(jsonObject, propertyName);
            if (valueStart < 0)
            {
                return "";
            }

            int valueEnd = valueStart;
            while (valueEnd < jsonObject.Length)
            {
                char c = jsonObject[valueEnd];
                if ((c >= '0' && c <= '9') || c == '-')
                {
                    valueEnd++;
                }
                else
                {
                    break;
                }
            }

            return jsonObject.Substring(valueStart, valueEnd - valueStart);
        }

        private static int FindJsonValueStart(string jsonObject, string propertyName)
        {
            string token = "\"" + propertyName + "\"";
            int index = jsonObject.IndexOf(token);
            if (index < 0)
            {
                return -1;
            }

            int colon = jsonObject.IndexOf(':', index + token.Length);
            if (colon < 0)
            {
                return -1;
            }

            int valueStart = colon + 1;
            while (valueStart < jsonObject.Length && Char.IsWhiteSpace(jsonObject[valueStart]))
            {
                valueStart++;
            }

            return valueStart;
        }

        private bool StartRecording(uint frequency, out string errorMessage)
        {
            SafeFileHandle handle_usb_device = null;
            int i_ret = 0;
            errorMessage = "";

            try
            {
                handle_usb_device = openUSBIR(this.Handle);
                if (handle_usb_device == null || handle_usb_device.IsInvalid)
                {
                    errorMessage = "USB DEVICEをオープンできません。";
                    return false;
                }

                i_ret = recUSBIRData_Start(handle_usb_device, frequency);
                if (i_ret != 0)
                {
                    errorMessage = "受信開始に失敗しました。戻り値: " + i_ret.ToString();
                    return false;
                }

                return true;
            }
            catch (Exception ex)
            {
                errorMessage = ex.Message;
                return false;
            }
            finally
            {
                if (handle_usb_device != null)
                {
                    closeUSBIR(handle_usb_device);
                }
            }
        }

        private bool StopRecording(out string errorMessage)
        {
            SafeFileHandle handle_usb_device = null;
            int i_ret = 0;
            errorMessage = "";

            try
            {
                handle_usb_device = openUSBIR(this.Handle);
                if (handle_usb_device == null || handle_usb_device.IsInvalid)
                {
                    errorMessage = "USB DEVICEをオープンできません。";
                    return false;
                }

                i_ret = recUSBIRData_Stop(handle_usb_device);
                if (i_ret != 0)
                {
                    errorMessage = "受信停止に失敗しました。戻り値: " + i_ret.ToString();
                    return false;
                }

                return true;
            }
            catch (Exception ex)
            {
                errorMessage = ex.Message;
                return false;
            }
            finally
            {
                if (handle_usb_device != null)
                {
                    closeUSBIR(handle_usb_device);
                }
            }
        }

        private bool ReadRecordedData(out byte[] data, out uint dataLen, out uint bitLen, out string errorMessage)
        {
            SafeFileHandle handle_usb_device = null;
            IntPtr p_data = IntPtr.Zero;
            int i_ret = 0;

            data = new byte[0];
            dataLen = 0;
            bitLen = 0;
            errorMessage = "";

            try
            {
                handle_usb_device = openUSBIR(this.Handle);
                if (handle_usb_device == null || handle_usb_device.IsInvalid)
                {
                    errorMessage = "USB DEVICEをオープンできません。";
                    return false;
                }

                i_ret = readUSBIRData(handle_usb_device, ref p_data, ref dataLen, ref bitLen);
                if (i_ret != 0)
                {
                    errorMessage = "受信データ取得に失敗しました。戻り値: " + i_ret.ToString();
                    return false;
                }

                if (dataLen == 0 || bitLen == 0 || p_data == IntPtr.Zero)
                {
                    errorMessage = "受信データがありません。";
                    return false;
                }

                if (dataLen > (uint)int.MaxValue)
                {
                    errorMessage = "受信データが大きすぎます。";
                    return false;
                }

                int copyLen = (int)dataLen;
                data = new byte[copyLen];
                Marshal.Copy(p_data, data, 0, copyLen);
                return true;
            }
            catch (Exception ex)
            {
                errorMessage = ex.Message;
                return false;
            }
            finally
            {
                if (p_data != IntPtr.Zero)
                {
                    freeMem(ref p_data);
                }

                if (handle_usb_device != null)
                {
                    closeUSBIR(handle_usb_device);
                }
            }
        }

        private bool SendIrData(IrSlot slot, out string errorMessage)
        {
            SafeFileHandle handle_usb_device = null;
            int i_ret = 0;
            errorMessage = "";

            if (slot == null || !slot.HasData)
            {
                errorMessage = "未学習です。";
                return false;
            }

            try
            {
                handle_usb_device = openUSBIR(this.Handle);
                if (handle_usb_device == null || handle_usb_device.IsInvalid)
                {
                    errorMessage = "USB DEVICEをオープンできません。";
                    return false;
                }

                i_ret = writeUSBIRData2(handle_usb_device, slot.Frequency, slot.Data, slot.BitLen);
                if (i_ret != 0)
                {
                    errorMessage = "赤外線送信に失敗しました。戻り値: " + i_ret.ToString();
                    return false;
                }

                return true;
            }
            catch (Exception ex)
            {
                errorMessage = ex.Message;
                return false;
            }
            finally
            {
                if (handle_usb_device != null)
                {
                    closeUSBIR(handle_usb_device);
                }
            }
        }

        private void LearnSlot(int index)
        {
            IrSlot slot = irSlots[index];
            string currentName = slot.Name.Length == 0 ? "ボタン" + slot.Index.ToString() : slot.Name;
            string name = PromptName(currentName);
            string errorMessage;

            if (name == null)
            {
                return;
            }

            name = name.Trim();
            if (name.Length == 0)
            {
                MessageBox.Show("名称を入力してください。", "赤外線学習", MessageBoxButtons.OK, MessageBoxIcon.Information);
                return;
            }

            DialogResult result = MessageBox.Show("OKを押したあと、3秒以内にリモコンを受信部へ向けて送信してください。", "赤外線学習", MessageBoxButtons.OKCancel, MessageBoxIcon.Information);
            if (result != DialogResult.OK)
            {
                return;
            }

            SetRemoteButtonsEnabled(false);
            lbl_status.Text = "学習中...";
            Application.DoEvents();

            try
            {
                if (!StartRecording(DefaultFrequency, out errorMessage))
                {
                    MessageBox.Show(errorMessage, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }

                Thread.Sleep(3000);

                if (!StopRecording(out errorMessage))
                {
                    MessageBox.Show(errorMessage, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }

                byte[] data;
                uint dataLen;
                uint bitLen;
                if (!ReadRecordedData(out data, out dataLen, out bitLen, out errorMessage))
                {
                    MessageBox.Show(errorMessage, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }

                slot.Name = name;
                slot.Frequency = DefaultFrequency;
                slot.Data = data;
                slot.DataLen = dataLen;
                slot.BitLen = bitLen;
                slot.UpdatedAt = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss");

                SaveIrSlots();
                RefreshRemoteButtons();
                lbl_status.Text = "保存しました: " + slot.Name;
            }
            finally
            {
                SetRemoteButtonsEnabled(true);
                UpdateModeView();
            }
        }

        private void SetRemoteButtonsEnabled(bool enabled)
        {
            for (int i = 0; i < remoteButtons.Length; i++)
            {
                remoteButtons[i].Enabled = enabled;
            }
            btn_mode.Enabled = enabled;
        }

        private string PromptName(string defaultName)
        {
            using (Form dialog = new Form())
            using (Label label = new Label())
            using (TextBox textBox = new TextBox())
            using (Button okButton = new Button())
            using (Button cancelButton = new Button())
            {
                dialog.Text = "名称設定";
                dialog.FormBorderStyle = FormBorderStyle.FixedDialog;
                dialog.StartPosition = FormStartPosition.CenterParent;
                dialog.ClientSize = new Size(260, 104);
                dialog.MaximizeBox = false;
                dialog.MinimizeBox = false;
                dialog.ShowInTaskbar = false;

                label.Text = "ボタン名称";
                label.Location = new Point(12, 14);
                label.Size = new Size(230, 16);

                textBox.Location = new Point(12, 34);
                textBox.Size = new Size(236, 19);
                textBox.Text = defaultName;

                okButton.Text = "OK";
                okButton.DialogResult = DialogResult.OK;
                okButton.Location = new Point(92, 68);
                okButton.Size = new Size(74, 24);

                cancelButton.Text = "キャンセル";
                cancelButton.DialogResult = DialogResult.Cancel;
                cancelButton.Location = new Point(174, 68);
                cancelButton.Size = new Size(74, 24);

                dialog.Controls.Add(label);
                dialog.Controls.Add(textBox);
                dialog.Controls.Add(okButton);
                dialog.Controls.Add(cancelButton);
                dialog.AcceptButton = okButton;
                dialog.CancelButton = cancelButton;

                return dialog.ShowDialog(this) == DialogResult.OK ? textBox.Text : null;
            }
        }

        private void remoteButton_Click(object sender, EventArgs e)
        {
            Button button = sender as Button;
            if (button == null)
            {
                return;
            }

            int index = (int)button.Tag;
            IrSlot slot = irSlots[index];

            if (learningMode)
            {
                LearnSlot(index);
                return;
            }

            string errorMessage;
            if (SendIrData(slot, out errorMessage))
            {
                lbl_status.Text = "送信しました: " + slot.Name;
            }
            else
            {
                lbl_status.Text = errorMessage;
            }
        }

        private void btn_mode_Click(object sender, EventArgs e)
        {
            learningMode = !learningMode;
            UpdateModeView();
        }

        private void Form1_Load(object sender, EventArgs e)
        {
        }
    }
}
