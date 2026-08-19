// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.fragments

import android.app.ActivityManager
import android.app.Dialog
import android.content.Context
import android.os.Build
import android.os.Bundle
import androidx.fragment.app.DialogFragment
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.databinding.DialogSystemInfoBinding

class SystemInfoDialogFragment : DialogFragment() {
    private var _binding: DialogSystemInfoBinding? = null
    private val binding get() = _binding!!

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        _binding = DialogSystemInfoBinding.inflate(layoutInflater)

        populateSystemInfo()

        binding.btnCloseDialog.setOnClickListener {
            dismiss()
        }

        binding.btnCopyInfo.setOnClickListener {
            copyDiagnosticsToClipboard()
        }

        return MaterialAlertDialogBuilder(requireContext(), R.style.EdenMaterialDialog)
            .setView(binding.root)
            .create()
    }

    override fun onStart() {
        super.onStart()
        dialog?.window?.let { window ->
            val dm = resources.displayMetrics
            val isLandscape = resources.configuration.orientation == android.content.res.Configuration.ORIENTATION_LANDSCAPE
            val width = if (isLandscape) (dm.widthPixels * 0.70).toInt() else (dm.widthPixels * 0.92).toInt()
            val height = if (isLandscape) (dm.heightPixels * 0.90).toInt() else (dm.heightPixels * 0.85).toInt()
            window.setLayout(width, height)
            window.setBackgroundDrawableResource(android.R.color.transparent)
        }
    }

    private fun populateSystemInfo() {
        binding.textAppBuild.text = "STORM EDEN ${NativeLibrary.getBuildVersion()}"

        // 1. Device Info
        val deviceInfo = buildString {
            append("• Производитель: ").append(Build.MANUFACTURER).append("\n")
            append("• Модель: ").append(Build.MODEL).append(" (").append(Build.DEVICE).append(")\n")
            append("• Продукт: ").append(Build.PRODUCT).append("\n")
            append("• Архитектура ABI: ").append(Build.SUPPORTED_ABIS.joinToString(", ")).append("\n")
            append("• Версия Android: ").append(Build.VERSION.RELEASE).append(" (API ").append(Build.VERSION.SDK_INT).append(")\n")
            append("• Патч безопасности: ").append(Build.VERSION.SECURITY_PATCH).append("\n")
            append("• Номер сборки: ").append(Build.ID)
        }
        binding.textDeviceInfo.text = deviceInfo

        // 2. CPU / SoC Info
        val cpuInfo = buildString {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S && Build.SOC_MODEL.isNotBlank()) {
                append("• Платформа SoC: ").append(Build.SOC_MODEL).append("\n")
            }
            val cpuSummary = NativeLibrary.getCpuSummary()
            if (cpuSummary.isNotEmpty() && cpuSummary != "Unknown") {
                append("• Процессор: ").append(cpuSummary)
            } else {
                append("• Архитектура: ARM64-v8a / 64-bit Core")
            }
        }
        binding.textCpuInfo.text = cpuInfo

        // 3. GPU Info
        val gpuInfo = buildString {
            try {
                val gpuModel = NativeLibrary.getGpuModel()
                append("• Модель GPU: ").append(if (gpuModel.isNotEmpty()) gpuModel else "Adreno / Mali GPU").append("\n")

                val vulkanApi = NativeLibrary.getVulkanApiVersion()
                append("• Vulkan API: ").append(if (vulkanApi.isNotEmpty()) vulkanApi else "1.3.x").append("\n")

                val vulkanDriver = NativeLibrary.getVulkanDriverVersion()
                append("• Версия драйвера: ").append(if (vulkanDriver.isNotEmpty()) vulkanDriver else "Vulkan Hardware Driver")
            } catch (e: Exception) {
                append("• Статус: ").append(e.message)
            }
        }
        binding.textGpuInfo.text = gpuInfo

        // 4. Memory Info
        val ramInfo = buildString {
            val activityManager =
                requireContext().getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager
            val memInfo = ActivityManager.MemoryInfo()
            activityManager.getMemoryInfo(memInfo)
            val totalDeviceRam = memInfo.totalMem / (1024 * 1024)
            val availDeviceRam = memInfo.availMem / (1024 * 1024)

            append("• Общий объем RAM: ").append(totalDeviceRam).append(" MB\n")
            append("• Доступно RAM: ").append(availDeviceRam).append(" MB\n")
            append("• Порог нехватки памяти: ").append(memInfo.threshold / (1024 * 1024)).append(" MB")
        }
        binding.textRamInfo.text = ramInfo
    }

    private fun copyDiagnosticsToClipboard() {
        val fullInfo = buildString {
            appendLine("=== STORM EDEN v${NativeLibrary.getBuildVersion()} System Diagnostics ===")
            appendLine("Date: ${java.util.Date()}")
            appendLine("Device: ${Build.MANUFACTURER} ${Build.MODEL} (${Build.DEVICE})")
            appendLine("Android: ${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT}), Patch: ${Build.VERSION.SECURITY_PATCH}")
            appendLine("CPU: ${binding.textCpuInfo.text}")
            appendLine("GPU: ${binding.textGpuInfo.text}")
            appendLine("RAM: ${binding.textRamInfo.text}")
        }
        val clipboard = requireContext().getSystemService(Context.CLIPBOARD_SERVICE) as android.content.ClipboardManager
        val clip = android.content.ClipData.newPlainText("STORM EDEN System Info", fullInfo)
        clipboard.setPrimaryClip(clip)
        android.widget.Toast.makeText(requireContext(), R.string.copied_to_clipboard, android.widget.Toast.LENGTH_SHORT).show()
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    companion object {
        const val TAG = "SystemInfoDialogFragment"

        fun newInstance(): SystemInfoDialogFragment {
            return SystemInfoDialogFragment()
        }
    }
}
