// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.model

import android.content.Intent
import android.net.Uri
import android.os.Parcelable
import java.util.HashSet
import kotlinx.parcelize.Parcelize
import kotlinx.serialization.Serializable
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.activities.EmulationActivity
import org.yuzu.yuzu_emu.utils.DirectoryInitialization
import org.yuzu.yuzu_emu.utils.FileUtil
import org.yuzu.yuzu_emu.utils.NativeConfig
import java.time.LocalDateTime
import java.time.format.DateTimeFormatter

@Parcelize
@Serializable
class Game(
    val title: String = "",
    val path: String,
    val programId: String = "",
    val developer: String = "",
    var version: String = "",
    var internalVersion: String = "",
    val isHomebrew: Boolean = false,
    var addonCount: Int = 0
) : Parcelable {
    val keyAddedToLibraryTime get() = "${path}_AddedToLibraryTime"
    val keyLastPlayedTime get() = "${path}_LastPlayed"

    val extension: String
        get() {
            val ext = FileUtil.getExtension(Uri.parse(path)).uppercase()
            return if (ext.isNotEmpty()) ext else "NSP"
        }

    val settingsName: String
        get() {
            val cleanId = programId.trim()
            val parsedHex = cleanId.toULongOrNull(16) ?: cleanId.toULongOrNull(10)
            return if (parsedHex != null && parsedHex != 0uL) {
                String.format(java.util.Locale.ROOT, "%016X", parsedHex.toLong())
            } else if (cleanId.isNotEmpty() && cleanId != "0") {
                cleanId
            } else {
                FileUtil.getFilename(Uri.parse(path))
            }
        }

    val programIdHex: String
        get() {
            val cleanId = programId.trim()
            val parsedHex = cleanId.toULongOrNull(16) ?: cleanId.toULongOrNull(10)
            return if (parsedHex != null && parsedHex != 0uL) {
                String.format(java.util.Locale.ROOT, "%016X", parsedHex.toLong())
            } else if (cleanId.isNotEmpty()) {
                cleanId
            } else {
                "0"
            }
        }

    val saveZipName: String
        get() = "$title ${YuzuApplication.appContext.getString(R.string.save_data).lowercase()} - ${
        LocalDateTime.now().format(DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm"))
        }.zip"

    val saveDir: String
        get() = try {
            NativeConfig.getSaveDir() + NativeLibrary.getSavePath(programIdHex)
        } catch (_: Throwable) {
            NativeConfig.getSaveDir()
        }

    val addonDir: String
        get() = DirectoryInitialization.userDirectory + "/load/" + programIdHex + "/"

    val launchIntent: Intent
        get() = Intent(YuzuApplication.appContext, EmulationActivity::class.java).apply {
            action = Intent.ACTION_VIEW
            data = Uri.parse(path)
        }

    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (javaClass != other?.javaClass) return false

        other as Game

        if (title != other.title) return false
        if (path != other.path) return false
        if (programId != other.programId) return false
        if (developer != other.developer) return false
        if (version != other.version) return false
        if (internalVersion != other.internalVersion) return false
        if (isHomebrew != other.isHomebrew) return false
        if (addonCount != other.addonCount) return false

        return true
    }

    override fun hashCode(): Int {
        var result = title.hashCode()
        result = 31 * result + path.hashCode()
        result = 31 * result + programId.hashCode()
        result = 31 * result + developer.hashCode()
        result = 31 * result + version.hashCode()
        result = 31 * result + internalVersion.hashCode()
        result = 31 * result + isHomebrew.hashCode()
        result = 31 * result + addonCount.hashCode()
        return result
    }

    companion object {
        val extensions: Set<String> = HashSet(
            listOf("xci", "nsp", "nca", "nro", "nsz", "xcz")
        )
    }
}
