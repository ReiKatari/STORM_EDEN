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
import org.yuzu.yuzu_emu.utils.StormHardwareCalibrator

class AutoOptimizationDialogFragment : DialogFragment() {

    private var _binding: DialogAutoOptimizationBinding? = null
    private val binding get() = _binding!!

    private var selectedMode = MODE_DEFAULT

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
        val profile = StormHardwareCalibrator.detectHardware(context)

        val formFactor = if (profile.isTablet) "Планшет" else "Смартфон"
        binding.textHwSoc.text = "SoC: ${profile.socName} (${profile.cpuCores} ядер, $formFactor)"
        binding.textHwGpu.text = "GPU: ${profile.gpuName}"
        binding.textHwRam.text = String.format("RAM: %.1f ГБ (Доступно: %.1f ГБ)", profile.totalRamGb, profile.availRamGb)
        val tierDesc = when (profile.tier) {
            StormHardwareCalibrator.HardwareTier.FLAGSHIP_ELITE -> "Ультра-флагман (Snapdragon 8 Elite / 12GB+ RAM)"
            StormHardwareCalibrator.HardwareTier.FLAGSHIP -> "Флагман (Высокая производительность, 12GB+ RAM)"
            StormHardwareCalibrator.HardwareTier.HIGH_MIDRANGE -> "Продвинутый (Сбалансированная производительность, 8GB RAM)"
            StormHardwareCalibrator.HardwareTier.MIDRANGE -> "Средний (6GB RAM)"
            StormHardwareCalibrator.HardwareTier.BUDGET -> "Базовый (< 6GB RAM)"
        }
        binding.textHwTier.text = "Класс устройства: $tierDesc"
    }

    private fun setupModeSelectors() {
        binding.cardModeFast.setOnClickListener { selectMode(MODE_FAST) }
        binding.cardModeNormal.setOnClickListener { selectMode(MODE_NORMAL) }
        binding.cardModeAccurate.setOnClickListener { selectMode(MODE_ACCURATE) }
        binding.cardModeDefault.setOnClickListener { selectMode(MODE_DEFAULT) }
        val prefs = androidx.preference.PreferenceManager.getDefaultSharedPreferences(requireContext())
        val savedMode = prefs.getInt("selected_auto_optimization_mode", MODE_DEFAULT)
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

        // Apply tailored "По умолчанию" defaults for this specific device
        StormHardwareCalibrator.applyPreset(StormHardwareCalibrator.StormPreset.DEFAULT, context)

        Toast.makeText(
            context,
            getString(R.string.auto_optimization_defaults_restored_toast),
            Toast.LENGTH_LONG
        ).show()

        dismiss()
    }

    private fun applyOptimization(mode: Int) {
        val context = requireContext()
        val prefs = androidx.preference.PreferenceManager.getDefaultSharedPreferences(context)
        prefs.edit().putInt("selected_auto_optimization_mode", mode).apply()

        val preset = when (mode) {
            MODE_FAST -> StormHardwareCalibrator.StormPreset.FAST
            MODE_NORMAL -> StormHardwareCalibrator.StormPreset.NORMAL
            MODE_ACCURATE -> StormHardwareCalibrator.StormPreset.ACCURATE
            else -> StormHardwareCalibrator.StormPreset.DEFAULT
        }

        StormHardwareCalibrator.applyPreset(preset, context)

        val modeName = when (mode) {
            MODE_FAST -> getString(R.string.auto_optimization_mode_fast)
            MODE_ACCURATE -> getString(R.string.auto_optimization_mode_accurate)
            MODE_NORMAL -> getString(R.string.auto_optimization_mode_normal)
            else -> getString(R.string.auto_optimization_mode_default)
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
