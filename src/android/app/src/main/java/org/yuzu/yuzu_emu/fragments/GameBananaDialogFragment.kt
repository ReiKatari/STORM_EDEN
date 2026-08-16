// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.fragments

import android.app.Dialog
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.core.view.isVisible
import androidx.fragment.app.DialogFragment
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.databinding.DialogGamebananaModsBinding
import org.yuzu.yuzu_emu.databinding.ListItemGamebananaModBinding
import org.yuzu.yuzu_emu.model.Game
import org.yuzu.yuzu_emu.utils.GameBananaFile
import org.yuzu.yuzu_emu.utils.GameBananaHelper
import org.yuzu.yuzu_emu.utils.GameBananaMod

class GameBananaDialogFragment : DialogFragment() {

    private var _binding: DialogGamebananaModsBinding? = null
    private val binding get() = _binding!!

    private var game: Game? = null
    private var onModsUpdated: (() -> Unit)? = null
    private val modsList = mutableListOf<GameBananaMod>()

    companion object {
        const val TAG = "GameBananaDialogFragment"

        fun newInstance(game: Game, onModsUpdated: (() -> Unit)? = null): GameBananaDialogFragment {
            return GameBananaDialogFragment().apply {
                this.game = game
                this.onModsUpdated = onModsUpdated
            }
        }
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        _binding = DialogGamebananaModsBinding.inflate(layoutInflater)

        setupUI()
        performSearch("")

        return MaterialAlertDialogBuilder(requireContext())
            .setTitle(getString(R.string.gamebanana_mods) + " - " + (game?.title ?: ""))
            .setView(binding.root)
            .setNegativeButton(R.string.close, null)
            .create()
    }

    private fun setupUI() {
        binding.listMods.layoutManager = LinearLayoutManager(requireContext())
        binding.listMods.adapter = GameBananaModAdapter()

        binding.buttonSearch.setOnClickListener {
            val query = binding.inputSearch.text.toString().trim()
            performSearch(query)
        }

        binding.inputSearch.setOnEditorActionListener { _, _, _ ->
            val query = binding.inputSearch.text.toString().trim()
            performSearch(query)
            true
        }
    }

    private fun performSearch(query: String) {
        val g = game ?: return
        binding.progressLoading.isVisible = true
        binding.textEmptyMods.isVisible = false

        lifecycleScope.launch {
            val results = GameBananaHelper.searchMods(g, query)
            binding.progressLoading.isVisible = false
            modsList.clear()
            modsList.addAll(results)
            binding.listMods.adapter?.notifyDataSetChanged()
            binding.textEmptyMods.isVisible = results.isEmpty()
        }
    }

    private inner class GameBananaModAdapter :
        RecyclerView.Adapter<GameBananaModAdapter.ModViewHolder>() {

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ModViewHolder {
            val binding = ListItemGamebananaModBinding.inflate(
                LayoutInflater.from(parent.context),
                parent,
                false
            )
            return ModViewHolder(binding)
        }

        override fun onBindViewHolder(holder: ModViewHolder, position: Int) {
            holder.bind(modsList[position])
        }

        override fun getItemCount(): Int = modsList.size

        inner class ModViewHolder(private val itemBinding: ListItemGamebananaModBinding) :
            RecyclerView.ViewHolder(itemBinding.root) {

            fun bind(mod: GameBananaMod) {
                itemBinding.textModTitle.text = mod.name
                val authorInfo = if (mod.submitter.isNotEmpty()) {
                    getString(R.string.gamebanana_author, mod.submitter) + " • " +
                            getString(R.string.gamebanana_downloads, mod.downloads)
                } else {
                    getString(R.string.gamebanana_downloads, mod.downloads)
                }
                itemBinding.textModAuthor.text = authorInfo

                itemBinding.buttonDownloadMod.isEnabled = true
                itemBinding.progressModDownload.isVisible = false

                itemBinding.buttonDownloadMod.setOnClickListener {
                    downloadMod(mod, itemBinding)
                }
            }

            private fun downloadMod(mod: GameBananaMod, itemBinding: ListItemGamebananaModBinding) {
                val g = game ?: return
                itemBinding.buttonDownloadMod.isEnabled = false
                itemBinding.progressModDownload.isVisible = true
                itemBinding.progressModDownload.isIndeterminate = true

                lifecycleScope.launch {
                    val files = GameBananaHelper.getModFiles(mod.id)
                    if (files.isEmpty()) {
                        Toast.makeText(
                            requireContext(),
                            R.string.gamebanana_install_error,
                            Toast.LENGTH_SHORT
                        ).show()
                        itemBinding.buttonDownloadMod.isEnabled = true
                        itemBinding.progressModDownload.isVisible = false
                        return@launch
                    }

                    val firstFile = files[0]
                    itemBinding.progressModDownload.isIndeterminate = false

                    val success = GameBananaHelper.downloadAndInstallMod(
                        game = g,
                        modName = mod.name,
                        file = firstFile,
                        onProgress = { progress ->
                            lifecycleScope.launch(Dispatchers.Main) {
                                itemBinding.progressModDownload.progress = progress
                            }
                        }
                    )

                    itemBinding.progressModDownload.isVisible = false
                    itemBinding.buttonDownloadMod.isEnabled = true

                    if (success) {
                        Toast.makeText(
                            requireContext(),
                            R.string.gamebanana_install_success,
                            Toast.LENGTH_SHORT
                        ).show()
                        onModsUpdated?.invoke()
                    } else {
                        Toast.makeText(
                            requireContext(),
                            R.string.gamebanana_install_error,
                            Toast.LENGTH_SHORT
                        ).show()
                    }
                }
            }
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}
