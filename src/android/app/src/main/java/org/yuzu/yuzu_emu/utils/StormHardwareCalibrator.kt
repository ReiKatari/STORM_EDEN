// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.utils

import android.app.ActivityManager
import android.content.Context
import android.os.Build
import androidx.preference.PreferenceManager
import org.yuzu.yuzu_emu.features.settings.model.BooleanSetting
import org.yuzu.yuzu_emu.features.settings.model.IntSetting

/**
 * Intelligent hardware detection and preset calibration engine.
 * Automatically scans SoC, GPU, and RAM capacity on first launch or reset,
 * configuring optimal settings for performance and stability.
 */
object StormHardwareCalibrator {

    private const val PREF_HARDWARE_CALIBRATED = "storm_hardware_calibrated"

    enum class HardwareTier {
        FLAGSHIP,
        MIDRANGE,
        BUDGET
    }

    enum class StormPreset {
        DEFAULT,
        FAST,
        NORMAL,
        ACCURATE
    }

    data class DeviceHardwareProfile(
        val tier: HardwareTier,
        val socName: String,
        val gpuName: String,
        val totalRamGb: Double,
        val isAdreno: Boolean,
        val isDimensity9400: Boolean
    )

    fun detectHardware(context: Context): DeviceHardwareProfile {
        val am = context.getSystemService(Context.ACTIVITY_SERVICE) as? ActivityManager
        val memInfo = ActivityManager.MemoryInfo()
        am?.getMemoryInfo(memInfo)
        val totalRamGb = memInfo.totalMem / (1024.0 * 1024.0 * 1024.0)

        val hw = (Build.HARDWARE + " " + Build.BOARD + " " + Build.MODEL + " " + Build.MANUFACTURER).lowercase()
        val isAdreno = GpuDriverHelper.isAdrenoGpu()
        val isDimensity9400 = hw.contains("mt6991") || hw.contains("9400") || hw.contains("x200") || hw.contains("immortalis-g925")

        val tier = when {
            totalRamGb >= 11.0 || hw.contains("8750") || hw.contains("sun") || hw.contains("8650") || isDimensity9400 -> HardwareTier.FLAGSHIP
            totalRamGb >= 6.0 || hw.contains("8550") || hw.contains("8450") || hw.contains("8475") || hw.contains("9300") -> HardwareTier.MIDRANGE
            else -> HardwareTier.BUDGET
        }

        val socName = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S && Build.SOC_MODEL.isNotEmpty()) {
            Build.SOC_MODEL
        } else {
            Build.HARDWARE
        }

        val gpuName = if (isAdreno) {
            when {
                hw.contains("sun") || hw.contains("8750") || hw.contains("s938") -> "Adreno 830 (Snapdragon 8 Elite)"
                hw.contains("8650") -> "Adreno 750 (Snapdragon 8 Gen 3)"
                hw.contains("8550") -> "Adreno 740 (Snapdragon 8 Gen 2)"
                hw.contains("8450") || hw.contains("8475") -> "Adreno 730 (Snapdragon 8 Gen 1)"
                else -> "Qualcomm Adreno"
            }
        } else {
            when {
                isDimensity9400 -> "ARM Immortalis-G925 (Dimensity 9400)"
                hw.contains("mt6989") || hw.contains("9300") -> "ARM Immortalis-G720 (Dimensity 9300)"
                hw.contains("2400") || hw.contains("9945") -> "Samsung Xclipse 940 (AMD RDNA3)"
                else -> "ARM Mali / Mobile GPU"
            }
        }

        return DeviceHardwareProfile(
            tier = tier,
            socName = socName,
            gpuName = gpuName,
            totalRamGb = totalRamGb,
            isAdreno = isAdreno,
            isDimensity9400 = isDimensity9400
        )
    }

    /**
     * Calibrate on first startup or when explicitly requested.
     */
    fun autoCalibrate(context: Context, force: Boolean = false) {
        val prefs = PreferenceManager.getDefaultSharedPreferences(context)
        if (!force && prefs.getBoolean(PREF_HARDWARE_CALIBRATED, false)) {
            return
        }

        applyPreset(StormPreset.NORMAL, context)
        prefs.edit().putBoolean(PREF_HARDWARE_CALIBRATED, true).apply()
    }

    /**
     * Apply a specific preset based on detected hardware profile.
     */
    fun applyPreset(preset: StormPreset, context: Context) {
        val profile = detectHardware(context)

        when (preset) {
            StormPreset.DEFAULT -> applyDefaultPreset(profile)
            StormPreset.FAST -> applyFastPreset(profile)
            StormPreset.NORMAL -> applyNormalPreset(profile)
            StormPreset.ACCURATE -> applyAccuratePreset(profile)
        }

        try {
            NativeConfig.saveGlobalConfig()
        } catch (_: Exception) {
        }
    }

    private fun applyDefaultPreset(profile: DeviceHardwareProfile) {
        IntSetting.RENDERER_BACKEND.setInt(1) // Vulkan
        IntSetting.RENDERER_ACCURACY.setInt(0) // Normal
        IntSetting.RENDERER_RESOLUTION.setInt(if (profile.tier == HardwareTier.BUDGET) 2 else 3) // 0.75x or 1.0x
        IntSetting.RENDERER_VSYNC.setInt(0) // FIFO
        IntSetting.RENDERER_ASTC_DECODE_METHOD.setInt(if (profile.tier == HardwareTier.BUDGET && !profile.isAdreno) 0 else 1)
        IntSetting.RENDERER_VRAM_USAGE_MODE.setInt(if (profile.tier == HardwareTier.FLAGSHIP) 1 else 0)
        IntSetting.RENDERER_NVDEC_EMULATION.setInt(2) // GPU
        IntSetting.DMA_ACCURACY.setInt(1)

        BooleanSetting.USE_DOCKED_MODE.setBoolean(profile.tier == HardwareTier.FLAGSHIP)
        BooleanSetting.RENDERER_ASYNCHRONOUS_GPU_EMULATION.setBoolean(true)
        BooleanSetting.RENDERER_USE_DISK_SHADER_CACHE.setBoolean(true)
        BooleanSetting.FASTMEM.setBoolean(true)
        BooleanSetting.FASTMEM_EXCLUSIVES.setBoolean(profile.tier != HardwareTier.BUDGET)
        BooleanSetting.RENDERER_ASYNCHRONOUS_SHADERS.setBoolean(true)
        BooleanSetting.RENDERER_FRAME_GEN_FP16.setBoolean(true)
        BooleanSetting.RENDERER_FRAME_GEN_FLOW_SCALE_AUTO.setBoolean(true)
        BooleanSetting.SKIP_CPU_INNER_INVALIDATION.setBoolean(profile.tier == HardwareTier.BUDGET)
    }

    private fun applyFastPreset(profile: DeviceHardwareProfile) {
        IntSetting.RENDERER_BACKEND.setInt(1)
        IntSetting.RENDERER_RESOLUTION.setInt(if (profile.tier == HardwareTier.FLAGSHIP) 3 else if (profile.tier == HardwareTier.MIDRANGE) 2 else 1)
        IntSetting.RENDERER_ACCURACY.setInt(0)
        IntSetting.RENDERER_VRAM_USAGE_MODE.setInt(if (profile.tier == HardwareTier.BUDGET) 2 else 0)
        IntSetting.GPU_FENCE_BEHAVIOR.setInt(2) // Fast

        BooleanSetting.USE_DOCKED_MODE.setBoolean(false)
        BooleanSetting.RENDERER_ASYNCHRONOUS_GPU_EMULATION.setBoolean(true)
        BooleanSetting.FASTMEM.setBoolean(true)
        BooleanSetting.FASTMEM_EXCLUSIVES.setBoolean(true)
        BooleanSetting.RENDERER_ASYNC_PRESENTATION.setBoolean(true)
        BooleanSetting.RENDERER_FRAME_GEN.setBoolean(profile.tier != HardwareTier.BUDGET)
        IntSetting.RENDERER_FRAME_GEN_MULTIPLIER.setInt(2)
        BooleanSetting.RENDERER_FRAME_GEN_FP16.setBoolean(true)
        BooleanSetting.RENDERER_FRAME_GEN_FLOW_SCALE_AUTO.setBoolean(true)
        BooleanSetting.SKIP_CPU_INNER_INVALIDATION.setBoolean(profile.tier != HardwareTier.FLAGSHIP)
    }

    private fun applyNormalPreset(profile: DeviceHardwareProfile) {
        IntSetting.RENDERER_BACKEND.setInt(1)
        IntSetting.RENDERER_RESOLUTION.setInt(if (profile.tier == HardwareTier.BUDGET || profile.isDimensity9400) 2 else 3)
        IntSetting.RENDERER_ACCURACY.setInt(0)
        IntSetting.RENDERER_VSYNC.setInt(if (profile.tier == HardwareTier.FLAGSHIP && !profile.isDimensity9400) 2 else 0) // Mailbox on Flagship, FIFO on others
        IntSetting.RENDERER_VRAM_USAGE_MODE.setInt(if (profile.tier == HardwareTier.BUDGET) 0 else 1)
        IntSetting.RENDERER_ASTC_DECODE_METHOD.setInt(1) // GPU
        IntSetting.CPU_ACCURACY.setInt(0) // Auto

        BooleanSetting.USE_DOCKED_MODE.setBoolean(false)
        BooleanSetting.RENDERER_ASYNCHRONOUS_GPU_EMULATION.setBoolean(true)
        BooleanSetting.RENDERER_USE_DISK_SHADER_CACHE.setBoolean(true)
        BooleanSetting.FASTMEM.setBoolean(true)
        BooleanSetting.RENDERER_ASYNCHRONOUS_SHADERS.setBoolean(true)
        BooleanSetting.RENDERER_FRAME_GEN_FP16.setBoolean(true)
        BooleanSetting.RENDERER_FRAME_GEN_FLOW_SCALE_AUTO.setBoolean(true)
        BooleanSetting.SKIP_CPU_INNER_INVALIDATION.setBoolean(false)
        BooleanSetting.SMART_SHADER_THROTTLE.setBoolean(profile.isDimensity9400 || profile.tier == HardwareTier.FLAGSHIP)
    }

    private fun applyAccuratePreset(profile: DeviceHardwareProfile) {
        IntSetting.RENDERER_BACKEND.setInt(1)
        IntSetting.RENDERER_RESOLUTION.setInt(if (profile.tier == HardwareTier.FLAGSHIP && profile.isAdreno) 5 else 3) // 1.5x or 1.0x
        IntSetting.RENDERER_ACCURACY.setInt(if (profile.tier == HardwareTier.FLAGSHIP && profile.isAdreno) 2 else 1) // Extreme / High
        IntSetting.RENDERER_VRAM_USAGE_MODE.setInt(if (profile.tier == HardwareTier.FLAGSHIP) 2 else 1)
        IntSetting.DMA_ACCURACY.setInt(3) // Safe
        IntSetting.GPU_FENCE_BEHAVIOR.setInt(0) // Strict
        IntSetting.RENDERER_ASTC_DECODE_METHOD.setInt(if (profile.tier == HardwareTier.FLAGSHIP) 0 else 1) // CPU / GPU

        BooleanSetting.USE_DOCKED_MODE.setBoolean(profile.tier == HardwareTier.FLAGSHIP && !profile.isDimensity9400)
        BooleanSetting.SYNC_MEMORY_OPERATIONS.setBoolean(true)
        BooleanSetting.RENDERER_REACTIVE_FLUSHING.setBoolean(true)
        BooleanSetting.FASTMEM.setBoolean(true)
        BooleanSetting.RENDERER_ASYNCHRONOUS_SHADERS.setBoolean(false)
    }
}
