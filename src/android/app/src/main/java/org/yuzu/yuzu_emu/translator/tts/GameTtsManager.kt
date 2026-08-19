// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.translator.tts

import android.content.Context
import android.os.Build
import android.speech.tts.TextToSpeech
import android.speech.tts.Voice
import androidx.preference.PreferenceManager
import java.util.Locale

class GameTtsManager(private val context: Context) {

    private var tts: TextToSpeech? = null
    private var isInitialized = false
    private var availableVoices: List<Voice> = emptyList()
    private var pendingSpeakText: String? = null
    private var pendingCharacterHint: String = ""

    init {
        initTts()
    }

    private fun initTts() {
        tts = TextToSpeech(context.applicationContext) { status ->
            if (status == TextToSpeech.SUCCESS) {
                isInitialized = true
                applyLocale()
                tts?.let { engine ->
                    try {
                        availableVoices = engine.voices?.toList() ?: emptyList()
                    } catch (_: Throwable) {}
                }
                pendingSpeakText?.let { text ->
                    val hint = pendingCharacterHint
                    pendingSpeakText = null
                    speak(text, hint)
                }
            }
        }
    }

    private fun applyLocale() {
        val prefs = PreferenceManager.getDefaultSharedPreferences(context)
        val targetLang = prefs.getString("translator_target_lang", "ru") ?: "ru"
        val locale = when (targetLang.lowercase()) {
            "ru" -> Locale("ru", "RU")
            "en" -> Locale.US
            "es" -> Locale("es", "ES")
            "de" -> Locale.GERMAN
            "fr" -> Locale.FRENCH
            "it" -> Locale.ITALIAN
            "zh" -> Locale.CHINESE
            "ja" -> Locale.JAPANESE
            "ko" -> Locale.KOREAN
            else -> Locale.getDefault()
        }
        val result = tts?.setLanguage(locale)
        if (result == TextToSpeech.LANG_MISSING_DATA || result == TextToSpeech.LANG_NOT_SUPPORTED) {
            tts?.setLanguage(Locale.getDefault())
        }
    }

    fun speak(text: String, characterHint: String = "") {
        if (text.isBlank()) return

        if (!isInitialized || tts == null) {
            pendingSpeakText = text
            pendingCharacterHint = characterHint
            return
        }

        applyLocale()
        val prefs = PreferenceManager.getDefaultSharedPreferences(context)
        val baseSpeed = prefs.getFloat("translator_tts_speed", 1.0f)
        val smartVoiceEnabled = prefs.getBoolean("translator_smart_voices", true)

        val voicePersona = if (smartVoiceEnabled) {
            analyzeVoicePersona(text, characterHint)
        } else {
            VoicePersona(pitch = 1.0f, speedMultiplier = 1.0f, gender = VoiceGender.NEUTRAL)
        }

        // Apply best matching system voice if available
        applyMatchingVoice(voicePersona.gender)

        val finalSpeed = (baseSpeed * voicePersona.speedMultiplier).coerceIn(0.5f, 2.0f)
        tts?.setSpeechRate(finalSpeed)
        tts?.setPitch(voicePersona.pitch.coerceIn(0.5f, 2.0f))
        tts?.speak(text, TextToSpeech.QUEUE_FLUSH, null, "EDEN_TTS_${System.currentTimeMillis()}")
    }

    fun stop() {
        tts?.stop()
    }

    fun shutdown() {
        tts?.stop()
        tts?.shutdown()
        tts = null
        isInitialized = false
    }

    private fun applyMatchingVoice(gender: VoiceGender) {
        if (availableVoices.isEmpty() || tts == null) return
        val currentLocale = tts?.voice?.locale ?: return

        try {
            val matchingVoices = availableVoices.filter { 
                it.locale.language.equals(currentLocale.language, ignoreCase = true)
            }
            if (matchingVoices.isNotEmpty()) {
                val selectedVoice = when (gender) {
                    VoiceGender.FEMALE -> matchingVoices.firstOrNull { 
                        it.name.contains("female", ignoreCase = true) || 
                        it.name.contains("fem", ignoreCase = true) ||
                        it.name.contains("woman", ignoreCase = true) ||
                        it.features?.contains("female") == true
                    } ?: matchingVoices.firstOrNull()

                    VoiceGender.MALE -> matchingVoices.firstOrNull { 
                        it.name.contains("male", ignoreCase = true) || 
                        it.name.contains("man", ignoreCase = true) ||
                        it.features?.contains("male") == true
                    } ?: matchingVoices.firstOrNull()

                    VoiceGender.NEUTRAL -> matchingVoices.firstOrNull()
                }
                if (selectedVoice != null) {
                    tts?.voice = selectedVoice
                }
            }
        } catch (_: Throwable) {}
    }

    private fun analyzeVoicePersona(text: String, characterHint: String): VoicePersona {
        val combined = "$text $characterHint".lowercase()

        var pitch = 1.0f
        var speedMultiplier = 1.0f
        var gender = VoiceGender.NEUTRAL

        // 1. Princess / Young Heroine / Fairy / Girl
        if (combined.contains("zelda") || combined.contains("зельда") ||
            combined.contains("peach") || combined.contains("пич") ||
            combined.contains("daisy") || combined.contains("дэйзи") ||
            combined.contains("mipha") || combined.contains("мифа") ||
            combined.contains("purah") || combined.contains("пура") ||
            combined.contains("riju") || combined.contains("риджу") ||
            combined.contains("sonia") || combined.contains("соня") ||
            combined.contains("fairy") || combined.contains("фея") ||
            combined.contains("girl") || combined.contains("девушка") ||
            combined.contains("princess") || combined.contains("принцесса") ||
            combined.contains("maiden") || combined.contains("дева")) {
            pitch = 1.32f
            speedMultiplier = 1.04f
            gender = VoiceGender.FEMALE
        }
        // 2. Ancient Boss / Demon / Titan / Dragon / Elder
        else if (combined.contains("ganon") || combined.contains("ганон") ||
            combined.contains("bowser") || combined.contains("боузер") ||
            combined.contains("boss") || combined.contains("босс") ||
            combined.contains("monster") || combined.contains("монстр") ||
            combined.contains("demon") || combined.contains("демон") ||
            combined.contains("dragon") || combined.contains("дракон") ||
            combined.contains("titan") || combined.contains("титан") ||
            combined.contains("king") || combined.contains("король") ||
            combined.contains("elder") || combined.contains("старейшина") ||
            combined.contains("wizard") || combined.contains("мудрец") ||
            combined.contains("rauru") || combined.contains("рауру") ||
            combined.contains("daruk") || combined.contains("дарук")) {
            pitch = 0.68f
            speedMultiplier = 0.88f
            gender = VoiceGender.MALE
        }
        // 3. Child / Small Creature
        else if (combined.contains("tulin") || combined.contains("тулин") ||
            combined.contains("toad") || combined.contains("тоад") ||
            combined.contains("korok") || combined.contains("корок") ||
            combined.contains("child") || combined.contains("ребенок") ||
            combined.contains("kid") || combined.contains("малыш") ||
            combined.contains("boy") || combined.contains("мальчик")) {
            pitch = 1.55f
            speedMultiplier = 1.15f
            gender = VoiceGender.FEMALE
        }
        // 4. Robot / System / Sheikah / UI
        else if (combined.contains("system") || combined.contains("система") ||
            combined.contains("robot") || combined.contains("робот") ||
            combined.contains("ai") || combined.contains("ии") ||
            combined.contains("obtained") || combined.contains("получено") ||
            combined.contains("log") || combined.contains("журнал") ||
            combined.contains("quest") || combined.contains("задание") ||
            combined.contains("pad") || combined.contains("планшет")) {
            pitch = 0.92f
            speedMultiplier = 1.02f
            gender = VoiceGender.NEUTRAL
        }
        // 5. Hero / Adventurer
        else if (combined.contains("link") || combined.contains("линк") ||
            combined.contains("mario") || combined.contains("марио") ||
            combined.contains("luigi") || combined.contains("луиджи") ||
            combined.contains("knight") || combined.contains("рыцарь") ||
            combined.contains("hero") || combined.contains("герой")) {
            pitch = 1.02f
            speedMultiplier = 1.0f
            gender = VoiceGender.MALE
        }

        // Emotional context modifiers
        if (text.contains("!") || combined.contains("осторожно") || combined.contains("берегись") || combined.contains("беги")) {
            pitch += 0.10f
            speedMultiplier += 0.08f
        } else if (text.contains("...") || combined.contains("тише") || combined.contains("тайна") || combined.contains("секрет")) {
            pitch -= 0.08f
            speedMultiplier -= 0.10f
        }

        return VoicePersona(pitch, speedMultiplier, gender)
    }

    private enum class VoiceGender {
        MALE, FEMALE, NEUTRAL
    }

    private data class VoicePersona(
        val pitch: Float,
        val speedMultiplier: Float,
        val gender: VoiceGender
    )
}
