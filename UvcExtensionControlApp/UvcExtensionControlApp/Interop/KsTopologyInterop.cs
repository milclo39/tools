using System;
using System.Runtime.InteropServices;

namespace UvcExtensionControlApp.Interop
{
    /// <summary>
    /// KSTOPOLOGY_CONNECTION 構造体。
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    internal struct KSTOPOLOGY_CONNECTION
    {
        public uint FromNode;
        public uint FromNodePin;
        public uint ToNode;
        public uint ToNodePin;
    }

    /// <summary>
    /// 既知のノードタイプ GUID。
    /// </summary>
    internal static class KsNodeTypes
    {
        /// <summary>KSNODETYPE_DEV_SPECIFIC — UVC Extension Unit はこのタイプで現れる</summary>
        public static readonly Guid DevSpecific = new Guid("941C7AC0-C559-11D0-8A2B-00A0C9255AC1");
    }

    /// <summary>
    /// IKsTopologyInfo。フィルタ内のノード (Entity) を列挙するデバッグ用途で使用。
    /// </summary>
    [ComImport]
    [Guid("720D4AC0-7533-11D0-A5D6-28DB04C10000")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IKsTopologyInfo
    {
        [PreserveSig]
        int get_NumCategories(out int pdwNumCategories);

        [PreserveSig]
        int get_Category([In] int dwIndex, out Guid pCategory);

        [PreserveSig]
        int get_NumConnections(out int pdwNumConnections);

        [PreserveSig]
        int get_ConnectionInfo([In] int dwIndex, out KSTOPOLOGY_CONNECTION pConnectionInfo);

        [PreserveSig]
        int get_NodeName(
            [In] int dwNodeId,
            [In] IntPtr pwchNodeName,
            [In] int dwBufSize,
            out int pdwNameLen);

        [PreserveSig]
        int get_NumNodes(out int pdwNumNodes);

        [PreserveSig]
        int get_NodeType([In] int dwNodeId, out Guid pNodeType);

        [PreserveSig]
        int CreateNodeInstance(
            [In] int dwNodeId,
            [In] ref Guid iid,
            out IntPtr ppvObject);
    }
}
