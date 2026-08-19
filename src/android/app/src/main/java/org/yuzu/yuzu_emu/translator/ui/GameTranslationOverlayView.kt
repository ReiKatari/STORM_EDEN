// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.translator.ui

import android.content.Context
import android.graphics.*
import android.os.Handler
import android.os.Looper
import android.text.TextPaint
import android.util.AttributeSet
import android.view.HapticFeedbackConstants
import android.view.MotionEvent
import android.view.View
import androidx.preference.PreferenceManager
import org.yuzu.yuzu_emu.translator.model.TranslatedTextBlock
import org.yuzu.yuzu_emu.translator.model.TranslationRegion
import org.yuzu.yuzu_emu.translator.model.TranslatorOverlayStyle
import kotlin.math.max
import kotlin.math.min

class GameTranslationOverlayView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    private var blocks: List<TranslatedTextBlock> = emptyList()
    val customRegions = mutableListOf<TranslationRegion>()

    var onSpeakRequested: ((String) -> Unit)? = null
    var onCloseRequested: (() -> Unit)? = null
    var onOpenSettingsRequested: (() -> Unit)? = null
    var onTriggerTranslationRequested: (() -> Unit)? = null
    var onRegionsSaved: ((List<TranslationRegion>) -> Unit)? = null

    var showFloatingButton: Boolean = false
        set(value) {
            field = value
            invalidate()
        }

    var isEditRegionsMode: Boolean = false
        set(value) {
            field = value
            if (value) {
                blocks = emptyList()
                visibility = VISIBLE
            }
            invalidate()
        }

    // Floating Translate Button State
    private var floatBtnX = 70f
    private var floatBtnY = 280f
    private val floatBtnRadius = 60f
    private var isDraggingFloatBtn = false
    private var dragStartX = 0f
    private var dragStartY = 0f
    private var hasMovedFloatBtn = false
    private val longPressHandler = Handler(Looper.getMainLooper())

    // Region Editor Touch State
    private enum class EditAction { NONE, DRAW_NEW, MOVE_REGION, RESIZE_REGION }
    private var currentEditAction = EditAction.NONE
    private var activeRegionIndex = -1
    private var touchDownX = 0f
    private var touchDownY = 0f
    private var regionInitialRect = RectF()
    private var drawingNewRect = RectF()

    // Dock Buttons
    private val btnAddRect = RectF()
    private val btnClearRect = RectF()
    private val btnSaveRect = RectF()
    private val btnCloseRect = RectF()

    // Normal translation UI rects
    private val closeButtonRect = RectF()
    private val settingsButtonRect = RectF()
    private val blockClickRects = mutableListOf<Pair<RectF, TranslatedTextBlock>>()

    // Paints
    private val bgPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.FILL }
    private val borderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(180, 0, 229, 255)
        style = Paint.Style.STROKE
        strokeWidth = 2f
    }
    private val shadowPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        color = Color.argb(140, 0, 0, 0)
    }
    private val textPaint = TextPaint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        isFakeBoldText = true
        setShadowLayer(3f, 1f, 1f, Color.argb(220, 0, 0, 0))
    }

    // Region Editor Paints
    private val regionBgPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        color = Color.argb(55, 0, 229, 255)
    }
    private val regionBorderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 3f
        color = Color.parseColor("#00E5FF")
    }
    private val regionHandlePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        color = Color.parseColor("#00E5FF")
    }
    private val regionBadgeBgPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        color = Color.parseColor("#E60F172A")
    }
    private val regionBadgeTextPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.parseColor("#00E5FF")
        typeface = Typeface.create("sans-serif-medium", Typeface.BOLD)
        textSize = 22f
    }
    private val closeBtnPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        color = Color.parseColor("#CCEF4444")
    }
    private val closeBtnTextPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        textAlign = Paint.Align.CENTER
        typeface = Typeface.create("sans-serif-medium", Typeface.BOLD)
        textSize = 20f
    }
    private val dockBtnPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        color = Color.parseColor("#E60F172A")
    }
    private val dockBtnBorderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 2f
        color = Color.parseColor("#8000E5FF")
    }
    private val dockTextPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        textAlign = Paint.Align.CENTER
    }

    // Floating button paints
    private val floatBtnPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        color = Color.parseColor("#E60F172A")
    }
    private val floatBtnGlowPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        color = Color.parseColor("#4D00E5FF")
    }
    private val floatBtnStrokePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 3f
        color = Color.parseColor("#FF00E5FF")
    }
    private val floatBtnTextPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.parseColor("#00E5FF")
        textAlign = Paint.Align.CENTER
        typeface = Typeface.create("sans-serif-medium", Typeface.BOLD)
        textSize = 26f
    }

    init {
        val prefs = PreferenceManager.getDefaultSharedPreferences(context)
        floatBtnX = prefs.getFloat("translator_float_btn_x", 70f)
        floatBtnY = prefs.getFloat("translator_float_btn_y", 280f)
        val savedJson = prefs.getString("translator_custom_regions", null)
        customRegions.addAll(TranslationRegion.listFromJson(savedJson))
    }

    fun updateBlocks(newBlocks: List<TranslatedTextBlock>) {
        this.blocks = newBlocks
        isEditRegionsMode = false
        visibility = if (newBlocks.isNotEmpty() || showFloatingButton) VISIBLE else GONE
        invalidate()
    }

    fun clear() {
        blocks = emptyList()
        if (!isEditRegionsMode && !showFloatingButton) {
            visibility = GONE
        }
        invalidate()
    }

    fun enterRegionEditMode() {
        val prefs = PreferenceManager.getDefaultSharedPreferences(context)
        val savedJson = prefs.getString("translator_custom_regions", null)
        customRegions.clear()
        customRegions.addAll(TranslationRegion.listFromJson(savedJson))
        if (customRegions.isEmpty()) {
            // Add default dialog region
            customRegions.add(TranslationRegion(rect = RectF(0.08f, 0.55f, 0.92f, 0.92f)))
        }
        isEditRegionsMode = true
    }

    fun exitRegionEditMode(save: Boolean) {
        if (save) {
            val prefs = PreferenceManager.getDefaultSharedPreferences(context)
            prefs.edit()
                .putString("translator_custom_regions", TranslationRegion.listToJson(customRegions))
                .putString("translator_scan_region", "custom")
                .apply()
            onRegionsSaved?.invoke(customRegions.toList())
        }
        isEditRegionsMode = false
        if (blocks.isEmpty() && !showFloatingButton) {
            visibility = GONE
        }
        invalidate()
    }

    private fun getGameViewportRect(): RectF {
        val w = width.toFloat()
        val h = height.toFloat()
        if (w <= 0f || h <= 0f) return RectF(0f, 0f, w, h)

        val targetAspect = 16f / 9f
        val currentAspect = w / h

        return if (currentAspect > targetAspect) {
            val gameW = h * targetAspect
            val offsetX = (w - gameW) / 2f
            RectF(offsetX, 0f, offsetX + gameW, h)
        } else {
            val gameH = w / targetAspect
            val offsetY = (h - gameH) / 2f
            RectF(0f, offsetY, w, offsetY + gameH)
        }
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val w = width.toFloat()
        val h = height.toFloat()
        if (w <= 0f || h <= 0f) return

        // 1. REGION EDIT MODE
        if (isEditRegionsMode) {
            canvas.drawColor(Color.argb(140, 0, 0, 0))

            for (i in customRegions.indices) {
                val region = customRegions[i]
                val l = region.rect.left * w
                val t = region.rect.top * h
                val r = region.rect.right * w
                val b = region.rect.bottom * h
                val rect = RectF(l, t, r, b)

                canvas.drawRoundRect(rect, 10f, 10f, regionBgPaint)
                canvas.drawRoundRect(rect, 10f, 10f, regionBorderPaint)

                val badgeText = "Область ${i + 1}"
                val badgeW = regionBadgeTextPaint.measureText(badgeText) + 20f
                val badgeH = 34f
                val badgeRect = RectF(l, max(0f, t - badgeH - 4f), l + badgeW, max(badgeH, t - 4f))
                canvas.drawRoundRect(badgeRect, 6f, 6f, regionBadgeBgPaint)
                canvas.drawRoundRect(badgeRect, 6f, 6f, regionBorderPaint)
                canvas.drawText(badgeText, badgeRect.left + 10f, badgeRect.bottom - 9f, regionBadgeTextPaint)

                val closeRadius = 18f
                val closeCx = r - 4f
                val closeCy = t + 4f
                canvas.drawCircle(closeCx, closeCy, closeRadius, closeBtnPaint)
                val textY = closeCy - (closeBtnTextPaint.descent() + closeBtnTextPaint.ascent()) / 2f
                canvas.drawText("✕", closeCx, textY, closeBtnTextPaint)

                val handleSize = 24f
                val handleRect = RectF(r - handleSize, b - handleSize, r, b)
                canvas.drawRoundRect(handleRect, 4f, 4f, regionHandlePaint)
            }

            if (currentEditAction == EditAction.DRAW_NEW) {
                canvas.drawRoundRect(drawingNewRect, 10f, 10f, regionBgPaint)
                canvas.drawRoundRect(drawingNewRect, 10f, 10f, regionBorderPaint)
            }

            drawLeftControlDock(canvas, w, h)
            return
        }

        // 2. NORMAL TRANSLATION MODE
        if (blocks.isNotEmpty()) {
            val viewport = getGameViewportRect()
            blockClickRects.clear()

            // Top control panel
            val barHeight = 76f
            val barRect = RectF(16f, 16f, w - 16f, 16f + barHeight)
            val barPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
                color = Color.argb(235, 12, 18, 30)
                style = Paint.Style.FILL
            }
            canvas.drawRoundRect(barRect, 18f, 18f, barPaint)
            canvas.drawRoundRect(barRect, 18f, 18f, borderPaint)

            // Title text
            val titlePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
                color = Color.rgb(0, 229, 255)
                textSize = 24f
                isFakeBoldText = true
            }
            canvas.drawText("🌐 STORM EDEN (${blocks.size})", barRect.left + 20f, barRect.centerY() + 8f, titlePaint)

            // Close Button (✕)
            closeButtonRect.set(barRect.right - 80f, barRect.top + 8f, barRect.right - 8f, barRect.bottom - 8f)
            val btnClosePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
                color = Color.argb(220, 239, 68, 68)
                style = Paint.Style.FILL
            }
            canvas.drawRoundRect(closeButtonRect, 14f, 14f, btnClosePaint)
            val btnTextPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
                color = Color.WHITE
                textSize = 30f
                textAlign = Paint.Align.CENTER
                isFakeBoldText = true
            }
            canvas.drawText("✕", closeButtonRect.centerX(), closeButtonRect.centerY() + 10f, btnTextPaint)

            // Settings Button (⚙)
            settingsButtonRect.set(closeButtonRect.left - 76f, barRect.top + 8f, closeButtonRect.left - 8f, barRect.bottom - 8f)
            val btnSettingsPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
                color = Color.argb(220, 51, 65, 85)
                style = Paint.Style.FILL
            }
            canvas.drawRoundRect(settingsButtonRect, 14f, 14f, btnSettingsPaint)
            btnTextPaint.textSize = 24f
            canvas.drawText("⚙", settingsButtonRect.centerX(), settingsButtonRect.centerY() + 8f, btnTextPaint)

            val prefs = PreferenceManager.getDefaultSharedPreferences(context)
            val overlayStyle = TranslatorOverlayStyle.fromPreference(prefs.getString("translator_overlay_style", null))

            for (block in blocks) {
                val textToDraw = if (block.isShowingOriginal) block.originalText else block.translatedText
                if (textToDraw.isBlank()) continue

                val boxLeft = viewport.left + block.boundingBox.left * viewport.width()
                val boxTop = viewport.top + block.boundingBox.top * viewport.height()
                val boxRight = viewport.left + block.boundingBox.right * viewport.width()
                val boxBottom = viewport.top + block.boundingBox.bottom * viewport.height()

                val padH = 8f
                val padV = 6f
                val cardRect = RectF(
                    (boxLeft - padH).coerceAtLeast(viewport.left + 4f),
                    (boxTop - padV).coerceAtLeast(barRect.bottom + 6f),
                    (boxRight + padH).coerceAtMost(viewport.right - 4f),
                    (boxBottom + padV).coerceAtMost(viewport.bottom - 4f)
                )

                blockClickRects.add(Pair(cardRect, block))

                when (overlayStyle) {
                    TranslatorOverlayStyle.SMART_BACKGROUND_MATCH -> {
                        val bgCol = if (block.backgroundColor != 0) {
                            Color.argb(240, Color.red(block.backgroundColor), Color.green(block.backgroundColor), Color.blue(block.backgroundColor))
                        } else {
                            Color.argb(240, 12, 16, 26)
                        }
                        bgPaint.color = bgCol
                        canvas.drawRoundRect(cardRect, 10f, 10f, bgPaint)
                        canvas.drawRoundRect(cardRect, 10f, 10f, borderPaint)
                    }
                    TranslatorOverlayStyle.SEMI_TRANSPARENT -> {
                        bgPaint.color = Color.argb(190, 10, 15, 25)
                        canvas.drawRoundRect(cardRect, 12f, 12f, bgPaint)
                        canvas.drawRoundRect(cardRect, 12f, 12f, borderPaint)
                    }
                    TranslatorOverlayStyle.TRANSLUCENT_BUBBLE -> {
                        bgPaint.color = Color.argb(225, 20, 26, 40)
                        canvas.drawRoundRect(cardRect, 16f, 16f, bgPaint)
                        borderPaint.color = Color.argb(220, 245, 0, 87)
                        canvas.drawRoundRect(cardRect, 16f, 16f, borderPaint)
                        borderPaint.color = Color.argb(180, 0, 229, 255)
                    }
                    TranslatorOverlayStyle.OUTLINE_ONLY -> {
                        bgPaint.color = Color.argb(90, 0, 0, 0)
                        canvas.drawRoundRect(cardRect, 8f, 8f, bgPaint)
                        canvas.drawRoundRect(cardRect, 8f, 8f, borderPaint)
                    }
                }

                val availableW = (cardRect.width() - 16f).coerceAtLeast(60f)
                val availableH = (cardRect.height() - 12f).coerceAtLeast(30f)

                val baseSize = (availableH * 0.75f).coerceIn(18f, 44f)
                textPaint.textSize = baseSize

                val lines = wrapText(textToDraw, textPaint, availableW)
                val totalTextHeight = lines.size * (textPaint.textSize + 4f)

                if (totalTextHeight > availableH && lines.size > 1) {
                    val scale = (availableH / totalTextHeight).coerceIn(0.6f, 1.0f)
                    textPaint.textSize = (baseSize * scale).coerceAtLeast(14f)
                }

                val finalLines = wrapText(textToDraw, textPaint, availableW)
                val lineSpacing = textPaint.textSize + 4f
                val startY = cardRect.top + ((cardRect.height() - (finalLines.size * lineSpacing)) / 2f).coerceAtLeast(4f) + textPaint.textSize

                var curY = startY
                for (line in finalLines) {
                    canvas.drawText(line, cardRect.left + 8f, curY, textPaint)
                    curY += lineSpacing
                }
            }
        }

        // 3. FLOATING BUTTON
        if (showFloatingButton) {
            floatBtnX = floatBtnX.coerceIn(floatBtnRadius + 8f, w - floatBtnRadius - 8f)
            floatBtnY = floatBtnY.coerceIn(floatBtnRadius + 8f, h - floatBtnRadius - 8f)

            canvas.drawCircle(floatBtnX + 2f, floatBtnY + 3f, floatBtnRadius, shadowPaint)
            canvas.drawCircle(floatBtnX, floatBtnY, floatBtnRadius * 1.15f, floatBtnGlowPaint)
            canvas.drawCircle(floatBtnX, floatBtnY, floatBtnRadius, floatBtnPaint)
            canvas.drawCircle(floatBtnX, floatBtnY, floatBtnRadius, floatBtnStrokePaint)

            val textY = floatBtnY - (floatBtnTextPaint.descent() + floatBtnTextPaint.ascent()) / 2f
            canvas.drawText("TR", floatBtnX, textY, floatBtnTextPaint)
        }
    }

    private fun drawLeftControlDock(canvas: Canvas, w: Float, h: Float) {
        val density = context.resources.displayMetrics.density
        val dockW = max(230f, 170f * density)
        val btnH = max(64f, 46f * density)
        val spacing = max(14f, 10f * density)
        val padV = max(16f, 12f * density)
        val totalH = btnH * 4 + spacing * 3 + padV * 2
        val startX = max(20f, 14f * density)
        val startY = max(20f, (h - totalH) / 2f)

        val dockRect = RectF(startX, startY, startX + dockW, startY + totalH)
        canvas.drawRoundRect(dockRect, 20f, 20f, shadowPaint)
        bgPaint.color = Color.parseColor("#E60F172A")
        canvas.drawRoundRect(dockRect, 20f, 20f, bgPaint)
        borderPaint.color = Color.parseColor("#4D00E5FF")
        canvas.drawRoundRect(dockRect, 20f, 20f, borderPaint)

        var curY = startY + padV
        drawColorfulDockButton(canvas, btnAddRect, startX + 16f, curY, dockW - 32f, btnH, "+ Добавить область", "#00E5FF")
        curY += btnH + spacing
        drawColorfulDockButton(canvas, btnClearRect, startX + 16f, curY, dockW - 32f, btnH, "🧹 Очистить", "#FFAA00")
        curY += btnH + spacing
        drawColorfulDockButton(canvas, btnSaveRect, startX + 16f, curY, dockW - 32f, btnH, "💾 Сохранить", "#10B981")
        curY += btnH + spacing
        drawColorfulDockButton(canvas, btnCloseRect, startX + 16f, curY, dockW - 32f, btnH, "✕ Закрыть", "#EF4444")
    }

    private fun drawColorfulDockButton(canvas: Canvas, targetRect: RectF, x: Float, y: Float, w: Float, h: Float, text: String, colorHex: String) {
        targetRect.set(x, y, x + w, y + h)
        val rx = 14f
        val color = Color.parseColor(colorHex)

        dockBtnPaint.color = Color.argb(45, Color.red(color), Color.green(color), Color.blue(color))
        dockBtnBorderPaint.color = color
        dockBtnBorderPaint.strokeWidth = 2.5f
        dockTextPaint.color = color

        canvas.drawRoundRect(targetRect, rx, rx, dockBtnPaint)
        canvas.drawRoundRect(targetRect, rx, rx, dockBtnBorderPaint)

        val density = context.resources.displayMetrics.density
        dockTextPaint.textSize = max(24f, 14f * density)
        dockTextPaint.typeface = Typeface.DEFAULT_BOLD
        val textY = targetRect.centerY() - (dockTextPaint.descent() + dockTextPaint.ascent()) / 2f
        canvas.drawText(text, targetRect.centerX(), textY, dockTextPaint)
    }

    private fun wrapText(text: String, paint: TextPaint, maxWidth: Float): List<String> {
        val lines = mutableListOf<String>()
        val paragraphs = text.split("\n")

        for (para in paragraphs) {
            val words = para.split(" ")
            var curLine = ""
            for (word in words) {
                val testLine = if (curLine.isEmpty()) word else "$curLine $word"
                if (paint.measureText(testLine) <= maxWidth) {
                    curLine = testLine
                } else {
                    if (curLine.isNotEmpty()) lines.add(curLine)
                    curLine = word
                }
            }
            if (curLine.isNotEmpty()) lines.add(curLine)
        }

        return if (lines.isEmpty()) listOf(text) else lines
    }

    private val longPressRunnable = Runnable {
        if (isDraggingFloatBtn && !hasMovedFloatBtn) {
            isDraggingFloatBtn = false
            performHapticFeedback(HapticFeedbackConstants.LONG_PRESS)
            enterRegionEditMode()
        }
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        val x = event.x
        val y = event.y
        val w = width.toFloat()
        val h = height.toFloat()

        if (isEditRegionsMode) {
            when (event.actionMasked) {
                MotionEvent.ACTION_DOWN -> {
                    if (btnAddRect.contains(x, y)) {
                        val defaultW = 0.80f
                        val defaultH = 0.28f
                        val newRegion = TranslationRegion(
                            rect = RectF(0.10f, 0.55f, 0.10f + defaultW, 0.55f + defaultH)
                        )
                        customRegions.add(newRegion)
                        invalidate()
                        return true
                    }
                    if (btnClearRect.contains(x, y)) {
                        customRegions.clear()
                        invalidate()
                        return true
                    }
                    if (btnSaveRect.contains(x, y)) {
                        exitRegionEditMode(save = true)
                        return true
                    }
                    if (btnCloseRect.contains(x, y)) {
                        exitRegionEditMode(save = false)
                        return true
                    }

                    for (i in customRegions.indices.reversed()) {
                        val region = customRegions[i]
                        val r = region.rect.right * w
                        val t = region.rect.top * h
                        val closeDist = Math.hypot((x - r).toDouble(), (y - t).toDouble()).toFloat()
                        if (closeDist <= 32f) {
                            customRegions.removeAt(i)
                            invalidate()
                            return true
                        }
                    }

                    for (i in customRegions.indices.reversed()) {
                        val region = customRegions[i]
                        val r = region.rect.right * w
                        val b = region.rect.bottom * h
                        val handleRect = RectF(r - 36f, b - 36f, r + 18f, b + 18f)
                        if (handleRect.contains(x, y)) {
                            currentEditAction = EditAction.RESIZE_REGION
                            activeRegionIndex = i
                            touchDownX = x
                            touchDownY = y
                            regionInitialRect.set(region.rect)
                            return true
                        }
                    }

                    for (i in customRegions.indices.reversed()) {
                        val region = customRegions[i]
                        val l = region.rect.left * w
                        val t = region.rect.top * h
                        val r = region.rect.right * w
                        val b = region.rect.bottom * h
                        if (RectF(l, t, r, b).contains(x, y)) {
                            currentEditAction = EditAction.MOVE_REGION
                            activeRegionIndex = i
                            touchDownX = x
                            touchDownY = y
                            regionInitialRect.set(region.rect)
                            return true
                        }
                    }

                    currentEditAction = EditAction.DRAW_NEW
                    touchDownX = x
                    touchDownY = y
                    drawingNewRect.set(x, y, x, y)
                    return true
                }
                MotionEvent.ACTION_MOVE -> {
                    when (currentEditAction) {
                        EditAction.DRAW_NEW -> {
                            drawingNewRect.set(
                                min(touchDownX, x),
                                min(touchDownY, y),
                                max(touchDownX, x),
                                max(touchDownY, y)
                            )
                            invalidate()
                            return true
                        }
                        EditAction.MOVE_REGION -> {
                            if (activeRegionIndex in customRegions.indices) {
                                val dx = (x - touchDownX) / w
                                val dy = (y - touchDownY) / h
                                val regW = regionInitialRect.width()
                                val regH = regionInitialRect.height()

                                val newL = (regionInitialRect.left + dx).coerceIn(0f, 1f - regW)
                                val newT = (regionInitialRect.top + dy).coerceIn(0f, 1f - regH)
                                customRegions[activeRegionIndex].rect.set(newL, newT, newL + regW, newT + regH)
                                invalidate()
                                return true
                            }
                        }
                        EditAction.RESIZE_REGION -> {
                            if (activeRegionIndex in customRegions.indices) {
                                val reg = customRegions[activeRegionIndex]
                                val newR = (x / w).coerceIn(reg.rect.left + 0.05f, 1f)
                                val newB = (y / h).coerceIn(reg.rect.top + 0.05f, 1f)
                                reg.rect.right = newR
                                reg.rect.bottom = newB
                                invalidate()
                                return true
                            }
                        }
                        else -> {}
                    }
                }
                MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                    if (currentEditAction == EditAction.DRAW_NEW) {
                        val minPx = 40f
                        if (drawingNewRect.width() >= minPx && drawingNewRect.height() >= minPx) {
                            val newRegion = TranslationRegion(
                                rect = RectF(
                                    drawingNewRect.left / w,
                                    drawingNewRect.top / h,
                                    drawingNewRect.right / w,
                                    drawingNewRect.bottom / h
                                )
                            )
                            customRegions.add(newRegion)
                        }
                        drawingNewRect.set(0f, 0f, 0f, 0f)
                    }
                    currentEditAction = EditAction.NONE
                    activeRegionIndex = -1
                    invalidate()
                    return true
                }
            }
            return true
        }

        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                if (showFloatingButton) {
                    val dist = Math.hypot((x - floatBtnX).toDouble(), (y - floatBtnY).toDouble()).toFloat()
                    if (dist <= floatBtnRadius * 1.3f) {
                        isDraggingFloatBtn = true
                        dragStartX = x
                        dragStartY = y
                        hasMovedFloatBtn = false
                        longPressHandler.postDelayed(longPressRunnable, 450)
                        return true
                    }
                }

                if (closeButtonRect.contains(x, y)) {
                    clear()
                    onCloseRequested?.invoke()
                    return true
                }

                if (settingsButtonRect.contains(x, y)) {
                    onOpenSettingsRequested?.invoke()
                    return true
                }

                for (pair in blockClickRects) {
                    if (pair.first.contains(x, y)) {
                        val block = pair.second
                        block.isShowingOriginal = !block.isShowingOriginal
                        val spoken = if (block.isShowingOriginal) block.originalText else block.translatedText
                        onSpeakRequested?.invoke(spoken)
                        invalidate()
                        return true
                    }
                }
            }
            MotionEvent.ACTION_MOVE -> {
                if (isDraggingFloatBtn) {
                    val dx = x - dragStartX
                    val dy = y - dragStartY
                    if (Math.hypot(dx.toDouble(), dy.toDouble()) > 10.0) {
                        hasMovedFloatBtn = true
                        longPressHandler.removeCallbacks(longPressRunnable)
                        floatBtnX = x
                        floatBtnY = y
                        invalidate()
                    }
                    return true
                }
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                if (isDraggingFloatBtn) {
                    longPressHandler.removeCallbacks(longPressRunnable)
                    isDraggingFloatBtn = false
                    if (!hasMovedFloatBtn) {
                        onTriggerTranslationRequested?.invoke()
                    } else {
                        val prefs = PreferenceManager.getDefaultSharedPreferences(context)
                        prefs.edit()
                            .putFloat("translator_float_btn_x", floatBtnX)
                            .putFloat("translator_float_btn_y", floatBtnY)
                            .apply()
                    }
                    return true
                }
            }
        }
        return super.onTouchEvent(event)
    }
}
