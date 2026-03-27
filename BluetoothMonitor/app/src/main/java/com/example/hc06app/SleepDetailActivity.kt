package com.example.hc06app

import android.graphics.Color
import android.os.Bundle
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import com.github.mikephil.charting.charts.LineChart
import com.github.mikephil.charting.charts.PieChart
import com.github.mikephil.charting.components.AxisBase
import com.github.mikephil.charting.components.XAxis
import com.github.mikephil.charting.data.Entry
import com.github.mikephil.charting.data.LineData
import com.github.mikephil.charting.data.LineDataSet
import com.github.mikephil.charting.data.PieData
import com.github.mikephil.charting.data.PieDataSet
import com.github.mikephil.charting.data.PieEntry
import com.github.mikephil.charting.formatter.ValueFormatter
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

class SleepDetailActivity : AppCompatActivity() {

    private lateinit var chart: LineChart
    private lateinit var pieChart: PieChart
    private lateinit var tvSessionSummary: TextView
    private val timeFormat = SimpleDateFormat("HH:mm:ss", Locale.getDefault())
    private val dateTimeFormat = SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.getDefault())

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_sleep_detail)

        chart = findViewById(R.id.sleepChart)
        pieChart = findViewById(R.id.sleepPieChart)
        tvSessionSummary = findViewById(R.id.tvSessionSummary)
        
        findViewById<android.view.View>(R.id.btnBack).setOnClickListener {
            finish()
        }

        setupPieChart()
        setupChart()
        updateSleepPieChart()
        updateSleepChart()
    }

    private fun setupPieChart() {
        pieChart.apply {
            description.isEnabled = false
            isDrawHoleEnabled = true
            holeRadius = 60f
            setHoleColor(Color.TRANSPARENT) // 透明背景
            setTransparentCircleAlpha(0)
            setUsePercentValues(true)
            setEntryLabelColor(Color.WHITE)
            setEntryLabelTextSize(11f)
            legend.isEnabled = false // 苹果风格通常不显示底部图例
            setNoDataText("暂无睡眠分布数据")
            setNoDataTextColor(Color.parseColor("#6A7D98"))
            centerText = "阶段分布"
            setCenterTextColor(Color.WHITE)
            setCenterTextSize(14f)
        }
    }

    private fun setupChart() {
        chart.apply {
            description.isEnabled = false
            legend.isEnabled = false
            setNoDataText("暂无趋势数据")
            setNoDataTextColor(Color.parseColor("#6A7D98"))
            setTouchEnabled(true)
            setPinchZoom(true)
            setBackgroundColor(Color.TRANSPARENT)

            axisRight.isEnabled = false
            axisLeft.apply {
                axisMinimum = -0.5f
                axisMaximum = 2.5f
                granularity = 1f
                textColor = Color.parseColor("#8E8E93") // iOS grey
                gridColor = Color.parseColor("#1AFFFFFF") // Subtle grid
                setDrawAxisLine(false)
                valueFormatter = object : ValueFormatter() {
                    override fun getAxisLabel(value: Float, axis: AxisBase?): String {
                        return when (value.toInt()) {
                            0 -> "Awake"
                            1 -> "Light"
                            2 -> "Deep"
                            else -> ""
                        }
                    }
                }
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

    private fun updateSleepChart() {
        val session = TrendDataRepository.latestSleepSessionSnapshot()
        val times = session?.times ?: emptyList()
        val stages = session?.stages ?: emptyList()
        if (times.isEmpty() || stages.isEmpty()) {
            chart.clear()
            chart.invalidate()
            tvSessionSummary.text = "当前没有可展示的睡眠会话"
            return
        }

        updateSummary(session)

        val firstTime = times.first()
        val entries = stages.mapIndexed { index, stage ->
            val secondsFromStart = ((times[index] - firstTime) / 1000f)
            Entry(secondsFromStart, stage.toFloat())
        }

        val dataSet = LineDataSet(entries, "睡眠状态变化").apply {
            color = Color.parseColor("#AF52DE") // iOS Purple
            valueTextColor = Color.WHITE
            lineWidth = 3f
            setDrawCircles(false)
            setDrawValues(false)
            mode = LineDataSet.Mode.STEPPED
            setDrawFilled(true)
            fillColor = Color.parseColor("#AF52DE")
            fillAlpha = 40
        }

        val timeFormatter = object : ValueFormatter() {
            private val sdf = SimpleDateFormat("HH:mm", Locale.getDefault())
            override fun getAxisLabel(value: Float, axis: AxisBase?): String {
                val timestamp = firstTime + (value * 1000L).toLong()
                return sdf.format(Date(timestamp))
            }
        }

        chart.apply {
            data = LineData(dataSet)
            xAxis.valueFormatter = timeFormatter
            xAxis.labelCount = 6

            val latestX = entries.last().x
            val windowSeconds = 8 * 60 * 60f
            if (latestX > windowSeconds) {
                setVisibleXRangeMaximum(windowSeconds)
                moveViewToX(latestX - windowSeconds)
            } else {
                fitScreen()
                moveViewToX(0f)
            }

            invalidate()
        }
    }

    private fun updateSleepPieChart() {
        val session = TrendDataRepository.latestSleepSessionSnapshot()
        val stages = session?.stages ?: emptyList()
        if (stages.isEmpty()) {
            pieChart.clear()
            pieChart.invalidate()
            return
        }

        val total = stages.size.toFloat()
        val awakeCount = stages.count { it == 0 }.toFloat()
        val lightCount = stages.count { it == 1 }.toFloat()
        val deepCount = stages.count { it == 2 }.toFloat()

        val entries = mutableListOf<PieEntry>()
        if (deepCount > 0f) entries.add(PieEntry(deepCount / total, "深睡"))
        if (lightCount > 0f) entries.add(PieEntry(lightCount / total, "浅睡"))
        if (awakeCount > 0f) entries.add(PieEntry(awakeCount / total, "清醒"))

        val dataSet = PieDataSet(entries, "阶段占比").apply {
            colors = listOf(
                Color.parseColor("#5856D6"), // Deep Purple
                Color.parseColor("#AF52DE"), // Light Purple
                Color.parseColor("#FFCC00")  // Yellow for Awake
            )
            valueTextColor = Color.WHITE
            valueTextSize = 10f
            sliceSpace = 3f
            setDrawValues(true)
        }

        val data = PieData(dataSet)
        data.setValueFormatter(object : ValueFormatter() {
            override fun getPieLabel(value: Float, pieEntry: PieEntry?): String {
                val percent = value * 100f
                return "${percent.toInt()}%"
            }
        })

        pieChart.data = data
        pieChart.invalidate()
        pieChart.animateY(500)
    }

    private fun updateSummary(session: TrendDataRepository.SleepSessionSnapshot?) {
        if (session == null) {
            tvSessionSummary.text = "当前没有可展示的睡眠会话"
            return
        }

        val startText = dateTimeFormat.format(Date(session.startTime))
        val endText = session.endTime?.let { dateTimeFormat.format(Date(it)) } ?: "进行中"
        val durationMs = (session.endTime ?: session.times.lastOrNull() ?: session.startTime) - session.startTime
        val durationMin = (durationMs / 60000L).coerceAtLeast(0L)
        val status = if (session.isCompleted) "已结束" else "进行中"

        tvSessionSummary.text = "本次睡眠：$status\n开始：$startText\n结束：$endText\n总时长：$durationMin 分钟"
    }
}
