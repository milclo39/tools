package com.example.dogwalk.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.example.dogwalk.tracking.WalkSessionManager
import com.example.dogwalk.util.Formatters

@Composable
fun ResultScreen(
    onBackHome: () -> Unit,
    onOpenStats: () -> Unit,
) {
    val result = WalkSessionManager.lastResult

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(24.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center,
    ) {
        Text("おつかれさま!", style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(24.dp))

        if (result != null) {
            Card(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.padding(20.dp)) {
                    ResultRow("距離", Formatters.distanceKm(result.distanceMeters))
                    ResultRow("時間", Formatters.duration(result.durationSeconds))
                    ResultRow("平均ペース", Formatters.pace(result.distanceMeters, result.durationSeconds))
                }
            }
            Spacer(Modifier.height(8.dp))
            Text(
                "記録は自動保存されました",
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        } else {
            Text("記録がありません")
        }

        Spacer(Modifier.height(32.dp))
        Button(onClick = onBackHome, modifier = Modifier.fillMaxWidth()) {
            Text("ホームへ")
        }
        Spacer(Modifier.height(8.dp))
        OutlinedButton(onClick = onOpenStats, modifier = Modifier.fillMaxWidth()) {
            Text("統計を見る")
        }
    }
}

@Composable
private fun ResultRow(label: String, value: String) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 6.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
    ) {
        Text(label, style = MaterialTheme.typography.bodyLarge)
        Text(value, style = MaterialTheme.typography.titleLarge)
    }
}
