// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.features.input.model

import androidx.annotation.StringRes
import org.yuzu.yuzu_emu.R

enum class NativeHotkey(
    val configKey: String,
    @StringRes val titleId: Int
) {
    TranslateScreen("hotkey_translate_screen", R.string.translator_translate_screen),
    PauseEmulation("hotkey_pause_emulation", R.string.emulation_pause),
    ToggleFastForward("hotkey_toggle_fast_forward", R.string.turbo_speed_limit),
    Screenshot("hotkey_screenshot", R.string.gamepad_screenshot);
}
