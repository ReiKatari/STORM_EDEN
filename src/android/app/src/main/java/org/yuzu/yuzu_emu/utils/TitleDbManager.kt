// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.utils

import android.content.Context
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request
import org.json.JSONObject
import java.io.File
import java.io.IOException

object TitleDbManager {
    private const val TITLEDB_URL = "https://tinfoil.media/repo/db/titles.json"
    private var jsonObject: JSONObject? = null
    private var isLoaded = false

    fun getTitleDbFile(): File {
        val dir = File(DirectoryInitialization.userDirectory + "/.switch")
        if (!dir.exists()) {
            dir.mkdirs()
        }
        return File(dir, "titledb.json")
    }

    suspend fun init(context: Context) {
        withContext(Dispatchers.IO) {
            val file = getTitleDbFile()
            // Check if we need to download/update
            val needsDownload = !file.exists() || file.length() == 0L ||
                    (System.currentTimeMillis() - file.lastModified() > 86400000L) // 24 hours

            if (needsDownload) {
                downloadTitleDb()
            }

            loadFromCache()
        }
    }

    private fun downloadTitleDb() {
        val client = OkHttpClient()
        val request = Request.Builder()
            .url(TITLEDB_URL)
            .build()
        try {
            client.newCall(request).execute().use { response ->
                if (response.isSuccessful) {
                    val body = response.body?.string()
                    if (!body.isNullOrEmpty()) {
                        val file = getTitleDbFile()
                        file.writeText(body)
                    }
                }
            }
        } catch (e: IOException) {
            e.printStackTrace()
        }
    }

    private fun loadFromCache() {
        val file = getTitleDbFile()
        if (file.exists() && file.length() > 0) {
            try {
                val content = file.readText()
                jsonObject = JSONObject(content)
                isLoaded = true
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }
    }

    fun getDlcName(titleIdHex: String): String? {
        if (!isLoaded || jsonObject == null) {
            // Lazy load if not loaded yet
            try {
                val file = getTitleDbFile()
                if (file.exists() && file.length() > 0) {
                    val content = file.readText()
                    jsonObject = JSONObject(content)
                    isLoaded = true
                }
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }

        val obj = jsonObject ?: return null
        val upperKey = titleIdHex.uppercase()
        val lowerKey = titleIdHex.lowercase()
        val item = obj.optJSONObject(upperKey) ?: obj.optJSONObject(lowerKey)
        return item?.optString("name")
    }
}
