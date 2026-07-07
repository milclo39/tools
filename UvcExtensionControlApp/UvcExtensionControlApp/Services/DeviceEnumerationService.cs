using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;
using UvcExtensionControlApp.Interop;

namespace UvcExtensionControlApp.Services
{
    /// <summary>
    /// 列挙されたカメラデバイス 1 台分の情報。
    /// IMoniker を保持しており、Dispose で解放する。
    /// </summary>
    public class CameraDeviceInfo : IDisposable
    {
        public string FriendlyName { get; set; }
        public string DevicePath { get; set; }
        internal IMoniker Moniker { get; set; }

        public override string ToString()
        {
            return FriendlyName ?? "(名称不明)";
        }

        public void Dispose()
        {
            if (Moniker != null)
            {
                Marshal.ReleaseComObject(Moniker);
                Moniker = null;
            }
        }
    }

    /// <summary>
    /// ICreateDevEnum による CLSID_VideoInputDeviceCategory のデバイス列挙。
    /// </summary>
    public class DeviceEnumerationService
    {
        public List<CameraDeviceInfo> EnumerateVideoInputDevices()
        {
            var result = new List<CameraDeviceInfo>();

            var devEnumType = Type.GetTypeFromCLSID(DirectShowGuids.SystemDeviceEnum);
            if (devEnumType == null)
            {
                throw new InvalidOperationException("CLSID_SystemDeviceEnum の型を取得できませんでした。");
            }

            object devEnumObj = Activator.CreateInstance(devEnumType);
            var devEnum = (ICreateDevEnum)devEnumObj;
            IEnumMoniker enumMoniker = null;

            try
            {
                Guid category = DirectShowGuids.VideoInputDeviceCategory;
                int hr = devEnum.CreateClassEnumerator(ref category, out enumMoniker, 0);

                // S_FALSE (1) = 該当デバイスなし
                if (hr != 0 || enumMoniker == null)
                {
                    return result;
                }

                var monikers = new IMoniker[1];
                while (enumMoniker.Next(1, monikers, IntPtr.Zero) == 0)
                {
                    IMoniker moniker = monikers[0];
                    var info = new CameraDeviceInfo { Moniker = moniker };

                    object bagObj = null;
                    try
                    {
                        Guid iidPropertyBag = DirectShowGuids.IID_IPropertyBag;
                        moniker.BindToStorage(null, null, ref iidPropertyBag, out bagObj);
                        var bag = (IPropertyBag)bagObj;

                        info.FriendlyName = ReadBagString(bag, "FriendlyName");
                        info.DevicePath = ReadBagString(bag, "DevicePath");
                    }
                    catch
                    {
                        // プロパティが読めなくても列挙自体は続行する
                    }
                    finally
                    {
                        if (bagObj != null)
                        {
                            Marshal.ReleaseComObject(bagObj);
                        }
                    }

                    result.Add(info);
                }
            }
            finally
            {
                if (enumMoniker != null)
                {
                    Marshal.ReleaseComObject(enumMoniker);
                }
                Marshal.ReleaseComObject(devEnumObj);
            }

            return result;
        }

        private static string ReadBagString(IPropertyBag bag, string propName)
        {
            object value = null;
            int hr = bag.Read(propName, ref value, IntPtr.Zero);
            if (hr == 0 && value != null)
            {
                return value.ToString();
            }
            return null;
        }
    }
}
