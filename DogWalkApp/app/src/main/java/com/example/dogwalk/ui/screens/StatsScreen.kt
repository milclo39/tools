package com.example.dogwalk.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material3.Card
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.example.dogwalk.data.WalkRecord
import com.example.dogwalk.ui.PeriodStats
import com.example.dogwalk.ui.StatsViewModel
import com.example.dogwalk.util.Formatters

@Composable
fun StatsScreen(
    statsViewModel: StatsViewModel = viewModel(),
) {
    val stats by statsViewModel.uiState.collectAsState()

    LazyColumn(
        modifier = Modifier
            .fillMaxSize()
            .padding(horizontal = 16.dp),
        contentPadding = androidx.compose.foundation.layout.PaddingValues(vertical = 16.dp),
    ) {
        item {
            Text("統計", style = MaterialTheme.typography.headlineMedium)
            Spacer(Modifier.height(16.dp))

            PeriodCard("今日", stats.today)
            Spacer(Modifier.height(8.dp))
            PeriodCard("今週(月曜始まり)", stats.week)
            Spacer(Modifier.height(8.dp))
            PeriodCard("今月", stats.month)
            Spacer(Modifier.height(8.dp))

            Card(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.padding(16.dp)) {
                    Text("1回あたりの平均", style = MaterialTheme.typography.titleMedium)
                    Spacer(Modifier.height(4.dp))
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                    ) {
                        Text("距離: ${Formatters.distanceKm(stats.avgDistanceMeters)}")
                        Text("時間: ${Formatters.duration(stats.avgDurationSeconds)}")
                    }
                }
            }

            Spacer(Modifier.height(24.dp))
            Text("履歴", style = MaterialTheme.typography.titleLarge)
            Spacer(Modifier.height(8.dp))
            if (stats.history.isEmpty()) {
                Text(
                    "まだ記録がありません",
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }

        items(stats.history, key = { it.id }) { record ->
            HistoryRow(record = record, onDelete = { statsViewModel.delete(record) })
            HorizontalDivider()
        }
    }
}

@Composable
private fun PeriodCard(title: String, stats: PeriodStats) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text(title, style = MaterialTheme.typography.titleMedium)
            Text("${Formatters.distanceKm(stats.distanceMeters)} / ${stats.count} 回")
        }
    }
}

@Composable
private fun HistoryRow(record: WalkRecord, onDelete: () -> Unit) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Column(modifier = Modifier.weight(1f)) {
            Text(
                Formatters.dateTime(record.startTime),
                style = MaterialTheme.typography.bodyMedium,
            )
            Text(
                "${Formatters.distanceKm(record.distanceMeters)}・${Formatters.duration(record.durationSeconds)}",
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
        IconButton(onClick = onDelete) {
            Icon(
                Icons.Default.Delete,
                contentDescription = "削除",
                tint = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
}
