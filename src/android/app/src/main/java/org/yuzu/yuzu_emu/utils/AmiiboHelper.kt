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
            "https://cdn.jsdelivr.net/gh/N3evin/AmiiboAPI@master/database/amiibo.json",
            "https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/database/amiibo.json",
            "https://www.amiiboapi.com/api/amiibo/"
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
            val amiiboSeriesMap = root.optJSONObject("amiibo_series")
            val gameSeriesMap = root.optJSONObject("game_series")
            val typesMap = root.optJSONObject("types")
            val charactersMap = root.optJSONObject("characters")

            if (root.has("amiibo")) {
                val array = root.optJSONArray("amiibo") ?: JSONArray()
                for (i in 0 until array.length()) {
                    val obj = array.optJSONObject(i) ?: continue
                    parseAmiiboObject(obj, "", amiiboSeriesMap, gameSeriesMap, typesMap, charactersMap)?.let { list.add(it) }
                }
            } else if (root.has("amiibos")) {
                val amiibosObj = root.optJSONObject("amiibos")
                if (amiibosObj != null) {
                    val keys = amiibosObj.keys()
                    while (keys.hasNext()) {
                        val key = keys.next()
                        val obj = amiibosObj.optJSONObject(key) ?: continue
                        val entry = parseAmiiboObject(obj, key, amiiboSeriesMap, gameSeriesMap, typesMap, charactersMap)
                        if (entry != null) list.add(entry)
                    }
                } else {
                    val arr = root.optJSONArray("amiibos") ?: JSONArray()
                    for (i in 0 until arr.length()) {
                        val obj = arr.optJSONObject(i) ?: continue
                        parseAmiiboObject(obj, "", amiiboSeriesMap, gameSeriesMap, typesMap, charactersMap)?.let { list.add(it) }
                    }
                }
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
        return list
    }

    private fun parseAmiiboObject(
        obj: JSONObject,
        keyFallback: String = "",
        amiiboSeriesMap: JSONObject? = null,
        gameSeriesMap: JSONObject? = null,
        typesMap: JSONObject? = null,
        charactersMap: JSONObject? = null
    ): AmiiboEntry? {
        val name = obj.optString("name", "").ifEmpty { obj.optString("character", "") }
        if (name.isEmpty()) return null

        var head = obj.optString("head", "")
        var tail = obj.optString("tail", "")

        var cleanKey = keyFallback.trim()
        if (cleanKey.startsWith("0x", ignoreCase = true)) {
            cleanKey = cleanKey.substring(2)
        }
        if (head.isEmpty() && cleanKey.length >= 16) {
            head = cleanKey.substring(0, 8)
            tail = cleanKey.substring(8, 16)
        }

        var amiiboSeries = obj.optString("amiiboSeries", "")
        if (amiiboSeries.isEmpty() && tail.length >= 4 && amiiboSeriesMap != null) {
            val seriesId = "0x" + tail.substring(2, 4).lowercase()
            amiiboSeries = amiiboSeriesMap.optString(seriesId, "")
        }
        if (amiiboSeries.isEmpty()) amiiboSeries = "Others"

        var type = obj.optString("type", "")
        if (type.isEmpty() && tail.length >= 8 && typesMap != null) {
            val typeId = "0x" + tail.substring(6, 8).lowercase()
            type = typesMap.optString(typeId, "")
        }
        if (type.isEmpty()) type = "Figure"

        var gameSeries = obj.optString("gameSeries", "")
        if (gameSeries.isEmpty() && head.length >= 3 && gameSeriesMap != null) {
            val gId = "0x" + head.substring(0, 3).lowercase()
            gameSeries = gameSeriesMap.optString(gId, "")
        }
        if (gameSeries.isEmpty()) gameSeries = "Nintendo"

        var character = obj.optString("character", "")
        if (character.isEmpty() && head.length >= 4 && charactersMap != null) {
            val cId = "0x" + head.substring(0, 4).lowercase()
            character = charactersMap.optString(cId, "")
        }
        if (character.isEmpty()) character = name

        var image = obj.optString("image", "")
        if (image.isEmpty() && head.isNotEmpty() && tail.isNotEmpty()) {
            image = "https://cdn.jsdelivr.net/gh/N3evin/AmiiboAPI@master/images/icon_${head.lowercase()}-${tail.lowercase()}.png"
        }

        val switchGamesList = mutableListOf<String>()
        val gamesSwitch = obj.optJSONArray("gamesSwitch")
        if (gamesSwitch != null && gamesSwitch.length() > 0) {
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

        if (switchGamesList.isEmpty()) {
            if (gameSeries.contains("Zelda", ignoreCase = true) || amiiboSeries.contains("Zelda", ignoreCase = true)) {
                switchGamesList.add("• The Legend of Zelda: Tears of the Kingdom: Эксклюзивная ткань параплана, оружие, ресурсы")
                switchGamesList.add("• The Legend of Zelda: Breath of the Wild: Доспехи, оружие, призыв Эпоны / Волка Линка, сундуки")
                switchGamesList.add("• The Legend of Zelda: Echoes of Wisdom: Уникальные костюмы, аксессуары и ресурсы")
                switchGamesList.add("• Super Smash Bros. Ultimate: Обучаемый боец FP (Figure Player)")
                switchGamesList.add("• Mario Kart 8 Deluxe: Гоночный костюм Mii")
            } else if (gameSeries.contains("Mario", ignoreCase = true) || amiiboSeries.contains("Mario", ignoreCase = true)) {
                switchGamesList.add("• Super Mario Odyssey: Уникальные костюмы для Марио и подсказки Лун энергии")
                switchGamesList.add("• Super Mario 3D World + Bowser's Fury: Костюм Белого Тануки Неуязвимости, бонусы")
                switchGamesList.add("• Super Smash Bros. Ultimate: Боец FP с прокачкой 1-50 ур.")
                switchGamesList.add("• Mario Kart 8 Deluxe: Специальный гоночный костюм Mii")
            } else if (gameSeries.contains("Splatoon", ignoreCase = true) || amiiboSeries.contains("Splatoon", ignoreCase = true)) {
                switchGamesList.add("• Splatoon 3 / 2: Эксклюзивные наборы экипировки и совместные фотосессии")
            } else if (gameSeries.contains("Metroid", ignoreCase = true) || amiiboSeries.contains("Metroid", ignoreCase = true)) {
                switchGamesList.add("• Metroid Dread: Дополнительный контейнер энергии (Energy Tank) и ракеты")
            } else if (gameSeries.contains("Animal Crossing", ignoreCase = true) || amiiboSeries.contains("Animal Crossing", ignoreCase = true)) {
                switchGamesList.add("• Animal Crossing: New Horizons: Плакаты, визит жителя на кемпинг, фотостудия")
            } else if (gameSeries.contains("Monster Hunter", ignoreCase = true) || amiiboSeries.contains("Monster Hunter", ignoreCase = true)) {
                switchGamesList.add("• Monster Hunter Rise: Многослойная броня и ежедневная лотерея Кагари")
            } else if (gameSeries.contains("Xenoblade", ignoreCase = true) || amiiboSeries.contains("Xenoblade", ignoreCase = true)) {
                switchGamesList.add("• Xenoblade Chronicles 3: Облик Меча Монадо и ресурсы")
            } else {
                switchGamesList.add("• Super Smash Bros. Ultimate: Обучаемый боец FP или бонусы")
                switchGamesList.add("• Mario Kart 8 Deluxe: Гоночный костюм Mii")
                switchGamesList.add("• Универсальная поддержка: Совместимо со всеми Switch играми с поддержкой Amiibo")
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

    fun getAmiibosForGame(allAmiibos: List<AmiiboEntry>, titleId: String, gameTitle: String): List<AmiiboEntry> {
        val gameLower = gameTitle.lowercase()
        return allAmiibos.filter { a ->
            when {
                gameLower.contains("zelda") -> a.gameSeries.contains("zelda", ignoreCase = true) || a.amiiboSeries.contains("zelda", ignoreCase = true)
                gameLower.contains("mario") -> a.gameSeries.contains("mario", ignoreCase = true) || a.amiiboSeries.contains("mario", ignoreCase = true)
                gameLower.contains("smash") || gameLower.contains("ssbu") -> true
                gameLower.contains("splatoon") -> a.gameSeries.contains("splatoon", ignoreCase = true) || a.amiiboSeries.contains("splatoon", ignoreCase = true)
                gameLower.contains("metroid") -> a.gameSeries.contains("metroid", ignoreCase = true) || a.amiiboSeries.contains("metroid", ignoreCase = true)
                gameLower.contains("pokemon") -> a.gameSeries.contains("pokemon", ignoreCase = true) || a.amiiboSeries.contains("pokemon", ignoreCase = true)
                gameLower.contains("kirby") -> a.gameSeries.contains("kirby", ignoreCase = true) || a.amiiboSeries.contains("kirby", ignoreCase = true)
                gameLower.contains("fire emblem") -> a.gameSeries.contains("fire emblem", ignoreCase = true) || a.amiiboSeries.contains("fire emblem", ignoreCase = true)
                gameLower.contains("xenoblade") -> a.gameSeries.contains("xenoblade", ignoreCase = true) || a.amiiboSeries.contains("xenoblade", ignoreCase = true)
                gameLower.contains("monster hunter") -> a.gameSeries.contains("monster hunter", ignoreCase = true) || a.amiiboSeries.contains("monster hunter", ignoreCase = true)
                gameLower.contains("animal crossing") -> a.gameSeries.contains("animal crossing", ignoreCase = true) || a.amiiboSeries.contains("animal crossing", ignoreCase = true)
                else -> a.amiiboSeries.contains("Smash", ignoreCase = true) || a.amiiboSeries.contains("Zelda", ignoreCase = true) || a.amiiboSeries.contains("Mario", ignoreCase = true)
            }
        }
    }

    fun isAmiiboSaved(entry: AmiiboEntry): Boolean {
        val userDir = File(DirectoryInitialization.userDirectory, "amiibo")
        val safeName = entry.name.replace(Regex("[^a-zA-Z0-9._ -]"), "_").trim()
        val fileName = if (safeName.isNotEmpty()) "$safeName.bin" else "Amiibo_${entry.fullId}.bin"
        return File(userDir, fileName).exists()
    }

    fun loadAmiiboDirectly(entry: AmiiboEntry): Boolean {
        val bytes = generateAmiiboBin(entry)
        val result = NativeLibrary.loadAmiibo(bytes)
        return result == 0
    }

    val imageMemoryCache = androidx.collection.LruCache<String, android.graphics.Bitmap>(300)
    private val fastImageClient = OkHttpClient.Builder()
        .connectTimeout(2, java.util.concurrent.TimeUnit.SECONDS)
        .readTimeout(3, java.util.concurrent.TimeUnit.SECONDS)
        .build()

    suspend fun getAmiiboImage(url: String): android.graphics.Bitmap? = withContext(Dispatchers.IO) {
        if (url.isEmpty()) return@withContext null

        imageMemoryCache.get(url)?.let { return@withContext it }

        val cacheDir = File(YuzuApplication.appContext.cacheDir, "amiibo_images")
        if (!cacheDir.exists()) cacheDir.mkdirs()
        val filename = url.substringAfterLast("/", "img.png").ifEmpty { "img.png" }
        val localFile = File(cacheDir, filename)

        if (localFile.exists() && localFile.length() > 0) {
            try {
                val bmp = android.graphics.BitmapFactory.decodeFile(localFile.absolutePath)
                if (bmp != null) {
                    imageMemoryCache.put(url, bmp)
                    return@withContext bmp
                }
            } catch (_: Exception) {}
        }

        val mirrorUrls = listOf(
            url,
            url.replace("cdn.jsdelivr.net/gh/N3evin/AmiiboAPI@master", "fastly.jsdelivr.net/gh/N3evin/AmiiboAPI@master"),
            url.replace("cdn.jsdelivr.net/gh/N3evin/AmiiboAPI@master", "raw.githubusercontent.com/N3evin/AmiiboAPI/master"),
            url.replace("cdn.jsdelivr.net/gh/N3evin/AmiiboAPI@master", "ghproxy.net/https://raw.githubusercontent.com/N3evin/AmiiboAPI/master"),
            url.replace("raw.githubusercontent.com/N3evin/AmiiboAPI/master", "fastly.jsdelivr.net/gh/N3evin/AmiiboAPI@master")
        ).distinct()

        for (mirror in mirrorUrls) {
            try {
                val req = Request.Builder()
                    .url(mirror)
                    .header("User-Agent", USER_AGENT)
                    .build()
                val resp = fastImageClient.newCall(req).execute()
                if (resp.isSuccessful) {
                    val bytes = resp.body?.bytes()
                    if (bytes != null && bytes.isNotEmpty()) {
                        try { localFile.writeBytes(bytes) } catch (_: Exception) {}
                        val bmp = android.graphics.BitmapFactory.decodeByteArray(bytes, 0, bytes.size)
                        if (bmp != null) {
                            imageMemoryCache.put(url, bmp)
                            return@withContext bmp
                        }
                    }
                }
            } catch (_: Exception) {}
        }
        null
    }
}
