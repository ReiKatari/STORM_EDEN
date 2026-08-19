// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.translator.engine

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import org.json.JSONArray
import org.json.JSONObject
import java.net.URLEncoder
import java.util.concurrent.TimeUnit

interface ITranslationEngine {
    suspend fun translate(text: String, sourceLang: String, targetLang: String): String
}

class YandexTranslateEngine(private val client: OkHttpClient) : ITranslationEngine {
    override suspend fun translate(text: String, sourceLang: String, targetLang: String): String = withContext(Dispatchers.IO) {
        if (text.isBlank()) return@withContext text

        val sl = if (sourceLang == "auto" || sourceLang.isBlank()) "" else "$sourceLang-"
        val langPair = "$sl$targetLang"
        val encodedText = URLEncoder.encode(text, "UTF-8").replace("+", "%20")
        val url = "https://translate.yandex.net/api/v1/tr.json/translate?srv=android&lang=$langPair&text=$encodedText"

        val request = Request.Builder()
            .url(url)
            .header("User-Agent", "Mozilla/5.0 (Android; Mobile; rv:128.0) Gecko/128.0 Firefox/128.0")
            .build()

        client.newCall(request).execute().use { response ->
            if (!response.isSuccessful) return@withContext text
            val responseBody = response.body?.string() ?: return@withContext text
            val root = JSONObject(responseBody)
            val textArray = root.optJSONArray("text")
            if (textArray != null && textArray.length() > 0) {
                val sb = StringBuilder()
                for (i in 0 until textArray.length()) {
                    sb.append(textArray.optString(i)).append(" ")
                }
                sb.toString().trim().ifEmpty { text }
            } else {
                root.optString("text", text)
            }
        }
    }
}

class LingvaTranslateEngine(private val client: OkHttpClient) : ITranslationEngine {
    private val mirrors = listOf(
        "https://lingva.ml",
        "https://lingva.thedaviddelta.com",
        "https://translate.plausibility.cloud"
    )

    override suspend fun translate(text: String, sourceLang: String, targetLang: String): String = withContext(Dispatchers.IO) {
        if (text.isBlank()) return@withContext text
        val sl = if (sourceLang == "auto") "auto" else sourceLang
        val encodedText = URLEncoder.encode(text, "UTF-8").replace("+", "%20")

        for (host in mirrors) {
            try {
                val url = "$host/api/v1/$sl/$targetLang/$encodedText"
                val request = Request.Builder()
                    .url(url)
                    .header("User-Agent", "Mozilla/5.0 (Android; Mobile; rv:128.0)")
                    .build()

                client.newCall(request).execute().use { response ->
                    if (response.isSuccessful) {
                        val body = response.body?.string() ?: return@use
                        val json = JSONObject(body)
                        val translation = json.optString("translation")
                        if (translation.isNotBlank()) {
                            return@withContext translation.replace("+", " ")
                        }
                    }
                }
            } catch (_: Throwable) {}
        }
        text
    }
}

class GoogleTranslateEngine(private val client: OkHttpClient) : ITranslationEngine {
    override suspend fun translate(text: String, sourceLang: String, targetLang: String): String = withContext(Dispatchers.IO) {
        if (text.isBlank()) return@withContext text

        val sl = if (sourceLang == "auto") "auto" else sourceLang
        val tl = targetLang
        val encodedText = URLEncoder.encode(text, "UTF-8").replace("+", "%20")
        val url = "https://translate.googleapis.com/translate_a/single?client=gtx&sl=$sl&tl=$tl&dt=t&q=$encodedText"

        val request = Request.Builder()
            .url(url)
            .header("User-Agent", "Mozilla/5.0 (Android; Mobile; rv:128.0) Gecko/128.0 Firefox/128.0")
            .build()

        client.newCall(request).execute().use { response ->
            if (!response.isSuccessful) return@withContext text
            val responseBody = response.body?.string() ?: return@withContext text
            val jsonArray = JSONArray(responseBody)
            val sentences = jsonArray.optJSONArray(0) ?: return@withContext text
            val sb = StringBuilder()
            for (i in 0 until sentences.length()) {
                val sentence = sentences.optJSONArray(i) ?: continue
                val part = sentence.optString(0, "")
                sb.append(part)
            }
            sb.toString().trim().ifEmpty { text }
        }
    }
}

class MyMemoryTranslateEngine(private val client: OkHttpClient) : ITranslationEngine {
    override suspend fun translate(text: String, sourceLang: String, targetLang: String): String = withContext(Dispatchers.IO) {
        if (text.isBlank()) return@withContext text

        val sl = if (sourceLang == "auto" || sourceLang.isBlank()) "en" else sourceLang
        val langPair = "$sl|$targetLang"
        val encodedText = URLEncoder.encode(text, "UTF-8").replace("+", "%20")
        val url = "https://api.mymemory.translated.net/get?q=$encodedText&langpair=$langPair"

        val request = Request.Builder()
            .url(url)
            .header("User-Agent", "Mozilla/5.0 (Android; Mobile; rv:128.0)")
            .build()

        client.newCall(request).execute().use { response ->
            if (!response.isSuccessful) return@withContext text
            val responseBody = response.body?.string() ?: return@withContext text
            val root = JSONObject(responseBody)
            val resData = root.optJSONObject("responseData")
            val trans = resData?.optString("translatedText", "") ?: ""
            if (trans.isNotBlank()) trans else text
        }
    }
}

class DeepLEngine(
    private val client: OkHttpClient,
    private val apiKeyProvider: () -> String
) : ITranslationEngine {
    override suspend fun translate(text: String, sourceLang: String, targetLang: String): String = withContext(Dispatchers.IO) {
        val apiKey = apiKeyProvider().trim()
        if (apiKey.isEmpty() || text.isBlank()) return@withContext text

        val isFreeApi = apiKey.endsWith(":fx")
        val endpoint = if (isFreeApi) "https://api-free.deepl.com/v2/translate" else "https://api.deepl.com/v2/translate"

        val json = JSONObject().apply {
            put("text", JSONArray().put(text))
            put("target_lang", targetLang.uppercase())
            if (sourceLang != "auto") {
                put("source_lang", sourceLang.uppercase())
            }
        }

        val requestBody = json.toString().toRequestBody("application/json; charset=utf-8".toMediaType())
        val request = Request.Builder()
            .url(endpoint)
            .header("Authorization", "DeepL-Auth-Key $apiKey")
            .post(requestBody)
            .build()

        client.newCall(request).execute().use { response ->
            if (!response.isSuccessful) return@withContext text
            val responseBody = response.body?.string() ?: return@withContext text
            val root = JSONObject(responseBody)
            val translations = root.optJSONArray("translations")
            translations?.optJSONObject(0)?.optString("text") ?: text
        }
    }
}

class LibreTranslateEngine(
    private val client: OkHttpClient,
    private val serverUrlProvider: () -> String = { "https://translate.terraprint.co/translate" },
    private val apiKeyProvider: () -> String = { "" }
) : ITranslationEngine {
    override suspend fun translate(text: String, sourceLang: String, targetLang: String): String = withContext(Dispatchers.IO) {
        if (text.isBlank()) return@withContext text

        val server = serverUrlProvider().ifBlank { "https://translate.terraprint.co/translate" }
        val json = JSONObject().apply {
            put("q", text)
            put("source", if (sourceLang == "auto") "auto" else sourceLang)
            put("target", targetLang)
            put("format", "text")
            val apiKey = apiKeyProvider().trim()
            if (apiKey.isNotEmpty()) {
                put("api_key", apiKey)
            }
        }

        val requestBody = json.toString().toRequestBody("application/json; charset=utf-8".toMediaType())
        val request = Request.Builder()
            .url(server)
            .header("Content-Type", "application/json; charset=utf-8")
            .post(requestBody)
            .build()

        client.newCall(request).execute().use { response ->
            if (!response.isSuccessful) return@withContext text
            val responseBody = response.body?.string() ?: return@withContext text
            val root = JSONObject(responseBody)
            val result = root.optString("translatedText", text)
            result.replace("+", " ")
        }
    }
}

class CustomAiEngine(
    private val client: OkHttpClient,
    private val apiKeyProvider: () -> String,
    private val endpointProvider: () -> String = { "https://api.openai.com/v1/chat/completions" },
    private val modelProvider: () -> String = { "gpt-4o-mini" }
) : ITranslationEngine {
    override suspend fun translate(text: String, sourceLang: String, targetLang: String): String = withContext(Dispatchers.IO) {
        val apiKey = apiKeyProvider().trim()
        if (apiKey.isEmpty() || text.isBlank()) return@withContext text

        val endpoint = endpointProvider().ifBlank { "https://api.openai.com/v1/chat/completions" }
        val model = modelProvider().ifBlank { "gpt-4o-mini" }

        val systemPrompt = "You are a professional video game localization expert. Translate the provided in-game dialogue/UI text accurately into natural, immersive $targetLang. Output ONLY the translated text without notes or quotes."

        val json = JSONObject().apply {
            put("model", model)
            put("messages", JSONArray().apply {
                put(JSONObject().apply {
                    put("role", "system")
                    put("content", systemPrompt)
                })
                put(JSONObject().apply {
                    put("role", "user")
                    put("content", text)
                })
            })
            put("temperature", 0.3)
        }

        val requestBody = json.toString().toRequestBody("application/json; charset=utf-8".toMediaType())
        val request = Request.Builder()
            .url(endpoint)
            .header("Authorization", "Bearer $apiKey")
            .post(requestBody)
            .build()

        client.newCall(request).execute().use { response ->
            if (!response.isSuccessful) return@withContext text
            val responseBody = response.body?.string() ?: return@withContext text
            val root = JSONObject(responseBody)
            val choices = root.optJSONArray("choices")
            val message = choices?.optJSONObject(0)?.optJSONObject("message")
            message?.optString("content")?.trim() ?: text
        }
    }
}

