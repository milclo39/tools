package com.example.dogwalk.ui

import androidx.compose.runtime.Composable
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import com.example.dogwalk.ui.screens.HomeScreen
import com.example.dogwalk.ui.screens.ResultScreen
import com.example.dogwalk.ui.screens.StatsScreen
import com.example.dogwalk.ui.screens.WalkScreen

object Routes {
    const val HOME = "home"
    const val WALK = "walk"
    const val RESULT = "result"
    const val STATS = "stats"
}

@Composable
fun AppNavHost() {
    val navController = rememberNavController()

    NavHost(navController = navController, startDestination = Routes.HOME) {
        composable(Routes.HOME) {
            HomeScreen(
                onWalkStarted = { navController.navigate(Routes.WALK) },
                onResumeWalk = { navController.navigate(Routes.WALK) },
                onOpenStats = { navController.navigate(Routes.STATS) },
            )
        }
        composable(Routes.WALK) {
            WalkScreen(
                onWalkEnded = {
                    navController.navigate(Routes.RESULT) {
                        popUpTo(Routes.HOME) // 散歩中画面には戻れないようにする
                    }
                },
            )
        }
        composable(Routes.RESULT) {
            ResultScreen(
                onBackHome = {
                    navController.popBackStack(Routes.HOME, inclusive = false)
                },
                onOpenStats = { navController.navigate(Routes.STATS) },
            )
        }
        composable(Routes.STATS) {
            StatsScreen()
        }
    }
}
