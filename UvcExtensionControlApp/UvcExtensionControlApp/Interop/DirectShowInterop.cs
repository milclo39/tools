using System;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;

namespace UvcExtensionControlApp.Interop
{
    /// <summary>
    /// DirectShow 関連の CLSID / IID 定義。
    /// </summary>
    internal static class DirectShowGuids
    {
        /// <summary>CLSID_SystemDeviceEnum</summary>
        public static readonly Guid SystemDeviceEnum = new Guid("62BE5D10-60EB-11D0-BD3B-00A0C911CE86");

        /// <summary>CLSID_VideoInputDeviceCategory</summary>
        public static readonly Guid VideoInputDeviceCategory = new Guid("860BB310-5D01-11D0-BD3B-00A0C911CE86");

        /// <summary>CLSID_FilterGraph</summary>
        public static readonly Guid FilterGraph = new Guid("E436EBB3-524F-11CE-9F53-0020AF0BA770");

        /// <summary>IID_IBaseFilter</summary>
        public static readonly Guid IID_IBaseFilter = new Guid("56A86895-0AD4-11CE-B03A-0020AF0BA770");

        /// <summary>IID_IPropertyBag</summary>
        public static readonly Guid IID_IPropertyBag = new Guid("55272A00-42CB-11CE-8135-00AA004BB851");
    }

    /// <summary>
    /// ICreateDevEnum (デバイスカテゴリ列挙)
    /// </summary>
    [ComImport]
    [Guid("29840822-5B84-11D0-BD3B-00A0C911CE86")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface ICreateDevEnum
    {
        [PreserveSig]
        int CreateClassEnumerator([In] ref Guid pType, out IEnumMoniker ppEnumMoniker, [In] int dwFlags);
    }

    /// <summary>
    /// IPropertyBag (モニカのプロパティ読み出し用)
    /// </summary>
    [ComImport]
    [Guid("55272A00-42CB-11CE-8135-00AA004BB851")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IPropertyBag
    {
        [PreserveSig]
        int Read(
            [In, MarshalAs(UnmanagedType.LPWStr)] string pszPropName,
            [In, Out] ref object pVar,
            [In] IntPtr pErrorLog);

        [PreserveSig]
        int Write(
            [In, MarshalAs(UnmanagedType.LPWStr)] string pszPropName,
            [In] ref object pVar);
    }

    /// <summary>
    /// IBaseFilter (IPersist → IMediaFilter → IBaseFilter の全vtable順で定義)
    /// 本アプリでは QueryInterface のターゲットおよび AddFilter の引数としてのみ使用する。
    /// </summary>
    [ComImport]
    [Guid("56A86895-0AD4-11CE-B03A-0020AF0BA770")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IBaseFilter
    {
        // --- IPersist ---
        [PreserveSig]
        int GetClassID(out Guid pClassID);

        // --- IMediaFilter ---
        [PreserveSig]
        int Stop();

        [PreserveSig]
        int Pause();

        [PreserveSig]
        int Run([In] long tStart);

        [PreserveSig]
        int GetState([In] int dwMilliSecsTimeout, out int filtState);

        [PreserveSig]
        int SetSyncSource([In] IntPtr pClock);

        [PreserveSig]
        int GetSyncSource(out IntPtr pClock);

        // --- IBaseFilter ---
        [PreserveSig]
        int EnumPins(out IntPtr ppEnum);

        [PreserveSig]
        int FindPin([In, MarshalAs(UnmanagedType.LPWStr)] string Id, out IntPtr ppPin);

        [PreserveSig]
        int QueryFilterInfo([In] IntPtr pInfo);

        [PreserveSig]
        int JoinFilterGraph([In] IntPtr pGraph, [In, MarshalAs(UnmanagedType.LPWStr)] string pName);

        [PreserveSig]
        int QueryVendorInfo([MarshalAs(UnmanagedType.LPWStr)] out string pVendorInfo);
    }

    /// <summary>
    /// IGraphBuilder (IFilterGraph → IGraphBuilder の全vtable順で定義)
    /// 本アプリでは AddFilter / RemoveFilter のみ使用する。
    /// </summary>
    [ComImport]
    [Guid("56A868A9-0AD4-11CE-B03A-0020AF0BA770")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IGraphBuilder
    {
        // --- IFilterGraph ---
        [PreserveSig]
        int AddFilter([In] IBaseFilter pFilter, [In, MarshalAs(UnmanagedType.LPWStr)] string pName);

        [PreserveSig]
        int RemoveFilter([In] IBaseFilter pFilter);

        [PreserveSig]
        int EnumFilters(out IntPtr ppEnum);

        [PreserveSig]
        int FindFilterByName([In, MarshalAs(UnmanagedType.LPWStr)] string pName, out IBaseFilter ppFilter);

        [PreserveSig]
        int ConnectDirect([In] IntPtr ppinOut, [In] IntPtr ppinIn, [In] IntPtr pmt);

        [PreserveSig]
        int Reconnect([In] IntPtr ppin);

        [PreserveSig]
        int Disconnect([In] IntPtr ppin);

        [PreserveSig]
        int SetDefaultSyncSource();

        // --- IGraphBuilder ---
        [PreserveSig]
        int Connect([In] IntPtr ppinOut, [In] IntPtr ppinIn);

        [PreserveSig]
        int Render([In] IntPtr ppinOut);

        [PreserveSig]
        int RenderFile(
            [In, MarshalAs(UnmanagedType.LPWStr)] string lpcwstrFile,
            [In, MarshalAs(UnmanagedType.LPWStr)] string lpcwstrPlayList);

        [PreserveSig]
        int AddSourceFilter(
            [In, MarshalAs(UnmanagedType.LPWStr)] string lpcwstrFileName,
            [In, MarshalAs(UnmanagedType.LPWStr)] string lpcwstrFilterName,
            out IBaseFilter ppFilter);

        [PreserveSig]
        int SetLogFile([In] IntPtr hFile);

        [PreserveSig]
        int Abort();

        [PreserveSig]
        int ShouldOperationContinue();
    }
}
