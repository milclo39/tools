package com.example.dogwalk.data

import androidx.room.Dao
import androidx.room.Delete
import androidx.room.Insert
import androidx.room.Query
import kotlinx.coroutines.flow.Flow

@Dao
interface WalkDao {

    @Insert
    suspend fun insert(record: WalkRecord): Long

    @Delete
    suspend fun delete(record: WalkRecord)

    @Query("SELECT * FROM walk_records ORDER BY startTime DESC")
    fun observeAll(): Flow<List<WalkRecord>>
}
