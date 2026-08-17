// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.utils

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request
import org.json.JSONArray
import org.json.JSONObject
import org.yuzu.yuzu_emu.model.Game
import java.io.File
import java.io.FileOutputStream
import java.net.URLEncoder
import java.util.concurrent.TimeUnit

data class GameBananaMod(
    val id: Int,
    val name: String,
    val submitter: String,
    val category: String,
    val downloads: Int,
    val likes: Int,
    val views: Int,
    val date: String,
    val description: String = ""
)

data class GameBananaFile(
    val id: Int,
    val filename: String,
    val downloadUrl: String,
    val filesize: Long,
    val description: String
)

object GameBananaHelper {
    private const val USER_AGENT =
        "Mozilla/5.0 (Linux; Android 14; Mobile) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Mobile Safari/537.36 STORM-EDEN/4.0.0"

    private val httpClient: OkHttpClient by lazy {
        OkHttpClient.Builder()
            .followRedirects(true)
            .followSslRedirects(true)
            .connectTimeout(25, TimeUnit.SECONDS)
            .readTimeout(35, TimeUnit.SECONDS)
            .writeTimeout(35, TimeUnit.SECONDS)
            .build()
    }

    fun resolveGameBananaGameId(titleIdLong: Long, gameName: String): Int {
        when (titleIdLong) {
            0x01007EF00011E000L -> return 5866   // The Legend of Zelda: Breath of the Wild (Switch)
            0x0100F2C0115B6000L -> return 17654  // The Legend of Zelda: Tears of the Kingdom
            0x01006A800016E000L -> return 6686   // Super Smash Bros. Ultimate
            0x0100000000010000L -> return 6150   // Super Mario Odyssey
            0x0100152000022000L -> return 6507   // Mario Kart 8 Deluxe
            0x01006F8002326000L -> return 8282   // Animal Crossing: New Horizons
            0x0100A3D008C5C000L -> return 16871  // Pokemon Scarlet
            0x01008F6008C5E000L -> return 16871  // Pokemon Violet
            0x0100ABF008968000L -> return 7616   // Pokemon Sword
            0x01008DB008C2C000L -> return 7616   // Pokemon Shield
            0x01001F5010DFA000L -> return 12845  // Pokemon Legends: Arceus
            0x0100000011D90000L -> return 18365  // Pokemon Legends: Z-A
            0x0100000000014000L -> return 7018   // Pokemon: Let's Go, Pikachu!
            0x0100000000015000L -> return 7018   // Pokemon: Let's Go, Eevee!
            0x0100000011DE8000L -> return 12844  // Pokemon Brilliant Diamond / Shining Pearl
            0x01002DA013484000L -> return 11394  // Metroid Dread
            0x0100121014688000L -> return 14457  // Metroid Prime Remastered
            0x01000A10041EA000L -> return 16777  // Persona 5 Royal (Switch)
            0x010074F013262000L -> return 16781  // Xenoblade Chronicles 3
            0x0100E95004038000L -> return 6927   // Xenoblade Chronicles 2
            0x0100FF500E34A000L -> return 8449   // Xenoblade Chronicles: Definitive Edition
            0x0100C2500FC20000L -> return 15797  // Splatoon 3
            0x01003BC0000A0000L -> return 5956   // Splatoon 2
            0x0100C9A00ECE6000L -> return 17462  // Fire Emblem Engage
            0x010055D009F78000L -> return 7460   // Fire Emblem: Three Houses
            0x01004A4010FE8000L -> return 17088  // Bayonetta 3
            0x01004D300C5AE000L -> return 15206  // Kirby and the Forgotten Land
            0x010003200D166000L -> return 17497  // Sonic Frontiers (Switch)
            0x010028600EBDA000L -> return 17655  // Super Mario Bros. Wonder
            0x01009B90006DC000L -> return 6848   // Super Mario Party
            0x0100D8701267E000L -> return 12797  // Mario Party Superstars
            0x01002B4019BCA000L -> return 18980  // Super Mario Party Jamboree
            0x01009AA000FAA000L -> return 6853   // Luigi's Mansion 3
            0x0100B04011742000L -> return 12836  // Super Mario 3D World + Bowser's Fury
            0x01004BC0000AA000L -> return 6554   // Super Mario Maker 2
            0x01004A5013054000L -> return 18600  // The Legend of Zelda: Echoes of Wisdom
            0x01006BB00C6F0000L -> return 7433   // The Legend of Zelda: Link's Awakening
            0x010065800002A000L -> return 6168   // The Legend of Zelda: Skyward Sword HD
        }

        val lower = gameName.lowercase()
        if (lower.contains("breath of the wild") || lower.contains("botw")) return 5866
        if (lower.contains("tears of the kingdom") || lower.contains("totk")) return 17654
        if (lower.contains("smash bros") || lower.contains("smash ultimate")) return 6686
        if (lower.contains("mario odyssey")) return 6150
        if (lower.contains("mario kart 8")) return 6507
        if (lower.contains("animal crossing")) return 8282
        if (lower.contains("metroid dread")) return 11394
        if (lower.contains("metroid prime")) return 14457
        if (lower.contains("splatoon 3")) return 15797
        if (lower.contains("splatoon 2")) return 5956
        if (lower.contains("persona 5")) return 16777
        if (lower.contains("xenoblade 3")) return 16781
        if (lower.contains("xenoblade 2")) return 6927
        if (lower.contains("sonic frontiers")) return 17497
        if (lower.contains("mario wonder")) return 17655
        if (lower.contains("echoes of wisdom")) return 18600
        if (lower.contains("jamboree")) return 18980

        return 0
    }

    fun cleanTitle(rawTitle: String): String {
        var clean = rawTitle
        clean = clean.replace(Regex("\\.(nsp|nsz|xci|xcz|zip|rar|7z)$", RegexOption.IGNORE_CASE), "")
        clean = clean.replace(Regex("\\[.*?\\]"), " ")
        clean = clean.replace(Regex("\\(.*?\\)"), " ")
        clean = clean.replace(Regex("\\{.*?\\}"), " ")
        clean = clean.replace("™", "").replace("®", "").replace(":", " ").replace("_", " ").replace("-", " ")
        return clean.replace(Regex("\\s+"), " ").trim()
    }

    private fun fetchJson(url: String): String? {
        return try {
            val request = Request.Builder()
                .url(url)
                .header("User-Agent", USER_AGENT)
                .header("Accept", "application/json, text/plain, */*")
                .build()

            val response = httpClient.newCall(request).execute()
            if (response.isSuccessful) {
                response.body?.string()
            } else {
                null
            }
        } catch (e: Exception) {
            e.printStackTrace()
            null
        }
    }

    private fun parseModObject(obj: JSONObject): GameBananaMod? {
        val id = obj.optInt("_idRow", 0)
        val name = obj.optString("_sName", "").ifEmpty { obj.optString("name", "") }
        if (id <= 0 || name.isEmpty()) return null

        val submitterObj = obj.optJSONObject("_aSubmitter")
        val submitter = submitterObj?.optString("_sName", "")
            ?: obj.optString("submitter", "")

        val rootCatObj = obj.optJSONObject("_aRootCategory")
        val category = rootCatObj?.optString("_sName", "")
            ?: obj.optString("category", "")

        val downloads = obj.optInt("_nDownloadCount", 0)
        val likes = obj.optInt("_nLikeCount", 0)
        val views = obj.optInt("_nViewCount", 0)
        val description = obj.optString("_sDescription", "").ifEmpty { obj.optString("_sText", "") }

        return GameBananaMod(
            id = id,
            name = name,
            submitter = submitter,
            category = category,
            downloads = downloads,
            likes = likes,
            views = views,
            date = "",
            description = description
        )
    }

    suspend fun searchMods(
        game: Game,
        query: String = "",
        page: Int = 1,
        sortIndex: Int = 0
    ): List<GameBananaMod> = withContext(Dispatchers.IO) {
        val programIdLong = try {
            game.programId.toLong()
        } catch (_: Exception) {
            0L
        }

        val gameBananaGameId = resolveGameBananaGameId(programIdLong, game.title)
        val baseTitle = cleanTitle(game.title)
        val trimmedQuery = query.trim()

        val sortParam = when (sortIndex) {
            0 -> "Generic_MostDownloaded"
            1 -> "Generic_MostLiked"
            2 -> "Generic_MostViewed"
            3 -> "Generic_Latest"
            4 -> "Generic_Alphabetical"
            else -> "Generic_MostDownloaded"
        }

        val results = mutableListOf<GameBananaMod>()

        if (gameBananaGameId > 0 && trimmedQuery.isEmpty()) {
            val url = "https://gamebanana.com/apiv11/Mod/Index?_aFilters[Generic_Game]=$gameBananaGameId&_sSort=$sortParam&_nPage=$page&_nPerpage=40"
            val json = fetchJson(url)
            if (json != null) {
                try {
                    val root = JSONObject(json)
                    val records = root.optJSONArray("_aRecords") ?: JSONArray()
                    for (i in 0 until records.length()) {
                        val mod = parseModObject(records.getJSONObject(i))
                        if (mod != null) results.add(mod)
                    }
                } catch (e: Exception) {
                    e.printStackTrace()
                }
            }
        }

        if (results.isEmpty()) {
            val searchTerm = if (trimmedQuery.isNotEmpty()) {
                if (gameBananaGameId > 0) trimmedQuery else "$baseTitle $trimmedQuery Switch"
            } else {
                "$baseTitle Switch"
            }

            val encodedTerm = URLEncoder.encode(searchTerm, "UTF-8")
            val url = if (gameBananaGameId > 0 && trimmedQuery.isNotEmpty()) {
                "https://gamebanana.com/apiv11/Util/Search/Results?_sSearchString=$encodedTerm&_idGameRow=$gameBananaGameId&_sModelName=Mod&_nPage=$page&_nPerpage=40"
            } else {
                "https://gamebanana.com/apiv11/Util/Search/Results?_sSearchString=$encodedTerm&_sModelName=Mod&_nPage=$page&_nPerpage=40"
            }

            val json = fetchJson(url)
            if (json != null) {
                try {
                    val root = JSONObject(json)
                    val records = root.optJSONArray("_aRecords") ?: JSONArray()
                    for (i in 0 until records.length()) {
                        val mod = parseModObject(records.getJSONObject(i))
                        if (mod != null) results.add(mod)
                    }
                } catch (e: Exception) {
                    e.printStackTrace()
                }
            }
        }

        // Apply local sort if from search endpoint
        when (sortIndex) {
            0 -> results.sortByDescending { it.downloads }
            1 -> results.sortByDescending { it.likes }
            2 -> results.sortByDescending { it.views }
            3 -> results.sortByDescending { it.id }
            4 -> results.sortBy { it.name.lowercase() }
        }

        results
    }

    suspend fun getModFiles(modId: Int): Pair<String, List<GameBananaFile>> = withContext(Dispatchers.IO) {
        val files = mutableListOf<GameBananaFile>()
        var description = ""
        val urlString = "https://gamebanana.com/apiv11/Mod/$modId/ProfilePage"

        try {
            val jsonText = fetchJson(urlString) ?: return@withContext Pair(description, files)
            val root = JSONObject(jsonText)
            description = root.optString("_sDescription", "").ifEmpty { root.optString("_sText", "") }
            val filesArray = root.optJSONArray("_aFiles") ?: JSONArray()

            for (i in 0 until filesArray.length()) {
                val f = filesArray.getJSONObject(i)
                val fileId = f.optInt("_idRow", 0)
                val filename = f.optString("_sFile", "")
                val downloadUrl = f.optString("_sDownloadUrl", "")
                val filesize = f.optLong("_nFilesize", 0L)
                val desc = f.optString("_sDescription", "")

                if (downloadUrl.isNotEmpty()) {
                    files.add(
                        GameBananaFile(
                            id = fileId,
                            filename = filename,
                            downloadUrl = downloadUrl,
                            filesize = filesize,
                            description = desc
                        )
                    )
                }
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
        Pair(description, files)
    }

    suspend fun downloadAndInstallMod(
        game: Game,
        modName: String,
        file: GameBananaFile,
        onProgress: (Int) -> Unit
    ): Boolean = withContext(Dispatchers.IO) {
        try {
            val request = Request.Builder()
                .url(file.downloadUrl)
                .header("User-Agent", USER_AGENT)
                .build()

            val response = httpClient.newCall(request).execute()
            if (!response.isSuccessful || response.body == null) {
                return@withContext false
            }

            val cleanModName = modName.replace(Regex("[^a-zA-Z0-9._ -]"), "_").trim()
            val targetDir = File(game.addonDir, if (cleanModName.isEmpty()) "GameBananaMod" else cleanModName)
            targetDir.mkdirs()

            val isZip = file.filename.endsWith(".zip", ignoreCase = true) ||
                file.filename.endsWith(".7z", ignoreCase = true) ||
                file.filename.endsWith(".rar", ignoreCase = true)

            val tempFile = File(targetDir, if (isZip) "download_temp.zip" else file.filename)
            val totalSize = if (file.filesize > 0) file.filesize else (response.body?.contentLength() ?: 0L)

            response.body!!.byteStream().use { input ->
                FileOutputStream(tempFile).use { output ->
                    val buffer = ByteArray(8192)
                    var bytesRead: Int
                    var totalRead = 0L
                    while (input.read(buffer).also { bytesRead = it } != -1) {
                        output.write(buffer, 0, bytesRead)
                        totalRead += bytesRead
                        if (totalSize > 0) {
                            val progress = ((totalRead * 100) / totalSize).toInt()
                            onProgress(progress.coerceIn(0, 100))
                        }
                    }
                }
            }

            if (isZip) {
                try {
                    FileUtil.unzipToInternalStorage(tempFile.absolutePath, targetDir)
                    tempFile.delete()
                } catch (_: Exception) {}
            }
            true
        } catch (e: Exception) {
            e.printStackTrace()
            false
        }
    }
}
