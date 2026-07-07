using System;
using System.Runtime.InteropServices;

namespace UvcExtensionControlApp.Interop
{
    /// <summary>
    /// KSPROPERTY の Flags 値。
    /// </summary>
    internal static class KsPropertyFlags
    {
        public const uint KSPROPERTY_TYPE_GET = 0x00000001;
        public const uint KSPROPERTY_TYPE_SET = 0x00000002;
        public const uint KSPROPERTY_TYPE_TOPOLOGY = 0x10000000;
    }

    /// <summary>
    /// KSP_NODE 構造体。
    /// KSIDENTIFIER (Set: GUID 16byte, Id: uint, Flags: uint) の後に
    /// NodeId (uint), Reserved (uint) が続く。合計 32 byte。
    /// </summary>
    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    internal struct KSP_NODE
    {
        /// <summary>Property Set GUID (Extension Unit の GUID)</summary>
        public Guid Set;

        /// <summary>Property ID (UVC の Control Selector に対応)</summary>
        public uint Id;

        /// <summary>KSPROPERTY_TYPE_GET / KSPROPERTY_TYPE_SET 等</summary>
        public uint Flags;

        /// <summary>Node ID (UVC の Entity ID に対応)</summary>
        public uint NodeId;

        /// <summary>予約 (0)</summary>
        public uint Reserved;
    }

    /// <summary>
    /// IKsControl。
    /// Windows 標準 UVC ドライバ (usbvideo.sys) が Extension Unit への
    /// GET/SET 要求を中継する正規ルート。
    /// </summary>
    [ComImport]
    [Guid("28F54685-06FD-11D2-B27A-00A0C9223196")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IKsControl
    {
        [PreserveSig]
        int KsProperty(
            [In] ref KSP_NODE Property,
            [In] int PropertyLength,
            [In] IntPtr PropertyData,
            [In] int DataLength,
            out int BytesReturned);

        [PreserveSig]
        int KsMethod(
            [In] ref KSP_NODE Method,
            [In] int MethodLength,
            [In] IntPtr MethodData,
            [In] int DataLength,
            out int BytesReturned);

        [PreserveSig]
        int KsEvent(
            [In] ref KSP_NODE Event,
            [In] int EventLength,
            [In] IntPtr EventData,
            [In] int DataLength,
            out int BytesReturned);
    }
}
