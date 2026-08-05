package com.example.uvccamera

import android.content.ContentResolver
import android.content.ContentValues
import android.hardware.usb.UsbDevice
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.provider.MediaStore
import android.util.Log
import com.herohan.uvcapp.CameraHelper
import com.herohan.uvcapp.ICameraHelper
import com.herohan.uvcapp.VideoCapture
import com.serenegiant.usb.Size
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

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

        fun onRecordingStarting()

        fun onRecordingStarted()

        fun onRecordingStopping()

        fun onRecordingSaved(uri: Uri?)

        fun onRecordingError(message: String)
    }

    private var cameraHelper: ICameraHelper? = null
    private var previewSurface: Any? = null
    private var recordingRequested = false
    private var stopRequested = false

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
            stopRecordingForCameraClose()
            previewSurface?.let { cameraHelper?.removeSurface(it) }
            listener.onCameraClosed()
        }

        override fun onDeviceClose(device: UsbDevice) {
            Log.d(TAG, "onDeviceClose")
        }

        override fun onDetach(device: UsbDevice) {
            Log.d(TAG, "onDetach: ${device.deviceName}")
            if (device == selectedDevice) {
                stopRecordingForCameraClose()
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
            val helper = CameraHelper()
            configureRecording(helper, null)
            helper.setStateCallback(stateCallback)
            cameraHelper = helper
        }
    }

    fun release() {
        stopRecording()
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
        if (recordingRequested) {
            Log.w(TAG, "Ignoring resolution change while recording")
            return
        }
        configureRecording(helper, size)
        helper.stopPreview()
        helper.previewSize = size
        helper.startPreview()
    }

    /**
     * MP4(H.264 + AAC)録画を開始する。
     * 音声と映像はUVCAndroid内で同じMediaMuxerへ書き込まれるため、別ファイルを後から
     * 結合する方式より長時間録画時の同期ずれを抑えられる。
     */
    fun startRecording(contentResolver: ContentResolver, legacyMoviesDirectory: File?) {
        val helper = cameraHelper
        if (helper == null || !helper.isCameraOpened) {
            listener.onRecordingError("カメラが準備できていません")
            return
        }
        if (recordingRequested) return

        val options = runCatching {
            createOutputFileOptions(contentResolver, legacyMoviesDirectory)
        }.getOrElse {
            Log.e(TAG, "Unable to create recording output", it)
            listener.onRecordingError("録画ファイルを作成できませんでした")
            return
        }

        // マイク権限が録画開始直前に許可された場合にも、AudioRecordを作り直す。
        // CameraHelper内部の非同期キューでは設定変更がstartRecordingより先に実行される。
        configureRecording(helper, helper.previewSize)
        recordingRequested = true
        stopRequested = false
        listener.onRecordingStarting()

        helper.startRecording(options, object : VideoCapture.OnVideoCaptureCallback {
            override fun onStart() {
                listener.onRecordingStarted()
                if (stopRequested) {
                    helper.stopRecording()
                }
            }

            override fun onVideoSaved(outputFileResults: VideoCapture.OutputFileResults) {
                recordingRequested = false
                stopRequested = false
                listener.onRecordingSaved(outputFileResults.savedUri)
            }

            override fun onError(videoCaptureError: Int, message: String, cause: Throwable?) {
                Log.e(TAG, "Recording failed: $message", cause)
                recordingRequested = false
                stopRequested = false
                listener.onRecordingError(message)
            }
        })
    }

    /** 停止完了はonRecordingSaved/onRecordingErrorで通知される非同期処理。 */
    fun stopRecording() {
        if (!recordingRequested || stopRequested) return
        stopRequested = true
        listener.onRecordingStopping()
        cameraHelper?.stopRecording()
    }

    private fun stopRecordingForCameraClose() {
        if (recordingRequested && !stopRequested) {
            Log.i(TAG, "Camera closed while recording; finalizing available data")
            stopRecording()
        }
    }

    /** AAC 48 kHzを使い、講義音声に十分なビットレートで録音する。 */
    private fun configureRecording(helper: ICameraHelper, size: Size?) {
        val config = helper.videoCaptureConfig
        config.setVideoFrameRate(30)
        config.setBitRate(recordingBitRate(size))
        config.setIFrameInterval(1)
        config.setAudioCaptureEnable(true)
        config.setAudioSampleRate(48_000)
        config.setAudioChannelCount(1)
        config.setAudioBitRate(128_000)
        helper.setVideoCaptureConfig(config)
    }

    private fun recordingBitRate(size: Size?): Int = when {
        size == null -> 8 * 1024 * 1024
        size.width >= 3840 -> 24 * 1024 * 1024
        size.width >= 1920 -> 8 * 1024 * 1024
        size.width >= 1280 -> 5 * 1024 * 1024
        else -> 2 * 1024 * 1024
    }

    private fun createOutputFileOptions(
        contentResolver: ContentResolver,
        legacyMoviesDirectory: File?
    ): VideoCapture.OutputFileOptions {
        val filename = "UVC_${SimpleDateFormat("yyyyMMdd_HHmmss", Locale.US).format(Date())}.mp4"
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            val values = ContentValues().apply {
                put(MediaStore.MediaColumns.DISPLAY_NAME, filename)
                put(MediaStore.MediaColumns.MIME_TYPE, "video/mp4")
                put(
                    MediaStore.MediaColumns.RELATIVE_PATH,
                    "${Environment.DIRECTORY_MOVIES}${File.separator}UVC Camera"
                )
            }
            VideoCapture.OutputFileOptions.Builder(
                contentResolver,
                MediaStore.Video.Media.EXTERNAL_CONTENT_URI,
                values
            ).build()
        } else {
            val directory = File(legacyMoviesDirectory ?: throw IllegalStateException("Movies directory unavailable"), "UVC Camera")
            if (!directory.exists() && !directory.mkdirs()) {
                throw IllegalStateException("Unable to create movies directory")
            }
            VideoCapture.OutputFileOptions.Builder(File(directory, filename)).build()
        }
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
