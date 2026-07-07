package com.example.dogwalk.ui

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.example.dogwalk.Graph
import com.example.dogwalk.data.WalkRecord
import com.example.dogwalk.tracking.WalkSessionManager
import com.example.dogwalk.tracking.WalkTrackingService
import kotlinx.coroutines.launch

class WalkViewModel(app: Application) : AndroidViewModel(app) {

    val session = WalkSessionManager.state

    fun startWalk() {
        WalkSessionManager.start()
        WalkTrackingService.start(getApplication())
    }

    /** 散歩を終了して自動保存し、完了後にonSavedを呼ぶ */
    fun endWalk(onSaved: () -> Unit) {
        WalkTrackingService.stop(getApplication())
        val result = WalkSessionManager.finish() ?: return
        viewModelScope.launch {
            Graph.repository.insert(
                WalkRecord(
                    startTime = result.startTime,
                    endTime = result.endTime,
                    distanceMeters = result.distanceMeters,
                    durationSeconds = result.durationSeconds,
                )
            )
            onSaved()
        }
    }
}
