// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.fragments

import android.app.Dialog
import android.os.Bundle
import android.widget.Toast
import androidx.fragment.app.DialogFragment
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.yuzu.yuzu_emu.databinding.DialogGameFixBinding
import org.yuzu.yuzu_emu.model.Game
import org.yuzu.yuzu_emu.model.GameFixDatabase
import org.yuzu.yuzu_emu.utils.GameIconUtils
import java.util.Locale

class GameFixDialogFragment : DialogFragment() {

    private var _binding: DialogGameFixBinding? = null
    private val binding get() = _binding!!

    private var game: Game? = null
    private var onLaunchCallback: ((Boolean) -> Unit)? = null

    companion object {
        const val TAG = "GameFixDialogFragment"

        fun newInstance(game: Game, onLaunch: (Boolean) -> Unit): GameFixDialogFragment {
            val fragment = GameFixDialogFragment()
            fragment.game = game
            fragment.onLaunchCallback = onLaunch
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
        _binding = DialogGameFixBinding.inflate(layoutInflater)

        val currentGame = game ?: return super.onCreateDialog(savedInstanceState)
        val profile = GameFixDatabase.getFix(currentGame)

        if (profile != null) {
            binding.textGameFixTitle.text = currentGame.title
            val hexId = GameFixDatabase.getProgramIdHex(currentGame)
            binding.textGameFixTitleId.text = "ID: $hexId"
            GameIconUtils.loadGameIcon(currentGame, binding.imageGameFixIcon)

            val isRu = Locale.getDefault().language == "ru"
            val issues = if (isRu) profile.issuesRu else profile.issuesEn
            val fixes = if (isRu) profile.fixesRu else profile.fixesEn
            binding.textGameFixIssues.text = sanitizeText(issues)
            binding.textGameFixRecommended.text = sanitizeText(fixes)
        }

        binding.btnApplyGameFix.setOnClickListener {
            val ctx = context
            try {
                if (binding.cbDontAskAgain.isChecked && ctx != null) {
                    GameFixDatabase.setDontAskAgain(ctx, currentGame, true)
                }
                GameFixDatabase.applyFix(currentGame)
                if (ctx != null) {
                    Toast.makeText(ctx, "⚡ Оптимизации STORM EDEN: Применено", Toast.LENGTH_SHORT).show()
                }
            } catch (e: Exception) {
                // Log and continue launching
            }
            val cb = onLaunchCallback
            dismissAllowingStateLoss()
            cb?.invoke(true)
        }

        binding.btnSkipGameFix.setOnClickListener {
            val ctx = context
            try {
                if (binding.cbDontAskAgain.isChecked && ctx != null) {
                    GameFixDatabase.setDontAskAgain(ctx, currentGame, true)
                }
                if (ctx != null) {
                    Toast.makeText(ctx, "⚠️ Оптимизации STORM EDEN: Не применено", Toast.LENGTH_SHORT).show()
                }
            } catch (e: Exception) {}
            val cb = onLaunchCallback
            dismissAllowingStateLoss()
            cb?.invoke(false)
        }

        val dialog = MaterialAlertDialogBuilder(requireContext())
            .setView(binding.root)
            .create()

        dialog.window?.setBackgroundDrawable(android.graphics.drawable.ColorDrawable(android.graphics.Color.TRANSPARENT))
        return dialog
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}
