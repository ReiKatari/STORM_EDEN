// SPDX-FileCopyrightText: 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.model

import org.yuzu.yuzu_emu.utils.GpuDriverMetadata

data class Driver(
    override var selected: Boolean,
    val title: String,
    val version: String = "",
    val description: String = ""
) : SelectableItem {
    override fun onSelectionStateChanged(selected: Boolean) {
        this.selected = selected
    }

    companion object {
        fun GpuDriverMetadata.toDriver(selected: Boolean = false): Driver {
            val ver = packageVersion?.takeIf { it.isNotBlank() } ?: version?.takeIf { it.isNotBlank() } ?: ""
            val baseName = name?.takeIf { it.isNotBlank() } ?: ""
            val displayTitle = if (ver.isNotEmpty() && baseName.isNotEmpty()) {
                if (baseName.contains(ver)) baseName else "$baseName $ver"
            } else if (baseName.isNotEmpty()) {
                baseName
            } else if (ver.isNotEmpty()) {
                ver
            } else {
                ""
            }
            return Driver(
                selected,
                displayTitle,
                ver,
                description ?: ""
            )
        }
    }
}
