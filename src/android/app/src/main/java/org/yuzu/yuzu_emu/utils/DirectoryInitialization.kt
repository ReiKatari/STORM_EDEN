// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.utils

import androidx.preference.PreferenceManager
import java.io.File
import java.io.IOException
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.features.settings.model.BooleanSetting
import org.yuzu.yuzu_emu.features.settings.model.IntSetting
import org.yuzu.yuzu_emu.features.settings.model.ShortSetting
import org.yuzu.yuzu_emu.features.settings.model.Settings
import org.yuzu.yuzu_emu.overlay.model.OverlayControlData
import org.yuzu.yuzu_emu.overlay.model.OverlayControl
import org.yuzu.yuzu_emu.overlay.model.OverlayLayout
import org.yuzu.yuzu_emu.utils.PreferenceUtil.migratePreference

object DirectoryInitialization {
    private var userPath: String? = null

    var areDirectoriesReady: Boolean = false

    fun start() {
        if (!areDirectoriesReady) {
            initializeInternalStorage()
            NativeConfig.initializeGlobalConfig()
            NativeLibrary.initializeSystem(false)
            NativeLibrary.reloadProfiles()
            migrateSettings()
            areDirectoriesReady = true
        }
    }

    val userDirectory: String?
        get() {
            check(areDirectoriesReady) { "Directory initialization is not ready!" }
            return userPath
        }

    fun initializeSharedStorage() {
        try {
            val rootExternal = File(android.os.Environment.getExternalStorageDirectory(), "STORM SWITCH")
            if (rootExternal.exists() || rootExternal.mkdirs()) {
                listOf("keys", "config", "config/custom", "load", "nand", "nand/user/save", "sdmc", "amiibo", "Amiibo", "cheats", "gpu_drivers", "logs", "screenshots", "profiles").forEach { sub ->
                    File(rootExternal, sub).mkdirs()
                }
                val internalBaseDir = YuzuApplication.appContext.getExternalFilesDir(null) ?: YuzuApplication.appContext.filesDir
                migrateDirectoryIfMissing(internalBaseDir, rootExternal)
                migrateFromLegacyDirectories(rootExternal)

                val newPath = rootExternal.canonicalPath
                if (userPath != newPath && !NativeLibrary.isRunning()) {
                    userPath = newPath
                    NativeLibrary.setAppDirectory(userPath!!)
                    NativeConfig.initializeGlobalConfig()
                    NativeLibrary.reloadProfiles()
                }
            }
        } catch (_: Throwable) {}
    }

    private fun migrateDirectoryIfMissing(sourceDir: File, targetDir: File) {
        try {
            if (!sourceDir.exists() || sourceDir.canonicalPath == targetDir.canonicalPath) {
                return
            }
            val subdirs = listOf("keys", "config", "load", "nand", "sdmc", "amiibo", "cheats", "gpu_drivers", "profiles")
            for (sub in subdirs) {
                val srcSub = File(sourceDir, sub)
                val dstSub = File(targetDir, sub)
                if (srcSub.exists() && srcSub.isDirectory) {
                    dstSub.mkdirs()
                    srcSub.listFiles()?.forEach { file ->
                        val targetFile = File(dstSub, file.name)
                        if (!targetFile.exists()) {
                            try {
                                if (file.isDirectory) {
                                    file.copyRecursively(targetFile, overwrite = false)
                                } else {
                                    file.copyTo(targetFile, overwrite = false)
                                }
                            } catch (_: Throwable) {}
                        }
                    }
                }
            }
        } catch (_: Throwable) {}
    }

    private fun migrateFromLegacyDirectories(targetDir: File) {
        try {
            val legacyCandidates = listOf(
                File(android.os.Environment.getExternalStorageDirectory(), "STORM EDEN"),
                File(android.os.Environment.getExternalStorageDirectory(), "Eden"),
                File("/data/user/0/dev.storm_eden/files"),
                File("/data/user/0/org.yuzu.yuzu_emu/files")
            )
            for (legacyDir in legacyCandidates) {
                if (legacyDir.exists() && legacyDir.isDirectory) {
                    migrateDirectoryIfMissing(legacyDir, targetDir)
                }
            }
        } catch (_: Throwable) {}
    }

    private fun initializeInternalStorage() {
        try {
            var initialized = false
            if (android.os.Build.VERSION.SDK_INT < android.os.Build.VERSION_CODES.R || android.os.Environment.isExternalStorageManager()) {
                val rootExternal = File(android.os.Environment.getExternalStorageDirectory(), "STORM SWITCH")
                if (rootExternal.exists() || rootExternal.mkdirs()) {
                    listOf("keys", "config", "config/custom", "load", "nand", "sdmc", "amiibo", "cheats", "gpu_drivers", "logs", "screenshots", "profiles").forEach { sub ->
                        File(rootExternal, sub).mkdirs()
                    }
                    val internalBaseDir = YuzuApplication.appContext.getExternalFilesDir(null) ?: YuzuApplication.appContext.filesDir
                    migrateDirectoryIfMissing(internalBaseDir, rootExternal)
                    migrateFromLegacyDirectories(rootExternal)

                    userPath = rootExternal.canonicalPath
                    NativeLibrary.setAppDirectory(userPath!!)
                    initialized = true
                }
            }
            if (!initialized) {
                val baseDir = YuzuApplication.appContext.getExternalFilesDir(null) ?: YuzuApplication.appContext.filesDir
                userPath = baseDir.canonicalPath
                listOf("keys", "config", "config/custom", "load", "nand", "sdmc", "amiibo", "cheats", "gpu_drivers", "logs", "screenshots", "profiles").forEach { sub ->
                    File(baseDir, sub).mkdirs()
                }
                NativeLibrary.setAppDirectory(userPath!!)
            }
        } catch (e: Throwable) {
            CrashHandler.logError(YuzuApplication.appContext, "DirectoryInitialization.initializeInternalStorage", e)
            try {
                userPath = YuzuApplication.appContext.filesDir.absolutePath
                NativeLibrary.setAppDirectory(userPath!!)
            } catch (ignored: Throwable) {}
        }
    }

    private fun migrateSettings() {
        val preferences = PreferenceManager.getDefaultSharedPreferences(YuzuApplication.appContext)
        var saveConfig = false
        val theme = preferences.migratePreference<Int>(Settings.PREF_THEME)
        if (theme != null) {
            IntSetting.THEME.setInt(theme)
            saveConfig = true
        }

        val themeMode = preferences.migratePreference<Int>(Settings.PREF_THEME_MODE)
        if (themeMode != null) {
            IntSetting.THEME_MODE.setInt(themeMode)
            saveConfig = true
        }

        val staticThemeColor = preferences.migratePreference<Int>(Settings.PREF_STATIC_THEME_COLOR)
        if (staticThemeColor != null) {
            IntSetting.STATIC_THEME_COLOR.setInt(staticThemeColor)
            saveConfig = true
        }

        val blackBackgrounds =
            preferences.migratePreference<Boolean>(Settings.PREF_BLACK_BACKGROUNDS)
        if (blackBackgrounds != null) {
            BooleanSetting.BLACK_BACKGROUNDS.setBoolean(blackBackgrounds)
            saveConfig = true
        }

        val joystickRelCenter =
            preferences.migratePreference<Boolean>(Settings.PREF_MENU_SETTINGS_JOYSTICK_REL_CENTER)
        if (joystickRelCenter != null) {
            BooleanSetting.JOYSTICK_REL_CENTER.setBoolean(joystickRelCenter)
            saveConfig = true
        }

        val dpadSlide =
            preferences.migratePreference<Boolean>(Settings.PREF_MENU_SETTINGS_DPAD_SLIDE)
        if (dpadSlide != null) {
            BooleanSetting.DPAD_SLIDE.setBoolean(dpadSlide)
            saveConfig = true
        }

        val hapticFeedback =
            preferences.migratePreference<Boolean>(Settings.PREF_MENU_SETTINGS_HAPTICS)
        if (hapticFeedback != null) {
            BooleanSetting.HAPTIC_FEEDBACK.setBoolean(hapticFeedback)
            saveConfig = true
        }

        val hasMigratedDefaults499 = preferences.getBoolean("migrated_defaults_499", false)
        if (!hasMigratedDefaults499) {
            val curSpeedLimit = ShortSetting.RENDERER_SPEED_LIMIT.getShort(true)
            if (curSpeedLimit <= 0 || curSpeedLimit > 1000) {
                ShortSetting.RENDERER_SPEED_LIMIT.setShort(100.toShort())
                saveConfig = true
            }
            if (!BooleanSetting.RENDERER_USE_SPEED_LIMIT.getBoolean(true)) {
                BooleanSetting.RENDERER_USE_SPEED_LIMIT.setBoolean(true)
                saveConfig = true
            }
            // Enforce low-latency Mailbox VSync Mode (1)
            if (IntSetting.RENDERER_VSYNC.getInt(true) != 1) {
                IntSetting.RENDERER_VSYNC.setInt(1)
                saveConfig = true
            }
            if (!BooleanSetting.RENDERER_ASYNCHRONOUS_SHADERS.getBoolean(true)) {
                BooleanSetting.RENDERER_ASYNCHRONOUS_SHADERS.setBoolean(true)
                saveConfig = true
            }
            if (!BooleanSetting.FASTMEM.getBoolean(true)) {
                BooleanSetting.FASTMEM.setBoolean(true)
                saveConfig = true
            }
            if (IntSetting.RENDERER_DYNA_STATE.getInt(true) == 0) {
                IntSetting.RENDERER_DYNA_STATE.setInt(1)
                saveConfig = true
            }
            // Ensure CPU Accuracy is Auto (1) to prevent unsafe 64-bit tagged pointer crashes
            if (IntSetting.CPU_ACCURACY.getInt(true) == 2) {
                IntSetting.CPU_ACCURACY.setInt(1)
                saveConfig = true
            }
            preferences.edit().putBoolean("migrated_defaults_499", true).apply()
        }

        val showPerformanceOverlay =
            preferences.migratePreference<Boolean>(Settings.PREF_MENU_SETTINGS_SHOW_FPS)
        if (showPerformanceOverlay != null) {
            BooleanSetting.SHOW_PERFORMANCE_OVERLAY.setBoolean(showPerformanceOverlay)
            saveConfig = true
        }

        val showInputOverlay =
            preferences.migratePreference<Boolean>(Settings.PREF_MENU_SETTINGS_SHOW_OVERLAY)
        if (showInputOverlay != null) {
            BooleanSetting.SHOW_INPUT_OVERLAY.setBoolean(showInputOverlay)
            saveConfig = true
        }

        val overlayOpacity = preferences.migratePreference<Int>(Settings.PREF_CONTROL_OPACITY)
        if (overlayOpacity != null) {
            IntSetting.OVERLAY_OPACITY.setInt(overlayOpacity)
            saveConfig = true
        }

        val overlayScale = preferences.migratePreference<Int>(Settings.PREF_CONTROL_SCALE)
        if (overlayScale != null) {
            IntSetting.OVERLAY_SCALE.setInt(overlayScale)
            saveConfig = true
        }

        var setOverlayData = false
        val overlayControlData = NativeConfig.getOverlayControlData()
        if (overlayControlData.isEmpty()) {
            val overlayControlDataMap =
                NativeConfig.getOverlayControlData().associateBy { it.id }.toMutableMap()
            for (button in Settings.overlayPreferences) {
                val buttonId = convertButtonId(button)
                var buttonEnabled = preferences.migratePreference<Boolean>(button)
                if (buttonEnabled == null) {
                    buttonEnabled = OverlayControl.map[buttonId]?.defaultVisibility == true
                }

                var landscapeXPosition = preferences.migratePreference<Float>(
                    "$button-X${Settings.PREF_LANDSCAPE_SUFFIX}"
                )?.toDouble()
                var landscapeYPosition = preferences.migratePreference<Float>(
                    "$button-Y${Settings.PREF_LANDSCAPE_SUFFIX}"
                )?.toDouble()
                if (landscapeXPosition == null || landscapeYPosition == null) {
                    val landscapePosition = OverlayControl.map[buttonId]
                        ?.getDefaultPositionForLayout(OverlayLayout.Landscape) ?: Pair(0.0, 0.0)
                    landscapeXPosition = landscapePosition.first
                    landscapeYPosition = landscapePosition.second
                }

                var portraitXPosition = preferences.migratePreference<Float>(
                    "$button-X${Settings.PREF_PORTRAIT_SUFFIX}"
                )?.toDouble()
                var portraitYPosition = preferences.migratePreference<Float>(
                    "$button-Y${Settings.PREF_PORTRAIT_SUFFIX}"
                )?.toDouble()
                if (portraitXPosition == null || portraitYPosition == null) {
                    val portraitPosition = OverlayControl.map[buttonId]
                        ?.getDefaultPositionForLayout(OverlayLayout.Portrait) ?: Pair(0.0, 0.0)
                    portraitXPosition = portraitPosition.first
                    portraitYPosition = portraitPosition.second
                }

                var foldableXPosition = preferences.migratePreference<Float>(
                    "$button-X${Settings.PREF_FOLDABLE_SUFFIX}"
                )?.toDouble()
                var foldableYPosition = preferences.migratePreference<Float>(
                    "$button-Y${Settings.PREF_FOLDABLE_SUFFIX}"
                )?.toDouble()
                if (foldableXPosition == null || foldableYPosition == null) {
                    val foldablePosition = OverlayControl.map[buttonId]
                        ?.getDefaultPositionForLayout(OverlayLayout.Foldable) ?: Pair(0.0, 0.0)
                    foldableXPosition = foldablePosition.first
                    foldableYPosition = foldablePosition.second
                }

                val controlData = OverlayControlData(
                    buttonId,
                    buttonEnabled,
                    Pair(landscapeXPosition, landscapeYPosition),
                    Pair(portraitXPosition, portraitYPosition),
                    Pair(foldableXPosition, foldableYPosition),
                    OverlayControl.map[buttonId]?.defaultIndividualScaleResource ?: 1.0f
                )
                overlayControlDataMap[buttonId] = controlData
                setOverlayData = true
            }

            if (setOverlayData) {
                NativeConfig.setOverlayControlData(
                    overlayControlDataMap.map { it.value }.toTypedArray()
                )
                saveConfig = true
            }
        }

        if (saveConfig) {
            NativeConfig.saveGlobalConfig()
        }
    }

    private fun convertButtonId(buttonId: String): String =
        when (buttonId) {
            Settings.PREF_BUTTON_A -> OverlayControl.BUTTON_A.id
            Settings.PREF_BUTTON_B -> OverlayControl.BUTTON_B.id
            Settings.PREF_BUTTON_X -> OverlayControl.BUTTON_X.id
            Settings.PREF_BUTTON_Y -> OverlayControl.BUTTON_Y.id
            Settings.PREF_BUTTON_L -> OverlayControl.BUTTON_L.id
            Settings.PREF_BUTTON_R -> OverlayControl.BUTTON_R.id
            Settings.PREF_BUTTON_ZL -> OverlayControl.BUTTON_ZL.id
            Settings.PREF_BUTTON_ZR -> OverlayControl.BUTTON_ZR.id
            Settings.PREF_BUTTON_PLUS -> OverlayControl.BUTTON_PLUS.id
            Settings.PREF_BUTTON_MINUS -> OverlayControl.BUTTON_MINUS.id
            Settings.PREF_BUTTON_DPAD -> OverlayControl.COMBINED_DPAD.id
            Settings.PREF_STICK_L -> OverlayControl.STICK_L.id
            Settings.PREF_STICK_R -> OverlayControl.STICK_R.id
            Settings.PREF_BUTTON_HOME -> OverlayControl.BUTTON_HOME.id
            Settings.PREF_BUTTON_SCREENSHOT -> OverlayControl.BUTTON_CAPTURE.id
            Settings.PREF_BUTTON_STICK_L -> OverlayControl.BUTTON_STICK_L.id
            Settings.PREF_BUTTON_STICK_R -> OverlayControl.BUTTON_STICK_R.id
            else -> ""
        }
}
