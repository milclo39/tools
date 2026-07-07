package com.example.dogwalk.ui.screens

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.DirectionsWalk
import androidx.compose.material.icons.filled.QueryStats
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.core.content.ContextCompat
import androidx.lifecycle.viewmodel.compose.viewModel
import com.example.dogwalk.ui.StatsViewModel
import com.example.dogwalk.ui.WalkViewModel
import com.example.dogwalk.util.Formatters

@Composable
fun HomeScreen(
    onWalkStarted: () -> Unit,
    onResumeWalk: () -> Unit,
    onOpenStats: () -> Unit,
    walkViewModel: WalkViewModel = viewModel(),
    statsViewModel: StatsViewModel = viewModel(),
) {
    val context = LocalContext.current
    val session by walkViewModel.session.collectAsState()
    val stats by statsViewModel.uiState.collectAsState()

    val permissionLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { result ->
        if (result[Manifest.permission.ACCESS_FINE_LOCATION] == true) {
            walkViewModel.startWalk()
            onWalkStarted()
        }
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(24.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center,
    ) {
        Text("わんぽ", style = MaterialTheme.typography.headlineLarge)
        Spacer(Modifier.height(32.dp))

        // 直近の統計サマリー
        Card(modifier = Modifier.fillMaxWidth()) {
            Column(modifier = Modifier.padding(16.dp)) {
                Text("今週のまとめ", style = MaterialTheme.typography.titleMedium)
                Spacer(Modifier.height(8.dp))
                Row(horizontalArrangement = Arrangement.SpaceBetween, modifier = Modifier.fillMaxWidth()) {
                    Text("合計距離")
                    Text(Formatters.distanceKm(stats.week.distanceMeters))
                }
                Row(horizontalArrangement = Arrangement.SpaceBetween, modifier = Modifier.fillMaxWidth()) {
                    Text("散歩回数")
                    Text("${stats.week.count} 回")
                }
                Row(horizontalArrangement = Arrangement.SpaceBetween, modifier = Modifier.fillMaxWidth()) {
                    Text("今日")
                    Text("${Formatters.distanceKm(stats.today.distanceMeters)} / ${stats.today.count} 回")
                }
            }
        }
        Spacer(Modifier.height(32.dp))

        if (session.isActive) {
            Button(onClick = onResumeWalk, modifier = Modifier.fillMaxWidth()) {
                Icon(Icons.Default.DirectionsWalk, contentDescription = null)
                Text("  散歩中に戻る")
            }
        } else {
            Button(
                onClick = {
                    val granted = ContextCompat.checkSelfPermission(
                        context, Manifest.permission.ACCESS_FINE_LOCATION
                    ) == PackageManager.PERMISSION_GRANTED
                    if (granted) {
                        walkViewModel.startWalk()
                        onWalkStarted()
                    } else {
                        val permissions = buildList {
                            add(Manifest.permission.ACCESS_FINE_LOCATION)
                            add(Manifest.permission.ACCESS_COARSE_LOCATION)
                            if (Build.VERSION.SDK_INT >= 33) {
                                add(Manifest.permission.POST_NOTIFICATIONS)
                            }
                        }
                        permissionLauncher.launch(permissions.toTypedArray())
                    }
                },
                modifier = Modifier.fillMaxWidth(),
            ) {
                Icon(Icons.Default.DirectionsWalk, contentDescription = null)
                Text("  散歩を始める")
            }
        }

        Spacer(Modifier.height(12.dp))
        OutlinedButton(onClick = onOpenStats, modifier = Modifier.fillMaxWidth()) {
            Icon(Icons.Default.QueryStats, contentDescription = null)
            Text("  統計を見る")
        }
    }
}
