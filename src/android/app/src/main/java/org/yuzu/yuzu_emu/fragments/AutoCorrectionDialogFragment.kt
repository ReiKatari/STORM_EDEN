// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.fragments

import android.app.Dialog
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.os.Bundle
import android.view.View
import android.widget.Toast
import androidx.fragment.app.DialogFragment
import androidx.preference.PreferenceManager
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.databinding.DialogAutoCorrectionBinding
import org.yuzu.yuzu_emu.model.Game
import org.yuzu.yuzu_emu.model.GameFixDatabase
import org.yuzu.yuzu_emu.utils.GameIconUtils
import java.util.Locale

class AutoCorrectionDialogFragment : DialogFragment() {

    private var _binding: DialogAutoCorrectionBinding? = null
    private val binding get() = _binding!!

    private var game: Game? = null

    companion object {
        const val TAG = "AutoCorrectionDialogFragment"
        private const val ARG_GAME = "game"

        fun newInstance(game: Game): AutoCorrectionDialogFragment {
            val fragment = AutoCorrectionDialogFragment()
            val args = Bundle()
            args.putParcelable(ARG_GAME, game)
            fragment.arguments = args
            fragment.game = game
            return fragment
        }
    }

    private fun sanitizeText(str: String): String {
        if (str.isEmpty()) return str
        if (str.contains("вЂ") || str.contains("вњ") || str.contains("Р") || str.contains("С")) {
            return try {
                val bytes = str.toByteArray(Charsets.ISO_8859_1)
                val decoded = String(bytes, Charsets.UTF_8)
                if (decoded.contains("•") || decoded.contains("✓") || decoded.any { it in 'а'..'я' || it in 'А'..'Я' }) {
                    decoded
                } else {
                    str
                }
            } catch (_: Exception) {
                str
            }
        }
        return str
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        _binding = DialogAutoCorrectionBinding.inflate(layoutInflater)

        val currentGame = game ?: arguments?.getParcelable(ARG_GAME) ?: return super.onCreateDialog(savedInstanceState)
        val profile = GameFixDatabase.getFix(currentGame)
        val prefs = PreferenceManager.getDefaultSharedPreferences(requireContext())
        val prefKey = "auto_corrected_${currentGame.programId}"
        val isAutoCorrected = prefs.getBoolean(prefKey, false)

        binding.textAutoCorrectionGameTitle.text = currentGame.title
        GameIconUtils.loadGameIcon(currentGame, binding.imageAutoCorrectionIcon)

        val isRu = Locale.getDefault().language == "ru"
        if (isAutoCorrected) {
            binding.textAutoCorrectionStatus.text = getString(R.string.auto_correction_status_active)
            binding.textAutoCorrectionStatus.setTextColor(Color.parseColor("#00F0FF"))
        } else {
            binding.textAutoCorrectionStatus.text = getString(R.string.auto_correction_status_inactive)
            binding.textAutoCorrectionStatus.setTextColor(Color.parseColor("#94A3B8"))
        }

        if (profile != null) {
            val issues = if (isRu) profile.issuesRu else profile.issuesEn
            val fixes = if (isRu) profile.fixesRu else profile.fixesEn
            binding.textAutoCorrectionIssues.text = sanitizeText(issues)
            binding.textAutoCorrectionRecommended.text = sanitizeText(fixes)
            binding.cardAutoCorrectionIssues.visibility = View.VISIBLE
        } else {
            binding.cardAutoCorrectionIssues.visibility = View.GONE
            binding.textAutoCorrectionRecommended.text = getString(R.string.auto_correction_no_profile)
        }

        binding.btnApplyAutoCorrection.setOnClickListener {
            try {
                GameFixDatabase.applyFix(currentGame, forceOverwrite = true)
                prefs.edit().putBoolean(prefKey, true).apply()
                Toast.makeText(requireContext(), getString(R.string.auto_correction_applied_toast), Toast.LENGTH_SHORT).show()
            } catch (e: Exception) {
                Toast.makeText(requireContext(), "Error: ${e.message}", Toast.LENGTH_SHORT).show()
            }
            dismissAllowingStateLoss()
        }

        binding.btnResetAutoCorrection.setOnClickListener {
            try {
                GameFixDatabase.clearActiveSessionFix(currentGame)
                prefs.edit().putBoolean(prefKey, false).apply()
                Toast.makeText(requireContext(), getString(R.string.auto_correction_reset_toast), Toast.LENGTH_SHORT).show()
            } catch (e: Exception) {
                Toast.makeText(requireContext(), "Error: ${e.message}", Toast.LENGTH_SHORT).show()
            }
            dismissAllowingStateLoss()
        }

        binding.btnCloseAutoCorrection.setOnClickListener {
            dismissAllowingStateLoss()
        }

        val dialog = MaterialAlertDialogBuilder(requireContext())
            .setView(binding.root)
            .create()

        dialog.window?.setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))
        return dialog
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}
