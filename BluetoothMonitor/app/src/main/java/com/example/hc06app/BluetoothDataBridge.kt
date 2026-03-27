package com.example.hc06app

import android.os.Handler
import android.os.Looper
import android.util.Log
import java.text.SimpleDateFormat
import java.util.*
import java.util.concurrent.CopyOnWriteArrayList

/**
 * 全局蓝牙数据中转站 (单例)
 * 解决 Activity 切换导致的数据丢失和 UI 更新冲突
 */
object BluetoothDataBridge {
    private const val TAG = "BTDataBridge"
    private const val MAX_LOG_SIZE = 500

    // 数据监听接口
    interface DataListener {
        fun onDataReceived(hr: String, spo2: String, slp: String, lgt: String, ns: String)
        fun onSnoreDetected(totalCount: Int)
        fun onStatusUpdate(status: String)
        fun onRawLog(log: String)
    }

    private val listeners = CopyOnWriteArrayList<DataListener>()
    private val logList = LinkedList<String>()
    private val mainHandler = Handler(Looper.getMainLooper())

    // 缓存最新数据
    var lastHr = "--"
    var lastSpo2 = "--"
    var lastSlp = "--"
    var lastLgt = "--"
    var lastNs = "--"
    var snoreCount = 0
    var currentStatus = "未连接"

    fun addListener(listener: DataListener) {
        if (!listeners.contains(listener)) {
            listeners.add(listener)
            // 立即同步当前状态
            listener.onStatusUpdate(currentStatus)
            listener.onDataReceived(lastHr, lastSpo2, lastSlp, lastLgt, lastNs)
            listener.onSnoreDetected(snoreCount)
        }
    }

    fun dispatchSnore(count: Int) {
        snoreCount = count
        mainHandler.post {
            listeners.forEach { it.onSnoreDetected(count) }
        }
    }

    fun removeListener(listener: DataListener) {
        listeners.remove(listener)
    }

    /**
     * 更新蓝牙连接状态
     */
    fun updateStatus(status: String) {
        currentStatus = status
        mainHandler.post {
            listeners.forEach { it.onStatusUpdate(status) }
        }
        appendLog("System: $status")
    }

    /**
     * 分发解析后的传感器数据
     */
    fun dispatchSensorData(hr: String, spo2: String, slp: String, lgt: String, ns: String) {
        lastHr = hr
        lastSpo2 = spo2
        lastSlp = slp
        lastLgt = lgt
        lastNs = ns
        
        mainHandler.post {
            listeners.forEach { it.onDataReceived(hr, spo2, slp, lgt, ns) }
        }
    }

    /**
     * 记录并分发原始日志
     */
    fun appendLog(msg: String) {
        val time = SimpleDateFormat("HH:mm:ss.SSS", Locale.getDefault()).format(Date())
        val fullLog = "[$time] $msg"
        
        synchronized(logList) {
            if (logList.size > MAX_LOG_SIZE) logList.removeFirst()
            logList.add(fullLog)
        }

        mainHandler.post {
            listeners.forEach { it.onRawLog(fullLog) }
        }
    }

    fun getFullLogs(): String {
        synchronized(logList) {
            return logList.joinToString("\n")
        }
    }

    fun clearLogs() {
        synchronized(logList) {
            logList.clear()
        }
        mainHandler.post {
            listeners.forEach { it.onRawLog("日志已清空") }
        }
    }
}
