package com.example.hc06app

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.RectF
import android.util.AttributeSet
import android.view.View

class SleepPieChartView @JvmOverloads constructor(
    context: Context, attrs: AttributeSet? = null, defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    private val paint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeCap = Paint.Cap.ROUND
    }

    private var deepSleepPercent = 0f
    private var lightSleepPercent = 0f
    private var awakePercent = 0f

    private val rectF = RectF()

    // 颜色配置 (对应 Apple 风格)
    private val colorDeep = 0xFF7D58FF.toInt()  // 深紫
    private val colorLight = 0xFFA18DFF.toInt() // 浅紫
    private val colorAwake = 0xFFFF64B4.toInt() // 粉红
    private val colorEmpty = 0xFF1C1C1E.toInt() // 深灰背景

    fun setData(deep: Float, light: Float, awake: Float) {
        val total = deep + light + awake
        if (total > 0) {
            this.deepSleepPercent = deep / total
            this.lightSleepPercent = light / total
            this.awakePercent = awake / total
        } else {
            this.deepSleepPercent = 0f
            this.lightSleepPercent = 0f
            this.awakePercent = 0f
        }
        invalidate()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)

        val size = Math.min(width, height).toFloat()
        val strokeWidth = size * 0.12f
        val padding = strokeWidth / 2f
        rectF.set(padding, padding, size - padding, size - padding)

        paint.strokeWidth = strokeWidth

        // 1. 背景底环
        paint.color = colorEmpty
        canvas.drawArc(rectF, 0f, 360f, false, paint)

        if (deepSleepPercent == 0f && lightSleepPercent == 0f && awakePercent == 0f) return

        var startAngle = -90f

        // 2. 绘制深睡 (Deep)
        if (deepSleepPercent > 0) {
            paint.color = colorDeep
            val sweep = deepSleepPercent * 360f
            canvas.drawArc(rectF, startAngle, sweep, false, paint)
            startAngle += sweep
        }

        // 3. 绘制浅睡 (Light)
        if (lightSleepPercent > 0) {
            paint.color = colorLight
            val sweep = lightSleepPercent * 360f
            canvas.drawArc(rectF, startAngle, sweep, false, paint)
            startAngle += sweep
        }

        // 4. 绘制清醒 (Awake)
        if (awakePercent > 0) {
            paint.color = colorAwake
            val sweep = awakePercent * 360f
            canvas.drawArc(rectF, startAngle, sweep, false, paint)
        }
    }
}
