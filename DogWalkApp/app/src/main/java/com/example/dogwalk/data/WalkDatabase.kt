package com.example.dogwalk.data

import androidx.room.Database
import androidx.room.RoomDatabase

@Database(entities = [WalkRecord::class], version = 1, exportSchema = false)
abstract class WalkDatabase : RoomDatabase() {
    abstract fun walkDao(): WalkDao
}
