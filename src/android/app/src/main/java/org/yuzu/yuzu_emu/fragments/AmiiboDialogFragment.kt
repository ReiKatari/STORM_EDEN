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

    companion object {
        const val TAG = "AmiiboDialogFragment"
        private const val ARG_IS_EMULATING = "arg_is_emulating"

        fun newInstance(isEmulating: Boolean = false): AmiiboDialogFragment {
            return AmiiboDialogFragment().apply {
                arguments = Bundle().apply {
                    putBoolean(ARG_IS_EMULATING, isEmulating)
                }
                this.isEmulating = isEmulating
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        isEmulating = arguments?.getBoolean(ARG_IS_EMULATING, false) ?: false
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        _binding = DialogAmiiboBrowserBinding.inflate(layoutInflater)

        setupUI()
        loadDatabase(false)

        return MaterialAlertDialogBuilder(requireContext())
            .setTitle(R.string.amiibo_database_title)
            .setView(binding.root)
            .setNegativeButton(R.string.close, null)
            .create()
    }

    private fun setupUI() {
        binding.listAmiibo.layoutManager = LinearLayoutManager(requireContext())
        binding.listAmiibo.adapter = AmiiboAdapter()

        binding.buttonRefresh.setOnClickListener {
            loadDatabase(true)
        }

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

    private fun loadDatabase(forceRefresh: Boolean) {
        binding.progressLoading.isVisible = true
        binding.textEmptyAmiibo.isVisible = false

        lifecycleScope.launch {
            val list = AmiiboHelper.getAmiiboDatabase(forceRefresh)
            allAmiibos = list
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
                lifecycleScope.launch(Dispatchers.IO) {
                    try {
                        val client = OkHttpClient()
                        var req = Request.Builder()
                            .url(imgUrl)
                            .header("User-Agent", "Mozilla/5.0 (Linux; Android 14; Mobile) AppleWebKit/537.36 STORM-EDEN/4.0.1")
                            .build()
                        var resp = client.newCall(req).execute()
                        if (!resp.isSuccessful && imgUrl.contains("jsdelivr.net")) {
                            val fallbackUrl = imgUrl.replace("https://cdn.jsdelivr.net/gh/N3evin/AmiiboAPI@master", "https://raw.githubusercontent.com/N3evin/AmiiboAPI/master")
                            req = Request.Builder()
                                .url(fallbackUrl)
                                .header("User-Agent", "Mozilla/5.0 (Linux; Android 14; Mobile) AppleWebKit/537.36 STORM-EDEN/4.0.1")
                                .build()
                            resp = client.newCall(req).execute()
                        }
                        if (resp.isSuccessful && resp.body != null) {
                            val stream: InputStream = resp.body!!.byteStream()
                            val bitmap = BitmapFactory.decodeStream(stream)
                            withContext(Dispatchers.Main) {
                                if (holder.bindingAdapterPosition == position) {
                                    b.imageAmiibo.setImageBitmap(bitmap)
                                }
                            }
                        }
                    } catch (_: Exception) {}
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
