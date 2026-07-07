package com.example.dogwalk.tracking

import com.google.android.gms.maps.model.LatLng
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update

/**
 * 進行中の散歩セッションの状態(メモリ上のみ)。
 * 軌跡は保存せず、セッション終了時に数値のみDBへ渡す。
 */
object WalkSessionManager {

    data class SessionState(
        val isActive: Boolean = false,
        val startTimeMillis: Long = 0L,
        val distanceMeters: Double = 0.0,
        val path: List<LatLng> = emptyList(),
    )

    data class WalkResult(
        val startTime: Long,
        val endTime: Long,
        val distanceMeters: Double,
        val durationSeconds: Int,
    )

    private val _state = MutableStateFlow(SessionState())
    val state: StateFlow<SessionState> = _state.asStateFlow()

    /** 直近に終了した散歩の結果(結果画面表示用) */
    var lastResult: WalkResult? = null
        private set

    fun start() {
        _state.value = SessionState(
            isActive = true,
            startTimeMillis = System.currentTimeMillis(),
        )
    }

    fun addPoint(point: LatLng, addedDistanceMeters: Double) {
        _state.update { s ->
            if (!s.isActive) return@update s
            s.copy(
                distanceMeters = s.distanceMeters + addedDistanceMeters,
                path = s.path + point,
            )
        }
    }

    /** セッションを終了し、結果を返す。アクティブでなければnull。 */
    fun finish(): WalkResult? {
        val s = _state.value
        if (!s.isActive) return null
        val end = System.currentTimeMillis()
        val result = WalkResult(
            startTime = s.startTimeMillis,
            endTime = end,
            distanceMeters = s.distanceMeters,
            durationSeconds = ((end - s.startTimeMillis) / 1000L).toInt(),
        )
        lastResult = result
        _state.value = SessionState()
        return result
    }
}
