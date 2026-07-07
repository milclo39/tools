package com.example.uvccamera

import android.hardware.usb.UsbDevice
import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import com.serenegiant.usb.Size

/**
 * カメラのオープン状態(対応解像度・コントロール範囲)
 */
data class CameraUiState(
    val device: UsbDevice,
    val supportedSizes: List<Size>,
    val currentSize: Size?,
    val controls: UvcCameraController.ControlStates
)

class MainViewModel : ViewModel(), UvcCameraController.Listener {

    val controller = UvcCameraController(this)

    private val _devices = MutableLiveData<List<UsbDevice>>(emptyList())
    val devices: LiveData<List<UsbDevice>> = _devices

    private val _cameraState = MutableLiveData<CameraUiState?>(null)
    val cameraState: LiveData<CameraUiState?> = _cameraState

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

    override fun onCleared() {
        controller.release()
        super.onCleared()
    }
}
