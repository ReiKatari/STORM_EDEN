// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.fragments

import android.app.Dialog
import android.graphics.BitmapFactory
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
import com.google.android.material.chip.Chip
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.databinding.DialogAmiiboBrowserBinding
import org.yuzu.yuzu_emu.databinding.ListItemAmiiboBinding
import org.yuzu.yuzu_emu.utils.AmiiboEntry
import org.yuzu.yuzu_emu.utils.AmiiboHelper
import java.io.InputStream

class AmiiboDialogFragment : DialogFragment() {

    private var _binding: DialogAmiiboBrowserBinding? = null
    private val binding get() = _binding!!

    private var allAmiibos = listOf<AmiiboEntry>()
    private var displayedAmiibos = mutableListOf<AmiiboEntry>()
    private var selectedSeries: String = ""
    private var isEmulating: Boolean = false
    private var gameTitle: String = ""
    private var titleId: String = ""

    companion object {
        const val TAG = "AmiiboDialogFragment"
        private const val ARG_IS_EMULATING = "arg_is_emulating"
        private const val ARG_GAME_TITLE = "arg_game_title"
        private const val ARG_TITLE_ID = "arg_title_id"

        fun newInstance(
            isEmulating: Boolean = false,
            gameTitle: String = "",
            titleId: String = ""
        ): AmiiboDialogFragment {
            return AmiiboDialogFragment().apply {
                arguments = Bundle().apply {
                    putBoolean(ARG_IS_EMULATING, isEmulating)
                    putString(ARG_GAME_TITLE, gameTitle)
                    putString(ARG_TITLE_ID, titleId)
                }
                this.isEmulating = isEmulating
                this.gameTitle = gameTitle
                this.titleId = titleId
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        isEmulating = arguments?.getBoolean(ARG_IS_EMULATING, false) ?: false
        gameTitle = arguments?.getString(ARG_GAME_TITLE, "") ?: ""
        titleId = arguments?.getString(ARG_TITLE_ID, "") ?: ""
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        _binding = DialogAmiiboBrowserBinding.inflate(layoutInflater)

        setupUI()
        loadDatabase(false)

        val dialogTitle = if (gameTitle.isNotBlank()) {
            getString(R.string.amiibo_for_game_title, gameTitle)
        } else {
            getString(R.string.amiibo_database_title)
        }

        return MaterialAlertDialogBuilder(requireActivity())
            .setTitle(dialogTitle)
            .setView(binding.root)
            .setNegativeButton(R.string.close, null)
            .create()
    }

    override fun onStart() {
        super.onStart()
        dialog?.window?.let { window ->
            val dm = resources.displayMetrics
            val isLandscape = resources.configuration.orientation == android.content.res.Configuration.ORIENTATION_LANDSCAPE
            val width = if (isLandscape) (dm.widthPixels * 0.92).toInt() else (dm.widthPixels * 0.95).toInt()
            val height = if (isLandscape) (dm.heightPixels * 0.92).toInt() else (dm.heightPixels * 0.85).toInt()
            window.setLayout(width, height)
            window.setBackgroundDrawableResource(R.drawable.eden_dialog_background)
        }
    }

    private fun setupUI() {
        val isLandscape = resources.configuration.orientation == android.content.res.Configuration.ORIENTATION_LANDSCAPE
        val spanCount = if (isLandscape) 2 else 1
        binding.listAmiibo.layoutManager = androidx.recyclerview.widget.GridLayoutManager(requireContext(), spanCount)
        binding.listAmiibo.adapter = AmiiboAdapter()

        binding.buttonRefresh.setOnClickListener {
            loadDatabase(true)
        }

        binding.buttonDisconnectAmiibo.setOnClickListener {
            NativeLibrary.closeAmiibo()
            Toast.makeText(
                requireContext(),
                R.string.amiibo_removed_toast,
                Toast.LENGTH_SHORT
            ).show()
            updateActiveAmiiboStatus()
        }

        updateActiveAmiiboStatus()

        binding.inputSearch.setOnEditorActionListener { _, _, _ ->
            applyFilters()
            true
        }

        binding.inputSearch.addTextChangedListener(object : android.text.TextWatcher {
            override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
            override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {
                applyFilters()
            }
            override fun afterTextChanged(s: android.text.Editable?) {}
        })
    }

    private fun updateActiveAmiiboStatus() {
        if (!isEmulating) {
            binding.cardActiveAmiibo.isVisible = false
            return
        }
        val state = NativeLibrary.getVirtualAmiiboState()
        // State 2 = TagNearby
        if (state == 2) {
            binding.cardActiveAmiibo.isVisible = true
            binding.textActiveAmiiboStatus.text = getString(R.string.amiibo_currently_attached, "NFC Active")
        } else {
            binding.cardActiveAmiibo.isVisible = false
        }
    }

    private fun loadDatabase(forceRefresh: Boolean) {
        binding.progressLoading.isVisible = true
        binding.textEmptyAmiibo.isVisible = false

        lifecycleScope.launch {
            val list = AmiiboHelper.getAmiiboDatabase(forceRefresh)
            allAmiibos = if (gameTitle.isNotBlank()) {
                AmiiboHelper.getAmiibosForGame(list, titleId, gameTitle)
            } else {
                list
            }
            binding.progressLoading.isVisible = false

            populateSeriesChips()
            applyFilters()
        }
    }

    private fun populateSeriesChips() {
        binding.chipGroupSeries.removeAllViews()

        val seriesSet = allAmiibos.map { it.amiiboSeries }.filter { it.isNotBlank() }.distinct().sorted()
        if (seriesSet.isEmpty()) return

        val allChip = Chip(requireContext()).apply {
            text = getString(R.string.all_series)
            isCheckable = true
            isChecked = selectedSeries.isEmpty()
            setOnClickListener {
                selectedSeries = ""
                applyFilters()
            }
        }
        binding.chipGroupSeries.addView(allChip)

        for (s in seriesSet) {
            val chip = Chip(requireContext()).apply {
                text = s
                isCheckable = true
                isChecked = (selectedSeries == s)
                setOnClickListener {
                    selectedSeries = s
                    applyFilters()
                }
            }
            binding.chipGroupSeries.addView(chip)
        }
    }

    private fun applyFilters() {
        val query = binding.inputSearch.text?.toString()?.trim()?.lowercase() ?: ""
        displayedAmiibos = allAmiibos.filter { entry ->
            val matchesQuery = query.isEmpty() ||
                entry.name.lowercase().contains(query) ||
                entry.character.lowercase().contains(query) ||
                entry.gameSeries.lowercase().contains(query) ||
                entry.fullId.lowercase().contains(query)

            val matchesSeries = selectedSeries.isEmpty() || entry.amiiboSeries == selectedSeries
            matchesQuery && matchesSeries
        }.toMutableList()

        binding.textEmptyAmiibo.isVisible = displayedAmiibos.isEmpty()
        binding.textStatus.text = getString(R.string.amiibo_count_format, displayedAmiibos.size, allAmiibos.size)
        binding.listAmiibo.adapter?.notifyDataSetChanged()
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    private inner class AmiiboAdapter : RecyclerView.Adapter<AmiiboAdapter.ViewHolder>() {

        inner class ViewHolder(val itemBinding: ListItemAmiiboBinding) :
            RecyclerView.ViewHolder(itemBinding.root)

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
            val itemBinding = ListItemAmiiboBinding.inflate(
                LayoutInflater.from(parent.context),
                parent,
                false
            )
            return ViewHolder(itemBinding)
        }

        override fun onBindViewHolder(holder: ViewHolder, position: Int) {
            val entry = displayedAmiibos[position]
            val b = holder.itemBinding

            b.textAmiiboName.text = entry.name
            b.textAmiiboSeries.text = "${entry.gameSeries} • ${entry.amiiboSeries} (${entry.type})"
            b.textAmiiboId.text = "ID: ${entry.fullId}"

            b.imageAmiibo.setImageResource(R.drawable.ic_amiibo)
            if (entry.imageUrl.isNotEmpty()) {
                val imgUrl = entry.imageUrl
                lifecycleScope.launch {
                    val bitmap = AmiiboHelper.getAmiiboImage(imgUrl)
                    if (bitmap != null && holder.bindingAdapterPosition == position) {
                        b.imageAmiibo.setImageBitmap(bitmap)
                    }
                }
            }

            b.buttonSaveBin.setOnClickListener {
                lifecycleScope.launch(Dispatchers.IO) {
                    try {
                        val file = AmiiboHelper.saveAmiiboToStorage(entry)
                        withContext(Dispatchers.Main) {
                            Toast.makeText(
                                requireContext(),
                                getString(R.string.amiibo_saved_format, file.name),
                                Toast.LENGTH_SHORT
                            ).show()
                        }
                    } catch (e: Exception) {
                        withContext(Dispatchers.Main) {
                            Toast.makeText(
                                requireContext(),
                                "Error: ${e.message}",
                                Toast.LENGTH_SHORT
                            ).show()
                        }
                    }
                }
            }

            b.buttonInjectAmiibo.setOnClickListener {
                lifecycleScope.launch(Dispatchers.IO) {
                    val success = AmiiboHelper.loadAmiiboDirectly(entry)
                    withContext(Dispatchers.Main) {
                        if (success) {
                            Toast.makeText(
                                requireContext(),
                                getString(R.string.amiibo_injected_success, entry.name),
                                Toast.LENGTH_SHORT
                            ).show()
                            if (isEmulating) {
                                dismiss()
                            }
                        } else {
                            Toast.makeText(
                                requireContext(),
                                getString(R.string.amiibo_injected_fail),
                                Toast.LENGTH_SHORT
                            ).show()
                        }
                    }
                }
            }

            holder.itemView.setOnClickListener {
                if (entry.switchGames.isNotEmpty()) {
                    MaterialAlertDialogBuilder(requireContext())
                        .setTitle(entry.name)
                        .setMessage(entry.switchGames.joinToString("\n\n"))
                        .setPositiveButton(android.R.string.ok, null)
                        .show()
                }
            }
        }

        override fun getItemCount(): Int = displayedAmiibos.size
    }
}
