package com.example.dogwalk

import android.content.Context
import androidx.room.Room
import com.example.dogwalk.data.WalkDatabase
import com.example.dogwalk.data.WalkRepository

/** シンプルなサービスロケータ */
object Graph {

    lateinit var repository: WalkRepository
        private set

    fun init(context: Context) {
        val db = Room.databaseBuilder(
            context.applicationContext,
            WalkDatabase::class.java,
            "dogwalk.db"
        ).build()
        repository = WalkRepository(db.walkDao())
    }
}
