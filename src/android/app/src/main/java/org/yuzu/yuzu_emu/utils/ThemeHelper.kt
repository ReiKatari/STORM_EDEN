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
        activity.setTheme(getSelectedStaticThemeColor())
        if (BooleanSetting.BLACK_BACKGROUNDS.getBoolean() && isNightMode(activity)) {
            activity.setTheme(R.style.ThemeOverlay_Yuzu_Dark)
        }
    }

    private fun getSelectedStaticThemeColor(): Int {
        val themeIndex = IntSetting.THEME_MODE.getInt()
        return when (themeIndex) {
            0 -> R.style.Theme_Storm_Night
            1 -> R.style.Theme_Storm_Day
            2 -> R.style.Theme_Storm_Midnight
            3 -> R.style.Theme_Storm_Cyberpunk
            4 -> R.style.Theme_Storm_Gothic
            5 -> R.style.Theme_Storm_Neon
            6 -> R.style.Theme_Storm_Crimson
            7 -> R.style.Theme_Storm_Emerald
            8 -> R.style.Theme_Storm_Glacier
            9 -> R.style.Theme_Storm_Amethyst
            else -> R.style.Theme_Storm_Night
        }
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

    @ColorInt
    fun getColorFromAttr(context: android.content.Context, @androidx.annotation.AttrRes attrRes: Int): Int {
        val typedValue = android.util.TypedValue()
        context.theme.resolveAttribute(attrRes, typedValue, true)
        return typedValue.data
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
