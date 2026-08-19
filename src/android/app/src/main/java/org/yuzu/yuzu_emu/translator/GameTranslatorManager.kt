// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.translator

import android.content.Context
import android.content.SharedPreferences
import android.graphics.Bitmap
import android.os.Handler
import android.os.Looper
import android.view.SurfaceView
import android.widget.Toast
import androidx.preference.PreferenceManager
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.translator.engine.CustomAiEngine
import org.yuzu.yuzu_emu.translator.engine.DeepLEngine
import org.yuzu.yuzu_emu.translator.engine.GoogleTranslateEngine
import org.yuzu.yuzu_emu.translator.engine.ITranslationEngine
import org.yuzu.yuzu_emu.translator.engine.LibreTranslateEngine
import org.yuzu.yuzu_emu.translator.engine.LingvaTranslateEngine
import org.yuzu.yuzu_emu.translator.engine.MyMemoryTranslateEngine
import org.yuzu.yuzu_emu.translator.engine.YandexTranslateEngine
import org.yuzu.yuzu_emu.translator.model.TranslatedTextBlock
import org.yuzu.yuzu_emu.translator.model.TranslatorEngineType
import org.yuzu.yuzu_emu.translator.model.TranslatorTriggerMode
import org.yuzu.yuzu_emu.translator.ocr.GameTextRecognizer
import org.yuzu.yuzu_emu.translator.tts.GameTtsManager
import org.yuzu.yuzu_emu.translator.ui.GameTranslationOverlayView
import java.nio.ByteBuffer
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.TimeUnit

class GameTranslatorManager(
    private val context: Context,
    private val coroutineScope: CoroutineScope
) {
    private val httpClient = OkHttpClient.Builder()
        .connectTimeout(5, TimeUnit.SECONDS)
        .readTimeout(8, TimeUnit.SECONDS)
        .build()

    private val recognizer = GameTextRecognizer()
    val ttsManager = GameTtsManager(context)

    private val translationCache = ConcurrentHashMap<String, String>()

    private val yandexEngine = YandexTranslateEngine(httpClient)
    private val googleEngine = GoogleTranslateEngine(httpClient)
    private val lingvaEngine = LingvaTranslateEngine(httpClient)
    private val myMemoryEngine = MyMemoryTranslateEngine(httpClient)
    private val deepLEngine = DeepLEngine(httpClient) {
        PreferenceManager.getDefaultSharedPreferences(context).getString("translator_deepl_api_key", "") ?: ""
    }
    private val libreEngine = LibreTranslateEngine(httpClient, {
        PreferenceManager.getDefaultSharedPreferences(context).getString("translator_libre_server", "https://translate.terraprint.co/translate") ?: "https://translate.terraprint.co/translate"
    }, {
        PreferenceManager.getDefaultSharedPreferences(context).getString("translator_libre_api_key", "") ?: ""
    })
    private val customAiEngine = CustomAiEngine(httpClient, {
        PreferenceManager.getDefaultSharedPreferences(context).getString("translator_ai_api_key", "") ?: ""
    }, {
        PreferenceManager.getDefaultSharedPreferences(context).getString("translator_ai_endpoint", "https://api.openai.com/v1/chat/completions") ?: "https://api.openai.com/v1/chat/completions"
    }, {
        PreferenceManager.getDefaultSharedPreferences(context).getString("translator_ai_model", "gpt-4o-mini") ?: "gpt-4o-mini"
    })

    private var activeTranslationJob: Job? = null
    private var autoTranslateJob: Job? = null
    private var lastRecognizedTextHash: Int = 0
    var overlayView: GameTranslationOverlayView? = null

    fun startAutoTranslateLoop(surfaceView: SurfaceView?) {
        stopAutoTranslateLoop()
        autoTranslateJob = coroutineScope.launch {
            while (true) {
                kotlinx.coroutines.delay(1800)
                val prefs = PreferenceManager.getDefaultSharedPreferences(context)
                val mode = TranslatorTriggerMode.fromPreference(prefs.getString("translator_trigger_mode", "on_demand"))
                if (mode == TranslatorTriggerMode.AUTO_SCREEN_CHANGE) {
                    val bitmap = captureFrame(surfaceView) ?: continue
                    val sourceLang = prefs.getString("translator_source_lang", "auto") ?: "auto"
                    val customRegionsJson = prefs.getString("translator_custom_regions", null)
                    val customRegions = org.yuzu.yuzu_emu.translator.model.TranslationRegion.listFromJson(customRegionsJson)
                    val scanRegion = prefs.getString("translator_scan_region", "all") ?: "all"

                    val rawBlocks = if (scanRegion == "custom" && customRegions.isNotEmpty()) {
                        recognizer.recognizeTextBlocks(bitmap, sourceLang, customRegions)
                    } else {
                        recognizer.recognizeTextBlocks(bitmap, sourceLang)
                    }
                    bitmap.recycle()

                    val combinedText = rawBlocks.joinToString(" ") { it.originalText }.trim()
                    if (combinedText.isNotBlank()) {
                        val textHash = combinedText.hashCode()
                        if (textHash != lastRecognizedTextHash) {
                            lastRecognizedTextHash = textHash
                            performTranslation(rawBlocks, prefs)
                        }
                    }
                }
            }
        }
    }

    fun stopAutoTranslateLoop() {
        autoTranslateJob?.cancel()
        autoTranslateJob = null
    }

    fun triggerTranslation(surfaceView: SurfaceView?) {
        activeTranslationJob?.cancel()
        activeTranslationJob = coroutineScope.launch {
            val bitmap = captureFrame(surfaceView)
            if (bitmap == null) {
                withContext(Dispatchers.Main) {
                    Toast.makeText(context, "Не удалось захватить кадр игры", Toast.LENGTH_SHORT).show()
                }
                return@launch
            }

            withContext(Dispatchers.Main) {
                Toast.makeText(context, "🔍 Распознавание и перевод...", Toast.LENGTH_SHORT).show()
            }

            val prefs = PreferenceManager.getDefaultSharedPreferences(context)
            val sourceLang = prefs.getString("translator_source_lang", "auto") ?: "auto"
            val scanRegion = prefs.getString("translator_scan_region", "all") ?: "all"
            val customRegionsJson = prefs.getString("translator_custom_regions", null)
            val customRegions = org.yuzu.yuzu_emu.translator.model.TranslationRegion.listFromJson(customRegionsJson)

            val rawBlocks = if (scanRegion == "custom" && customRegions.isNotEmpty()) {
                recognizer.recognizeTextBlocks(bitmap, sourceLang, customRegions)
            } else {
                recognizer.recognizeTextBlocks(bitmap, sourceLang)
            }
            bitmap.recycle()

            performTranslation(rawBlocks, prefs)
        }
    }

    private suspend fun performTranslation(rawBlocks: List<TranslatedTextBlock>, prefs: SharedPreferences) {
        val sourceLang = prefs.getString("translator_source_lang", "auto") ?: "auto"
        val targetLang = prefs.getString("translator_target_lang", "ru") ?: "ru"
        val engineType = TranslatorEngineType.fromPreference(prefs.getString("translator_engine", "google"))
        val autoSpeak = prefs.getBoolean("translator_auto_speak", false) || prefs.getBoolean("translator_auto_tts", false)
        val scanRegion = prefs.getString("translator_scan_region", "all") ?: "all"
        val customRegionsJson = prefs.getString("translator_custom_regions", null)
        val customRegions = org.yuzu.yuzu_emu.translator.model.TranslationRegion.listFromJson(customRegionsJson)

        val engine: ITranslationEngine = when (engineType) {
            TranslatorEngineType.YANDEX -> yandexEngine
            TranslatorEngineType.LINGVA -> lingvaEngine
            TranslatorEngineType.DEEPL -> deepLEngine
            TranslatorEngineType.LIBRE -> libreEngine
            TranslatorEngineType.MYMEMORY -> myMemoryEngine
            TranslatorEngineType.CUSTOM_AI -> customAiEngine
            else -> googleEngine
        }

        val detectedBlocks = when (scanRegion) {
            "bottom" -> rawBlocks.filter { it.boundingBox.centerY() >= 0.50f }
            "center" -> rawBlocks.filter { it.boundingBox.centerY() in 0.20f..0.85f }
            "top" -> rawBlocks.filter { it.boundingBox.centerY() <= 0.50f }
            "custom" -> if (customRegions.isNotEmpty()) {
                rawBlocks.filter { block ->
                    val cx = block.boundingBox.centerX()
                    val cy = block.boundingBox.centerY()
                    customRegions.any { r -> r.rect.contains(cx, cy) }
                }
            } else rawBlocks
            else -> rawBlocks
        }

        if (detectedBlocks.isEmpty()) {
            withContext(Dispatchers.Main) {
                val msg = if (rawBlocks.isNotEmpty()) "В выбранной области текст не найден" else "Текст на экране не обнаружен"
                Toast.makeText(context, msg, Toast.LENGTH_SHORT).show()
            }
            return
        }

        // Translate blocks with in-memory caching
        val translatedBlocks = mutableListOf<TranslatedTextBlock>()
        for (block in detectedBlocks) {
            val text = block.originalText.trim()
            val cacheKey = "$text|$sourceLang|$targetLang|${engineType.preferenceValue}"
            val cached = translationCache[cacheKey]
            val translated = if (cached != null) {
                cached
            } else {
                val result = engine.translate(text, sourceLang, targetLang)
                if (result.isNotBlank() && result != text) {
                    translationCache[cacheKey] = result
                }
                result
            }
            block.translatedText = translated
            translatedBlocks.add(block)
        }

        withContext(Dispatchers.Main) {
            overlayView?.apply {
                onSpeakRequested = { txt -> ttsManager.speak(txt) }
                updateBlocks(translatedBlocks)
            }
            if (autoSpeak) {
                val fullText = translatedBlocks.joinToString(". ") { it.translatedText }
                ttsManager.speak(fullText)
            }
        }
    }

    private suspend fun captureFrame(surfaceView: SurfaceView?): Bitmap? = withContext(Dispatchers.Default) {
        // Try native capture buffer first
        val frameData = NativeLibrary.getAppletCaptureBuffer()
        val width = NativeLibrary.getAppletCaptureWidth()
        val height = NativeLibrary.getAppletCaptureHeight()

        if (frameData.isNotEmpty() && width > 0 && height > 0 && frameData.size >= width * height * 4) {
            val bmp = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
            bmp.copyPixelsFromBuffer(ByteBuffer.wrap(frameData, 0, width * height * 4))
            return@withContext bmp
        }

        // Fallback: PixelCopy from SurfaceView
        if (surfaceView != null && surfaceView.holder.surface.isValid) {
            val bmp = Bitmap.createBitmap(surfaceView.width, surfaceView.height, Bitmap.Config.ARGB_8888)
            val success = kotlinx.coroutines.suspendCancellableCoroutine<Boolean> { cont ->
                android.view.PixelCopy.request(surfaceView, bmp, { result ->
                    cont.resume(result == android.view.PixelCopy.SUCCESS, null)
                }, Handler(Looper.getMainLooper()))
            }
            if (success) return@withContext bmp
            bmp.recycle()
        }

        null
    }

    fun onDestroy() {
        stopAutoTranslateLoop()
        activeTranslationJob?.cancel()
        ttsManager.shutdown()
    }
}
