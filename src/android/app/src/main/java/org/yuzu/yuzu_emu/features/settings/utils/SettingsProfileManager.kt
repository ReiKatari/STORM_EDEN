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
        BooleanSetting.AIRPLANE_MODE,
        BooleanSetting.ECO_THERMAL_MODE,
        BooleanSetting.ECO_FRAME_PACING,
        BooleanSetting.SMART_SHADER_THROTTLE,
        BooleanSetting.CPU_AFFINITY_PINNING,
        BooleanSetting.RENDERER_USE_DISK_SHADER_CACHE,
        BooleanSetting.RENDERER_FORCE_MAX_CLOCK,
        BooleanSetting.RENDERER_ASYNCHRONOUS_GPU_EMULATION,
        BooleanSetting.RENDERER_ASYNC_PRESENTATION,
        BooleanSetting.ENABLE_BUFFER_HISTORY,
        BooleanSetting.USE_OPTIMIZED_VERTEX_BUFFERS,
        BooleanSetting.SYNC_MEMORY_OPERATIONS,
        BooleanSetting.RENDERER_SAMPLE_SHADING,
        BooleanSetting.RENDERER_FRAME_GEN
    )

    private val trackedIntSettings = listOf(
        IntSetting.RENDERER_BACKEND,
        IntSetting.RENDERER_ACCURACY,
        IntSetting.RENDERER_RESOLUTION,
        IntSetting.RENDERER_SCALING_FILTER,
        IntSetting.RENDERER_ANTI_ALIASING,
        IntSetting.RENDERER_NVDEC_EMULATION,
        IntSetting.RENDERER_ASTC_DECODE_METHOD,
        IntSetting.ASTC_RECOMPRESSION,
        IntSetting.RENDERER_DYNA_STATE,
        IntSetting.RENDERER_VRAM_USAGE_MODE,
        IntSetting.RENDERER_VSYNC,
        IntSetting.RENDERER_ASPECT_RATIO,
        IntSetting.DMA_ACCURACY,
        IntSetting.GPU_FENCE_BEHAVIOR,
        IntSetting.FRAME_PACING_MODE,
        IntSetting.FSR_SHARPENING_SLIDER,
        IntSetting.MAX_ANISOTROPY,
        IntSetting.CPU_ACCURACY,
        IntSetting.CPU_BACKEND,
        IntSetting.MEMORY_LAYOUT,
        IntSetting.FAST_GPU_TIME,
        IntSetting.FAST_CPU_TIME,
        IntSetting.AUDIO_OUTPUT_ENGINE,
        IntSetting.ANDROID_PIPELINE_WORKERS
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

    private fun ensureDefaultProfiles(dir: File) {
        val files = dir.listFiles { f -> f.isFile && f.name.endsWith(FILE_EXTENSION) }
        if (files.isNullOrEmpty()) {
            try {
                // 1. Balanced profile
                createDefaultProfile(
                    dir = dir,
                    id = "profile_balanced",
                    name = "Сбалансированный",
                    description = "Оптимальный баланс производительности, нагрева и качества графики",
                    booleans = mapOf(
                        BooleanSetting.RENDERER_USE_SPEED_LIMIT.key to true,
                        BooleanSetting.RENDERER_ASYNCHRONOUS_SHADERS.key to true,
                        BooleanSetting.RENDERER_REACTIVE_FLUSHING.key to true,
                        BooleanSetting.VULKAN_PIPELINE_CACHE.key to true,
                        BooleanSetting.VRAM_GARBAGE_COLLECTION.key to true,
                        BooleanSetting.FASTMEM.key to true,
                        BooleanSetting.FASTMEM_EXCLUSIVES.key to true,
                        BooleanSetting.USE_DOCKED_MODE.key to false,
                        BooleanSetting.AIRPLANE_MODE.key to false,
                        BooleanSetting.ECO_THERMAL_MODE.key to true,
                        BooleanSetting.ECO_FRAME_PACING.key to true,
                        BooleanSetting.SMART_SHADER_THROTTLE.key to true,
                        BooleanSetting.RENDERER_USE_DISK_SHADER_CACHE.key to true,
                        BooleanSetting.RENDERER_ASYNCHRONOUS_GPU_EMULATION.key to true
                    ),
                    integers = mapOf(
                        IntSetting.RENDERER_BACKEND.key to 1,
                        IntSetting.RENDERER_ACCURACY.key to 0,
                        IntSetting.RENDERER_RESOLUTION.key to 2,
                        IntSetting.RENDERER_SCALING_FILTER.key to 1,
                        IntSetting.RENDERER_NVDEC_EMULATION.key to 2,
                        IntSetting.RENDERER_ASTC_DECODE_METHOD.key to 1,
                        IntSetting.RENDERER_DYNA_STATE.key to 1,
                        IntSetting.RENDERER_VRAM_USAGE_MODE.key to 0,
                        IntSetting.RENDERER_VSYNC.key to 1,
                        IntSetting.CPU_ACCURACY.key to 0,
                        IntSetting.CPU_BACKEND.key to 0,
                        IntSetting.FAST_GPU_TIME.key to 1
                    ),
                    shorts = mapOf(ShortSetting.RENDERER_SPEED_LIMIT.key to 100)
                )

                // 2. Maximum Performance profile
                createDefaultProfile(
                    dir = dir,
                    id = "profile_performance",
                    name = "Максимальная производительность",
                    description = "Приоритет максимальной частоты кадров и снижения энергопотребления",
                    booleans = mapOf(
                        BooleanSetting.RENDERER_USE_SPEED_LIMIT.key to true,
                        BooleanSetting.RENDERER_ASYNCHRONOUS_SHADERS.key to true,
                        BooleanSetting.RENDERER_REACTIVE_FLUSHING.key to true,
                        BooleanSetting.VULKAN_PIPELINE_CACHE.key to true,
                        BooleanSetting.VRAM_GARBAGE_COLLECTION.key to true,
                        BooleanSetting.FASTMEM.key to true,
                        BooleanSetting.FASTMEM_EXCLUSIVES.key to true,
                        BooleanSetting.USE_DOCKED_MODE.key to false,
                        BooleanSetting.AIRPLANE_MODE.key to false,
                        BooleanSetting.ECO_THERMAL_MODE.key to true,
                        BooleanSetting.ECO_FRAME_PACING.key to true,
                        BooleanSetting.SMART_SHADER_THROTTLE.key to true,
                        BooleanSetting.RENDERER_USE_DISK_SHADER_CACHE.key to true,
                        BooleanSetting.RENDERER_FORCE_MAX_CLOCK.key to true,
                        BooleanSetting.RENDERER_ASYNCHRONOUS_GPU_EMULATION.key to true
                    ),
                    integers = mapOf(
                        IntSetting.RENDERER_BACKEND.key to 1,
                        IntSetting.RENDERER_ACCURACY.key to 0,
                        IntSetting.RENDERER_RESOLUTION.key to 1,
                        IntSetting.RENDERER_SCALING_FILTER.key to 0,
                        IntSetting.RENDERER_NVDEC_EMULATION.key to 2,
                        IntSetting.RENDERER_ASTC_DECODE_METHOD.key to 1,
                        IntSetting.RENDERER_DYNA_STATE.key to 1,
                        IntSetting.RENDERER_VRAM_USAGE_MODE.key to 1,
                        IntSetting.RENDERER_VSYNC.key to 0,
                        IntSetting.CPU_ACCURACY.key to 1,
                        IntSetting.CPU_BACKEND.key to 0,
                        IntSetting.FAST_GPU_TIME.key to 2,
                        IntSetting.FAST_CPU_TIME.key to 1
                    ),
                    shorts = mapOf(ShortSetting.RENDERER_SPEED_LIMIT.key to 100)
                )

                // 3. Maximum Quality profile
                createDefaultProfile(
                    dir = dir,
                    id = "profile_quality",
                    name = "Максимальное качество",
                    description = "Приоритет высокого разрешения, точности рендеринга и качества эффектов",
                    booleans = mapOf(
                        BooleanSetting.RENDERER_USE_SPEED_LIMIT.key to true,
                        BooleanSetting.RENDERER_ASYNCHRONOUS_SHADERS.key to true,
                        BooleanSetting.RENDERER_REACTIVE_FLUSHING.key to true,
                        BooleanSetting.VULKAN_PIPELINE_CACHE.key to true,
                        BooleanSetting.VRAM_GARBAGE_COLLECTION.key to false,
                        BooleanSetting.FASTMEM.key to true,
                        BooleanSetting.FASTMEM_EXCLUSIVES.key to true,
                        BooleanSetting.USE_DOCKED_MODE.key to true,
                        BooleanSetting.AIRPLANE_MODE.key to false,
                        BooleanSetting.ECO_THERMAL_MODE.key to false,
                        BooleanSetting.ECO_FRAME_PACING.key to false,
                        BooleanSetting.SMART_SHADER_THROTTLE.key to false,
                        BooleanSetting.RENDERER_USE_DISK_SHADER_CACHE.key to true,
                        BooleanSetting.RENDERER_ASYNCHRONOUS_GPU_EMULATION.key to true
                    ),
                    integers = mapOf(
                        IntSetting.RENDERER_BACKEND.key to 1,
                        IntSetting.RENDERER_ACCURACY.key to 1,
                        IntSetting.RENDERER_RESOLUTION.key to 2,
                        IntSetting.RENDERER_SCALING_FILTER.key to 1,
                        IntSetting.RENDERER_NVDEC_EMULATION.key to 2,
                        IntSetting.RENDERER_ASTC_DECODE_METHOD.key to 1,
                        IntSetting.RENDERER_DYNA_STATE.key to 1,
                        IntSetting.RENDERER_VRAM_USAGE_MODE.key to 0,
                        IntSetting.RENDERER_VSYNC.key to 1,
                        IntSetting.CPU_ACCURACY.key to 0,
                        IntSetting.CPU_BACKEND.key to 0,
                        IntSetting.FAST_GPU_TIME.key to 0
                    ),
                    shorts = mapOf(ShortSetting.RENDERER_SPEED_LIMIT.key to 100)
                )
            } catch (e: Exception) {
                Log.error("[SettingsProfileManager] Error ensuring default profiles: ${e.message}")
            }
        }
    }

    private fun createDefaultProfile(
        dir: File,
        id: String,
        name: String,
        description: String,
        booleans: Map<String, Boolean>,
        integers: Map<String, Int>,
        shorts: Map<String, Int>
    ) {
        val file = File(dir, "$id$FILE_EXTENSION")
        val json = JSONObject().apply {
            put("id", id)
            put("name", name)
            put("description", description)
            put("version", "7.3.2")
            put("timestamp", System.currentTimeMillis())

            val boolObj = JSONObject()
            for ((k, v) in booleans) boolObj.put(k, v)
            put("booleans", boolObj)

            val intObj = JSONObject()
            for ((k, v) in integers) intObj.put(k, v)
            put("integers", intObj)

            val shortObj = JSONObject()
            for ((k, v) in shorts) shortObj.put(k, v)
            put("shorts", shortObj)
        }
        file.writeText(json.toString(2))
    }

    fun getCustomProfiles(): List<SettingsProfile> {
        val dir = getProfilesDirectory()
        ensureDefaultProfiles(dir)
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
            json.put("version", "7.3.2")
            json.put("timestamp", System.currentTimeMillis())

            val needsGlobal = !NativeConfig.isPerGameConfigLoaded()

            val boolObj = JSONObject()
            for (setting in trackedBooleanSettings) {
                boolObj.put(setting.key, setting.getBoolean(needsGlobal))
            }
            json.put("booleans", boolObj)

            val intObj = JSONObject()
            for (setting in trackedIntSettings) {
                intObj.put(setting.key, setting.getInt(needsGlobal))
            }
            json.put("integers", intObj)

            val shortObj = JSONObject()
            for (setting in trackedShortSettings) {
                shortObj.put(setting.key, setting.getShort(needsGlobal).toInt())
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

            if (NativeConfig.isPerGameConfigLoaded()) {
                NativeConfig.savePerGameConfig()
            } else {
                NativeConfig.saveGlobalConfig()
            }
            if (org.yuzu.yuzu_emu.NativeLibrary.isRunning()) {
                org.yuzu.yuzu_emu.NativeLibrary.applySettings()
            }
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
                putExtra(Intent.EXTRA_SUBJECT, "STORM SWITCH Profile: ${profile.name}")
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