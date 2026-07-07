package com.example.dogwalk.tracking

import android.annotation.SuppressLint
import android.app.Notification
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.location.Location
import android.os.IBinder
import android.os.Looper
import androidx.core.app.NotificationCompat
import androidx.core.app.ServiceCompat
import androidx.core.content.ContextCompat
import com.example.dogwalk.DogWalkApplication
import com.example.dogwalk.MainActivity
import com.example.dogwalk.R
import com.google.android.gms.location.FusedLocationProviderClient
import com.google.android.gms.location.LocationCallback
import com.google.android.gms.location.LocationRequest
import com.google.android.gms.location.LocationResult
import com.google.android.gms.location.LocationServices
import com.google.android.gms.location.Priority
import com.google.android.gms.maps.model.LatLng

/**
 * 散歩中のGPS記録を行うForeground Service。
 * 画面消灯中・他アプリ使用中も記録を継続する。
 * ACCESS_BACKGROUND_LOCATION は不要(フォアグラウンド中にサービスを開始するため)。
 */
class WalkTrackingService : Service() {

    private lateinit var fusedClient: FusedLocationProviderClient
    private var lastAcceptedLocation: Location? = null

    private val locationCallback = object : LocationCallback() {
        override fun onLocationResult(result: LocationResult) {
            for (location in result.locations) {
                handleLocation(location)
            }
        }
    }

    override fun onCreate() {
        super.onCreate()
        fusedClient = LocationServices.getFusedLocationProviderClient(this)
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_STOP -> {
                stopSelf()
                return START_NOT_STICKY
            }
            else -> {
                startAsForeground()
                startLocationUpdates()
            }
        }
        return START_STICKY
    }

    private fun startAsForeground() {
        ServiceCompat.startForeground(
            this,
            NOTIFICATION_ID,
            buildNotification(),
            ServiceInfo.FOREGROUND_SERVICE_TYPE_LOCATION,
        )
    }

    private fun buildNotification(): Notification {
        val contentIntent = PendingIntent.getActivity(
            this,
            0,
            Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE,
        )
        return NotificationCompat.Builder(this, DogWalkApplication.CHANNEL_ID_TRACKING)
            .setContentTitle(getString(R.string.notification_title))
            .setSmallIcon(android.R.drawable.ic_menu_mylocation)
            .setContentIntent(contentIntent)
            .setOngoing(true)
            .build()
    }

    @SuppressLint("MissingPermission") // 権限確認後にのみ開始される
    private fun startLocationUpdates() {
        val request = LocationRequest.Builder(
            Priority.PRIORITY_HIGH_ACCURACY,
            UPDATE_INTERVAL_MILLIS,
        )
            .setMinUpdateIntervalMillis(MIN_UPDATE_INTERVAL_MILLIS)
            .build()
        fusedClient.requestLocationUpdates(request, locationCallback, Looper.getMainLooper())
    }

    private fun handleLocation(location: Location) {
        // 精度の悪い測位点は除外(距離の水増し防止)
        if (location.hasAccuracy() && location.accuracy > MAX_ACCURACY_METERS) return

        val last = lastAcceptedLocation
        val added = if (last != null) {
            val d = last.distanceTo(location).toDouble()
            // GPSノイズによる微小なブレは加算しない
            if (d < MIN_STEP_METERS) return else d
        } else {
            0.0
        }
        lastAcceptedLocation = location
        WalkSessionManager.addPoint(LatLng(location.latitude, location.longitude), added)
    }

    override fun onDestroy() {
        fusedClient.removeLocationUpdates(locationCallback)
        super.onDestroy()
    }

    override fun onBind(intent: Intent?): IBinder? = null

    companion object {
        private const val NOTIFICATION_ID = 1
        private const val ACTION_STOP = "com.example.dogwalk.action.STOP"

        private const val UPDATE_INTERVAL_MILLIS = 3_000L
        private const val MIN_UPDATE_INTERVAL_MILLIS = 2_000L

        /** これより精度が悪い(値が大きい)測位点は捨てる */
        private const val MAX_ACCURACY_METERS = 25f

        /** これ未満の移動はノイズとみなして無視する */
        private const val MIN_STEP_METERS = 2.0

        fun start(context: Context) {
            ContextCompat.startForegroundService(
                context,
                Intent(context, WalkTrackingService::class.java),
            )
        }

        fun stop(context: Context) {
            context.startService(
                Intent(context, WalkTrackingService::class.java).setAction(ACTION_STOP),
            )
        }
    }
}
