package com.example.uvccamera

import android.Manifest
import android.content.pm.PackageManager
import android.hardware.usb.UsbDevice
import android.os.Bundle
import android.os.Environment
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.view.SurfaceHolder
import android.view.View
import android.widget.AdapterView
import android.widget.ArrayAdapter
import android.widget.Button
import android.widget.SeekBar
import android.widget.Spinner
import android.widget.TextView
import androidx.activity.viewModels
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.SwitchCompat
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import com.serenegiant.usb.Size
import com.serenegiant.widget.AspectRatioSurfaceView
import java.util.Locale

class MainActivity : AppCompatActivity() {

    private val viewModel: MainViewModel by viewModels()

    private lateinit var spinnerDevice: Spinner
    private lateinit var spinnerResolution: Spinner
    private lateinit var surfaceView: AspectRatioSurfaceView
    private lateinit var textNoDevice: TextView
    private lateinit var buttonRecord: Button
    private lateinit var textRecordingStatus: TextView
    private lateinit var seekZoom: SeekBar
    private lateinit var textZoomValue: TextView
    private lateinit var switchFocusAuto: SwitchCompat
    private lateinit var seekFocus: SeekBar
    private lateinit var textFocusValue: TextView
    private lateinit var seekExposure: SeekBar
    private lateinit var textExposureValue: TextView
    private lateinit var seekBrightness: SeekBar
    private lateinit var textBrightnessValue: TextView

    private var deviceList: List<UsbDevice> = emptyList()
    private var resolutionList: List<Size> = emptyList()
    private var currentResolution: Size? = null
    private var recordingState = RecordingUiState()
    private var startRecordingAfterPermission = false
    private val recordingHandler = Handler(Looper.getMainLooper())
    private val updateRecordingElapsed = object : Runnable {
        override fun run() {
            val startedAt = recordingState.startedAtElapsedRealtime ?: return
            val elapsedSeconds = (SystemClock.elapsedRealtime() - startedAt) / 1_000
            textRecordingStatus.text = getString(
                R.string.recording_elapsed,
                String.format(
                    Locale.US,
                    "%02d:%02d:%02d",
                    elapsedSeconds / 3_600,
                    (elapsedSeconds % 3_600) / 60,
                    elapsedSeconds % 60
                )
            )
            recordingHandler.postDelayed(this, 1_000)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        bindViews()
        setupSurface()
        setupSpinners()
        setupControls()
        observeViewModel()

        requestInitialPermissions()
    }

    override fun onResume() {
        super.onResume()
        viewModel.refreshDeviceList()
    }

    private fun bindViews() {
        spinnerDevice = findViewById(R.id.spinnerDevice)
        spinnerResolution = findViewById(R.id.spinnerResolution)
        surfaceView = findViewById(R.id.surfaceView)
        textNoDevice = findViewById(R.id.textNoDevice)
        buttonRecord = findViewById(R.id.buttonRecord)
        textRecordingStatus = findViewById(R.id.textRecordingStatus)
        seekZoom = findViewById(R.id.seekZoom)
        textZoomValue = findViewById(R.id.textZoomValue)
        switchFocusAuto = findViewById(R.id.switchFocusAuto)
        seekFocus = findViewById(R.id.seekFocus)
        textFocusValue = findViewById(R.id.textFocusValue)
        seekExposure = findViewById(R.id.seekExposure)
        textExposureValue = findViewById(R.id.textExposureValue)
        seekBrightness = findViewById(R.id.seekBrightness)
        textBrightnessValue = findViewById(R.id.textBrightnessValue)
    }

    private fun setupSurface() {
        surfaceView.holder.addCallback(object : SurfaceHolder.Callback {
            override fun surfaceCreated(holder: SurfaceHolder) {
                viewModel.setSurface(holder.surface)
            }

            override fun surfaceChanged(holder: SurfaceHolder, format: Int, w: Int, h: Int) = Unit

            override fun surfaceDestroyed(holder: SurfaceHolder) {
                viewModel.setSurface(null)
            }
        })
    }

    private fun setupSpinners() {
        spinnerDevice.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(parent: AdapterView<*>?, v: View?, pos: Int, id: Long) {
                // selectDevice()は同一デバイスなら何もしないため、
                // アダプタ設定直後の自動発火イベントも安全(未選択なら自動選択として機能する)
                deviceList.getOrNull(pos)?.let { viewModel.selectDevice(it) }
            }

            override fun onNothingSelected(parent: AdapterView<*>?) = Unit
        }

        spinnerResolution.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(parent: AdapterView<*>?, v: View?, pos: Int, id: Long) {
                resolutionList.getOrNull(pos)?.let { size ->
                    // 既に同じ解像度なら再ネゴシエーションしない
                    if (size.width == currentResolution?.width &&
                        size.height == currentResolution?.height
                    ) return
                    currentResolution = size
                    viewModel.setResolution(size)
                    surfaceView.setAspectRatio(size.width, size.height)
                }
            }

            override fun onNothingSelected(parent: AdapterView<*>?) = Unit
        }
    }

    private fun setupControls() {
        buttonRecord.setOnClickListener {
            when (recordingState.status) {
                RecordingStatus.IDLE -> startRecordingWhenPermitted()
                RecordingStatus.STARTING, RecordingStatus.RECORDING -> viewModel.stopRecording()
                RecordingStatus.STOPPING -> Unit
            }
        }

        switchFocusAuto.setOnCheckedChangeListener { _, isChecked ->
            viewModel.setFocusAuto(isChecked)
            seekFocus.isEnabled = !isChecked && seekFocus.max > 0
        }
    }

    private fun observeViewModel() {
        viewModel.devices.observe(this) { devices ->
            deviceList = devices
            val labels = if (devices.isEmpty()) {
                listOf(getString(R.string.no_device))
            } else {
                devices.map { deviceLabel(it) }
            }
            val adapter = ArrayAdapter(
                this, android.R.layout.simple_spinner_item, labels
            ).apply {
                setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
            }
            spinnerDevice.adapter = adapter
            spinnerDevice.isEnabled = devices.isNotEmpty()
            // 選択中デバイスがあればその位置へ。なければ位置0の自動発火イベントが
            // そのまま先頭デバイスの自動選択(=USB権限リクエスト)になる
            val selectedIndex = devices.indexOf(viewModel.controller.selectedDevice)
            if (selectedIndex > 0) {
                spinnerDevice.setSelection(selectedIndex)
            }
            updateRecordingControlAvailability()
        }

        viewModel.cameraState.observe(this) { state ->
            if (state == null) {
                textNoDevice.visibility = View.VISIBLE
                updateResolutionSpinner(emptyList(), null)
                disableAllControls()
            } else {
                textNoDevice.visibility = View.GONE
                state.currentSize?.let { surfaceView.setAspectRatio(it.width, it.height) }
                updateResolutionSpinner(state.supportedSizes, state.currentSize)
                applyControlStates(state.controls)
            }
            updateRecordingControlAvailability()
        }

        viewModel.recordingState.observe(this) { state ->
            recordingState = state
            renderRecordingState()
        }
    }

    // ---- USBカメラ選択 ----

    /** PCのデバイスマネージャーと同様、製品名(取得不可時は vendorId:productId)を表示 */
    private fun deviceLabel(device: UsbDevice): String =
        device.productName
            ?: String.format(Locale.US, "%04x:%04x", device.vendorId, device.productId)

    // ---- 解像度 ----

    private fun updateResolutionSpinner(sizes: List<Size>, current: Size?) {
        resolutionList = sizes
        currentResolution = current
        val labels = sizes.map { resolutionLabel(it) }
        val adapter = ArrayAdapter(
            this, android.R.layout.simple_spinner_item, labels
        ).apply {
            setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        }
        spinnerResolution.adapter = adapter
        spinnerResolution.isEnabled = sizes.isNotEmpty()

        val currentIndex = sizes.indexOfFirst {
            it.width == current?.width && it.height == current.height
        }
        if (currentIndex >= 0) {
            spinnerResolution.setSelection(currentIndex)
        }
        updateRecordingControlAvailability()
    }

    private fun requestInitialPermissions() {
        val permissions = listOf(Manifest.permission.CAMERA, Manifest.permission.RECORD_AUDIO)
            .filter { ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED }
        if (permissions.isNotEmpty()) {
            ActivityCompat.requestPermissions(this, permissions.toTypedArray(), REQUEST_INITIAL_PERMISSIONS)
        }
    }

    private fun startRecordingWhenPermitted() {
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED) {
            startRecording()
            return
        }
        startRecordingAfterPermission = true
        ActivityCompat.requestPermissions(
            this,
            arrayOf(Manifest.permission.RECORD_AUDIO),
            REQUEST_RECORD_AUDIO
        )
    }

    private fun startRecording() {
        viewModel.startRecording(
            contentResolver,
            getExternalFilesDir(Environment.DIRECTORY_MOVIES)
        )
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode != REQUEST_RECORD_AUDIO || !startRecordingAfterPermission) return

        startRecordingAfterPermission = false
        if (grantResults.firstOrNull() == PackageManager.PERMISSION_GRANTED) {
            startRecording()
        } else {
            textRecordingStatus.text = getString(R.string.record_audio_permission_required)
        }
    }

    private fun renderRecordingState() {
        recordingHandler.removeCallbacks(updateRecordingElapsed)
        when (recordingState.status) {
            RecordingStatus.IDLE -> {
                buttonRecord.text = getString(R.string.record_start)
                textRecordingStatus.text = recordingState.message ?: getString(R.string.record_idle)
            }

            RecordingStatus.STARTING -> {
                buttonRecord.text = getString(R.string.record_stop)
                textRecordingStatus.text = getString(R.string.record_starting)
            }

            RecordingStatus.RECORDING -> {
                buttonRecord.text = getString(R.string.record_stop)
                updateRecordingElapsed.run()
            }

            RecordingStatus.STOPPING -> {
                buttonRecord.text = getString(R.string.record_stopping)
                textRecordingStatus.text = getString(R.string.record_stopping)
            }
        }
        updateRecordingControlAvailability()
    }

    private fun updateRecordingControlAvailability() {
        val recordingActive = recordingState.status != RecordingStatus.IDLE
        spinnerDevice.isEnabled = deviceList.isNotEmpty() && !recordingActive
        spinnerResolution.isEnabled = resolutionList.isNotEmpty() && !recordingActive
        buttonRecord.isEnabled = when (recordingState.status) {
            RecordingStatus.IDLE -> viewModel.cameraState.value != null
            RecordingStatus.STARTING, RecordingStatus.RECORDING -> true
            RecordingStatus.STOPPING -> false
        }
    }

    override fun onDestroy() {
        recordingHandler.removeCallbacks(updateRecordingElapsed)
        super.onDestroy()
    }

    private fun resolutionLabel(size: Size): String {
        val name = when (size.width to size.height) {
            640 to 480 -> "VGA"
            1280 to 720 -> "720p"
            1920 to 1080 -> "1080p"
            3840 to 2160 -> "4K"
            else -> null
        }
        return if (name != null) {
            "${size.width}x${size.height} ($name)"
        } else {
            "${size.width}x${size.height}"
        }
    }

    // ---- コントロールパネル ----

    private fun applyControlStates(controls: UvcCameraController.ControlStates) {
        bindSeekBar(seekZoom, textZoomValue, controls.zoom) { viewModel.setZoom(it) }
        bindSeekBar(seekExposure, textExposureValue, controls.exposure) { viewModel.setExposure(it) }
        bindSeekBar(seekBrightness, textBrightnessValue, controls.brightness) {
            viewModel.setBrightness(it)
        }
        bindSeekBar(seekFocus, textFocusValue, controls.focus) { viewModel.setFocus(it) }

        // Focus Auto/Manual トグル
        switchFocusAuto.setOnCheckedChangeListener(null)
        switchFocusAuto.isEnabled = controls.focusAutoEnabled
        switchFocusAuto.isChecked = controls.focusAuto
        switchFocusAuto.setOnCheckedChangeListener { _, isChecked ->
            viewModel.setFocusAuto(isChecked)
            seekFocus.isEnabled = !isChecked && controls.focus.enabled
        }
        // Auto時はManualスライダーを無効化
        if (controls.focusAuto) {
            seekFocus.isEnabled = false
        }
    }

    private fun disableAllControls() {
        listOf(seekZoom, seekFocus, seekExposure, seekBrightness).forEach {
            it.isEnabled = false
        }
        switchFocusAuto.isEnabled = false
        listOf(textZoomValue, textFocusValue, textExposureValue, textBrightnessValue).forEach {
            it.text = ""
        }
    }

    /**
     * ControlRangeをSeekBarへ反映する。
     * 負値(Brightness等)に対応するため progress = value - min のオフセット方式。
     * 刻み(GET_RES相当)はSeekBarの1ステップとして扱う。
     */
    private fun bindSeekBar(
        seekBar: SeekBar,
        valueText: TextView,
        range: UvcCameraController.ControlRange,
        onValue: (Int) -> Unit
    ) {
        seekBar.setOnSeekBarChangeListener(null)
        if (!range.enabled) {
            seekBar.isEnabled = false
            valueText.text = "-"
            return
        }
        seekBar.isEnabled = true
        seekBar.max = range.max - range.min
        seekBar.progress = (range.cur - range.min).coerceIn(0, seekBar.max)
        valueText.text = range.cur.toString()

        seekBar.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(sb: SeekBar, progress: Int, fromUser: Boolean) {
                val value = range.min + progress
                valueText.text = value.toString()
                if (fromUser) {
                    onValue(value)
                }
            }

            override fun onStartTrackingTouch(sb: SeekBar) = Unit
            override fun onStopTrackingTouch(sb: SeekBar) = Unit
        })
    }

    private companion object {
        const val REQUEST_INITIAL_PERMISSIONS = 1001
        const val REQUEST_RECORD_AUDIO = 1002
    }
}
