// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.translator.model

import android.graphics.RectF
import org.json.JSONArray
import org.json.JSONObject
import java.util.UUID

enum class TranslatorEngineType(val preferenceValue: String, val displayName: String) {
    YANDEX("yandex", "Яндекс Переводчик (Бесплатно / Идеальный русский)"),
    GOOGLE("google", "Google Translate (Бесплатно / Высокая скорость)"),
    LINGVA("lingva", "Lingva Neural (Бесплатно / Без ограничений)"),
    DEEPL("deepl", "DeepL Neural API"),
    LIBRE("libre", "LibreTranslate (Open-Source / Бесплатно)"),
    MYMEMORY("mymemory", "MyMemory Translated"),
    CUSTOM_AI("custom_ai", "Нейросеть / Custom AI (OpenAI / Claude / Gemini / DeepSeek)");

    companion object {
        fun fromPreference(value: String?): TranslatorEngineType {
            return entries.firstOrNull { it.preferenceValue == value } ?: GOOGLE
        }
    }
}

enum class TranslatorTriggerMode(val preferenceValue: String) {
    ON_DEMAND("on_demand"),
    AUTO_SCREEN_CHANGE("auto_screen_change");

    companion object {
        fun fromPreference(value: String?): TranslatorTriggerMode {
            return entries.firstOrNull { it.preferenceValue == value } ?: ON_DEMAND
        }
    }
}

enum class TranslatorOverlayStyle(val preferenceValue: String) {
    SMART_BACKGROUND_MATCH("smart_background_match"),
    SEMI_TRANSPARENT("semi_transparent"),
    TRANSLUCENT_BUBBLE("translucent_bubble"),
    OUTLINE_ONLY("outline_only");

    companion object {
        fun fromPreference(value: String?): TranslatorOverlayStyle {
            return entries.firstOrNull { it.preferenceValue == value } ?: SMART_BACKGROUND_MATCH
        }
    }
}

data class TranslatedTextBlock(
    val originalText: String,
    var translatedText: String,
    val boundingBox: RectF,
    val backgroundColor: Int,
    val textColor: Int,
    var isShowingOriginal: Boolean = false,
)

data class TranslationRegion(
    val id: String = UUID.randomUUID().toString(),
    var rect: RectF, // Relative coordinates 0.0f..1.0f
    var name: String = "",
) {
    fun toJson(): JSONObject {
        return JSONObject().apply {
            put("id", id)
            put("left", rect.left.toDouble())
            put("top", rect.top.toDouble())
            put("right", rect.right.toDouble())
            put("bottom", rect.bottom.toDouble())
            put("name", name)
        }
    }

    companion object {
        fun fromJson(json: JSONObject): TranslationRegion? {
            return runCatching {
                val id = json.optString("id", UUID.randomUUID().toString())
                val left = json.getDouble("left").toFloat().coerceIn(0f, 1f)
                val top = json.getDouble("top").toFloat().coerceIn(0f, 1f)
                val right = json.getDouble("right").toFloat().coerceIn(0f, 1f)
                val bottom = json.getDouble("bottom").toFloat().coerceIn(0f, 1f)
                val name = json.optString("name", "")
                TranslationRegion(id, RectF(left, top, right, bottom), name)
            }.getOrNull()
        }

        fun listToJson(regions: List<TranslationRegion>): String {
            val arr = JSONArray()
            for (r in regions) {
                arr.put(r.toJson())
            }
            return arr.toString()
        }

        fun listFromJson(jsonStr: String?): List<TranslationRegion> {
            if (jsonStr.isNullOrBlank()) return emptyList()
            return runCatching {
                val arr = JSONArray(jsonStr)
                val list = mutableListOf<TranslationRegion>()
                for (i in 0 until arr.length()) {
                    val obj = arr.optJSONObject(i) ?: continue
                    fromJson(obj)?.let { list.add(it) }
                }
                list
            }.getOrDefault(emptyList())
        }
    }
}
