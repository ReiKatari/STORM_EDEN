// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.translator.ocr

import android.graphics.Bitmap
import android.graphics.Color
import android.graphics.RectF
import com.google.mlkit.vision.common.InputImage
import com.google.mlkit.vision.text.Text
import com.google.mlkit.vision.text.TextRecognition
import com.google.mlkit.vision.text.TextRecognizer
import com.google.mlkit.vision.text.chinese.ChineseTextRecognizerOptions
import com.google.mlkit.vision.text.japanese.JapaneseTextRecognizerOptions
import com.google.mlkit.vision.text.korean.KoreanTextRecognizerOptions
import com.google.mlkit.vision.text.latin.TextRecognizerOptions
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withContext
import org.yuzu.yuzu_emu.translator.model.TranslatedTextBlock
import org.yuzu.yuzu_emu.translator.model.TranslationRegion
import kotlin.coroutines.resumeWithException
import kotlin.math.max
import kotlin.math.min

class GameTextRecognizer {

    var lastOcrError: String? = null
        private set

    private val latinRecognizer: TextRecognizer by lazy {
        TextRecognition.getClient(TextRecognizerOptions.DEFAULT_OPTIONS)
    }

    private val japaneseRecognizer: TextRecognizer by lazy {
        TextRecognition.getClient(JapaneseTextRecognizerOptions.Builder().build())
    }

    private val chineseRecognizer: TextRecognizer by lazy {
        TextRecognition.getClient(ChineseTextRecognizerOptions.Builder().build())
    }

    private val koreanRecognizer: TextRecognizer by lazy {
        TextRecognition.getClient(KoreanTextRecognizerOptions.Builder().build())
    }

    suspend fun recognizeTextBlocks(
        bitmap: Bitmap,
        sourceLang: String,
        regions: List<TranslationRegion> = emptyList()
    ): List<TranslatedTextBlock> = withContext(Dispatchers.Default) {
        lastOcrError = null
        val safeBitmap = ensureSoftwareBitmap(bitmap)

        if (regions.isNotEmpty()) {
            val allRegionBlocks = mutableListOf<TranslatedTextBlock>()
            val imgW = safeBitmap.width
            val imgH = safeBitmap.height

            for (region in regions) {
                val leftPx = (region.rect.left * imgW).toInt().coerceIn(0, imgW - 1)
                val topPx = (region.rect.top * imgH).toInt().coerceIn(0, imgH - 1)
                val rightPx = (region.rect.right * imgW).toInt().coerceIn(leftPx + 1, imgW)
                val bottomPx = (region.rect.bottom * imgH).toInt().coerceIn(topPx + 1, imgH)

                val cropW = rightPx - leftPx
                val cropH = bottomPx - topPx
                if (cropW < 6 || cropH < 6) continue

                val cropBitmap = try {
                    Bitmap.createBitmap(safeBitmap, leftPx, topPx, cropW, cropH)
                } catch (_: Throwable) {
                    null
                } ?: continue

                val localBlocks = recognizeWithMultiPass(cropBitmap, sourceLang)
                cropBitmap.recycle()

                val regionWidthRel = region.rect.width()
                val regionHeightRel = region.rect.height()

                for (lb in localBlocks) {
                    val globalBox = RectF(
                        region.rect.left + lb.boundingBox.left * regionWidthRel,
                        region.rect.top + lb.boundingBox.top * regionHeightRel,
                        region.rect.left + lb.boundingBox.right * regionWidthRel,
                        region.rect.top + lb.boundingBox.bottom * regionHeightRel
                    )
                    allRegionBlocks.add(lb.copy(boundingBox = globalBox))
                }
            }

            if (allRegionBlocks.isNotEmpty()) {
                return@withContext allRegionBlocks
            }
        }

        recognizeWithMultiPass(safeBitmap, sourceLang)
    }

    private suspend fun recognizeWithMultiPass(
        bitmap: Bitmap,
        sourceLang: String
    ): List<TranslatedTextBlock> {
        val recognizer = getRecognizerForLang(sourceLang)
        val pass1 = runOcr(recognizer, bitmap)
        if (pass1.textBlocks.isNotEmpty()) {
            return convertMlKitBlocks(pass1, bitmap)
        }

        // Pass 2: Upscaling x2 for small pixel font rendering
        val upscaled = try {
            Bitmap.createScaledBitmap(bitmap, bitmap.width * 2, bitmap.height * 2, true)
        } catch (_: Throwable) {
            null
        }

        if (upscaled != null) {
            val pass2 = runOcr(recognizer, upscaled)
            val result = convertMlKitBlocks(pass2, upscaled)
            upscaled.recycle()
            if (result.isNotEmpty()) return result
        }

        return emptyList()
    }

    private fun getRecognizerForLang(sourceLang: String): TextRecognizer {
        return when (sourceLang.lowercase()) {
            "ja", "jpn", "japanese" -> japaneseRecognizer
            "zh", "zho", "chi", "chinese", "zh-cn", "zh-tw" -> chineseRecognizer
            "ko", "kor", "korean" -> koreanRecognizer
            else -> latinRecognizer
        }
    }

    private suspend fun runOcr(recognizer: TextRecognizer, bitmap: Bitmap): Text {
        val inputImage = InputImage.fromBitmap(bitmap, 0)
        return suspendCancellableCoroutine { cont ->
            recognizer.process(inputImage)
                .addOnSuccessListener { text ->
                    cont.resume(text, null)
                }
                .addOnFailureListener { e ->
                    lastOcrError = e.message
                    cont.resumeWithException(e)
                }
        }
    }

    private fun convertMlKitBlocks(mlKitText: Text, sourceBitmap: Bitmap): List<TranslatedTextBlock> {
        val width = sourceBitmap.width.toFloat()
        val height = sourceBitmap.height.toFloat()
        if (width <= 0 || height <= 0) return emptyList()

        val results = mutableListOf<TranslatedTextBlock>()

        for (block in mlKitText.textBlocks) {
            val blockBox = block.boundingBox ?: continue
            val rawText = block.text.trim()
            if (rawText.isBlank()) continue

            val relBox = RectF(
                (blockBox.left / width).coerceIn(0f, 1f),
                (blockBox.top / height).coerceIn(0f, 1f),
                (blockBox.right / width).coerceIn(0f, 1f),
                (blockBox.bottom / height).coerceIn(0f, 1f)
            )

            val (bgColor, textColor) = sampleColors(sourceBitmap, blockBox)

            results.add(
                TranslatedTextBlock(
                    originalText = rawText,
                    translatedText = "",
                    boundingBox = relBox,
                    backgroundColor = bgColor,
                    textColor = textColor
                )
            )
        }

        return results
    }

    private fun sampleColors(bitmap: Bitmap, box: android.graphics.Rect): Pair<Int, Int> {
        return try {
            val cx = (box.centerX()).coerceIn(0, bitmap.width - 1)
            val cy = (box.centerY()).coerceIn(0, bitmap.height - 1)
            val bg = bitmap.getPixel(cx, cy)
            val r = Color.red(bg)
            val g = Color.green(bg)
            val b = Color.blue(bg)
            val lum = 0.299 * r + 0.587 * g + 0.114 * b
            val textCol = if (lum > 128) Color.BLACK else Color.WHITE
            Pair(bg, textCol)
        } catch (_: Exception) {
            Pair(Color.argb(220, 15, 23, 42), Color.WHITE)
        }
    }

    private fun ensureSoftwareBitmap(bitmap: Bitmap): Bitmap {
        return if (bitmap.config == Bitmap.Config.HARDWARE || !bitmap.isMutable) {
            bitmap.copy(Bitmap.Config.ARGB_8888, false)
        } else {
            bitmap
        }
    }
}
