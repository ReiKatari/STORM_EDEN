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
        "Mozilla/5.0 (Linux; Android 14; Mobile) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Mobile Safari/537.36 STORM-EDEN/3.2.9"

    private val httpClient: OkHttpClient by lazy {
        OkHttpClient.Builder()
            .followRedirects(true)
            .followSslRedirects(true)
            .connectTimeout(25, TimeUnit.SECONDS)
            .readTimeout(35, TimeUnit.SECONDS)
            .writeTimeout(35, TimeUnit.SECONDS)
            .build()
    }

    fun cleanTitle(rawTitle: String): String {
        var clean = rawTitle
        // Remove file extensions (.nsp, .nsz, .xci, .xcz, .zip, .rar, etc.)
        clean = clean.replace(Regex("\\.(nsp|nsz|xci|xcz|zip|rar|7z)$", RegexOption.IGNORE_CASE), "")
        // Remove bracketed and parenthesized tags: [0100...], [v0], (USA), (v1.3.0), {DLC}
        clean = clean.replace(Regex("\\[.*?\\]"), " ")
        clean = clean.replace(Regex("\\(.*?\\)"), " ")
        clean = clean.replace(Regex("\\{.*?\\}"), " ")
        // Remove symbol noise
        clean = clean.replace("™", "")
        clean = clean.replace("®", "")
        clean = clean.replace(":", " ")
        clean = clean.replace("_", " ")
        clean = clean.replace("-", " ")
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

        return GameBananaMod(
            id = id,
            name = name,
            submitter = submitter,
            category = category,
            downloads = downloads,
            likes = likes,
            views = views,
            date = ""
        )
    }

    private fun fetchModsFromSearchEndpoint(searchTerm: String): List<GameBananaMod> {
        val mods = mutableListOf<GameBananaMod>()
        if (searchTerm.isBlank()) return mods

        try {
            val encodedTerm = URLEncoder.encode(searchTerm, "UTF-8")
            val url =
                "https://gamebanana.com/apiv11/Util/Search/Results?_sSearchString=$encodedTerm&_sModelName=Mod&_nPage=1&_nPerpage=40"

            val jsonText = fetchJson(url) ?: return mods
            val root = JSONObject(jsonText)
            val records = root.optJSONArray("_aRecords") ?: JSONArray()

            for (i in 0 until records.length()) {
                val mod = parseModObject(records.getJSONObject(i))
                if (mod != null) {
                    mods.add(mod)
                }
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
        return mods
    }

    private fun fetchModsFromIndexEndpoint(searchTerm: String): List<GameBananaMod> {
        val mods = mutableListOf<GameBananaMod>()
        if (searchTerm.isBlank()) return mods

        try {
            val encodedTerm = URLEncoder.encode(searchTerm, "UTF-8")
            val url =
                "https://gamebanana.com/apiv11/Mod/Index?_sSearchString=$encodedTerm&_nPage=1&_nPerpage=40"

            val jsonText = fetchJson(url) ?: return mods
            val root = JSONObject(jsonText)
            val records = root.optJSONArray("_aRecords") ?: JSONArray()

            for (i in 0 until records.length()) {
                val mod = parseModObject(records.getJSONObject(i))
                if (mod != null) {
                    mods.add(mod)
                }
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
        return mods
    }

    private fun fetchGameIdAndMods(searchTerm: String): List<GameBananaMod> {
        val mods = mutableListOf<GameBananaMod>()
        if (searchTerm.isBlank()) return mods

        try {
            val encodedTerm = URLEncoder.encode(searchTerm, "UTF-8")
            val gameSearchUrl =
                "https://gamebanana.com/apiv11/Game/Index?_sSearchString=$encodedTerm&_nPage=1&_nPerpage=5"

            val gameJson = fetchJson(gameSearchUrl) ?: return mods
            val root = JSONObject(gameJson)
            val records = root.optJSONArray("_aRecords") ?: JSONArray()

            for (i in 0 until records.length()) {
                val gameObj = records.getJSONObject(i)
                val gameId = gameObj.optInt("_idRow", 0)
                if (gameId > 0) {
                    val gameModsUrl =
                        "https://gamebanana.com/apiv11/Mod/Index?_aFilters[Generic_Game]=$gameId&_nPage=1&_nPerpage=40"
                    val modsJson = fetchJson(gameModsUrl)
                    if (modsJson != null) {
                        val modsRoot = JSONObject(modsJson)
                        val modRecords = modsRoot.optJSONArray("_aRecords") ?: JSONArray()
                        for (j in 0 until modRecords.length()) {
                            val mod = parseModObject(modRecords.getJSONObject(j))
                            if (mod != null) {
                                mods.add(mod)
                            }
                        }
                    }
                    if (mods.isNotEmpty()) break
                }
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
        return mods
    }

    suspend fun searchMods(game: Game, query: String = ""): List<GameBananaMod> = withContext(Dispatchers.IO) {
        val baseTitle = cleanTitle(game.title)
        val queriesToTry = mutableListOf<String>()

        if (query.isNotBlank()) {
            queriesToTry.add(query.trim())
            if (baseTitle.isNotBlank()) {
                queriesToTry.add("$baseTitle ${query.trim()}")
            }
        } else {
            if (baseTitle.isNotBlank()) {
                queriesToTry.add(baseTitle)
                val words = baseTitle.split(" ").filter { it.isNotBlank() }
                if (words.size > 2) {
                    val withoutArticles = words.filter {
                        it.lowercase() !in listOf("the", "of", "a", "an", "and", "edition", "deluxe", "switch", "nintendo")
                    }
                    if (withoutArticles.isNotEmpty()) {
                        queriesToTry.add(withoutArticles.take(3).joinToString(" "))
                    }
                }
            }
        }

        val allResults = mutableListOf<GameBananaMod>()
        val seenIds = mutableSetOf<Int>()

        for (term in queriesToTry.distinct()) {
            // Method 1: Util/Search/Results
            val results1 = fetchModsFromSearchEndpoint(term)
            for (mod in results1) {
                if (seenIds.add(mod.id)) {
                    allResults.add(mod)
                }
            }

            // Method 2: Mod/Index
            if (allResults.isEmpty()) {
                val results2 = fetchModsFromIndexEndpoint(term)
                for (mod in results2) {
                    if (seenIds.add(mod.id)) {
                        allResults.add(mod)
                    }
                }
            }

            // Method 3: Game ID filter
            if (allResults.isEmpty()) {
                val results3 = fetchGameIdAndMods(term)
                for (mod in results3) {
                    if (seenIds.add(mod.id)) {
                        allResults.add(mod)
                    }
                }
            }

            if (allResults.isNotEmpty() && query.isBlank()) {
                break
            }
        }
        allResults
    }

    suspend fun getModFiles(modId: Int): List<GameBananaFile> = withContext(Dispatchers.IO) {
        val files = mutableListOf<GameBananaFile>()
        val urlString = "https://gamebanana.com/apiv11/Mod/$modId/ProfilePage"

        try {
            val jsonText = fetchJson(urlString) ?: return@withContext files
            val root = JSONObject(jsonText)
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
        files
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

            val isZip = file.filename.endsWith(".zip", ignoreCase = true) || file.filename.endsWith(".7z", ignoreCase = true) || file.filename.endsWith(".rar", ignoreCase = true)
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
                } catch (_: Exception) {
                    // leave as is if unzip failed
                }
            }
            true
        } catch (e: Exception) {
            e.printStackTrace()
            false
        }
    }
}
