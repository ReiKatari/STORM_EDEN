// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.utils

import android.content.SharedPreferences
import android.content.res.Configuration
import android.graphics.Color
import android.os.Build
import androidx.annotation.ColorInt
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.app.AppCompatDelegate
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsControllerCompat
import kotlin.math.roundToInt
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.features.settings.model.BooleanSetting
import org.yuzu.yuzu_emu.features.settings.model.IntSetting
import org.yuzu.yuzu_emu.ui.main.ThemeProvider
import androidx.preference.PreferenceManager
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.features.settings.model.Settings

object ThemeHelper {
    const val SYSTEM_BAR_ALPHA = 0.9f

    // Listener that detects if the theme keys are being changed from the setting menu and recreates the activity
    private var listener: SharedPreferences.OnSharedPreferenceChangeListener? = null
    private val preferences = PreferenceManager.getDefaultSharedPreferences(
        YuzuApplication.appContext
    )

    fun setTheme(activity: AppCompatActivity) {
        setThemeMode(activity)
        val themeIndex = IntSetting.THEME_MODE.getInt()
        if (themeIndex == 2) {
            activity.setTheme(R.style.Theme_Eden_Main)
            activity.setTheme(R.style.ThemeOverlay_Yuzu_Dark)
        } else {
            activity.setTheme(getSelectedStaticThemeColor())
            if (BooleanSetting.BLACK_BACKGROUNDS.getBoolean() && isNightMode(activity)) {
                activity.setTheme(R.style.ThemeOverlay_Yuzu_Dark)
            }
        }
    }

    private fun getSelectedStaticThemeColor(): Int {
        val themeIndex = IntSetting.THEME_MODE.getInt()
        val themes = arrayOf(
            R.style.Theme_Eden_Main,
            R.style.Theme_Yuzu_Main_Blue,
            R.style.Theme_Eden_Main,
            R.style.Theme_Yuzu_Main_Violet,
            R.style.Theme_Yuzu_Main_Gray,
            R.style.Theme_Yuzu_Main_Cyan,
            R.style.Theme_Yuzu_Main_Red,
            R.style.Theme_Yuzu_Main_Green,
            R.style.Theme_Yuzu_Main_Cyan,
            R.style.Theme_Yuzu_Main_Pink
        )
        if (themeIndex in themes.indices) {
            return themes[themeIndex]
        }
        return R.style.Theme_Eden_Main
    }

    @ColorInt
    fun getColorWithOpacity(@ColorInt color: Int, alphaFactor: Float): Int {
        return Color.argb(
            (alphaFactor * Color.alpha(color)).roundToInt(),
            Color.red(color),
            Color.green(color),
            Color.blue(color)
        )
    }

    fun setCorrectTheme(activity: AppCompatActivity) {
        val currentTheme = (activity as ThemeProvider).themeId
        setTheme(activity)
        if (currentTheme != (activity as ThemeProvider).themeId) {
            activity.recreate()
        }
    }

    fun setThemeMode(activity: AppCompatActivity) {
        val themeMode = IntSetting.THEME_MODE.getInt()
        val windowController = WindowCompat.getInsetsController(
            activity.window,
            activity.window.decorView
        )
        if (themeMode == 1) {
            activity.delegate.localNightMode = AppCompatDelegate.MODE_NIGHT_NO
            setLightModeSystemBars(windowController)
        } else {
            activity.delegate.localNightMode = AppCompatDelegate.MODE_NIGHT_YES
            setDarkModeSystemBars(windowController)
        }
    }

    private fun isNightMode(activity: AppCompatActivity): Boolean {
        return when (activity.resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK) {
            Configuration.UI_MODE_NIGHT_NO -> false
            Configuration.UI_MODE_NIGHT_YES -> true
            else -> false
        }
    }

    private fun setLightModeSystemBars(windowController: WindowInsetsControllerCompat) {
        windowController.isAppearanceLightStatusBars = true
        windowController.isAppearanceLightNavigationBars = true
    }

    private fun setDarkModeSystemBars(windowController: WindowInsetsControllerCompat) {
        windowController.isAppearanceLightStatusBars = false
        windowController.isAppearanceLightNavigationBars = false
    }

    fun ThemeChangeListener(activity: AppCompatActivity) {
        listener = SharedPreferences.OnSharedPreferenceChangeListener { _, key ->
            val relevantKeys = listOf(
                Settings.PREF_STATIC_THEME_COLOR,
                Settings.PREF_THEME_MODE,
                Settings.PREF_BLACK_BACKGROUNDS
            )
            if (key in relevantKeys) {
                activity.recreate()
            }
        }
        preferences.registerOnSharedPreferenceChangeListener(listener)
    }
}

enum class Theme(val int: Int) {
    Default(0),
    MaterialYou(1);

    companion object {
        fun from(int: Int): Theme = entries.firstOrNull { it.int == int } ?: Default
    }
}
