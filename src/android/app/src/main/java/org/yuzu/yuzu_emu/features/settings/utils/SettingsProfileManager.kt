// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.features.settings.utils

import android.content.Context
import android.content.Intent
import androidx.core.content.FileProvider
import org.json.JSONObject
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.features.settings.model.BooleanSetting
import org.yuzu.yuzu_emu.features.settings.model.IntSetting
import org.yuzu.yuzu_emu.features.settings.model.ShortSetting
import org.yuzu.yuzu_emu.utils.DirectoryInitialization
import org.yuzu.yuzu_emu.utils.Log
import org.yuzu.yuzu_emu.utils.NativeConfig
import java.io.File
import java.io.InputStream

data class SettingsProfile(
    val id: String,
    var name: String,
    var description: String,
    val isPreset: Boolean = false,
    val timestamp: Long = System.currentTimeMillis(),
    val file: File? = null
)

object SettingsProfileManager {
    private const val PROFILES_DIR_NAME = "profiles"
    private const val FILE_EXTENSION = ".stormprofile"

    private val trackedBooleanSettings = listOf(
        BooleanSetting.RENDERER_USE_SPEED_LIMIT,
        BooleanSetting.RENDERER_ASYNCHRONOUS_SHADERS,
        BooleanSetting.RENDERER_REACTIVE_FLUSHING,
        BooleanSetting.VULKAN_PIPELINE_CACHE,
        BooleanSetting.VRAM_GARBAGE_COLLECTION,
        BooleanSetting.FASTMEM,
        BooleanSetting.FASTMEM_EXCLUSIVES,
        BooleanSetting.USE_DOCKED_MODE,
        BooleanSetting.AIRPLANE_MODE
    )

    private val trackedIntSettings = listOf(
        IntSetting.RENDERER_BACKEND,
        IntSetting.RENDERER_ACCURACY,
        IntSetting.RENDERER_RESOLUTION,
        IntSetting.RENDERER_SCALING_FILTER,
        IntSetting.RENDERER_ANTI_ALIASING,
        IntSetting.ASTC_RECOMPRESSION,
        IntSetting.RENDERER_DYNA_STATE,
        IntSetting.RENDERER_VRAM_USAGE_MODE,
        IntSetting.FSR_SHARPENING_SLIDER,
        IntSetting.MAX_ANISOTROPY,
        IntSetting.CPU_ACCURACY,
        IntSetting.CPU_BACKEND,
        IntSetting.MEMORY_LAYOUT,
        IntSetting.FAST_GPU_TIME,
        IntSetting.AUDIO_OUTPUT_ENGINE
    )

    private val trackedShortSettings = listOf(
        ShortSetting.RENDERER_SPEED_LIMIT
    )

    fun getProfilesDirectory(): File {
        val dir = File(DirectoryInitialization.userDirectory, PROFILES_DIR_NAME)
        if (!dir.exists()) {
            dir.mkdirs()
        }
        return dir
    }

    fun getCustomProfiles(): List<SettingsProfile> {
        val dir = getProfilesDirectory()
        val files = dir.listFiles { f -> f.isFile && f.name.endsWith(FILE_EXTENSION) } ?: return emptyList()
        val list = mutableListOf<SettingsProfile>()
        for (file in files) {
            try {
                val json = JSONObject(file.readText())
                val id = json.optString("id", file.nameWithoutExtension)
                val name = json.optString("name", file.nameWithoutExtension)
                val desc = json.optString("description", "")
                val ts = json.optLong("timestamp", file.lastModified())
                list.add(SettingsProfile(id = id, name = name, description = desc, isPreset = false, timestamp = ts, file = file))
            } catch (e: Exception) {
                Log.error("[SettingsProfileManager] Error reading profile ${file.name}: ${e.message}")
            }
        }
        list.sortByDescending { it.timestamp }
        return list
    }

    fun saveCurrentSettingsAsProfile(name: String, description: String = ""): SettingsProfile? {
        try {
            val safeName = name.trim().ifEmpty { "Profile_" + System.currentTimeMillis() }
            val id = "profile_" + System.currentTimeMillis()
            val file = File(getProfilesDirectory(), "$id$FILE_EXTENSION")

            val json = JSONObject()
            json.put("id", id)
            json.put("name", safeName)
            json.put("description", description.trim())
            json.put("version", "6.0.0")
            json.put("timestamp", System.currentTimeMillis())

            val boolObj = JSONObject()
            for (setting in trackedBooleanSettings) {
                boolObj.put(setting.key, setting.getBoolean(true))
            }
            json.put("booleans", boolObj)

            val intObj = JSONObject()
            for (setting in trackedIntSettings) {
                intObj.put(setting.key, setting.getInt(true))
            }
            json.put("integers", intObj)

            val shortObj = JSONObject()
            for (setting in trackedShortSettings) {
                shortObj.put(setting.key, setting.getShort(true).toInt())
            }
            json.put("shorts", shortObj)

            file.writeText(json.toString(2))
            Log.info("[SettingsProfileManager] Saved profile: $safeName to ${file.absolutePath}")
            return SettingsProfile(id = id, name = safeName, description = description, isPreset = false, file = file)
        } catch (e: Exception) {
            Log.error("[SettingsProfileManager] Failed to save profile: ${e.message}")
            return null
        }
    }

    fun applyProfile(profile: SettingsProfile): Boolean {
        try {
            val file = profile.file ?: return false
            if (!file.exists()) return false

            val json = JSONObject(file.readText())
            val booleans = json.optJSONObject("booleans")
            if (booleans != null) {
                for (setting in trackedBooleanSettings) {
                    if (booleans.has(setting.key)) {
                        setting.setBoolean(booleans.getBoolean(setting.key))
                    }
                }
            }

            val integers = json.optJSONObject("integers")
            if (integers != null) {
                for (setting in trackedIntSettings) {
                    if (integers.has(setting.key)) {
                        setting.setInt(integers.getInt(setting.key))
                    }
                }
            }

            val shorts = json.optJSONObject("shorts")
            if (shorts != null) {
                for (setting in trackedShortSettings) {
                    if (shorts.has(setting.key)) {
                        setting.setShort(shorts.getInt(setting.key).toShort())
                    }
                }
            }

            NativeConfig.saveGlobalConfig()
            Log.info("[SettingsProfileManager] Applied custom profile: ${profile.name}")
            return true
        } catch (e: Exception) {
            Log.error("[SettingsProfileManager] Failed to apply profile: ${e.message}")
            return false
        }
    }

    fun renameProfile(profile: SettingsProfile, newName: String): Boolean {
        val file = profile.file ?: return false
        val cleanName = newName.trim()
        if (cleanName.isEmpty()) return false
        try {
            val json = JSONObject(file.readText())
            json.put("name", cleanName)
            file.writeText(json.toString(2))
            profile.name = cleanName
            Log.info("[SettingsProfileManager] Renamed profile to: $cleanName")
            return true
        } catch (e: Exception) {
            Log.error("[SettingsProfileManager] Error renaming profile: ${e.message}")
            return false
        }
    }

    fun deleteProfile(profile: SettingsProfile): Boolean {
        val file = profile.file ?: return false
        return try {
            val deleted = file.delete()
            Log.info("[SettingsProfileManager] Deleted profile: ${profile.name} (success=$deleted)")
            deleted
        } catch (e: Exception) {
            Log.error("[SettingsProfileManager] Error deleting profile: ${e.message}")
            false
        }
    }

    fun shareProfile(context: Context, profile: SettingsProfile) {
        val file = profile.file ?: return
        if (!file.exists()) return

        try {
            val uri = FileProvider.getUriForFile(
                context,
                context.packageName + ".provider",
                file
            )
            val intent = Intent(Intent.ACTION_SEND).apply {
                type = "application/octet-stream"
                putExtra(Intent.EXTRA_STREAM, uri)
                putExtra(Intent.EXTRA_SUBJECT, "STORM EDEN Profile: ${profile.name}")
                addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            }
            context.startActivity(Intent.createChooser(intent, context.getString(R.string.profile_share_title)))
        } catch (e: Exception) {
            Log.error("[SettingsProfileManager] Error sharing profile: ${e.message}")
        }
    }

    fun importProfileFromStream(stream: InputStream, originalFileName: String? = null): SettingsProfile? {
        try {
            val text = stream.bufferedReader().use { it.readText() }
            val json = JSONObject(text)
            val name = json.optString("name", originalFileName?.removeSuffix(FILE_EXTENSION) ?: "Imported_Profile")
            val desc = json.optString("description", "")
            val id = "imported_" + System.currentTimeMillis()
            val targetFile = File(getProfilesDirectory(), "$id$FILE_EXTENSION")

            json.put("id", id)
            targetFile.writeText(json.toString(2))
            Log.info("[SettingsProfileManager] Successfully imported profile: $name")
            return SettingsProfile(id = id, name = name, description = desc, isPreset = false, file = targetFile)
        } catch (e: Exception) {
            Log.error("[SettingsProfileManager] Failed to import profile: ${e.message}")
            return null
        }
    }
}