package com.example.dogwalk.ui

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.example.dogwalk.Graph
import com.example.dogwalk.data.WalkRecord
import com.example.dogwalk.util.TimeRanges
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch

data class PeriodStats(
    val distanceMeters: Double = 0.0,
    val count: Int = 0,
)

data class StatsUiState(
    val today: PeriodStats = PeriodStats(),
    val week: PeriodStats = PeriodStats(),
    val month: PeriodStats = PeriodStats(),
    val avgDistanceMeters: Double = 0.0,
    val avgDurationSeconds: Int = 0,
    val history: List<WalkRecord> = emptyList(),
)

class StatsViewModel : ViewModel() {

    val uiState = Graph.repository.allRecords
        .map { records -> buildStats(records) }
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(5_000), StatsUiState())

    fun delete(record: WalkRecord) {
        viewModelScope.launch { Graph.repository.delete(record) }
    }

    private fun buildStats(records: List<WalkRecord>): StatsUiState {
        fun since(from: Long): PeriodStats {
            val filtered = records.filter { it.startTime >= from }
            return PeriodStats(
                distanceMeters = filtered.sumOf { it.distanceMeters },
                count = filtered.size,
            )
        }
        return StatsUiState(
            today = since(TimeRanges.todayStart()),
            week = since(TimeRanges.weekStart()),
            month = since(TimeRanges.monthStart()),
            avgDistanceMeters = if (records.isEmpty()) 0.0 else records.sumOf { it.distanceMeters } / records.size,
            avgDurationSeconds = if (records.isEmpty()) 0 else records.sumOf { it.durationSeconds } / records.size,
            history = records,
        )
    }
}
