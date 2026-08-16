// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.utils

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject
import org.yuzu.yuzu_emu.model.Game
import java.io.File
import java.io.FileOutputStream
import java.net.HttpURLConnection
import java.net.URL
import java.net.URLEncoder

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
        "Mozilla/5.0 (Linux; Android 14; Mobile) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Mobile Safari/537.36 STORM-EDEN/3.2.7"

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

    private fun openConnectionWithRedirects(initialUrl: String, maxRedirects: Int = 6): HttpURLConnection {
        var currentUrl = initialUrl
        var redirects = 0
        while (redirects < maxRedirects) {
            val url = URL(currentUrl)
            val conn = url.openConnection() as HttpURLConnection
            conn.setRequestProperty("User-Agent", USER_AGENT)
            conn.setRequestProperty("Accept", "*/*")
            conn.connectTimeout = 30000
            conn.readTimeout = 30000
            conn.instanceFollowRedirects = true
            val status = conn.responseCode
            if (status in listOf(
                    HttpURLConnection.HTTP_MOVED_PERM,
                    HttpURLConnection.HTTP_MOVED_TEMP,
                    HttpURLConnection.HTTP_SEE_OTHER,
                    307,
                    308
                )
            ) {
                val newLocation = conn.getHeaderField("Location")
                conn.disconnect()
                if (!newLocation.isNullOrEmpty()) {
                    currentUrl = if (newLocation.startsWith("http://") || newLocation.startsWith("https://")) {
                        newLocation
                    } else {
                        URL(url, newLocation).toString()
                    }
                    redirects++
                    continue
                }
            }
            return conn
        }
        return URL(currentUrl).openConnection() as HttpURLConnection
    }

    private fun fetchModsFromGameBanana(searchTerm: String): List<GameBananaMod> {
        val mods = mutableListOf<GameBananaMod>()
        if (searchTerm.isBlank()) return mods

        try {
            val encodedTerm = URLEncoder.encode(searchTerm, "UTF-8")
            val urlString =
                "https://gamebanana.com/apiv11/Util/Search/Results?_sSearchString=$encodedTerm&_sModelName=Mod&_nPage=1&_nPerpage=40"

            val conn = openConnectionWithRedirects(urlString)
            conn.requestMethod = "GET"

            if (conn.responseCode == 200) {
                val jsonText = conn.inputStream.bufferedReader().use { it.readText() }
                val root = JSONObject(jsonText)
                val records = root.optJSONArray("_aRecords") ?: JSONArray()

                for (i in 0 until records.length()) {
                    val obj = records.getJSONObject(i)
                    val id = obj.optInt("_idRow", 0)
                    val name = obj.optString("_sName", "")
                    val submitterObj = obj.optJSONObject("_aSubmitter")
                    val submitter = submitterObj?.optString("_sName", "") ?: ""
                    val rootCatObj = obj.optJSONObject("_aRootCategory")
                    val category = rootCatObj?.optString("_sName", "") ?: ""
                    val downloads = obj.optInt("_nDownloadCount", 0)
                    val likes = obj.optInt("_nLikeCount", 0)
                    val views = obj.optInt("_nViewCount", 0)

                    if (id > 0 && name.isNotEmpty()) {
                        mods.add(
                            GameBananaMod(
                                id = id,
                                name = name,
                                submitter = submitter,
                                category = category,
                                downloads = downloads,
                                likes = likes,
                                views = views,
                                date = ""
                            )
                        )
                    }
                }
            }
            conn.disconnect()
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
                if (words.size > 3) {
                    val withoutArticles = words.filter {
                        it.lowercase() !in listOf("the", "of", "a", "an", "and", "edition", "deluxe")
                    }
                    if (withoutArticles.isNotEmpty()) {
                        queriesToTry.add(withoutArticles.take(4).joinToString(" "))
                    }
                }
            }
        }

        val allResults = mutableListOf<GameBananaMod>()
        val seenIds = mutableSetOf<Int>()

        for (term in queriesToTry.distinct()) {
            val results = fetchModsFromGameBanana(term)
            for (mod in results) {
                if (seenIds.add(mod.id)) {
                    allResults.add(mod)
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
            val conn = openConnectionWithRedirects(urlString)
            conn.requestMethod = "GET"

            if (conn.responseCode == 200) {
                val jsonText = conn.inputStream.bufferedReader().use { it.readText() }
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
            }
            conn.disconnect()
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
            val conn = openConnectionWithRedirects(file.downloadUrl)

            val cleanModName = modName.replace(Regex("[^a-zA-Z0-9._ -]"), "_").trim()
            val targetDir = File(game.addonDir, if (cleanModName.isEmpty()) "GameBananaMod" else cleanModName)
            targetDir.mkdirs()

            val tempFile = File(targetDir, "download_temp.zip")
            val totalSize = if (file.filesize > 0) file.filesize else conn.contentLengthLong

            conn.inputStream.use { input ->
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
            conn.disconnect()

            // If it's a zip archive, extract it directly into mod directory
            try {
                FileUtil.unzipToInternalStorage(tempFile.absolutePath, targetDir)
                tempFile.delete()
            } catch (_: Exception) {
                // leave as is if not zip
            }
            true
        } catch (e: Exception) {
            e.printStackTrace()
            false
        }
    }
}
