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

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        _binding = DialogGameFixBinding.inflate(layoutInflater)

        val currentGame = game ?: return super.onCreateDialog(savedInstanceState)
        val profile = GameFixDatabase.getFix(currentGame.programId)

        if (profile != null) {
            binding.textGameFixTitle.text = currentGame.title
            val hexId = GameFixDatabase.getProgramIdHex(currentGame.programId)
            binding.textGameFixTitleId.text = "ID: $hexId"
            GameIconUtils.loadGameIcon(currentGame, binding.imageGameFixIcon)

            val isRu = Locale.getDefault().language == "ru"
            binding.textGameFixIssues.text = if (isRu) profile.issuesRu else profile.issuesEn
            binding.textGameFixRecommended.text = if (isRu) profile.fixesRu else profile.fixesEn
        }

        binding.btnApplyGameFix.setOnClickListener {
            if (binding.cbDontAskAgain.isChecked) {
                GameFixDatabase.setDontAskAgain(requireContext(), currentGame.programId, true)
            }
            GameFixDatabase.applyFix(currentGame)
            Toast.makeText(requireContext(), "⚡ Оптимизации STORM EDEN применены!", Toast.LENGTH_SHORT).show()
            dismiss()
            onLaunchCallback?.invoke(true)
        }

        binding.btnSkipGameFix.setOnClickListener {
            if (binding.cbDontAskAgain.isChecked) {
                GameFixDatabase.setDontAskAgain(requireContext(), currentGame.programId, true)
            }
            dismiss()
            onLaunchCallback?.invoke(false)
        }

        return MaterialAlertDialogBuilder(requireContext())
            .setView(binding.root)
            .create()
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}
