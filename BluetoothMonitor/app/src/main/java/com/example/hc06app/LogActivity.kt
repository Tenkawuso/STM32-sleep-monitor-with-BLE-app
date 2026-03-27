package com.example.hc06app

import android.os.Bundle
import android.widget.Button
import android.widget.ScrollView
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity

class LogActivity : AppCompatActivity(), BluetoothDataBridge.DataListener {

    private lateinit var tvLogContent: TextView
    private lateinit var scrollLog: ScrollView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_log)

        tvLogContent = findViewById(R.id.tvLogContent)
        scrollLog = findViewById(R.id.scrollLog)

        // 绑定 BluetoothDataBridge
        BluetoothDataBridge.addListener(this)

        // 初始化已有日志
        tvLogContent.text = BluetoothDataBridge.getFullLogs()

        findViewById<Button>(R.id.btnClearLog).setOnClickListener {
            BluetoothDataBridge.clearLogs()
            tvLogContent.text = ""
        }
    }

    override fun onRawLog(log: String) {
        // 全量刷新或增量刷新取决于需求，这里采用增量追加
        tvLogContent.append("\n$log")
        scrollLog.post {
            scrollLog.fullScroll(ScrollView.FOCUS_DOWN)
        }
    }

    // 实现了 DataListener 接口的其他方法 (在 LogActivity 中暂时只关注 rawLog)
    override fun onDataReceived(hr: String, spo2: String, slp: String, lgt: String, ns: String) {}
    override fun onStatusUpdate(status: String) {}

    override fun onDestroy() {
        super.onDestroy()
        // 解绑 BluetoothDataBridge 防止内存泄漏
        BluetoothDataBridge.removeListener(this)
    }
}
