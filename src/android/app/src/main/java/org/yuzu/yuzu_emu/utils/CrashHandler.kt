// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.utils

import android.content.ContentValues
import android.content.Context
import android.os.Build
import android.os.Environment
import android.provider.MediaStore
import android.util.Log
import java.io.File
import java.io.FileOutputStream
import java.io.PrintWriter
import java.io.StringWriter
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.concurrent.atomic.AtomicBoolean

object CrashHandler {
    private const val TAG = "STORM_EDEN_CRASH"
    private const val FILE_NAME = "STORM_EDEN_CRASH.txt"
    private val installed = AtomicBoolean(false)
    private var appContext: Context? = null
    private var defaultHandler: Thread.UncaughtExceptionHandler? = null

    init {
        setupUncaughtHandler()
    }

    fun install(context: Context? = null) {
        if (context != null) {
            appContext = context.applicationContext ?: context
        }
        setupUncaughtHandler()
    }

    private fun setupUncaughtHandler() {
        if (installed.compareAndSet(false, true)) {
            defaultHandler = Thread.getDefaultUncaughtExceptionHandler()
            Thread.setDefaultUncaughtExceptionHandler { thread, throwable ->
                try {
                    recordException(thread, throwable, appContext)
                } catch (e: Throwable) {
                    Log.e(TAG, "Failed to record crash: ${e.message}", e)
                } finally {
                    defaultHandler?.uncaughtException(thread, throwable)
                }
            }
            Log.i(TAG, "STORM EDEN CrashHandler installed successfully")
        }
    }

    fun logError(context: Context?, source: String, error: Throwable) {
        try {
            recordException(Thread.currentThread(), error, context ?: appContext, extraNotes = "Source: $source")
        } catch (e: Throwable) {
            Log.e(TAG, "Error in logError: ${e.message}", e)
        }
    }

    fun recordException(thread: Thread?, throwable: Throwable, context: Context? = null, extraNotes: String? = null) {
        val dateStr = SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS", Locale.US).format(Date())
        val sw = StringWriter()
        val pw = PrintWriter(sw)
        throwable.printStackTrace(pw)
        val stackTrace = sw.toString()

        val sb = StringBuilder()
        sb.append("=======================================================\n")
        sb.append("STORM EDEN Android - Emergency Crash Report\n")
        sb.append("=======================================================\n")
        sb.append("Timestamp: ").append(dateStr).append("\n")
        sb.append("Package: ").append(context?.packageName ?: "dev.eden.eden_emulator").append("\n\n")

        if (!extraNotes.isNullOrEmpty()) {
            sb.append("--- CONTEXT NOTES ---\n")
            sb.append(extraNotes).append("\n\n")
        }

        sb.append("--- DEVICE INFORMATION ---\n")
        sb.append("Brand: ").append(Build.BRAND).append("\n")
        sb.append("Manufacturer: ").append(Build.MANUFACTURER).append("\n")
        sb.append("Model: ").append(Build.MODEL).append("\n")
        sb.append("Device: ").append(Build.DEVICE).append("\n")
        sb.append("Product: ").append(Build.PRODUCT).append("\n")
        sb.append("Hardware: ").append(Build.HARDWARE).append("\n")
        sb.append("Board: ").append(Build.BOARD).append("\n")
        sb.append("Android OS: ").append(Build.VERSION.RELEASE).append(" (SDK ").append(Build.VERSION.SDK_INT).append(")\n")
        sb.append("Supported ABIs: ").append(Build.SUPPORTED_ABIS.joinToString(", ")).append("\n")
        sb.append("Fingerprint: ").append(Build.FINGERPRINT).append("\n\n")

        sb.append("--- THREAD INFORMATION ---\n")
        if (thread != null) {
            sb.append("Thread: ").append(thread.name).append(" (ID: ").append(thread.id).append(", Priority: ").append(thread.priority).append(")\n\n")
        } else {
            sb.append("Thread: Unknown\n\n")
        }

        sb.append("--- EXCEPTION DETAILS ---\n")
        sb.append("Exception Class: ").append(throwable.javaClass.name).append("\n")
        sb.append("Message: ").append(throwable.message ?: "No message").append("\n\n")

        sb.append("--- FULL STACK TRACE ---\n")
        sb.append(stackTrace).append("\n")

        // Causes
        var cause: Throwable? = throwable.cause
        var depth = 1
        while (cause != null && depth < 10) {
            sb.append("--- CAUSE #").append(depth).append(" ---\n")
            sb.append(cause.javaClass.name).append(": ").append(cause.message ?: "").append("\n")
            val causeSw = StringWriter()
            val causePw = PrintWriter(causeSw)
            cause.printStackTrace(causePw)
            sb.append(causeSw.toString()).append("\n")
            cause = cause.cause
            depth++
        }
        sb.append("=======================================================\n")

        val crashReportText = sb.toString()

        // Log to Android Logcat
        Log.e(TAG, crashReportText)

        val ctx = context ?: appContext

        // 1. Write via MediaStore into Downloads (guaranteed on Android 10-15)
        if (ctx != null && Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            try {
                val resolver = ctx.contentResolver
                val values = ContentValues().apply {
                    put(MediaStore.MediaColumns.DISPLAY_NAME, FILE_NAME)
                    put(MediaStore.MediaColumns.MIME_TYPE, "text/plain")
                    put(MediaStore.MediaColumns.RELATIVE_PATH, Environment.DIRECTORY_DOWNLOADS)
                    put(MediaStore.MediaColumns.IS_PENDING, 1)
                }
                val uri = resolver.insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, values)
                if (uri != null) {
                    resolver.openOutputStream(uri, "wt")?.use { os ->
                        os.write(crashReportText.toByteArray(Charsets.UTF_8))
                        os.flush()
                    }
                    values.clear()
                    values.put(MediaStore.MediaColumns.IS_PENDING, 0)
                    resolver.update(uri, values, null, null)
                    Log.i(TAG, "Crash log successfully exported to MediaStore Downloads: $uri")
                }
            } catch (e: Throwable) {
                Log.w(TAG, "MediaStore export failed: ${e.message}")
            }
        }

        // 2. Direct paths
        val targetDirs = mutableListOf<File>()
        try {
            val publicDownloads = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS)
            if (publicDownloads != null) {
                targetDirs.add(publicDownloads)
            }
        } catch (ignored: Throwable) {}

        targetDirs.add(File("/storage/emulated/0/Download"))
        targetDirs.add(File("/sdcard/Download"))

        if (ctx != null) {
            try { ctx.getExternalFilesDir(null)?.let { targetDirs.add(it) } } catch (ignored: Throwable) {}
            try { targetDirs.add(ctx.filesDir) } catch (ignored: Throwable) {}
            try { ctx.externalCacheDir?.let { targetDirs.add(it) } } catch (ignored: Throwable) {}
            try { targetDirs.add(ctx.cacheDir) } catch (ignored: Throwable) {}
        }

        targetDirs.add(File("/data/data/org.yuzu.yuzu_emu/files"))
        targetDirs.add(File("/data/data/dev.eden.eden_emulator/files"))
        targetDirs.add(File("/data/user/0/dev.eden.eden_emulator/files"))

        for (dir in targetDirs) {
            try {
                if (!dir.exists()) {
                    dir.mkdirs()
                }
                val crashFile = File(dir, FILE_NAME)
                FileOutputStream(crashFile, false).use { fos ->
                    fos.write(crashReportText.toByteArray(Charsets.UTF_8))
                    fos.flush()
                }
                Log.i(TAG, "Crash report written to: ${crashFile.absolutePath}")
            } catch (e: Throwable) {
                // ignore permission errors on inaccessible paths
            }
        }
    }
}
