package com.example.dogwalk.data

import androidx.room.Entity
import androidx.room.PrimaryKey

@Entity(tableName = "walk_records")
data class WalkRecord(
    @PrimaryKey(autoGenerate = true) val id: Long = 0,
    /** 開始日時 (epoch millis) */
    val startTime: Long,
    /** 終了日時 (epoch millis) */
    val endTime: Long,
    /** 総距離 (メートル) */
    val distanceMeters: Double,
    /** 所要時間 (秒) = endTime - startTime */
    val durationSeconds: Int,
)
