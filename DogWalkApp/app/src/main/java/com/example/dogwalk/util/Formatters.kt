package com.example.dogwalk.util

import java.time.DayOfWeek
import java.time.Instant
import java.time.LocalDate
import java.time.ZoneId
import java.time.format.DateTimeFormatter
import java.time.temporal.TemporalAdjusters
import java.util.Locale

object TimeRanges {
    private fun LocalDate.toEpochMillis(): Long =
        atStartOfDay(ZoneId.systemDefault()).toInstant().toEpochMilli()

    fun todayStart(): Long = LocalDate.now().toEpochMillis()

    /** 週の起点は月曜(ISO 8601) */
    fun weekStart(): Long = LocalDate.now()
        .with(TemporalAdjusters.previousOrSame(DayOfWeek.MONDAY))
        .toEpochMillis()

    fun monthStart(): Long = LocalDate.now().withDayOfMonth(1).toEpochMillis()
}

object Formatters {
    private val dateTimeFormatter = DateTimeFormatter.ofPattern("M/d(E) HH:mm", Locale.JAPANESE)

    fun dateTime(epochMillis: Long): String =
        Instant.ofEpochMilli(epochMillis).atZone(ZoneId.systemDefault()).format(dateTimeFormatter)

    /** 1234.5m → "1.23 km" */
    fun distanceKm(meters: Double): String = String.format(Locale.JAPAN, "%.2f km", meters / 1000.0)

    /** 3725秒 → "1時間2分" / 185秒 → "3分5秒" */
    fun duration(seconds: Int): String {
        val h = seconds / 3600
        val m = (seconds % 3600) / 60
        val s = seconds % 60
        return when {
            h > 0 -> "${h}時間${m}分"
            m > 0 -> "${m}分${s}秒"
            else -> "${s}秒"
        }
    }

    /** 平均ペース "8'30\"/km"。距離ゼロなら "--" */
    fun pace(distanceMeters: Double, durationSeconds: Int): String {
        if (distanceMeters < 10.0 || durationSeconds <= 0) return "--"
        val secPerKm = durationSeconds / (distanceMeters / 1000.0)
        val min = (secPerKm / 60).toInt()
        val sec = (secPerKm % 60).toInt()
        return String.format(Locale.JAPAN, "%d'%02d\"/km", min, sec)
    }
}
