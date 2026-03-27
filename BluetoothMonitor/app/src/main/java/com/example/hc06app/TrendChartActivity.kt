package com.example.hc06app

import android.graphics.Color
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import com.github.mikephil.charting.charts.LineChart
import com.github.mikephil.charting.components.AxisBase
import com.github.mikephil.charting.formatter.ValueFormatter
import com.github.mikephil.charting.components.XAxis
import com.github.mikephil.charting.data.Entry
import com.github.mikephil.charting.data.LineData
import com.github.mikephil.charting.data.LineDataSet
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

class TrendChartActivity : AppCompatActivity() {

    companion object {
        const val METRIC_HEART_RATE = "heart_rate"
        const val METRIC_SPO2 = "spo2"
    }

    private lateinit var chart: LineChart
    private lateinit var chartTitle: String
    private lateinit var chartUnit: String
    private lateinit var chartMetric: String

    private val refreshHandler = Handler(Looper.getMainLooper())
    private val refreshTask = object : Runnable {
        override fun run() {
            updateChartData()
            refreshHandler.postDelayed(this, 1000)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_trend_chart)

        chartTitle = intent.getStringExtra("chart_title") ?: "趋势图"
        chartUnit = intent.getStringExtra("chart_unit") ?: ""
        chartMetric = intent.getStringExtra("chart_metric") ?: METRIC_HEART_RATE

        val tvChartTitle = findViewById<TextView>(R.id.tvChartTitle)
        chart = findViewById(R.id.lineChart)

        tvChartTitle.text = chartTitle
        setupChart(chart)
        updateChartData()
    }

    override fun onStart() {
        super.onStart()
        refreshHandler.post(refreshTask)
    }

    override fun onStop() {
        super.onStop()
        refreshHandler.removeCallbacks(refreshTask)
    }

    private fun setupChart(chart: LineChart) {
        chart.apply {
            description.isEnabled = false
            legend.textColor = Color.parseColor("#8E8E93")
            setNoDataText("暂无可用数据")
            setNoDataTextColor(Color.parseColor("#6A7D98"))
            setTouchEnabled(true)
            setPinchZoom(true)
            setBackgroundColor(Color.TRANSPARENT)

            axisRight.isEnabled = false
            axisLeft.apply {
                textColor = Color.parseColor("#8E8E93")
                gridColor = Color.parseColor("#1AFFFFFF")
                setDrawAxisLine(false)
            }

            xAxis.apply {
                position = XAxis.XAxisPosition.BOTTOM
                textColor = Color.parseColor("#8E8E93")
                gridColor = Color.parseColor("#1AFFFFFF")
                setDrawAxisLine(false)
                granularity = 1f
                labelRotationAngle = 0f
            }
        }
    }

    private fun updateChartData() {
        val (times, values) = when (chartMetric) {
            METRIC_HEART_RATE -> TrendDataRepository.heartRateSnapshot()
            METRIC_SPO2 -> TrendDataRepository.spo2Snapshot()
            else -> TrendDataRepository.heartRateSnapshot()
        }

        if (values.isEmpty() || times.isEmpty()) {
            chart.clear()
            chart.invalidate()
            return
        }

        val firstTime = times.first()
        val entries = values.mapIndexed { index, value ->
            val secondsFromStart = ((times[index] - firstTime) / 1000f)
            Entry(secondsFromStart, value)
        }

        // Heart Rate: Red/Pink, SpO2: Blue
        val lineColor = if (chartMetric == METRIC_HEART_RATE) {
            Color.parseColor("#FF2D55") // iOS Pink/Red
        } else {
            Color.parseColor("#007AFF") // iOS Blue
        }

        val dataSet = LineDataSet(entries, "$chartTitle ($chartUnit)").apply {
            color = lineColor
            valueTextColor = Color.WHITE
            lineWidth = 3f
            setDrawCircles(false)
            setDrawValues(false)
            mode = LineDataSet.Mode.CUBIC_BEZIER
            setDrawFilled(true)
            fillColor = lineColor
            fillAlpha = 40
        }

        val timeFormatter = object : ValueFormatter() {
            private val sdf = SimpleDateFormat("HH:mm:ss", Locale.getDefault())
            override fun getAxisLabel(value: Float, axis: AxisBase?): String {
                val timestamp = firstTime + (value * 1000L).toLong()
                return sdf.format(Date(timestamp))
            }
        }

        chart.apply {
            data = LineData(dataSet)
            xAxis.valueFormatter = timeFormatter
            xAxis.labelCount = 4
            setVisibleXRangeMaximum(60f)

            val latestX = entries.last().x
            if (latestX > 60f) {
                moveViewToX(latestX - 60f)
            } else {
                moveViewToX(0f)
            }

            invalidate()
        }
    }
}
