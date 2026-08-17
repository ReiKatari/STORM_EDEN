// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.utils

import android.content.Context
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request
import org.json.JSONArray
import org.json.JSONObject
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.YuzuApplication
import java.io.File
import java.io.FileOutputStream
import java.util.concurrent.TimeUnit
import kotlin.random.Random

data class AmiiboEntry(
    val name: String,
    val character: String,
    val gameSeries: String,
    val amiiboSeries: String,
    val type: String,
    val head: String,
    val tail: String,
    val imageUrl: String,
    val switchGames: List<String> = emptyList()
) {
    val fullId: String get() = "${head}${tail}".uppercase()
}

object AmiiboHelper {
    private const val USER_AGENT =
        "Mozilla/5.0 (Linux; Android 14; Mobile) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Mobile Safari/537.36 STORM-EDEN/4.0.0"

    private val httpClient: OkHttpClient by lazy {
        OkHttpClient.Builder()
            .followRedirects(true)
            .followSslRedirects(true)
            .connectTimeout(20, TimeUnit.SECONDS)
            .readTimeout(30, TimeUnit.SECONDS)
            .build()
    }

    private var cachedAmiibos: List<AmiiboEntry> = emptyList()

    suspend fun getAmiiboDatabase(forceRefresh: Boolean = false): List<AmiiboEntry> =
        withContext(Dispatchers.IO) {
            if (cachedAmiibos.isNotEmpty() && !forceRefresh) {
                return@withContext cachedAmiibos
            }

            val cacheFile = File(YuzuApplication.appContext.cacheDir, "amiibo_cache.json")
            if (cacheFile.exists() && !forceRefresh) {
                try {
                    val jsonText = cacheFile.readText()
                    val parsed = parseAmiiboJson(jsonText)
                    if (parsed.isNotEmpty()) {
                        cachedAmiibos = parsed
                        return@withContext parsed
                    }
                } catch (e: Exception) {
                    e.printStackTrace()
                }
            }

            val urls = listOf(
                "https://www.amiiboapi.com/api/amiibo/",
                "https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/database/amiibo.json",
                "https://cdn.jsdelivr.net/gh/N3evin/AmiiboAPI@master/database/amiibo.json"
            )

            for (url in urls) {
                try {
                    val request = Request.Builder()
                        .url(url)
                        .header("User-Agent", USER_AGENT)
                        .header("Accept", "application/json, text/plain, */*")
                        .build()

                    val response = httpClient.newCall(request).execute()
                    if (response.isSuccessful) {
                        val bodyString = response.body?.string()
                        if (!bodyString.isNullOrEmpty()) {
                            val parsed = parseAmiiboJson(bodyString)
                            if (parsed.isNotEmpty()) {
                                try {
                                    cacheFile.writeText(bodyString)
                                } catch (_: Exception) {}
                                cachedAmiibos = parsed
                                return@withContext parsed
                            }
                        }
                    }
                } catch (e: Exception) {
                    e.printStackTrace()
                }
            }

            if (cacheFile.exists()) {
                try {
                    cachedAmiibos = parseAmiiboJson(cacheFile.readText())
                } catch (_: Exception) {}
            }

            return@withContext cachedAmiibos
        }

    private fun parseAmiiboJson(jsonText: String): List<AmiiboEntry> {
        val list = mutableListOf<AmiiboEntry>()
        try {
            val root = JSONObject(jsonText)
            if (root.has("amiibo")) {
                val array = root.optJSONArray("amiibo") ?: JSONArray()
                for (i in 0 until array.length()) {
                    val obj = array.optJSONObject(i) ?: continue
                    parseAmiiboObject(obj)?.let { list.add(it) }
                }
            } else if (root.has("amiibos")) {
                val amiibosObj = root.optJSONObject("amiibos")
                if (amiibosObj != null) {
                    val keys = amiibosObj.keys()
                    while (keys.hasNext()) {
                        val key = keys.next()
                        val obj = amiibosObj.optJSONObject(key) ?: continue
                        val entry = parseAmiiboObject(obj, key)
                        if (entry != null) list.add(entry)
                    }
                } else {
                    val arr = root.optJSONArray("amiibos") ?: JSONArray()
                    for (i in 0 until arr.length()) {
                        val obj = arr.optJSONObject(i) ?: continue
                        parseAmiiboObject(obj)?.let { list.add(it) }
                    }
                }
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
        return list
    }

    private fun parseAmiiboObject(obj: JSONObject, keyFallback: String = ""): AmiiboEntry? {
        val name = obj.optString("name", "").ifEmpty { obj.optString("character", "") }
        if (name.isEmpty()) return null

        val character = obj.optString("character", name)
        val gameSeries = obj.optString("gameSeries", "Nintendo")
        val amiiboSeries = obj.optString("amiiboSeries", "Others")
        val type = obj.optString("type", "Figure")
        var head = obj.optString("head", "")
        var tail = obj.optString("tail", "")
        val image = obj.optString("image", "")

        if (head.isEmpty() && keyFallback.length >= 16) {
            head = keyFallback.substring(0, 8)
            tail = keyFallback.substring(8, 16)
        }

        val switchGamesList = mutableListOf<String>()
        val gamesSwitch = obj.optJSONArray("gamesSwitch")
        if (gamesSwitch != null) {
            for (i in 0 until gamesSwitch.length()) {
                val g = gamesSwitch.optJSONObject(i) ?: continue
                val gName = g.optString("gameName", "")
                if (gName.isNotEmpty()) {
                    val usages = g.optJSONArray("amiiboUsage")
                    val usageText = StringBuilder()
                    if (usages != null) {
                        for (j in 0 until usages.length()) {
                            val u = usages.optJSONObject(j) ?: continue
                            val uStr = u.optString("Usage", "")
                            if (uStr.isNotEmpty()) {
                                if (usageText.isNotEmpty()) usageText.append("; ")
                                usageText.append(uStr)
                            }
                        }
                    }
                    if (usageText.isNotEmpty()) {
                        switchGamesList.add("• $gName: $usageText")
                    } else {
                        switchGamesList.add("• $gName")
                    }
                }
            }
        }

        return AmiiboEntry(
            name = name,
            character = character,
            gameSeries = gameSeries,
            amiiboSeries = amiiboSeries,
            type = type,
            head = head,
            tail = tail,
            imageUrl = image,
            switchGames = switchGamesList
        )
    }

    fun generateAmiiboBin(entry: AmiiboEntry): ByteArray {
        val bin = ByteArray(540)

        // NTAG215 Header (UID + BCC)
        bin[0] = 0x04.toByte()
        bin[1] = Random.nextInt(0x01, 0xFE).toByte()
        bin[2] = Random.nextInt(0x01, 0xFE).toByte()
        bin[3] = (0x88.toByte().toInt() xor bin[0].toInt() xor bin[1].toInt() xor bin[2].toInt()).toByte()
        bin[4] = Random.nextInt(0x01, 0xFE).toByte()
        bin[5] = Random.nextInt(0x01, 0xFE).toByte()
        bin[6] = Random.nextInt(0x01, 0xFE).toByte()
        bin[7] = Random.nextInt(0x01, 0xFE).toByte()
        bin[8] = (bin[4].toInt() xor bin[5].toInt() xor bin[6].toInt() xor bin[7].toInt()).toByte()

        // Internal bytes
        bin[9] = 0x48.toByte()
        bin[10] = 0x00.toByte()
        bin[11] = 0x00.toByte()

        // Capability Container (CC) for NTAG215
        bin[12] = 0xE1.toByte()
        bin[13] = 0x10.toByte()
        bin[14] = 0x3E.toByte()
        bin[15] = 0x00.toByte()

        // Amiibo Model ID at offset 0x54 (8 bytes = 4 bytes head + 4 bytes tail)
        try {
            val headVal = entry.head.toLong(16)
            bin[0x54] = ((headVal ushr 24) and 0xFF).toByte()
            bin[0x55] = ((headVal ushr 16) and 0xFF).toByte()
            bin[0x56] = ((headVal ushr 8) and 0xFF).toByte()
            bin[0x57] = (headVal and 0xFF).toByte()

            val tailVal = entry.tail.toLong(16)
            bin[0x58] = ((tailVal ushr 24) and 0xFF).toByte()
            bin[0x59] = ((tailVal ushr 16) and 0xFF).toByte()
            bin[0x5A] = ((tailVal ushr 8) and 0xFF).toByte()
            bin[0x5B] = (tailVal and 0xFF).toByte()
        } catch (_: Exception) {}

        // Nickname UTF-16BE at offset 0x38 (up to 10 characters)
        val cleanName = entry.name.take(10)
        for (i in cleanName.indices) {
            val code = cleanName[i].code
            bin[0x38 + i * 2] = ((code ushr 8) and 0xFF).toByte()
            bin[0x38 + i * 2 + 1] = (code and 0xFF).toByte()
        }

        // Initialize write counter & flags
        bin[0xB4] = 0x01.toByte() // Initialized flag
        bin[0xB8] = 0x01.toByte() // Write count

        return bin
    }

    fun saveAmiiboToStorage(entry: AmiiboEntry): File {
        val userDir = File(DirectoryInitialization.userDirectory, "amiibo")
        userDir.mkdirs()

        val safeName = entry.name.replace(Regex("[^a-zA-Z0-9._ -]"), "_").trim()
        val fileName = if (safeName.isNotEmpty()) "$safeName.bin" else "Amiibo_${entry.fullId}.bin"
        val targetFile = File(userDir, fileName)

        val bytes = generateAmiiboBin(entry)
        FileOutputStream(targetFile).use { it.write(bytes) }
        return targetFile
    }

    fun loadAmiiboDirectly(entry: AmiiboEntry): Boolean {
        val bytes = generateAmiiboBin(entry)
        val result = NativeLibrary.loadAmiibo(bytes)
        return result == 0
    }
}
