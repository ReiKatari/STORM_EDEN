// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.utils
import org.yuzu.yuzu_emu.NativeLibrary

import android.os.Build

object Log {
    // Tracks whether we should share the old log or the current log
    var gameLaunched = false

    external fun debug(message: String)

    external fun warning(message: String)

    external fun info(message: String)

    external fun error(message: String)

    external fun critical(message: String)

    fun logDeviceInfo() {
        info("Device Manufacturer - ${Build.MANUFACTURER}")
        info("Device Model - ${Build.MODEL}")
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.R) {
            info("SoC Manufacturer - ${Build.SOC_MANUFACTURER}")
            info("SoC Model - ${Build.SOC_MODEL}")
            NativeLibrary.getCpuSummary().split('\n').forEach {
                info("CPU Info - $it")
            }
        }
        NativeLibrary.getVulkanDriverVersion().split('\n').forEach {
            info("Vulkan Driver: - $it")
        }
        NativeLibrary.getVulkanApiVersion().split('\n').forEach {
            info("Vulkan API Version: - $it")
        }
        info("Total System Memory - ${MemoryUtil.getDeviceRAM()}")
    }

    fun exportLogToDownloads(context: android.content.Context?) {
        try {
            val ctx = context ?: return
            val userDir = DirectoryInitialization.userDirectory
            val candidateLogFiles = listOfNotNull(
                if (userDir != null) java.io.File(userDir, "log/storm_eden_log.txt") else null,
                java.io.File(ctx.getExternalFilesDir(null), "log/storm_eden_log.txt"),
                java.io.File(ctx.filesDir, "log/storm_eden_log.txt"),
                java.io.File(ctx.filesDir, "storm_eden_log.txt")
            )

            var logContent = ""
            for (f in candidateLogFiles) {
                if (f.exists() && f.length() > 0) {
                    try {
                        logContent = f.readText()
                        if (logContent.isNotBlank()) break
                    } catch (_: Throwable) {}
                }
            }

            if (logContent.isBlank()) {
                // Read from logcat if file not yet flushed
                try {
                    val process = Runtime.getRuntime().exec(arrayOf("logcat", "-d", "-s", "YuzuNative:V", "Frontend:V", "STORM_EDEN_CRASH:V"))
                    logContent = process.inputStream.bufferedReader().use { it.readText() }
                } catch (_: Throwable) {}
            }

            if (logContent.isNotBlank()) {
                val fileName = "storm_eden_log.txt"
                // 1. Write to standard direct paths
                val targetDirs = listOf(
                    android.os.Environment.getExternalStoragePublicDirectory(android.os.Environment.DIRECTORY_DOWNLOADS),
                    java.io.File("/storage/emulated/0/Download"),
                    java.io.File("/sdcard/Download")
                )
                for (dir in targetDirs) {
                    try {
                        if (dir != null) {
                            if (!dir.exists()) dir.mkdirs()
                            val dest = java.io.File(dir, fileName)
                            dest.writeText(logContent)
                            val altDest = java.io.File(dir, "STORM_EDEN_LOG.txt")
                            altDest.writeText(logContent)
                        }
                    } catch (_: Throwable) {}
                }

                // 2. MediaStore for Android 10+
                if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.Q) {
                    try {
                        val resolver = ctx.contentResolver
                        val values = android.content.ContentValues().apply {
                            put(android.provider.MediaStore.MediaColumns.DISPLAY_NAME, fileName)
                            put(android.provider.MediaStore.MediaColumns.MIME_TYPE, "text/plain")
                            put(android.provider.MediaStore.MediaColumns.RELATIVE_PATH, android.os.Environment.DIRECTORY_DOWNLOADS)
                            put(android.provider.MediaStore.MediaColumns.IS_PENDING, 1)
                        }
                        val uri = resolver.insert(android.provider.MediaStore.Downloads.EXTERNAL_CONTENT_URI, values)
                        if (uri != null) {
                            resolver.openOutputStream(uri, "wt")?.use { it.write(logContent.toByteArray(Charsets.UTF_8)) }
                            values.clear()
                            values.put(android.provider.MediaStore.MediaColumns.IS_PENDING, 0)
                            resolver.update(uri, values, null, null)
                        }
                    } catch (_: Throwable) {}
                }
            }
        } catch (_: Throwable) {}
    }
}
