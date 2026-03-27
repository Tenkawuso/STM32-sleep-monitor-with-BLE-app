package com.example.hc06app

import java.util.ArrayDeque

object TrendDataRepository {
    private const val MAX_POINTS = 120
    private const val MAX_SLEEP_POINTS = 30000
    private const val SLEEP_START_DEBOUNCE_SAMPLES = 3
    private const val SLEEP_END_DEBOUNCE_SAMPLES = 5

    data class SleepSessionSnapshot(
        val startTime: Long,
        val endTime: Long?,
        val times: List<Long>,
        val stages: List<Int>,
        val isCompleted: Boolean
    )

    private data class MutableSleepSession(
        val startTime: Long,
        var endTime: Long?,
        val times: MutableList<Long>,
        val stages: MutableList<Int>
    )

    private val heartRateTimes = ArrayDeque<Long>()
    private val heartRateValues = ArrayDeque<Float>()

    private val spo2Times = ArrayDeque<Long>()
    private val spo2Values = ArrayDeque<Float>()

    private val sleepTimes = ArrayDeque<Long>()
    private val sleepStages = ArrayDeque<Int>()

    private val completedSleepSessions = mutableListOf<MutableSleepSession>()
    private var currentSleepSession: MutableSleepSession? = null

    private val pendingStartTimes = ArrayDeque<Long>()
    private val pendingStartStages = ArrayDeque<Int>()

    private val pendingEndTimes = ArrayDeque<Long>()

    @Synchronized
    fun appendHeartRate(value: Float) {
        append(heartRateTimes, heartRateValues, value)
    }

    @Synchronized
    fun appendSpO2(value: Float) {
        append(spo2Times, spo2Values, value)
    }

    @Synchronized
    fun heartRateSnapshot(): Pair<List<Long>, List<Float>> {
        return heartRateTimes.toList() to heartRateValues.toList()
    }

    @Synchronized
    fun spo2Snapshot(): Pair<List<Long>, List<Float>> {
        return spo2Times.toList() to spo2Values.toList()
    }

    @Synchronized
    fun appendSleepStage(stage: Int) {
        val now = System.currentTimeMillis()

        sleepTimes.addLast(now)
        sleepStages.addLast(stage)

        while (sleepTimes.size > MAX_SLEEP_POINTS) {
            sleepTimes.removeFirst()
            sleepStages.removeFirst()
        }

        processSleepSessionWithDebounce(now, stage)
    }

    @Synchronized
    fun sleepSnapshot(): Pair<List<Long>, List<Int>> {
        return sleepTimes.toList() to sleepStages.toList()
    }

    @Synchronized
    fun latestSleepSessionSnapshot(): SleepSessionSnapshot? {
        val latestCompleted = completedSleepSessions.lastOrNull()
        val session = latestCompleted ?: currentSleepSession ?: return null
        return SleepSessionSnapshot(
            startTime = session.startTime,
            endTime = session.endTime,
            times = session.times.toList(),
            stages = session.stages.toList(),
            isCompleted = latestCompleted != null
        )
    }

    private fun append(timeDeque: ArrayDeque<Long>, valueDeque: ArrayDeque<Float>, value: Float) {
        timeDeque.addLast(System.currentTimeMillis())
        valueDeque.addLast(value)

        while (timeDeque.size > MAX_POINTS) {
            timeDeque.removeFirst()
            valueDeque.removeFirst()
        }
    }

    private fun processSleepSessionWithDebounce(now: Long, stage: Int) {
        val isSleepStage = stage == 1 || stage == 2

        if (currentSleepSession == null) {
            if (isSleepStage) {
                pendingStartTimes.addLast(now)
                pendingStartStages.addLast(stage)

                while (pendingStartTimes.size > SLEEP_START_DEBOUNCE_SAMPLES) {
                    pendingStartTimes.removeFirst()
                    pendingStartStages.removeFirst()
                }

                if (pendingStartTimes.size >= SLEEP_START_DEBOUNCE_SAMPLES) {
                    val session = MutableSleepSession(
                        startTime = pendingStartTimes.first(),
                        endTime = null,
                        times = pendingStartTimes.toMutableList(),
                        stages = pendingStartStages.toMutableList()
                    )
                    currentSleepSession = session
                    pendingStartTimes.clear()
                    pendingStartStages.clear()
                }
            } else {
                pendingStartTimes.clear()
                pendingStartStages.clear()
            }
            return
        }

        val session = currentSleepSession ?: return
        session.times.add(now)
        session.stages.add(stage)

        if (stage == 0) {
            pendingEndTimes.addLast(now)
            while (pendingEndTimes.size > SLEEP_END_DEBOUNCE_SAMPLES) {
                pendingEndTimes.removeFirst()
            }

            if (pendingEndTimes.size >= SLEEP_END_DEBOUNCE_SAMPLES) {
                session.endTime = pendingEndTimes.first()
                completedSleepSessions.add(session)
                currentSleepSession = null
                pendingEndTimes.clear()
            }
        } else {
            pendingEndTimes.clear()
        }
    }
}
