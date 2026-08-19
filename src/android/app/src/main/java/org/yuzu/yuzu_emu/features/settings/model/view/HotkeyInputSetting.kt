// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.features.settings.model.view

import androidx.preference.PreferenceManager
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.features.input.model.InputType
import org.yuzu.yuzu_emu.features.input.model.NativeHotkey
import org.yuzu.yuzu_emu.utils.ParamPackage

class HotkeyInputSetting(
    override val playerIndex: Int,
    val hotkey: NativeHotkey
) : InputSetting(hotkey.titleId, "") {
    override val type = TYPE_INPUT
    override val inputType = InputType.Button

    private val prefKey: String
        get() = "player_${playerIndex}_${hotkey.configKey}"

    override fun getSelectedValue(): String {
        val prefs = PreferenceManager.getDefaultSharedPreferences(context)
        val serialized = prefs.getString(prefKey, "") ?: ""
        if (serialized.isEmpty()) {
            return context.getString(R.string.not_set)
        }
        val params = ParamPackage(serialized)
        val button = buttonToText(params)
        return getDisplayString(params, button)
    }

    override fun setSelectedValue(param: ParamPackage) {
        val prefs = PreferenceManager.getDefaultSharedPreferences(context)
        prefs.edit().putString(prefKey, param.serialize()).apply()
    }
}
