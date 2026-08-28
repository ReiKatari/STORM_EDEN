// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.fragments

import android.app.ActivityManager
import android.app.Dialog
import android.content.Context
import android.content.res.Configuration
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.os.Build
import android.os.Bundle
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.fragment.app.DialogFragment
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.databinding.DialogAutoOptimizationBinding
import org.yuzu.yuzu_emu.features.settings.model.BooleanSetting
import org.yuzu.yuzu_emu.features.settings.model.IntSetting
import org.yuzu.yuzu_emu.utils.GpuDriverHelper
import org.yuzu.yuzu_emu.utils.NativeConfig

class AutoOptimizationDialogFragment : DialogFragment() {

    private var _binding: DialogAutoOptimizationBinding? = null
    private val binding get() = _binding!!

    private var selectedMode = MODE_NORMAL

    companion object {
        const val TAG = "AutoOptimizationDialogFragment"

        const val MODE_FAST = 0
        const val MODE_NORMAL = 1
        const val MODE_ACCURATE = 2
        const val MODE_DEFAULT = 3

        fun newInstance(): AutoOptimizationDialogFragment {
            return AutoOptimizationDialogFragment()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setStyle(STYLE_NO_TITLE, 0)
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val dialog = super.onCreateDialog(savedInstanceState)
        dialog.requestWindowFeature(android.view.Window.FEATURE_NO_TITLE)
        dialog.window?.setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))
        return dialog
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = DialogAutoOptimizationBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        binding.btnCloseWizard.setOnClickListener {
            dismiss()
        }

        binding.btnCancelWizard.setOnClickListener {
            dismiss()
        }

        binding.btnResetDefaults.setOnClickListener {
            restoreDefaults()
        }

        detectAndDisplayHardwareInfo()
        setupModeSelectors()

        binding.btnApplyWizard.setOnClickListener {
            applyOptimization(selectedMode)
        }
    }

    override fun onStart() {
        super.onStart()
        dialog?.window?.let { window ->
            val dm = resources.displayMetrics
            val isLandscape = resources.configuration.orientation == Configuration.ORIENTATION_LANDSCAPE
            val width = if (isLandscape) (dm.widthPixels * 0.88).toInt().coerceIn(550, 1100) else (dm.widthPixels * 0.94).toInt()
            val height = if (isLandscape) (dm.heightPixels * 0.94).toInt().coerceIn(360, 900) else ViewGroup.LayoutParams.WRAP_CONTENT
            window.setLayout(width, height)
            window.setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))
            window.setGravity(Gravity.CENTER)
            val lp = window.attributes
            lp.width = width
            lp.height = height
            lp.gravity = Gravity.CENTER
            window.attributes = lp
        }
    }

    private fun detectAndDisplayHardwareInfo() {
        val context = requireContext()
        val am = context.getSystemService(Context.ACTIVITY_SERVICE) as? ActivityManager
        val memInfo = ActivityManager.MemoryInfo()
        am?.getMemoryInfo(memInfo)

        val totalRamGb = memInfo.totalMem / (1024.0 * 1024.0 * 1024.0)
        val availRamGb = memInfo.availMem / (1024.0 * 1024.0 * 1024.0)

        val soc = detectSoCName()
        val gpu = detectGpuName()
        val tier = calculateDeviceTier(totalRamGb)

        binding.textHwSoc.text = "SoC: $soc (${Runtime.getRuntime().availableProcessors()} ядер, ${Build.SUPPORTED_ABIS.firstOrNull() ?: "arm64-v8a"})"
        binding.textHwGpu.text = "GPU: $gpu"
        binding.textHwRam.text = String.format("RAM: %.1f ГБ (Доступно: %.1f ГБ)", totalRamGb, availRamGb)
        binding.textHwTier.text = "Класс устройства: $tier"
    }

    private fun detectSoCName(): String {
        val hardware = Build.HARDWARE.lowercase()
        val board = Build.BOARD.lowercase()
        val soc = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) Build.SOC_MODEL else ""

        if (soc.isNotBlank() && soc != Build.UNKNOWN) {
            return when {
                soc.contains("8750", ignoreCase = true) || soc.contains("sun", ignoreCase = true) -> "Qualcomm Snapdragon 8 Elite (SM8750)"
                soc.contains("8650", ignoreCase = true) || soc.contains("pineapple", ignoreCase = true) -> "Qualcomm Snapdragon 8 Gen 3 (SM8650)"
                soc.contains("8550", ignoreCase = true) || soc.contains("kalama", ignoreCase = true) -> "Qualcomm Snapdragon 8 Gen 2 (SM8550)"
                soc.contains("8450", ignoreCase = true) || soc.contains("taro", ignoreCase = true) -> "Qualcomm Snapdragon 8 Gen 1 (SM8450)"
                soc.contains("8475", ignoreCase = true) || soc.contains("cape", ignoreCase = true) -> "Qualcomm Snapdragon 8+ Gen 1 (SM8475)"
                soc.contains("8350", ignoreCase = true) || soc.contains("lahaina", ignoreCase = true) -> "Qualcomm Snapdragon 888 (SM8350)"
                soc.contains("8250", ignoreCase = true) || soc.contains("kona", ignoreCase = true) -> "Qualcomm Snapdragon 865 / 870 (SM8250)"
                soc.contains("9400", ignoreCase = true) || soc.contains("mt6991", ignoreCase = true) -> "MediaTek Dimensity 9400"
                soc.contains("9300", ignoreCase = true) || soc.contains("mt6989", ignoreCase = true) -> "MediaTek Dimensity 9300 / 9300+"
                soc.contains("2400", ignoreCase = true) || soc.contains("s5e9945", ignoreCase = true) -> "Samsung Exynos 2400 (Xclipse 940)"
                soc.contains("2200", ignoreCase = true) || soc.contains("s5e9925", ignoreCase = true) -> "Samsung Exynos 2200 (Xclipse 920)"
                soc.contains("g4", ignoreCase = true) || soc.contains("zuma", ignoreCase = true) -> "Google Tensor G4 / G3"
                else -> soc.uppercase()
            }
        }

        return when {
            hardware.contains("qcom") || board.contains("qualcomm") || hardware.contains("snapdragon") || hardware.contains("sun") -> {
                when {
                    hardware.contains("sun") || board.contains("sun") || hardware.contains("sm8750") -> "Snapdragon 8 Elite (SM8750)"
                    hardware.contains("sm8650") || board.contains("sm8650") -> "Snapdragon 8 Gen 3"
                    hardware.contains("sm8550") || board.contains("sm8550") -> "Snapdragon 8 Gen 2"
                    hardware.contains("sm8450") || board.contains("sm8450") || hardware.contains("sm8475") -> "Snapdragon 8 Gen 1 / 8+ Gen 1"
                    hardware.contains("sm8350") || board.contains("sm8350") -> "Snapdragon 888"
                    hardware.contains("sm8250") || board.contains("sm8250") -> "Snapdragon 865 / 870"
                    hardware.contains("sm8150") || board.contains("sm8150") -> "Snapdragon 855"
                    hardware.contains("sdm845") || board.contains("sdm845") -> "Snapdragon 845"
                    hardware.contains("sm7475") || board.contains("sm7475") -> "Snapdragon 7+ Gen 2"
                    hardware.contains("sm7675") || board.contains("sm7675") -> "Snapdragon 7+ Gen 3"
                    hardware.contains("sm7325") || board.contains("sm7325") -> "Snapdragon 778G"
                    else -> "Qualcomm Snapdragon (${Build.HARDWARE})"
                }
            }
            hardware.contains("mt") || board.contains("mediatek") || hardware.contains("dimensity") -> {
                when {
                    hardware.contains("mt6991") -> "MediaTek Dimensity 9400"
                    hardware.contains("mt6989") -> "MediaTek Dimensity 9300 / 9300+"
                    hardware.contains("mt6985") -> "MediaTek Dimensity 9200"
                    hardware.contains("mt6983") -> "MediaTek Dimensity 9000"
                    hardware.contains("mt6896") || hardware.contains("mt6897") -> "MediaTek Dimensity 8200 / 8300"
                    else -> "MediaTek Dimensity (${Build.HARDWARE})"
                }
            }
            hardware.contains("exynos") || board.contains("universal") || hardware.contains("s5e") -> {
                when {
                    hardware.contains("9945") || board.contains("9945") -> "Samsung Exynos 2400 (Xclipse 940)"
                    hardware.contains("9925") || board.contains("9925") -> "Samsung Exynos 2200 (Xclipse 920)"
                    else -> "Samsung Exynos (${Build.HARDWARE})"
                }
            }
            hardware.contains("tensor") || board.contains("gs") || board.contains("zuma") -> {
                "Google Tensor (${Build.HARDWARE})"
            }
            hardware.contains("kirin") || board.contains("hi") -> {
                "HiSilicon Kirin (${Build.HARDWARE})"
            }
            hardware.contains("tegra") || board.contains("tegra") -> {
                "NVIDIA Tegra (${Build.HARDWARE})"
            }
            else -> "${Build.MANUFACTURER} ${Build.MODEL}"
        }
    }

    private fun detectGpuName(): String {
        val hw = (Build.HARDWARE + " " + Build.BOARD + " " + Build.MODEL).lowercase()
        return if (GpuDriverHelper.isAdrenoGpu()) {
            when {
                hw.contains("sun") || hw.contains("8750") || hw.contains("s938") -> "Qualcomm Adreno 830 (Snapdragon 8 Elite / Turnip)"
                hw.contains("8650") -> "Qualcomm Adreno 750 (Snapdragon 8 Gen 3 / Turnip)"
                hw.contains("8550") -> "Qualcomm Adreno 740 (Snapdragon 8 Gen 2 / Turnip)"
                hw.contains("8450") || hw.contains("8475") -> "Qualcomm Adreno 730 (Snapdragon 8 Gen 1 / Turnip)"
                hw.contains("8350") -> "Qualcomm Adreno 660 (Snapdragon 888 / Turnip)"
                hw.contains("8250") -> "Qualcomm Adreno 650 (Snapdragon 865/870 / Turnip)"
                else -> "Qualcomm Adreno (Turnip Vulkan 1.3 / 1.4)"
            }
        } else {
            when {
                hw.contains("mt6991") || hw.contains("9400") -> "ARM Immortalis-G925 (Vulkan 1.3)"
                hw.contains("mt6989") || hw.contains("9300") -> "ARM Immortalis-G720 (Vulkan 1.3)"
                hw.contains("mt6985") || hw.contains("9200") -> "ARM Immortalis-G715 (Vulkan 1.3)"
                hw.contains("9945") || hw.contains("2400") -> "Samsung Xclipse 940 (AMD RDNA3 Vulkan)"
                hw.contains("9925") || hw.contains("2200") -> "Samsung Xclipse 920 (AMD RDNA2 Vulkan)"
                hw.contains("powervr") || hw.contains("img") || hw.contains("rogue") -> "PowerVR GPU (Vulkan)"
                hw.contains("tegra") || hw.contains("nvidia") -> "NVIDIA Tegra (Vulkan)"
                hw.contains("mt") || hw.contains("dimensity") || hw.contains("mali") || hw.contains("tensor") -> "ARM Mali (Vulkan Standard / PanVK)"
                else -> "Vulkan 1.3 Compatible GPU (${Build.HARDWARE})"
            }
        }
    }

    private fun calculateDeviceTier(totalRamGb: Double): String {
        return when {
            totalRamGb >= 11.0 -> "Флагман (Высокая производительность, 12GB+ RAM)"
            totalRamGb >= 7.0 -> "Продвинутый (Сбалансированная производительность, 8GB RAM)"
            totalRamGb >= 5.0 -> "Средний (6GB RAM)"
            else -> "Базовый (< 6GB RAM)"
        }
    }

    private fun setupModeSelectors() {
        binding.cardModeFast.setOnClickListener { selectMode(MODE_FAST) }
        binding.cardModeNormal.setOnClickListener { selectMode(MODE_NORMAL) }
        binding.cardModeAccurate.setOnClickListener { selectMode(MODE_ACCURATE) }
        binding.cardModeDefault.setOnClickListener { selectMode(MODE_DEFAULT) }
        val prefs = androidx.preference.PreferenceManager.getDefaultSharedPreferences(requireContext())
        val savedMode = prefs.getInt("selected_auto_optimization_mode", MODE_NORMAL)
        selectMode(savedMode)
    }

    private fun selectMode(mode: Int) {
        selectedMode = mode

        binding.radioModeFast.isChecked = (mode == MODE_FAST)
        binding.radioModeNormal.isChecked = (mode == MODE_NORMAL)
        binding.radioModeAccurate.isChecked = (mode == MODE_ACCURATE)
        binding.radioModeDefault.isChecked = (mode == MODE_DEFAULT)

        val primaryColor = Color.parseColor("#00F0FF")
        val outlineColor = Color.parseColor("#374151")

        binding.cardModeFast.strokeColor = if (mode == MODE_FAST) primaryColor else outlineColor
        binding.cardModeFast.strokeWidth = if (mode == MODE_FAST) 4 else 2

        binding.cardModeNormal.strokeColor = if (mode == MODE_NORMAL) primaryColor else outlineColor
        binding.cardModeNormal.strokeWidth = if (mode == MODE_NORMAL) 4 else 2

        binding.cardModeAccurate.strokeColor = if (mode == MODE_ACCURATE) primaryColor else outlineColor
        binding.cardModeAccurate.strokeWidth = if (mode == MODE_ACCURATE) 4 else 2

        binding.cardModeDefault.strokeColor = if (mode == MODE_DEFAULT) primaryColor else outlineColor
        binding.cardModeDefault.strokeWidth = if (mode == MODE_DEFAULT) 4 else 2
    }

    private fun restoreDefaults() {
        val context = requireContext()
        val prefs = androidx.preference.PreferenceManager.getDefaultSharedPreferences(context)
        prefs.edit().putInt("selected_auto_optimization_mode", MODE_DEFAULT).apply()

        // 1. Renderer / Video Defaults
        IntSetting.RENDERER_BACKEND.setInt(1) // Vulkan (1 = Vulkan, 2 = Null)
        IntSetting.RENDERER_ACCURACY.setInt(0) // Normal
        IntSetting.RENDERER_RESOLUTION.setInt(3) // 1.0X (720p/1080p)
        IntSetting.RENDERER_VSYNC.setInt(0) // FIFO
        IntSetting.RENDERER_ASPECT_RATIO.setInt(0) // 16:9
        IntSetting.RENDERER_ANTI_ALIASING.setInt(0) // None
        IntSetting.RENDERER_SCALING_FILTER.setInt(0) // Nearest / Bilinear
        IntSetting.RENDERER_ASTC_DECODE_METHOD.setInt(0) // CPU / Default
        IntSetting.ASTC_RECOMPRESSION.setInt(0) // Uncompressed
        IntSetting.RENDERER_NVDEC_EMULATION.setInt(2) // GPU
        IntSetting.DMA_ACCURACY.setInt(1) // Normal
        IntSetting.RENDERER_VRAM_USAGE_MODE.setInt(1) // Normal
        IntSetting.GPU_FENCE_BEHAVIOR.setInt(1) // Balanced
        IntSetting.MAX_ANISOTROPY.setInt(0) // Automatic
        IntSetting.RENDERER_DYNA_STATE.setInt(0) // Default

        // 2. CPU / System Defaults
        BooleanSetting.USE_DOCKED_MODE.setBoolean(false) // Handheld for speed/efficiency
        IntSetting.CPU_BACKEND.setInt(1) // NCE
        IntSetting.CPU_ACCURACY.setInt(0) // Auto
        IntSetting.MEMORY_LAYOUT.setInt(0) // 4GB
        val cpuCores = Runtime.getRuntime().availableProcessors()
        IntSetting.ANDROID_PIPELINE_WORKERS.setInt((cpuCores - 2).coerceIn(2, 4))

        // 3. Audio Defaults
        IntSetting.AUDIO_OUTPUT_ENGINE.setInt(0) // Auto
        BooleanSetting.AUDIO_MUTED.setBoolean(false)

        // 4. Renderer Booleans
        BooleanSetting.RENDERER_ASYNCHRONOUS_GPU_EMULATION.setBoolean(true)
        BooleanSetting.RENDERER_ASYNC_PRESENTATION.setBoolean(false)
        BooleanSetting.RENDERER_USE_DISK_SHADER_CACHE.setBoolean(true)
        BooleanSetting.FASTMEM.setBoolean(true)
        BooleanSetting.RENDERER_REACTIVE_FLUSHING.setBoolean(false)
        BooleanSetting.SYNC_MEMORY_OPERATIONS.setBoolean(false)
        BooleanSetting.RENDERER_ASYNCHRONOUS_SHADERS.setBoolean(true)
        BooleanSetting.SKIP_CPU_INNER_INVALIDATION.setBoolean(false)
        BooleanSetting.RENDERER_FORCE_MAX_CLOCK.setBoolean(false)
        BooleanSetting.ENABLE_BUFFER_HISTORY.setBoolean(false)
        BooleanSetting.ENABLE_GPU_BUFFER_READBACK.setBoolean(false)
        BooleanSetting.RENDERER_VERTEX_INPUT_DYNAMIC_STATE.setBoolean(true)

        // 5. Auto-Optimization Engine Flags
        BooleanSetting.ECO_THERMAL_MODE.setBoolean(false)
        BooleanSetting.ECO_FRAME_PACING.setBoolean(false)
        BooleanSetting.SMART_SHADER_THROTTLE.setBoolean(false)
        BooleanSetting.CPU_AFFINITY_PINNING.setBoolean(false)
        BooleanSetting.VULKAN_PIPELINE_CACHE.setBoolean(true)
        BooleanSetting.VRAM_GARBAGE_COLLECTION.setBoolean(false)

        try {
            NativeConfig.saveGlobalConfig()
        } catch (e: Exception) {
            e.printStackTrace()
        }

        Toast.makeText(
            context,
            getString(R.string.auto_optimization_defaults_restored_toast),
            Toast.LENGTH_LONG
        ).show()

        dismiss()
    }

    private fun applyOptimization(mode: Int) {
        if (mode == MODE_DEFAULT) {
            restoreDefaults()
            return
        }

        val context = requireContext()
        val prefs = androidx.preference.PreferenceManager.getDefaultSharedPreferences(context)
        prefs.edit().putInt("selected_auto_optimization_mode", mode).apply()

        val am = context.getSystemService(Context.ACTIVITY_SERVICE) as? ActivityManager
        val memInfo = ActivityManager.MemoryInfo()
        am?.getMemoryInfo(memInfo)
        val totalRamGb = memInfo.totalMem / (1024.0 * 1024.0 * 1024.0)

        val isAdreno = GpuDriverHelper.isAdrenoGpu()
        val isFlagship = totalRamGb >= 11.0
        val isMidRange = totalRamGb in 6.0..10.9
        val isLowEnd = totalRamGb < 6.0
        val cpuCores = Runtime.getRuntime().availableProcessors()
        val optimalWorkers = (cpuCores - 2).coerceIn(2, 6)

        when (mode) {
            MODE_FAST -> {
                // Resolution: 0.75X (2) for budget / Mali, 1X (3) for Flagship Adreno
                IntSetting.RENDERER_RESOLUTION.setInt(if (isFlagship && isAdreno) 3 else 2)
                // GPU Accuracy: Normal (0)
                IntSetting.RENDERER_ACCURACY.setInt(0)
                // DMA Accuracy: Normal (1)
                IntSetting.DMA_ACCURACY.setInt(1)
                // VRAM Usage Mode: Conservative (0)
                IntSetting.RENDERER_VRAM_USAGE_MODE.setInt(0)
                // GPU Fence Behavior: Fast (2)
                IntSetting.GPU_FENCE_BEHAVIOR.setInt(2)
                // Anti-Aliasing: None (0)
                IntSetting.RENDERER_ANTI_ALIASING.setInt(0)
                // Scaling Filter: AMD FSR (6)
                IntSetting.RENDERER_SCALING_FILTER.setInt(6)
                IntSetting.FSR_SHARPENING_SLIDER.setInt(80)
                // ASTC: GPU (1) for Adreno/Flagship, CPU (0) for budget Mali
                IntSetting.RENDERER_ASTC_DECODE_METHOD.setInt(if (isLowEnd && !isAdreno) 0 else 1)
                // ASTC Recompression: BC1 (1) for Max RAM savings
                IntSetting.ASTC_RECOMPRESSION.setInt(1)
                // NVDEC: GPU (2)
                IntSetting.RENDERER_NVDEC_EMULATION.setInt(2)
                // Anisotropy: Default (0)
                IntSetting.MAX_ANISOTROPY.setInt(0)
                // CPU Backend: NCE (1)
                IntSetting.CPU_BACKEND.setInt(1)
                IntSetting.CPU_ACCURACY.setInt(0)
                // Memory Layout: 4GB (0)
                IntSetting.MEMORY_LAYOUT.setInt(0)
                // System / Docked: Handheld (false)
                BooleanSetting.USE_DOCKED_MODE.setBoolean(false)
                // Audio: Auto engine, unmuted
                IntSetting.AUDIO_OUTPUT_ENGINE.setInt(0)
                BooleanSetting.AUDIO_MUTED.setBoolean(false)
                // Dynamic State (Extended Dynamic State): Enabled (1) for Adreno/Qualcomm for games like LEGO Star Wars
                IntSetting.RENDERER_DYNA_STATE.setInt(if (isAdreno) 1 else 0)
                // Pipeline Workers: 2 (reduces background CPU load, battery drain and heat)
                IntSetting.ANDROID_PIPELINE_WORKERS.setInt(2)

                // Booleans
                BooleanSetting.RENDERER_ASYNCHRONOUS_GPU_EMULATION.setBoolean(true)
                BooleanSetting.RENDERER_ASYNC_PRESENTATION.setBoolean(true)
                BooleanSetting.RENDERER_USE_DISK_SHADER_CACHE.setBoolean(true)
                BooleanSetting.FASTMEM.setBoolean(true)
                BooleanSetting.RENDERER_REACTIVE_FLUSHING.setBoolean(false)
                BooleanSetting.SYNC_MEMORY_OPERATIONS.setBoolean(false)
                BooleanSetting.RENDERER_ASYNCHRONOUS_SHADERS.setBoolean(true)
                BooleanSetting.SKIP_CPU_INNER_INVALIDATION.setBoolean(true)
                BooleanSetting.RENDERER_FORCE_MAX_CLOCK.setBoolean(false)
                BooleanSetting.ENABLE_BUFFER_HISTORY.setBoolean(false)
                BooleanSetting.ENABLE_GPU_BUFFER_READBACK.setBoolean(false)
                BooleanSetting.RENDERER_VERTEX_INPUT_DYNAMIC_STATE.setBoolean(true)
                BooleanSetting.ECO_THERMAL_MODE.setBoolean(true)
                BooleanSetting.ECO_FRAME_PACING.setBoolean(true)
                BooleanSetting.SMART_SHADER_THROTTLE.setBoolean(true)
                BooleanSetting.CPU_AFFINITY_PINNING.setBoolean(true)
                BooleanSetting.VULKAN_PIPELINE_CACHE.setBoolean(true)
                BooleanSetting.VRAM_GARBAGE_COLLECTION.setBoolean(true)
            }

            MODE_NORMAL -> {
                // Resolution: 1X (3) standard
                IntSetting.RENDERER_RESOLUTION.setInt(if (isLowEnd && !isAdreno) 2 else 3)
                // GPU Accuracy: Normal (0)
                IntSetting.RENDERER_ACCURACY.setInt(0)
                // DMA Accuracy: Normal (1)
                IntSetting.DMA_ACCURACY.setInt(1)
                // VRAM Usage Mode: Normal (1)
                IntSetting.RENDERER_VRAM_USAGE_MODE.setInt(if (isLowEnd) 0 else 1)
                // GPU Fence Behavior: Balanced (1)
                IntSetting.GPU_FENCE_BEHAVIOR.setInt(1)
                // Anti-Aliasing: FXAA (1)
                IntSetting.RENDERER_ANTI_ALIASING.setInt(1)
                // Scaling Filter: AMD FSR (6)
                IntSetting.RENDERER_SCALING_FILTER.setInt(6)
                IntSetting.FSR_SHARPENING_SLIDER.setInt(85)
                // ASTC: GPU (1)
                IntSetting.RENDERER_ASTC_DECODE_METHOD.setInt(1)
                // ASTC Recompression: BC3 (2) for balanced RAM reduction
                IntSetting.ASTC_RECOMPRESSION.setInt(if (isLowEnd) 1 else 2)
                // NVDEC: GPU (2)
                IntSetting.RENDERER_NVDEC_EMULATION.setInt(2)
                // Anisotropy: Default (0)
                IntSetting.MAX_ANISOTROPY.setInt(0)
                // CPU Backend: NCE (1)
                IntSetting.CPU_BACKEND.setInt(1)
                IntSetting.CPU_ACCURACY.setInt(0)
                // Memory Layout: 6GB (1) if RAM >= 11GB, else 4GB (0)
                IntSetting.MEMORY_LAYOUT.setInt(if (isFlagship) 1 else 0)
                // System / Docked: Handheld (false) for speed
                BooleanSetting.USE_DOCKED_MODE.setBoolean(false)
                // Audio: Auto engine, unmuted
                IntSetting.AUDIO_OUTPUT_ENGINE.setInt(0)
                BooleanSetting.AUDIO_MUTED.setBoolean(false)
                // Dynamic State: Enabled (1) for Adreno
                IntSetting.RENDERER_DYNA_STATE.setInt(if (isAdreno) 1 else 0)
                // Pipeline Workers
                IntSetting.ANDROID_PIPELINE_WORKERS.setInt(optimalWorkers.coerceAtMost(3))

                // Booleans
                BooleanSetting.RENDERER_ASYNCHRONOUS_GPU_EMULATION.setBoolean(true)
                BooleanSetting.RENDERER_ASYNC_PRESENTATION.setBoolean(true)
                BooleanSetting.RENDERER_USE_DISK_SHADER_CACHE.setBoolean(true)
                BooleanSetting.FASTMEM.setBoolean(true)
                BooleanSetting.RENDERER_REACTIVE_FLUSHING.setBoolean(false)
                BooleanSetting.SYNC_MEMORY_OPERATIONS.setBoolean(false)
                BooleanSetting.RENDERER_ASYNCHRONOUS_SHADERS.setBoolean(true)
                BooleanSetting.SKIP_CPU_INNER_INVALIDATION.setBoolean(true)
                BooleanSetting.RENDERER_FORCE_MAX_CLOCK.setBoolean(false)
                BooleanSetting.ENABLE_BUFFER_HISTORY.setBoolean(false)
                BooleanSetting.ENABLE_GPU_BUFFER_READBACK.setBoolean(false)
                BooleanSetting.RENDERER_VERTEX_INPUT_DYNAMIC_STATE.setBoolean(true)
                BooleanSetting.ECO_THERMAL_MODE.setBoolean(true)
                BooleanSetting.ECO_FRAME_PACING.setBoolean(true)
                BooleanSetting.SMART_SHADER_THROTTLE.setBoolean(true)
                BooleanSetting.CPU_AFFINITY_PINNING.setBoolean(true)
                BooleanSetting.VULKAN_PIPELINE_CACHE.setBoolean(true)
                BooleanSetting.VRAM_GARBAGE_COLLECTION.setBoolean(true)
            }

            MODE_ACCURATE -> {
                // Resolution: 1.5X (5) for Flagship Adreno, 1X (3) for Mid-Range
                IntSetting.RENDERER_RESOLUTION.setInt(if (isFlagship && isAdreno) 5 else 3)
                // GPU Accuracy: High (1)
                IntSetting.RENDERER_ACCURACY.setInt(1)
                // DMA Accuracy: Safe (3)
                IntSetting.DMA_ACCURACY.setInt(3)
                // VRAM Usage Mode: Aggressive (2) for Flagship, Normal (1) for others
                IntSetting.RENDERER_VRAM_USAGE_MODE.setInt(if (isFlagship) 2 else 1)
                // GPU Fence Behavior: Strict (0)
                IntSetting.GPU_FENCE_BEHAVIOR.setInt(0)
                // Anti-Aliasing: SMAA (2)
                IntSetting.RENDERER_ANTI_ALIASING.setInt(2)
                // Scaling Filter: AMD FSR (6)
                IntSetting.RENDERER_SCALING_FILTER.setInt(6)
                IntSetting.FSR_SHARPENING_SLIDER.setInt(90)
                // ASTC: GPU (1)
                IntSetting.RENDERER_ASTC_DECODE_METHOD.setInt(1)
                // ASTC Recompression: Uncompressed (0) for Flagship with 12GB+, BC3 (2) for 6-8GB
                IntSetting.ASTC_RECOMPRESSION.setInt(if (isFlagship) 0 else 2)
                // NVDEC: GPU (2)
                IntSetting.RENDERER_NVDEC_EMULATION.setInt(2)
                // Anisotropy: 16x (4) for Flagship/Mid, 4x (2) for Low
                IntSetting.MAX_ANISOTROPY.setInt(if (isFlagship) 4 else 2)
                // CPU Backend: NCE (1)
                IntSetting.CPU_BACKEND.setInt(1)
                IntSetting.CPU_ACCURACY.setInt(1) // Accurate
                // Memory Layout: 6GB/8GB if RAM >= 11GB
                IntSetting.MEMORY_LAYOUT.setInt(if (isFlagship) 2 else 0)
                // System / Docked: Docked for Flagship, Handheld for others
                BooleanSetting.USE_DOCKED_MODE.setBoolean(isFlagship)
                // Audio: Auto engine, unmuted
                IntSetting.AUDIO_OUTPUT_ENGINE.setInt(0)
                BooleanSetting.AUDIO_MUTED.setBoolean(false)
                // Dynamic State: Enabled (1)
                IntSetting.RENDERER_DYNA_STATE.setInt(1)
                // Pipeline Workers
                IntSetting.ANDROID_PIPELINE_WORKERS.setInt(optimalWorkers.coerceIn(3, 4))

                // Booleans
                BooleanSetting.RENDERER_ASYNCHRONOUS_GPU_EMULATION.setBoolean(true)
                BooleanSetting.RENDERER_ASYNC_PRESENTATION.setBoolean(true)
                BooleanSetting.RENDERER_USE_DISK_SHADER_CACHE.setBoolean(true)
                BooleanSetting.FASTMEM.setBoolean(true)
                BooleanSetting.RENDERER_REACTIVE_FLUSHING.setBoolean(true)
                BooleanSetting.SYNC_MEMORY_OPERATIONS.setBoolean(true)
                BooleanSetting.RENDERER_ASYNCHRONOUS_SHADERS.setBoolean(true)
                BooleanSetting.SKIP_CPU_INNER_INVALIDATION.setBoolean(false)
                BooleanSetting.RENDERER_FORCE_MAX_CLOCK.setBoolean(false)
                BooleanSetting.ENABLE_BUFFER_HISTORY.setBoolean(true)
                BooleanSetting.ENABLE_GPU_BUFFER_READBACK.setBoolean(false)
                BooleanSetting.RENDERER_VERTEX_INPUT_DYNAMIC_STATE.setBoolean(true)
                BooleanSetting.ECO_THERMAL_MODE.setBoolean(true)
                BooleanSetting.ECO_FRAME_PACING.setBoolean(true)
                BooleanSetting.SMART_SHADER_THROTTLE.setBoolean(true)
                BooleanSetting.CPU_AFFINITY_PINNING.setBoolean(true)
                BooleanSetting.VULKAN_PIPELINE_CACHE.setBoolean(true)
                BooleanSetting.VRAM_GARBAGE_COLLECTION.setBoolean(true)
            }
        }

        try {
            NativeConfig.saveGlobalConfig()
        } catch (e: Exception) {
            e.printStackTrace()
        }

        val modeName = when (mode) {
            MODE_FAST -> getString(R.string.auto_optimization_mode_fast)
            MODE_ACCURATE -> getString(R.string.auto_optimization_mode_accurate)
            else -> getString(R.string.auto_optimization_mode_normal)
        }

        Toast.makeText(
            context,
            getString(R.string.auto_optimization_applied_toast, modeName),
            Toast.LENGTH_LONG
        ).show()

        dismiss()
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}
