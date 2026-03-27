package com.example.hc06app

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.media.MediaPlayer
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.widget.Button
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import com.google.android.material.card.MaterialCardView
import java.io.File
import java.io.FileOutputStream
import java.text.SimpleDateFormat
import java.util.*

class MainActivity : AppCompatActivity(), BluetoothDataBridge.DataListener {

    private companion object {
        const val TAG = "BT"
        const val RECONNECT_BASE_DELAY_MS = 2000L
        const val CONNECTION_WATCHDOG_MS = 5000L
        const val PREFS_NAME = "bt_prefs"
        const val KEY_LAST_ADDR = "last_device_addr"
        val CCCD_UUID: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
        // 常见的 BLE 串口 UUID 列表
        val BLE_SERIAL_UUIDS = listOf(
            UUID.fromString("0000ffe1-0000-1000-8000-00805f9b34fb"), // HM-10, JDY-31
            UUID.fromString("0000dfb1-0000-1000-8000-00805f9b34fb"), // Bluno
            UUID.fromString("6e400003-b5a3-f393-e0a9-e50e24dcca9e")  // Nordic UART
        )
    }

    // UI 组件
    private lateinit var tvStatus: TextView
    private lateinit var tvHeartValue: TextView
    private lateinit var tvSpO2Value: TextView
    private lateinit var tvSleepValue: TextView
    private lateinit var tvLightValue: TextView
    private lateinit var tvNoiseValue: TextView
    private lateinit var tvMusicStatus: TextView
    private lateinit var tvSnoreCount: TextView
    private lateinit var btnConnect: Button
    private lateinit var btnLogData: Button
    private lateinit var btnViewLog: android.view.View
    private lateinit var sleepPieChart: SleepPieChartView
    private lateinit var cardHeartRate: android.view.View
    private lateinit var cardSpO2: android.view.View
    private lateinit var cardSleep: android.view.View

    // 蓝牙相关
    private var bluetoothAdapter: BluetoothAdapter? = null
    private var bluetoothGatt: BluetoothGatt? = null
    private val HC06_NAME = "HC-06" // 蓝牙模块名称
    @Volatile private var isConnecting = false
    @Volatile private var shouldAutoReconnect = true
    private var reconnectAttempts = 0
    private var lastConnectedDevice: BluetoothDevice? = null
    private var reconnectRunnable: Runnable? = null
    private var watchdogRunnable: Runnable? = null
    @Volatile private var bleConnected = false
    @Volatile private var reconnectScheduled = false
    private val incomingLineBuffer = StringBuilder()
    private var bleScanCallback: ScanCallback? = null
    private var bleScanTimeoutRunnable: Runnable? = null

    // 数据记录
    private var isLogging = false
    private var logFile: File? = null

    // 音乐播放
    private var mediaPlayer: MediaPlayer? = null

    // 权限请求码
    private val REQUEST_PERMISSION = 2

    // 保证在主线程更新 UI
    private val handler = Handler(Looper.getMainLooper())

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        // 初始化控件
        tvStatus = findViewById(R.id.tvStatus)
        tvHeartValue = findViewById(R.id.tvHeartValue)
        tvSpO2Value = findViewById(R.id.tvSpO2Value)
        tvSleepValue = findViewById(R.id.tvSleepValue)
        tvLightValue = findViewById(R.id.tvLightValue)
        tvNoiseValue = findViewById(R.id.tvNoiseValue)
        tvMusicStatus = findViewById(R.id.tvMusicStatus)
        tvSnoreCount = findViewById(R.id.tvSnoreCount)
        btnConnect = findViewById(R.id.btnConnect)
        btnLogData = findViewById(R.id.btnLogData)
        btnViewLog = findViewById(R.id.btnViewLog)
        sleepPieChart = findViewById(R.id.sleepPieChart)
        cardHeartRate = findViewById(R.id.cardHeartRate)
        cardSpO2 = findViewById(R.id.cardSpO2)
        cardSleep = findViewById(R.id.cardSleep)

        // 绑定 BluetoothDataBridge
        BluetoothDataBridge.addListener(this)

        // 按钮事件
        btnConnect.setOnClickListener { checkPermissionsAndConnect() }
        btnLogData.setOnClickListener { toggleLogging() }
        btnViewLog.setOnClickListener {
            startActivity(Intent(this, LogActivity::class.java))
        }
        cardHeartRate.setOnClickListener {
            openTrendChart("心率趋势", "bpm", TrendChartActivity.METRIC_HEART_RATE)
        }
        cardSpO2.setOnClickListener {
            openTrendChart("血氧趋势", "%", TrendChartActivity.METRIC_SPO2)
        }
        cardSleep.setOnClickListener {
            openSleepDetail()
        }

        // 获取蓝牙适配器
        val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothAdapter = bluetoothManager.adapter

        watchdogRunnable = Runnable {
            if (shouldAutoReconnect && !bleConnected && !isConnecting) {
                Log.w(TAG, "watchdog: BLE disconnected, trigger reconnect")
                scheduleReconnect("watchdog")
            }
            handler.postDelayed(watchdogRunnable!!, CONNECTION_WATCHDOG_MS)
        }
        handler.postDelayed(watchdogRunnable!!, CONNECTION_WATCHDOG_MS)
    }

    // 实现 DataListener 接口，让 DataBridge 统一通知 UI
    override fun onDataReceived(hr: String, spo2: String, slp: String, lgt: String, ns: String) {
        tvHeartValue.text = hr
        tvSpO2Value.text = spo2
        tvSleepValue.text = slp
        tvLightValue.text = lgt
        tvNoiseValue.text = ns
    }

    override fun onSnoreDetected(totalCount: Int) {
        tvSnoreCount.text = totalCount.toString()
    }

    override fun onStatusUpdate(status: String) {
        tvStatus.text = status
        if (status.contains("已连接")) {
            btnConnect.isEnabled = false
        } else if (status.contains("断开")) {
            btnConnect.isEnabled = true
        }
    }

    override fun onRawLog(log: String) {
        // MainActivity 只需要接收数据，暂时不需要处理原始日志
    }

    private fun checkPermissionsAndConnect() {
        val permissions = mutableListOf<String>()

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
                permissions.add(Manifest.permission.BLUETOOTH_CONNECT)
            }
            if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED) {
                permissions.add(Manifest.permission.BLUETOOTH_SCAN)
            }
        } else {
            if (ActivityCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
                permissions.add(Manifest.permission.ACCESS_FINE_LOCATION)
            }
        }

        if (permissions.isNotEmpty()) {
            ActivityCompat.requestPermissions(this, permissions.toTypedArray(), REQUEST_PERMISSION)
        } else {
            connectToHC06()
        }
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        val allGranted = grantResults.isNotEmpty() && grantResults.all { it == PackageManager.PERMISSION_GRANTED }
        if (requestCode == REQUEST_PERMISSION && allGranted) {
            connectToHC06()
        } else {
            Toast.makeText(this, "需要蓝牙权限才能工作！", Toast.LENGTH_SHORT).show()
        }
    }

    @SuppressLint("MissingPermission")
    private fun connectToHC06() {
        reconnectRunnable?.let { handler.removeCallbacks(it) }
        reconnectScheduled = false

        if (isConnecting) {
            Toast.makeText(this, "正在连接中，请稍候", Toast.LENGTH_SHORT).show()
            return
        }

        if (bleConnected) {
            tvStatus.text = "✅ 蓝牙已连接"
            btnConnect.isEnabled = false
            Toast.makeText(this, "设备已连接", Toast.LENGTH_SHORT).show()
            return
        }

        if (bluetoothAdapter == null || !bluetoothAdapter!!.isEnabled) {
            Toast.makeText(this, "请先在手机设置中打开蓝牙", Toast.LENGTH_SHORT).show()
            return
        }

        tvStatus.text = "正在搜索配对设备..."
        btnConnect.isEnabled = false

        val pairedDevices: Set<BluetoothDevice>? = bluetoothAdapter?.bondedDevices
        val hc06Device = findTargetDevice(pairedDevices)

        if (hc06Device == null) {
            tvStatus.text = "开始BLE扫描..."
            startBleScanAndConnect()
        } else {
            lastConnectedDevice = hc06Device
            saveLastDeviceAddress(hc06Device.address)
            connectBleFallback(hc06Device)
        }
    }

    @SuppressLint("MissingPermission")
    private fun connectToDevice(device: BluetoothDevice) {
        if (isConnecting) return
        Log.i(TAG, "connectToDevice(BLE) start: ${device.name} ${device.address}")
        isConnecting = true
        reconnectScheduled = false
        connectBleFallback(device)
    }

    @SuppressLint("MissingPermission")
    private fun connectBleFallback(device: BluetoothDevice) {
        try {
            bluetoothGatt?.close()
        } catch (_: Exception) {}

        bleConnected = false
        BluetoothDataBridge.updateStatus("正在连接: ${device.name ?: "Unknown"}")
        bluetoothGatt = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            device.connectGatt(this, false, bleGattCallback, BluetoothDevice.TRANSPORT_LE)
        } else {
            device.connectGatt(this, false, bleGattCallback)
        }

        btnConnect.isEnabled = false
    }

    @SuppressLint("MissingPermission")
    private fun startBleScanAndConnect() {
        val scanner = bluetoothAdapter?.bluetoothLeScanner
        if (scanner == null) {
            tvStatus.text = "BLE扫描不可用"
            btnConnect.isEnabled = true
            return
        }

        bleScanCallback?.let { scanner.stopScan(it) }
        bleScanTimeoutRunnable?.let { handler.removeCallbacks(it) }

        bleScanCallback = object : ScanCallback() {
            override fun onScanResult(callbackType: Int, result: ScanResult) {
                val device = result.device ?: return
                val name = result.scanRecord?.deviceName ?: device.name
                if (name != null && isTargetDeviceName(name)) {
                    Log.i(TAG, "BLE scan hit: $name ${device.address}")
                    scanner.stopScan(this)
                    bleScanCallback = null
                    bleScanTimeoutRunnable?.let { handler.removeCallbacks(it) }
                    lastConnectedDevice = device
                    saveLastDeviceAddress(device.address)
                    connectToDevice(device)
                }
            }

            override fun onScanFailed(errorCode: Int) {
                Log.e(TAG, "BLE scan failed: $errorCode")
                tvStatus.text = "BLE扫描失败: $errorCode，尝试直连"
                isConnecting = false
                if (!fallbackConnectWithoutScan()) {
                    btnConnect.isEnabled = true
                    scheduleReconnect("scan_failed_$errorCode")
                }
            }
        }

        scanner.startScan(bleScanCallback)
        tvStatus.text = "BLE扫描中..."
        bleScanTimeoutRunnable = Runnable {
            bleScanCallback?.let { scanner.stopScan(it) }
            bleScanCallback = null
            if (lastConnectedDevice == null) {
                tvStatus.text = "未扫描到HC设备"
                isConnecting = false
                if (!fallbackConnectWithoutScan()) {
                    btnConnect.isEnabled = true
                }
            }
        }
        handler.postDelayed(bleScanTimeoutRunnable!!, 10000L)
    }

    @SuppressLint("MissingPermission")
    private fun fallbackConnectWithoutScan(): Boolean {
        val adapter = bluetoothAdapter ?: return false
        val byMemory = lastConnectedDevice
        if (byMemory != null) {
            connectToDevice(byMemory)
            return true
        }

        val addr = getLastDeviceAddress()
        if (!addr.isNullOrBlank()) {
            return try {
                val device = adapter.getRemoteDevice(addr)
                lastConnectedDevice = device
                connectToDevice(device)
                true
            } catch (e: Exception) {
                Log.w(TAG, "fallback by address failed: ${e.message}")
                false
            }
        }
        return false
    }

    private fun saveLastDeviceAddress(address: String?) {
        if (address.isNullOrBlank()) return
        getSharedPreferences(PREFS_NAME, MODE_PRIVATE)
            .edit()
            .putString(KEY_LAST_ADDR, address)
            .apply()
    }

    private fun getLastDeviceAddress(): String? {
        return getSharedPreferences(PREFS_NAME, MODE_PRIVATE)
            .getString(KEY_LAST_ADDR, null)
    }

    private fun scheduleReconnect(reason: String) {
        if (!shouldAutoReconnect) return
        if (reconnectScheduled) {
            Log.i(TAG, "[$reason] reconnect already scheduled")
            return
        }

        reconnectAttempts++
        val delay = (RECONNECT_BASE_DELAY_MS + reconnectAttempts.coerceAtMost(10) * 500L).coerceAtMost(10000L)
        Log.w(TAG, "[$reason] 计划重连: 第${reconnectAttempts}次, delay=${delay}ms")
        reconnectScheduled = true
        reconnectRunnable = Runnable {
            reconnectScheduled = false
            if (shouldAutoReconnect && !bleConnected) {
                handler.post {
                    tvStatus.text = "正在自动重连($reconnectAttempts)..."
                }
                val device = lastConnectedDevice
                if (device != null) {
                    connectToDevice(device)
                } else {
                    startBleScanAndConnect()
                }
            }
        }
        handler.postDelayed(reconnectRunnable!!, delay)
    }

    @SuppressLint("MissingPermission")
    private fun findTargetDevice(devices: Set<BluetoothDevice>?): BluetoothDevice? {
        if (devices.isNullOrEmpty()) return null
        devices.firstOrNull { isTargetDeviceName(it.name ?: "") }?.let { return it }
        return null
    }

    private fun isTargetDeviceName(name: String): Boolean {
        if (name.isBlank()) return false
        val n = name.lowercase(Locale.getDefault())
        return n == HC06_NAME.lowercase(Locale.getDefault()) ||
                n.contains("hc-06") || n.contains("hc06") ||
                n.contains("linvor") || n.contains("jdy") ||
                n.contains("at-09") || n.contains("hmsoft")
    }

    private fun processIncomingChunk(chunk: String) {
        if (chunk.isEmpty()) return
        BluetoothDataBridge.appendLog("RX: $chunk")
        incomingLineBuffer.append(chunk)

        var end = findLineEnd(incomingLineBuffer)
        while (end > 0) {
            val line = incomingLineBuffer.substring(0, end).trim()
            var removeLen = end + 1
            if (removeLen < incomingLineBuffer.length &&
                ((incomingLineBuffer[end] == '\r' && incomingLineBuffer[removeLen] == '\n') ||
                 (incomingLineBuffer[end] == '\n' && incomingLineBuffer[removeLen] == '\r')))
            {
                removeLen += 1
            }
            incomingLineBuffer.delete(0, removeLen)
            if (line.isNotEmpty()) {
                BluetoothDataBridge.appendLog("Line: $line")
                processReceivedData(line)
            }
            end = findLineEnd(incomingLineBuffer)
        }
    }

    private fun findLineEnd(sb: StringBuilder): Int {
        val rn = sb.indexOf("\r\n")
        if (rn >= 0) return rn
        val nr = sb.indexOf("\n\r")
        if (nr >= 0) return nr
        val r = sb.indexOf("\r")
        if (r >= 0) return r
        return sb.indexOf("\n")
    }

    @SuppressLint("MissingPermission")
    private val bleGattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            isConnecting = false
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                bleConnected = true
                reconnectAttempts = 0
                reconnectScheduled = false
                BluetoothDataBridge.updateStatus("✅ 已连接: ${gatt.device?.name ?: "Unknown"}")
                gatt.discoverServices()
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                bleConnected = false
                BluetoothDataBridge.updateStatus("❌ 连接断开")
                try {
                    gatt.close()
                } catch (_: Exception) {}
                if (bluetoothGatt === gatt) {
                    bluetoothGatt = null
                }
                scheduleReconnect("ble_disconnected")
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                Log.w(TAG, "BLE service discover failed: $status")
                return
            }

            var subscribed = false
            for (service in gatt.services) {
                Log.d(TAG, "Service: ${service.uuid}")
                for (ch in service.characteristics) {
                    val props = ch.properties
                    Log.d(TAG, "  Char: ${ch.uuid}, Props: $props")
                    
                    val hasNotify = (props and BluetoothGattCharacteristic.PROPERTY_NOTIFY) != 0
                    val hasIndicate = (props and BluetoothGattCharacteristic.PROPERTY_INDICATE) != 0
                    val hasRead = (props and BluetoothGattCharacteristic.PROPERTY_READ) != 0

                    // 只要发现能通知/指示的特征，或者 UUID 在常见串口列表里的，都尝试订阅
                    if (hasNotify || hasIndicate || BLE_SERIAL_UUIDS.contains(ch.uuid)) {
                        try {
                            if (gatt.setCharacteristicNotification(ch, true)) {
                                val cccd = ch.getDescriptor(CCCD_UUID)
                                if (cccd != null) {
                                    cccd.value = if (hasNotify) {
                                        BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                                    } else {
                                        BluetoothGattDescriptor.ENABLE_INDICATION_VALUE
                                    }
                                    gatt.writeDescriptor(cccd)
                                }
                                subscribed = true
                                Log.i(TAG, "Successfully subscribed to ${ch.uuid}")
                            }
                        } catch (e: Exception) {
                            Log.w(TAG, "BLE notify setup fail ${ch.uuid}: ${e.message}")
                        }
                    }

                    if (hasRead) {
                        try {
                            gatt.readCharacteristic(ch)
                        } catch (_: Exception) {}
                    }
                }
            }

            handler.post {
                tvStatus.text = if (subscribed) "✅ BLE订阅成功" else "✅ BLE已连(无通知特征)"
            }
        }

        override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
            val value = characteristic.value ?: return
            processIncomingChunk(String(value))
        }

        @Deprecated("Deprecated in Java")
        override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray) {
            processIncomingChunk(String(value))
        }

        @Deprecated("Deprecated in Java")
        override fun onCharacteristicRead(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                val value = characteristic.value ?: return
                processIncomingChunk(String(value))
            }
        }

        override fun onCharacteristicRead(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                processIncomingChunk(String(value))
            }
        }
    }

    private fun processReceivedData(data: String) {
        if (data.startsWith("DATA:")) {
            try {
                val valuesStr = data.substring(5)
                val params = valuesStr.split(",")
                var hr = "--"
                var spo2 = "--"
                var slp = "--"
                var lgt = "--"
                var ns = "--"

                for (param in params) {
                    val kv = param.split("=")
                    if (kv.size == 2) {
                        when (kv[0]) {
                            "HR" -> hr = kv[1]
                            "SPO2" -> spo2 = kv[1]
                            "SLP" -> {
                                val stage = kv[1].toIntOrNull() ?: 0
                                slp = when(stage) {
                                    0 -> "清醒"
                                    1 -> "浅睡"
                                    2 -> "深睡"
                                    else -> "未知"
                                }
                                TrendDataRepository.appendSleepStage(stage)
                            }
                             "LGT" -> lgt = kv[1]
                             "NS" -> ns = kv[1]
                             "SNR" -> {
                                 val snore = kv[1].toIntOrNull() ?: 0
                                 BluetoothDataBridge.dispatchSnore(snore)
                             }
                         }
                    }
                }
                BluetoothDataBridge.dispatchSensorData(hr, spo2, slp, lgt, ns)
                hr.toFloatOrNull()?.let { TrendDataRepository.appendHeartRate(it) }
                spo2.toFloatOrNull()?.let { TrendDataRepository.appendSpO2(it) }
                
                // 更新圆环饼图
                updatePieChart()
                
                if (isLogging) logDataToFile(hr, spo2, slp, lgt, ns)
            } catch (e: Exception) {
                Log.e("Parser", "解析数据出错: $data", e)
                BluetoothDataBridge.appendLog("Error Parsing: $data")
            }
        } else if (data.startsWith("CMD:")) {
            val cmd = data.substring(4)
            handler.post {
                if (cmd == "PLAY") playMusic() else if (cmd == "STOP") stopMusic()
            }
        }
    }

    private fun openTrendChart(title: String, unit: String, metric: String) {
        val hasData = when (metric) {
            TrendChartActivity.METRIC_HEART_RATE -> TrendDataRepository.heartRateSnapshot().second.isNotEmpty()
            TrendChartActivity.METRIC_SPO2 -> TrendDataRepository.spo2Snapshot().second.isNotEmpty()
            else -> false
        }
        if (!hasData) {
            Toast.makeText(this, "暂无趋势数据，请先连接并接收数据", Toast.LENGTH_SHORT).show()
            return
        }
        val intent = Intent(this, TrendChartActivity::class.java)
        intent.putExtra("chart_title", title)
        intent.putExtra("chart_unit", unit)
        intent.putExtra("chart_metric", metric)
        startActivity(intent)
    }

    private fun openSleepDetail() {
        if (TrendDataRepository.sleepSnapshot().second.isEmpty()) {
            Toast.makeText(this, "暂无睡眠数据", Toast.LENGTH_SHORT).show()
            return
        }
        startActivity(Intent(this, SleepDetailActivity::class.java))
    }

    private fun playMusic() {
        tvMusicStatus.text = "音乐/警报：正在播放"
        tvMusicStatus.setTextColor(android.graphics.Color.RED)
    }

    private fun stopMusic() {
        tvMusicStatus.text = "音乐/警报：已停止"
        tvMusicStatus.setTextColor(android.graphics.Color.GRAY)
    }

    private fun toggleLogging() {
        if (isLogging) {
            isLogging = false
            btnLogData.text = "开始记录数据 (CSV)"
            btnLogData.setBackgroundColor(android.graphics.Color.parseColor("#4CAF50"))
            Toast.makeText(this, "记录已停止，文件保存在: ${logFile?.absolutePath}", Toast.LENGTH_LONG).show()
        } else {
            val dir = getExternalFilesDir(Environment.DIRECTORY_DOCUMENTS)
            val timeStamp = SimpleDateFormat("yyyyMMdd_HHmmss", Locale.getDefault()).format(Date())
            logFile = File(dir, "SensorData_$timeStamp.csv")
            try {
                val fos = FileOutputStream(logFile, true)
                fos.write("时间,心率(bpm),血氧(%),睡眠状态,光照(Lux),音量(dB)\n".toByteArray())
                fos.close()
                isLogging = true
                btnLogData.text = "🛑 停止记录数据"
                btnLogData.setBackgroundColor(android.graphics.Color.RED)
                Toast.makeText(this, "开始记录...", Toast.LENGTH_SHORT).show()
            } catch (e: Exception) {
                Toast.makeText(this, "创建文件失败", Toast.LENGTH_SHORT).show()
            }
        }
    }

    private fun logDataToFile(hr: String, spo2: String, slp: String, lgt: String, ns: String) {
        try {
            val fos = FileOutputStream(logFile, true)
            val time = SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.getDefault()).format(Date())
            fos.write("$time,$hr,$spo2,$slp,$lgt,$ns\n".toByteArray())
            fos.close()
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    private fun updatePieChart() {
        val session = TrendDataRepository.latestSleepSessionSnapshot() ?: return
        val stages = session.stages
        if (stages.isEmpty()) return

        var deep = 0f
        var light = 0f
        var awake = 0f

        for (s in stages) {
            when (s) {
                0 -> awake++
                1 -> light++
                2 -> deep++
            }
        }
        
        handler.post {
            sleepPieChart.setData(deep, light, awake)
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        BluetoothDataBridge.removeListener(this)
        shouldAutoReconnect = false
        isConnecting = false
        bleConnected = false
        reconnectRunnable?.let { handler.removeCallbacks(it) }
        watchdogRunnable?.let { handler.removeCallbacks(it) }
        bleScanTimeoutRunnable?.let { handler.removeCallbacks(it) }
        try { bluetoothGatt?.close() } catch (_: Exception) {}
        bluetoothGatt = null
        mediaPlayer?.release()
        mediaPlayer = null
    }
}
