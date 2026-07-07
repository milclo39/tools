package com.example.uvccamera

import android.hardware.usb.UsbDevice
import android.util.Log
import com.herohan.uvcapp.CameraHelper
import com.herohan.uvcapp.ICameraHelper
import com.serenegiant.usb.Size

/**
 * UVCカメラ操作をまとめたControllerクラス。
 * CameraHelper(UVCAndroidライブラリ)のラッパー。
 * USB権限リクエストはライブラリが selectDevice() 内で自動的に行う。
 */
class UvcCameraController(private val listener: Listener) {

    companion object {
        private const val TAG = "UvcCameraController"

        /** 解像度候補: VGA / 720p / 1080p / 4K */
        val RESOLUTION_CANDIDATES = listOf(
            640 to 480,
            1280 to 720,
            1920 to 1080,
            3840 to 2160
        )

        /** MJPEGフレームタイプ (UVC記述子 subtype: FORMAT_MJPEG=6, FRAME_MJPEG=7) */
        private fun isMjpeg(type: Int) = type == 6 || type == 7
    }

    /** min/max/def/cur と対応可否を持つコントロール情報 */
    data class ControlRange(
        val enabled: Boolean,
        val min: Int = 0,
        val max: Int = 0,
        val def: Int = 0,
        val cur: Int = 0
    )

    /** カメラオープン時に取得する全コントロール状態 */
    data class ControlStates(
        val zoom: ControlRange,
        val focusAutoEnabled: Boolean,
        val focusAuto: Boolean,
        val focus: ControlRange,
        val exposure: ControlRange,
        val brightness: ControlRange
    )

    interface Listener {
        /** デバイスの接続/切断でリストが変化した */
        fun onDeviceListChanged(devices: List<UsbDevice>)

        /** カメラオープン完了。対応解像度・現在解像度・コントロール範囲を通知 */
        fun onCameraOpened(
            device: UsbDevice,
            supportedSizes: List<Size>,
            currentSize: Size?,
            controls: ControlStates
        )

        fun onCameraClosed()
    }

    private var cameraHelper: ICameraHelper? = null
    private var previewSurface: Any? = null

    var selectedDevice: UsbDevice? = null
        private set

    private val stateCallback = object : ICameraHelper.StateCallback {
        override fun onAttach(device: UsbDevice) {
            Log.d(TAG, "onAttach: ${device.deviceName}")
            notifyDeviceList()
            // 未選択なら自動選択(1台のみ接続時の自動選択に相当)
            if (selectedDevice == null) {
                selectDevice(device)
            }
        }

        override fun onDeviceOpen(device: UsbDevice, isFirstOpen: Boolean) {
            Log.d(TAG, "onDeviceOpen")
            cameraHelper?.openCamera()
        }

        override fun onCameraOpen(device: UsbDevice) {
            val helper = cameraHelper ?: return
            Log.i(TAG, "onCameraOpen: size=${helper.previewSize}, " +
                    "supported=${helper.supportedSizeList}, surface=${previewSurface != null}")
            helper.startPreview()
            previewSurface?.let { helper.addSurface(it, false) }
            listener.onCameraOpened(
                device,
                getSupportedResolutions(),
                helper.previewSize,
                readControlStates()
            )
        }

        override fun onCameraClose(device: UsbDevice) {
            Log.d(TAG, "onCameraClose")
            previewSurface?.let { cameraHelper?.removeSurface(it) }
            listener.onCameraClosed()
        }

        override fun onDeviceClose(device: UsbDevice) {
            Log.d(TAG, "onDeviceClose")
        }

        override fun onDetach(device: UsbDevice) {
            Log.d(TAG, "onDetach: ${device.deviceName}")
            if (device == selectedDevice) {
                selectedDevice = null
                listener.onCameraClosed()
            }
            notifyDeviceList()
        }

        override fun onCancel(device: UsbDevice) {
            // USB権限が拒否された場合
            Log.d(TAG, "onCancel (permission denied)")
            if (device == selectedDevice) {
                selectedDevice = null
            }
        }
    }

    /** 初期化。Activity/ViewModel生成時に一度呼ぶ */
    fun init() {
        if (cameraHelper == null) {
            cameraHelper = CameraHelper().apply {
                setStateCallback(stateCallback)
            }
        }
    }

    fun release() {
        cameraHelper?.release()
        cameraHelper = null
        selectedDevice = null
    }

    /** 接続中のUVCデバイス一覧(ライブラリがVideoクラスでフィルタ済み) */
    fun getDeviceList(): List<UsbDevice> =
        cameraHelper?.deviceList ?: emptyList()

    private fun notifyDeviceList() {
        listener.onDeviceListChanged(getDeviceList())
    }

    /**
     * デバイス選択。既にオープン中なら閉じてから再接続する。
     * 初回はライブラリがUSB権限ダイアログを表示する。
     */
    fun selectDevice(device: UsbDevice) {
        if (device == selectedDevice) return
        val helper = cameraHelper ?: return
        Log.i(TAG, "selectDevice: ${device.deviceName} (${device.productName})")
        if (helper.isCameraOpened) {
            helper.closeCamera()
        }
        selectedDevice = device
        helper.selectDevice(device)
    }

    /** プレビュー用Surfaceの設定/解除 */
    fun setSurface(surface: Any?) {
        val helper = cameraHelper
        if (surface == null) {
            previewSurface?.let { helper?.removeSurface(it) }
        } else if (helper?.isCameraOpened == true) {
            helper.addSurface(surface, false)
        }
        previewSurface = surface
    }

    /**
     * 対応解像度一覧。UVC記述子由来のサイズリストから
     * VGA/720p/1080p/4K のうちデバイスが対応するもののみ返す。
     * 同一解像度が複数フォーマットにある場合はMJPEGを優先。
     */
    fun getSupportedResolutions(): List<Size> {
        val all = cameraHelper?.supportedSizeList ?: return emptyList()
        return RESOLUTION_CANDIDATES.mapNotNull { (w, h) ->
            val matches = all.filter { it.width == w && it.height == h }
            matches.firstOrNull { isMjpeg(it.type) } ?: matches.firstOrNull()
        }
    }

    /** 解像度切替。ストリームを停止→再ネゴシエーション→再開 */
    fun setResolution(size: Size) {
        val helper = cameraHelper ?: return
        if (!helper.isCameraOpened) return
        helper.stopPreview()
        helper.previewSize = size
        helper.startPreview()
    }

    /** 起動時にGET_MIN/MAX/DEF/CURを取得してUIへ反映するための一括読み出し */
    private fun readControlStates(): ControlStates {
        val ctrl = cameraHelper?.getUVCControl()
            ?: return ControlStates(
                ControlRange(false), false, false,
                ControlRange(false), ControlRange(false), ControlRange(false)
            )

        fun range(enabled: Boolean, limits: IntArray?, cur: () -> Int): ControlRange {
            if (!enabled || limits == null || limits.size < 3 || limits[0] >= limits[1]) {
                return ControlRange(false)
            }
            val current = runCatching(cur).getOrDefault(limits[2])
            return ControlRange(true, limits[0], limits[1], limits[2], current)
        }

        // Zoom Absolute (CT, CS=0x0B)
        val zoom = range(
            ctrl.isZoomAbsoluteEnable,
            runCatching { ctrl.updateZoomAbsoluteLimit() }.getOrNull()
        ) { ctrl.zoomAbsolute }

        // Focus Auto (CT, CS=0x08)
        val focusAutoEnabled = ctrl.isFocusAutoEnable
        val focusAuto = focusAutoEnabled && runCatching { ctrl.focusAuto }.getOrDefault(false)

        // Focus Absolute (CT, CS=0x06)
        val focus = range(
            ctrl.isFocusAbsoluteEnable,
            runCatching { ctrl.updateFocusAbsoluteLimit() }.getOrNull()
        ) { ctrl.focusAbsolute }

        // Exposure Time Absolute (CT, CS=0x04)
        val exposure = range(
            ctrl.isExposureTimeAbsoluteEnable,
            runCatching { ctrl.updateExposureTimeAbsoluteLimit() }.getOrNull()
        ) { ctrl.exposureTimeAbsolute }

        // Brightness (PU, CS=0x02, signed)
        val brightness = range(
            ctrl.isBrightnessEnable,
            runCatching { ctrl.updateBrightnessLimit() }.getOrNull()
        ) { ctrl.brightness }

        return ControlStates(zoom, focusAutoEnabled, focusAuto, focus, exposure, brightness)
    }

    // ---- コントロール設定 ----

    fun setZoom(value: Int) {
        runCatching { cameraHelper?.getUVCControl()?.setZoomAbsolute(value) }
    }

    fun setFocusAuto(auto: Boolean) {
        runCatching { cameraHelper?.getUVCControl()?.setFocusAuto(auto) }
    }

    fun setFocus(value: Int) {
        runCatching { cameraHelper?.getUVCControl()?.setFocusAbsolute(value) }
    }

    /**
     * Exposure設定。多くのデバイスはAEモードがManualでないと
     * 手動値を受け付けないため、内部でManualへ切り替えてから送信する。
     */
    fun setExposure(value: Int) {
        val ctrl = cameraHelper?.getUVCControl() ?: return
        runCatching {
            if (ctrl.isExposureTimeAuto) {
                ctrl.setExposureTimeAuto(false)
            }
            ctrl.exposureTimeAbsolute = value
        }.onFailure { Log.w(TAG, "setExposure failed", it) }
    }

    fun setBrightness(value: Int) {
        runCatching { cameraHelper?.getUVCControl()?.setBrightness(value) }
    }
}
