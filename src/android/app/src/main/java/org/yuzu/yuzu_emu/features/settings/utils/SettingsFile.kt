// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.utils

import android.net.Uri
import org.yuzu.yuzu_emu.model.Game
import java.io.*
import org.yuzu.yuzu_emu.utils.DirectoryInitialization
import org.yuzu.yuzu_emu.utils.FileUtil
import org.yuzu.yuzu_emu.utils.NativeConfig

/**
 * Contains static methods for interacting with .ini files in which settings are stored.
 */
object SettingsFile {
    const val FILE_NAME_CONFIG = "config.ini"

    fun getSettingsFile(fileName: String): File =
        File(DirectoryInitialization.userDirectory + "/config/" + fileName)

    fun getCustomSettingsFile(game: Game): File {
        val configDir = File(DirectoryInitialization.userDirectory, "config/custom")
        val hexId = if (game.programId.isNotEmpty() && game.programId != "0") {
            try {
                String.format(java.util.Locale.US, "%016X", game.programId.toULong())
            } catch (_: Exception) {
                game.programIdHex
            }
        } else {
            game.programIdHex
        }
        val byHex = File(configDir, "$hexId.ini")
        if (byHex.exists()) return byHex
        val byProg = File(configDir, "${game.programIdHex}.ini")
        if (byProg.exists()) return byProg
        val bySettings = File(configDir, "${game.settingsName}.ini")
        if (bySettings.exists()) return bySettings
        return byHex
    }

    fun loadCustomConfig(game: Game) {
        val fileName = FileUtil.getFilename(Uri.parse(game.path))
        NativeConfig.initializePerGameConfig(game.programId, fileName)
    }
}
