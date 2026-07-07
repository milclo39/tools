using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using UvcExtensionControlApp.Interop;

namespace UvcExtensionControlApp.Services
{
    /// <summary>
    /// トポロジ内ノードの情報 (デバッグ表示用)。
    /// </summary>
    public class NodeInfo
    {
        public int NodeId { get; set; }
        public Guid NodeType { get; set; }
        public string NodeName { get; set; }
        public bool IsExtensionUnit { get; set; }
    }

    /// <summary>
    /// 選択したカメラの IBaseFilter をフィルタグラフに追加し、
    /// IKsControl 経由で Extension Unit への SET_CUR / GET_CUR を実行するサービス。
    /// </summary>
    public class UvcExtensionUnitService : IDisposable
    {
        /// <summary>今回対象デバイスの Extension Unit GUID (デフォルト値)</summary>
        public static readonly Guid DefaultExtensionGuid = new Guid("EA94E4B5-D4E3-47C8-B8B3-4E5DC89DEBB2");

        private object _filterObj;      // IBaseFilter (RCW)
        private object _graphObj;       // IGraphBuilder (RCW)
        private IKsControl _ksControl;  // _filterObj への QI 結果

        public bool IsOpen
        {
            get { return _ksControl != null; }
        }

        /// <summary>
        /// デバイスの IBaseFilter を取得してフィルタグラフに追加し、IKsControl を取得する。
        /// 失敗時は例外を送出する (メッセージに HRESULT を含む)。
        /// </summary>
        public void Open(CameraDeviceInfo device)
        {
            if (device == null || device.Moniker == null)
            {
                throw new ArgumentException("デバイスが選択されていません。");
            }

            Close();

            try
            {
                // 1. IBaseFilter の取得
                Guid iidBaseFilter = DirectShowGuids.IID_IBaseFilter;
                object filterObj;
                device.Moniker.BindToObject(null, null, ref iidBaseFilter, out filterObj);
                if (filterObj == null)
                {
                    throw new InvalidOperationException("IBaseFilter の取得に失敗しました (BindToObject が null を返しました)。");
                }
                _filterObj = filterObj;

                // 2. フィルタグラフへの追加
                var graphType = Type.GetTypeFromCLSID(DirectShowGuids.FilterGraph);
                _graphObj = Activator.CreateInstance(graphType);
                var graph = (IGraphBuilder)_graphObj;
                int hr = graph.AddFilter((IBaseFilter)_filterObj, device.FriendlyName ?? "Capture");
                if (hr < 0)
                {
                    throw new COMException(
                        string.Format("IGraphBuilder.AddFilter が失敗しました。HRESULT=0x{0:X8}", hr), hr);
                }

                // 3. IKsControl の取得 (QueryInterface)
                _ksControl = _filterObj as IKsControl;
                if (_ksControl == null)
                {
                    throw new InvalidOperationException(
                        "IKsControl を取得できませんでした。このデバイスは KS プロパティ要求に対応していない可能性があります " +
                        "(標準 UVC ドライバ usbvideo.sys で動作しているか確認してください)。");
                }
            }
            catch
            {
                Close();
                throw;
            }
        }

        /// <summary>
        /// SET_CUR を送信する。戻り値は HRESULT。
        /// </summary>
        public int SetCur(Guid extensionGuid, uint nodeId, uint controlSelector, byte[] data, bool useTopologyFlag)
        {
            EnsureOpen();
            if (data == null || data.Length == 0)
            {
                throw new ArgumentException("送信データが空です。");
            }

            uint flags = KsPropertyFlags.KSPROPERTY_TYPE_SET;
            if (useTopologyFlag)
            {
                flags |= KsPropertyFlags.KSPROPERTY_TYPE_TOPOLOGY;
            }

            var prop = new KSP_NODE
            {
                Set = extensionGuid,
                Id = controlSelector,
                Flags = flags,
                NodeId = nodeId,
                Reserved = 0
            };

            IntPtr buffer = Marshal.AllocHGlobal(data.Length);
            try
            {
                Marshal.Copy(data, 0, buffer, data.Length);
                int bytesReturned;
                return _ksControl.KsProperty(
                    ref prop,
                    Marshal.SizeOf(typeof(KSP_NODE)),
                    buffer,
                    data.Length,
                    out bytesReturned);
            }
            finally
            {
                Marshal.FreeHGlobal(buffer);
            }
        }

        /// <summary>
        /// GET_CUR を実行する。戻り値は HRESULT。成功時 received に受信バイト列が入る。
        /// </summary>
        public int GetCur(Guid extensionGuid, uint nodeId, uint controlSelector, int length, bool useTopologyFlag, out byte[] received)
        {
            EnsureOpen();
            if (length <= 0)
            {
                throw new ArgumentException("受信データ長は 1 以上を指定してください。");
            }

            uint flags = KsPropertyFlags.KSPROPERTY_TYPE_GET;
            if (useTopologyFlag)
            {
                flags |= KsPropertyFlags.KSPROPERTY_TYPE_TOPOLOGY;
            }

            var prop = new KSP_NODE
            {
                Set = extensionGuid,
                Id = controlSelector,
                Flags = flags,
                NodeId = nodeId,
                Reserved = 0
            };

            IntPtr buffer = Marshal.AllocHGlobal(length);
            try
            {
                int bytesReturned;
                int hr = _ksControl.KsProperty(
                    ref prop,
                    Marshal.SizeOf(typeof(KSP_NODE)),
                    buffer,
                    length,
                    out bytesReturned);

                if (hr >= 0)
                {
                    int copyLen = Math.Min(bytesReturned, length);
                    if (copyLen < 0) copyLen = 0;
                    received = new byte[copyLen];
                    Marshal.Copy(buffer, received, 0, copyLen);
                }
                else
                {
                    received = null;
                }
                return hr;
            }
            finally
            {
                Marshal.FreeHGlobal(buffer);
            }
        }

        /// <summary>
        /// IKsTopologyInfo でノード一覧を列挙する (デバッグ用)。
        /// </summary>
        public List<NodeInfo> EnumerateNodes()
        {
            EnsureOpen();

            var topo = _filterObj as IKsTopologyInfo;
            if (topo == null)
            {
                throw new InvalidOperationException("IKsTopologyInfo を取得できませんでした (ノード列挙非対応)。");
            }

            var list = new List<NodeInfo>();
            int numNodes;
            int hr = topo.get_NumNodes(out numNodes);
            if (hr < 0)
            {
                throw new COMException(
                    string.Format("get_NumNodes が失敗しました。HRESULT=0x{0:X8}", hr), hr);
            }

            for (int i = 0; i < numNodes; i++)
            {
                var node = new NodeInfo { NodeId = i };

                Guid nodeType;
                if (topo.get_NodeType(i, out nodeType) >= 0)
                {
                    node.NodeType = nodeType;
                    node.IsExtensionUnit = nodeType == KsNodeTypes.DevSpecific;
                }

                node.NodeName = TryGetNodeName(topo, i);
                list.Add(node);
            }

            return list;
        }

        private static string TryGetNodeName(IKsTopologyInfo topo, int nodeId)
        {
            const int bufChars = 256;
            IntPtr buf = Marshal.AllocHGlobal(bufChars * 2);
            try
            {
                int nameLen;
                int hr = topo.get_NodeName(nodeId, buf, bufChars * 2, out nameLen);
                if (hr >= 0 && nameLen > 0)
                {
                    string name = Marshal.PtrToStringUni(buf);
                    return string.IsNullOrEmpty(name) ? null : name;
                }
                return null;
            }
            catch
            {
                return null;
            }
            finally
            {
                Marshal.FreeHGlobal(buf);
            }
        }

        private void EnsureOpen()
        {
            if (_ksControl == null)
            {
                throw new InvalidOperationException("デバイスに接続されていません。先に「接続」を実行してください。");
            }
        }

        public void Close()
        {
            _ksControl = null; // _filterObj と同一 RCW のため個別解放は不要

            if (_graphObj != null)
            {
                try
                {
                    if (_filterObj != null)
                    {
                        ((IGraphBuilder)_graphObj).RemoveFilter((IBaseFilter)_filterObj);
                    }
                }
                catch
                {
                    // 解放時の失敗は無視
                }
                Marshal.ReleaseComObject(_graphObj);
                _graphObj = null;
            }

            if (_filterObj != null)
            {
                Marshal.ReleaseComObject(_filterObj);
                _filterObj = null;
            }
        }

        public void Dispose()
        {
            Close();
        }
    }
}
