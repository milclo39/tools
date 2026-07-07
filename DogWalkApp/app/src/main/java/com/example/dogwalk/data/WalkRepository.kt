package com.example.dogwalk.data

import kotlinx.coroutines.flow.Flow

class WalkRepository(private val dao: WalkDao) {

    /** 全履歴 (新しい順)。統計はこのFlowからメモリ上で集計する。 */
    val allRecords: Flow<List<WalkRecord>> = dao.observeAll()

    suspend fun insert(record: WalkRecord): Long = dao.insert(record)

    suspend fun delete(record: WalkRecord) = dao.delete(record)
}
