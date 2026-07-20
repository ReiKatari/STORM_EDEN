// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.utils

import android.content.Context
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request
import java.io.File
import java.io.IOException

object TitleDbManager {
    private const val TITLEDB_URL = "https://tinfoil.media/repo/db/titles.json"
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
            isLoaded = file.exists() && file.length() > 0L
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
                    val body = response.body
                    if (body != null) {
                        val file = getTitleDbFile()
                        body.byteStream().use { input ->
                            file.outputStream().use { output ->
                                input.copyTo(output)
                            }
                        }
                    }
                }
            }
        } catch (e: IOException) {
            e.printStackTrace()
        }
    }

    fun getDlcName(titleIdHex: String): String? {
        val file = getTitleDbFile()
        if (!file.exists() || file.length() == 0L) {
            return null
        }

        val targetKeyUpper = titleIdHex.uppercase()
        val targetKeyLower = titleIdHex.lowercase()

        try {
            file.inputStream().bufferedReader().use { reader ->
                val jsonReader = android.util.JsonReader(reader)
                jsonReader.beginObject()
                while (jsonReader.hasNext()) {
                    val key = jsonReader.nextName()
                    if (key == targetKeyUpper || key == targetKeyLower) {
                        jsonReader.beginObject()
                        var name: String? = null
                        while (jsonReader.hasNext()) {
                            val propName = jsonReader.nextName()
                            if (propName == "name") {
                                name = jsonReader.nextString()
                            } else {
                                jsonReader.skipValue()
                            }
                        }
                        jsonReader.endObject()
                        return name
                    } else {
                        jsonReader.skipValue()
                    }
                }
                jsonReader.endObject()
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
        return null
    }
}
