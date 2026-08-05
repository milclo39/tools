package com.example.uvccamera

import android.content.ContentResolver
import android.hardware.usb.UsbDevice
import android.net.Uri
import android.os.SystemClock
import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import com.serenegiant.usb.Size
import java.io.File

/**
 * カメラのオープン状態(対応解像度・コントロール範囲)
 */
data class CameraUiState(
    val device: UsbDevice,
    val supportedSizes: List<Size>,
    val currentSize: Size?,
    val controls: UvcCameraController.ControlStates
)

enum class RecordingStatus {
    IDLE,
    STARTING,
    RECORDING,
    STOPPING
}

data class RecordingUiState(
    val status: RecordingStatus = RecordingStatus.IDLE,
    val startedAtElapsedRealtime: Long? = null,
    val message: String? = null
)

class MainViewModel : ViewModel(), UvcCameraController.Listener {

    val controller = UvcCameraController(this)

    private val _devices = MutableLiveData<List<UsbDevice>>(emptyList())
    val devices: LiveData<List<UsbDevice>> = _devices

    private val _cameraState = MutableLiveData<CameraUiState?>(null)
    val cameraState: LiveData<CameraUiState?> = _cameraState

    private val _recordingState = MutableLiveData(RecordingUiState())
    val recordingState: LiveData<RecordingUiState> = _recordingState

    init {
        controller.init()
    }

    // ---- UvcCameraController.Listener (ライブラリのコールバックスレッドから呼ばれる) ----

    override fun onDeviceListChanged(devices: List<UsbDevice>) {
        _devices.postValue(devices)
    }

    override fun onCameraOpened(
        device: UsbDevice,
        supportedSizes: List<Size>,
        currentSize: Size?,
        controls: UvcCameraController.ControlStates
    ) {
        _cameraState.postValue(CameraUiState(device, supportedSizes, currentSize, controls))
    }

    override fun onCameraClosed() {
        _cameraState.postValue(null)
    }

    override fun onRecordingStarting() {
        _recordingState.postValue(RecordingUiState(RecordingStatus.STARTING))
    }

    override fun onRecordingStarted() {
        _recordingState.postValue(
            RecordingUiState(
                status = RecordingStatus.RECORDING,
                startedAtElapsedRealtime = SystemClock.elapsedRealtime()
            )
        )
    }

    override fun onRecordingStopping() {
        _recordingState.postValue(RecordingUiState(RecordingStatus.STOPPING))
    }

    override fun onRecordingSaved(uri: Uri?) {
        _recordingState.postValue(
            RecordingUiState(
                message = if (uri == null) "録画を保存しました" else "録画を保存しました: $uri"
            )
        )
    }

    override fun onRecordingError(message: String) {
        _recordingState.postValue(RecordingUiState(message = "録画エラー: $message"))
    }

    // ---- UIからの操作 ----

    fun refreshDeviceList() {
        val list = controller.getDeviceList()
        _devices.postValue(list)
        // onAttachが既接続デバイスで発火しない環境向けのフォールバック:
        // 未選択なら先頭デバイスを自動選択(USB権限リクエストが走る)
        if (controller.selectedDevice == null) {
            list.firstOrNull()?.let { controller.selectDevice(it) }
        }
    }

    fun selectDevice(device: UsbDevice) = controller.selectDevice(device)

    fun setResolution(size: Size) = controller.setResolution(size)

    fun setSurface(surface: Any?) = controller.setSurface(surface)

    fun setZoom(value: Int) = controller.setZoom(value)

    fun setFocusAuto(auto: Boolean) {
        controller.setFocusAuto(auto)
    }

    fun setFocus(value: Int) = controller.setFocus(value)

    fun setExposure(value: Int) = controller.setExposure(value)

    fun setBrightness(value: Int) = controller.setBrightness(value)

    fun startRecording(contentResolver: ContentResolver, legacyMoviesDirectory: File?) {
        controller.startRecording(contentResolver, legacyMoviesDirectory)
    }

    fun stopRecording() = controller.stopRecording()

    override fun onCleared() {
        controller.release()
        super.onCleared()
    }
}
