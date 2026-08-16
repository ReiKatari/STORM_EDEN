// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu

import android.app.Application
import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.Context
import org.yuzu.yuzu_emu.features.input.NativeInput
import java.io.File
import java.io.FileOutputStream
import java.security.KeyStore
import javax.net.ssl.TrustManagerFactory
import javax.net.ssl.X509TrustManager
import android.content.res.Configuration
import android.os.LocaleList
import org.yuzu.yuzu_emu.features.settings.model.IntSetting
import org.yuzu.yuzu_emu.utils.DirectoryInitialization
import org.yuzu.yuzu_emu.utils.DocumentsTree
import org.yuzu.yuzu_emu.utils.GpuDriverHelper
import org.yuzu.yuzu_emu.utils.Log
import org.yuzu.yuzu_emu.utils.PowerStateUpdater
import org.yuzu.yuzu_emu.utils.ControllerNavigationGlobalHook
import java.util.Locale

fun Context.getPublicFilesDir(): File = getExternalFilesDir(null) ?: filesDir

class YuzuApplication : Application() {
    private fun createNotificationChannels() {
        val name: CharSequence = getString(R.string.app_notification_channel_name)
        val description = getString(R.string.app_notification_channel_description)
        val foregroundService = NotificationChannel(
            getString(R.string.app_notification_channel_id),
            name,
            NotificationManager.IMPORTANCE_DEFAULT
        )
        foregroundService.description = description
        foregroundService.setSound(null, null)
        foregroundService.vibrationPattern = null

        val noticeChannel = NotificationChannel(
            getString(R.string.notice_notification_channel_id),
            getString(R.string.notice_notification_channel_name),
            NotificationManager.IMPORTANCE_HIGH
        )
        noticeChannel.description = getString(R.string.notice_notification_channel_description)
        noticeChannel.setSound(null, null)

        // Register the channel with the system; you can't change the importance
        // or other notification behaviors after this
        val notificationManager = getSystemService(NotificationManager::class.java)
        notificationManager.createNotificationChannel(noticeChannel)
        notificationManager.createNotificationChannel(foregroundService)
    }

    override fun attachBaseContext(base: Context) {
        super.attachBaseContext(base)
        org.yuzu.yuzu_emu.utils.CrashHandler.install(base)
    }

    override fun onCreate() {
        super.onCreate()
        application = this
        org.yuzu.yuzu_emu.utils.CrashHandler.install(this)

        try {
            documentsTree = DocumentsTree()
        } catch (t: Throwable) {
            org.yuzu.yuzu_emu.utils.CrashHandler.logError(this, "DocumentsTree", t)
        }

        try {
            DirectoryInitialization.start()
        } catch (t: Throwable) {
            org.yuzu.yuzu_emu.utils.CrashHandler.logError(this, "DirectoryInitialization", t)
        }

        try {
            GpuDriverHelper.initializeFreedrenoConfigEarly()
        } catch (t: Throwable) {
            org.yuzu.yuzu_emu.utils.CrashHandler.logError(this, "GpuDriverHelper.initializeFreedrenoConfigEarly", t)
        }

        try {
            NativeLibrary.playTimeManagerInit()
        } catch (t: Throwable) {
            org.yuzu.yuzu_emu.utils.CrashHandler.logError(this, "NativeLibrary.playTimeManagerInit", t)
        }

        try {
            GpuDriverHelper.initializeDriverParameters()
        } catch (t: Throwable) {
            org.yuzu.yuzu_emu.utils.CrashHandler.logError(this, "GpuDriverHelper.initializeDriverParameters", t)
        }

        try {
            NativeInput.reloadInputDevices()
        } catch (t: Throwable) {
            org.yuzu.yuzu_emu.utils.CrashHandler.logError(this, "NativeInput.reloadInputDevices", t)
        }

        try {
            NativeLibrary.logDeviceInfo()
        } catch (t: Throwable) {
            org.yuzu.yuzu_emu.utils.CrashHandler.logError(this, "NativeLibrary.logDeviceInfo", t)
        }

        try {
            PowerStateUpdater.start()
        } catch (t: Throwable) {
            org.yuzu.yuzu_emu.utils.CrashHandler.logError(this, "PowerStateUpdater.start", t)
        }

        try {
            Log.logDeviceInfo()
        } catch (t: Throwable) {
            org.yuzu.yuzu_emu.utils.CrashHandler.logError(this, "Log.logDeviceInfo", t)
        }

        try {
            ControllerNavigationGlobalHook.install(this)
        } catch (t: Throwable) {
            org.yuzu.yuzu_emu.utils.CrashHandler.logError(this, "ControllerNavigationGlobalHook.install", t)
        }

        try {
            createNotificationChannels()
        } catch (t: Throwable) {
            org.yuzu.yuzu_emu.utils.CrashHandler.logError(this, "createNotificationChannels", t)
        }
    }

    companion object {
        var documentsTree: DocumentsTree? = null
        lateinit var application: YuzuApplication

        val appContext: Context
            get() = application.applicationContext

        private val LANGUAGE_CODES = arrayOf(
            "system", "en", "es", "fr", "de", "it", "pt", "pt-BR", "ru", "ja", "ko",
            "zh-CN", "zh-TW", "pl", "cs", "nb", "hu", "uk", "vi", "id", "ar", "ckb", "fa", "he", "sr"
        )

        fun applyLanguage(context: Context): Context {
            val languageIndex = IntSetting.APP_LANGUAGE.getInt()
            val langCode = if (languageIndex in LANGUAGE_CODES.indices) {
                LANGUAGE_CODES[languageIndex]
            } else {
                "system"
            }

            if (langCode == "system") {
                return context
            }

            val locale = when {
                langCode.contains("-") -> {
                    val parts = langCode.split("-")
                    Locale.Builder().setLanguage(parts[0]).setRegion(parts[1]).build()
                }
                else -> Locale.Builder().setLanguage(langCode).build()
            }

            Locale.setDefault(locale)

            val config = Configuration(context.resources.configuration)
            config.setLocales(LocaleList(locale))

            return context.createConfigurationContext(config)
        }
    }
}
