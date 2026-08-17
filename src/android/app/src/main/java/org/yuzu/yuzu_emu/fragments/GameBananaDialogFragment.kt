// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.fragments

import android.app.Dialog
import android.os.Build
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.AdapterView
import android.widget.ArrayAdapter
import android.widget.Toast
import androidx.core.view.isVisible
import androidx.fragment.app.DialogFragment
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
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

    private var currentPage: Int = 1
    private var currentSortIndex: Int = 0

    companion object {
        const val TAG = "GameBananaDialogFragment"
        private const val ARG_GAME = "arg_game"

        fun newInstance(game: Game, onModsUpdated: (() -> Unit)? = null): GameBananaDialogFragment {
            return GameBananaDialogFragment().apply {
                arguments = Bundle().apply {
                    putParcelable(ARG_GAME, game)
                }
                this.game = game
                this.onModsUpdated = onModsUpdated
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        if (game == null) {
            game = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                arguments?.getParcelable(ARG_GAME, Game::class.java)
            } else {
                @Suppress("DEPRECATION")
                arguments?.getParcelable(ARG_GAME)
            }
        }
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        _binding = DialogGamebananaModsBinding.inflate(layoutInflater)

        setupUI()
        performSearch(1)

        return MaterialAlertDialogBuilder(requireContext())
            .setTitle(getString(R.string.gamebanana_mods) + " - " + (game?.title ?: ""))
            .setView(binding.root)
            .setNegativeButton(R.string.close, null)
            .create()
    }

    private fun setupUI() {
        binding.listMods.layoutManager = LinearLayoutManager(requireContext())
        binding.listMods.adapter = GameBananaModAdapter()

        // Sort spinner
        val sortOptions = arrayOf(
            getString(R.string.sort_downloads),
            getString(R.string.sort_likes),
            getString(R.string.sort_views),
            getString(R.string.sort_latest),
            getString(R.string.sort_name)
        )
        val spinnerAdapter = ArrayAdapter(
            requireContext(),
            android.R.layout.simple_spinner_dropdown_item,
            sortOptions
        )
        binding.spinnerSort.adapter = spinnerAdapter
        binding.spinnerSort.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(parent: AdapterView<*>?, view: View?, position: Int, id: Long) {
                if (currentSortIndex != position) {
                    currentSortIndex = position
                    performSearch(1)
                }
            }
            override fun onNothingSelected(parent: AdapterView<*>?) {}
        }

        binding.buttonSearch.setOnClickListener {
            performSearch(1)
        }

        binding.inputSearch.setOnEditorActionListener { _, _, _ ->
            performSearch(1)
            true
        }

        binding.buttonFirstPage.setOnClickListener {
            if (currentPage > 1) performSearch(1)
        }

        binding.buttonPrevPage.setOnClickListener {
            if (currentPage > 1) performSearch(currentPage - 1)
        }

        binding.buttonNextPage.setOnClickListener {
            performSearch(currentPage + 1)
        }
    }

    private fun performSearch(page: Int) {
        val g = game ?: return
        val query = binding.inputSearch.text.toString().trim()
        currentPage = page

        binding.progressLoading.isVisible = true
        binding.textEmptyMods.isVisible = false
        binding.textPageIndicator.text = getString(R.string.page_format, currentPage)
        binding.buttonFirstPage.isEnabled = (currentPage > 1)
        binding.buttonPrevPage.isEnabled = (currentPage > 1)

        lifecycleScope.launch {
            val results = GameBananaHelper.searchMods(
                game = g,
                query = query,
                page = currentPage,
                sortIndex = currentSortIndex
            )
            binding.progressLoading.isVisible = false
            modsList.clear()
            modsList.addAll(results)
            binding.listMods.adapter?.notifyDataSetChanged()
            binding.textEmptyMods.isVisible = results.isEmpty()
            binding.buttonNextPage.isEnabled = results.size >= 15
        }
    }

    private fun showModDetailsDialog(mod: GameBananaMod) {
        val g = game ?: return
        val loadingDialog = MaterialAlertDialogBuilder(requireContext())
            .setTitle(mod.name)
            .setMessage(getString(R.string.loading_mod_details))
            .setCancelable(true)
            .show()

        lifecycleScope.launch {
            val (desc, files) = GameBananaHelper.getModFiles(mod.id)
            loadingDialog.dismiss()

            if (files.isEmpty()) {
                Toast.makeText(requireContext(), R.string.gamebanana_no_files, Toast.LENGTH_SHORT).show()
                return@launch
            }

            val fileNames = files.map {
                val sizeMb = if (it.filesize > 0) " (${it.filesize / 1024 / 1024} MB)" else ""
                "${it.filename}$sizeMb\n${it.description.ifEmpty { mod.submitter }}"
            }.toTypedArray()

            var selectedFileIndex = 0

            MaterialAlertDialogBuilder(requireContext())
                .setTitle(mod.name)
                .setSingleChoiceItems(fileNames, 0) { _, which ->
                    selectedFileIndex = which
                }
                .setPositiveButton(R.string.gamebanana_download_button) { _, _ ->
                    val fileToDownload = files[selectedFileIndex]
                    startDownloadFile(mod, fileToDownload)
                }
                .setNegativeButton(R.string.close, null)
                .show()
        }
    }

    private fun startDownloadFile(mod: GameBananaMod, file: GameBananaFile) {
        val g = game ?: return
        val progressDialog = MaterialAlertDialogBuilder(requireContext())
            .setTitle(getString(R.string.installing_mod_title, mod.name))
            .setMessage(file.filename)
            .setCancelable(false)
            .show()

        lifecycleScope.launch {
            val success = GameBananaHelper.downloadAndInstallMod(
                game = g,
                modName = mod.name,
                file = file,
                onProgress = { progress ->
                    lifecycleScope.launch(Dispatchers.Main) {
                        progressDialog.setMessage("${file.filename}\n$progress%")
                    }
                }
            )

            progressDialog.dismiss()

            if (success) {
                Toast.makeText(
                    requireContext(),
                    getString(R.string.gamebanana_installed_success, mod.name),
                    Toast.LENGTH_LONG
                ).show()
                onModsUpdated?.invoke()
            } else {
                Toast.makeText(
                    requireContext(),
                    R.string.gamebanana_install_error,
                    Toast.LENGTH_LONG
                ).show()
            }
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
                            getString(R.string.gamebanana_downloads, mod.downloads) + " • " +
                            getString(R.string.gamebanana_likes, mod.likes)
                } else {
                    getString(R.string.gamebanana_downloads, mod.downloads)
                }
                itemBinding.textModAuthor.text = authorInfo
                itemBinding.buttonDownloadMod.isEnabled = true
                itemBinding.progressModDownload.isVisible = false

                itemBinding.buttonDownloadMod.setOnClickListener {
                    showModDetailsDialog(mod)
                }

                itemView.setOnClickListener {
                    showModDetailsDialog(mod)
                }
            }
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}
