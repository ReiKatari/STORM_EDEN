// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.translator.ui

import android.animation.ValueAnimator
import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RadialGradient
import android.graphics.Shader
import android.os.Build
import android.os.CombinedVibration
import android.os.VibrationEffect
import android.os.Vibrator
import android.os.VibratorManager
import android.util.AttributeSet
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import androidx.preference.PreferenceManager
import kotlin.math.hypot

class FloatingTranslateButtonView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    var onSingleTap: (() -> Unit)? = null
    var onLongPress: (() -> Unit)? = null

    private var dX = 0f
    private var dY = 0f
    private var touchDownRawX = 0f
    private var touchDownRawY = 0f
    private var isDragging = false
    private val longPressThresholdMs = 420L
    private var touchDownTime = 0L

    private val defaultAlpha = 0.30f
    private val activeAlpha = 0.85f

    private val bgPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
    }

    private val borderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(160, 0, 229, 255)
        style = Paint.Style.STROKE
        strokeWidth = 2.0f
    }

    private val textPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        textAlign = Paint.Align.CENTER
        isFakeBoldText = true
    }

    private val longPressRunnable = Runnable {
        vibrate()
        onLongPress?.invoke()
    }

    init {
        // Enforce true view transparency
        alpha = defaultAlpha
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val cx = width / 2f
        val cy = height / 2f
        val radius = (width.coerceAtMost(height) / 2f) - 4f

        // 1. Ultra-translucent dark glass disc background
        bgPaint.color = Color.argb(75, 11, 15, 25)
        canvas.drawCircle(cx, cy, radius, bgPaint)

        // 2. Glassmorphic Radial Dome Highlight
        val domePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            style = Paint.Style.FILL
            shader = RadialGradient(
                cx, cy - radius * 0.35f, radius * 1.15f,
                intArrayOf(
                    Color.argb(80, 255, 255, 255),
                    Color.argb(40, 0, 229, 255),
                    Color.argb(10, 124, 58, 237),
                    Color.TRANSPARENT
                ),
                floatArrayOf(0f, 0.45f, 0.8f, 1f),
                Shader.TileMode.CLAMP
            )
        }
        canvas.drawCircle(cx, cy, radius * 0.92f, domePaint)

        // 3. Neon Cyan / Violet Dual Border
        borderPaint.color = Color.argb(140, 0, 229, 255)
        borderPaint.strokeWidth = 1.8f
        canvas.drawCircle(cx, cy, radius * 0.92f, borderPaint)

        // 4. Globe Icon with slight 3D shadow
        textPaint.textSize = radius * 0.90f
        textPaint.color = Color.argb(120, 0, 0, 0)
        canvas.drawText("🌐", cx + 1.5f, cy + (textPaint.textSize * 0.34f) + 1.5f, textPaint)

        textPaint.color = Color.WHITE
        canvas.drawText("🌐", cx, cy + (textPaint.textSize * 0.34f), textPaint)
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        post {
            val prefs = PreferenceManager.getDefaultSharedPreferences(context)
            val savedX = prefs.getFloat("translator_floating_btn_x", -1f)
            val savedY = prefs.getFloat("translator_floating_btn_y", -1f)
            val parentView = parent as? ViewGroup
            if (parentView != null) {
                val maxX = (parentView.width - width).toFloat().coerceAtLeast(0f)
                val maxY = (parentView.height - height).toFloat().coerceAtLeast(0f)
                if (savedX >= 0f && savedY >= 0f) {
                    x = savedX.coerceIn(0f, maxX)
                    y = savedY.coerceIn(0f, maxY)
                } else {
                    // Default position: Left Center
                    x = 16f
                    y = (parentView.height - height) / 2f
                }
            }
        }
    }

    private fun animateAlphaTo(target: Float) {
        ValueAnimator.ofFloat(alpha, target).apply {
            duration = 180
            addUpdateListener {
                alpha = it.animatedValue as Float
            }
            start()
        }
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        val parentView = parent as? ViewGroup ?: return super.onTouchEvent(event)

        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                touchDownRawX = event.rawX
                touchDownRawY = event.rawY
                dX = x - event.rawX
                dY = y - event.rawY
                isDragging = false
                touchDownTime = System.currentTimeMillis()
                animateAlphaTo(activeAlpha)
                postDelayed(longPressRunnable, longPressThresholdMs)
                return true
            }

            MotionEvent.ACTION_MOVE -> {
                val moveDist = hypot(event.rawX - touchDownRawX, event.rawY - touchDownRawY)
                if (moveDist > 12f) {
                    isDragging = true
                    removeCallbacks(longPressRunnable)

                    val newX = (event.rawX + dX).coerceIn(0f, (parentView.width - width).toFloat().coerceAtLeast(0f))
                    val newY = (event.rawY + dY).coerceIn(0f, (parentView.height - height).toFloat().coerceAtLeast(0f))
                    x = newX
                    y = newY
                }
                return true
            }

            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                removeCallbacks(longPressRunnable)
                animateAlphaTo(defaultAlpha)
                val duration = System.currentTimeMillis() - touchDownTime
                if (!isDragging && duration < longPressThresholdMs) {
                    onSingleTap?.invoke()
                } else if (isDragging) {
                    val prefs = PreferenceManager.getDefaultSharedPreferences(context)
                    prefs.edit()
                        .putFloat("translator_floating_btn_x", x)
                        .putFloat("translator_floating_btn_y", y)
                        .apply()
                }
                return true
            }
        }
        return super.onTouchEvent(event)
    }

    private fun vibrate() {
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                val vm = context.getSystemService(Context.VIBRATOR_MANAGER_SERVICE) as? VibratorManager
                vm?.defaultVibrator?.vibrate(VibrationEffect.createPredefined(VibrationEffect.EFFECT_HEAVY_CLICK))
            } else {
                @Suppress("DEPRECATION")
                val v = context.getSystemService(Context.VIBRATOR_SERVICE) as? Vibrator
                @Suppress("DEPRECATION")
                v?.vibrate(50)
            }
        } catch (_: Throwable) {}
    }
}
