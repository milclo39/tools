package com.example.dogwalk

import android.app.Application
import android.app.NotificationChannel
import android.app.NotificationManager

class DogWalkApplication : Application() {

    override fun onCreate() {
        super.onCreate()
        Graph.init(this)
        createNotificationChannel()
    }

    private fun createNotificationChannel() {
        val channel = NotificationChannel(
            CHANNEL_ID_TRACKING,
            getString(R.string.notification_channel_name),
            NotificationManager.IMPORTANCE_LOW
        )
        getSystemService(NotificationManager::class.java).createNotificationChannel(channel)
    }

    companion object {
        const val CHANNEL_ID_TRACKING = "walk_tracking"
    }
}
