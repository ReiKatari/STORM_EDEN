// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.fragments

import android.annotation.SuppressLint
import android.app.Dialog
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import android.widget.EditText
import android.widget.Toast
import androidx.core.widget.doOnTextChanged
import androidx.fragment.app.DialogFragment
import androidx.lifecycle.lifecycleScope
import androidx.preference.PreferenceManager
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONObject
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.databinding.DialogCheatsBinding
import org.yuzu.yuzu_emu.databinding.ItemCheatBinding
import org.yuzu.yuzu_emu.model.Game
import org.yuzu.yuzu_emu.utils.DirectoryInitialization
import org.yuzu.yuzu_emu.utils.NativeConfig
import java.io.BufferedReader
import java.io.File
import java.io.InputStreamReader
import java.net.HttpURLConnection
import java.net.URL

class CheatsDialogFragment : DialogFragment() {

    private var _binding: DialogCheatsBinding? = null
    private val binding get() = _binding!!

    private lateinit var game: Game
    private val allCheats = mutableListOf<CheatModel>()
    private val displayedCheats = mutableListOf<CheatModel>()
    private lateinit var adapter: CheatsAdapter

    data class CheatModel(
        var name: String,
        var code: String,
        var buildId: String = "",
        var isEnabled: Boolean = false,
        var isCustom: Boolean = false,
        var sourceFile: File? = null
    )

    companion object {
        const val TAG = "CheatsDialogFragment"
        private const val ARG_GAME = "game"

        fun newInstance(game: Game): CheatsDialogFragment {
            val fragment = CheatsDialogFragment()
            val args = Bundle().apply {
                putParcelable(ARG_GAME, game)
            }
            fragment.arguments = args
            return fragment
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        game = requireArguments().getParcelable(ARG_GAME)!!
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val dialog = super.onCreateDialog(savedInstanceState)
        dialog.window?.setBackgroundDrawableResource(android.R.color.transparent)
        return dialog
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = DialogCheatsBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        val isRunning = NativeLibrary.isRunning()
        val buildIdStr = if (isRunning) {
            NativeLibrary.getBuildVersion().split("-").getOrNull(0) ?: ""
        } else {
            ""
        }

        binding.textGameInfo.text = buildString {
            append("ID: ")
            append(game.programIdHex)
            if (buildIdStr.isNotEmpty()) {
                append(" | Build: ")
                append(buildIdStr.take(16).uppercase())
            }
            append(if (isRunning) " | ⚡ В игре" else " | ⚪ Оффлайн")
        }

        adapter = CheatsAdapter(displayedCheats, isRunning) { cheat, isChecked ->
            cheat.isEnabled = isChecked
            updateSummary()
        }

        binding.recyclerCheats.layoutManager = LinearLayoutManager(requireContext())
        binding.recyclerCheats.adapter = adapter

        binding.btnClose.setOnClickListener { dismiss() }
        binding.btnSaveCheats.setOnClickListener {
            saveCheats()
            dismiss()
        }

        binding.btnDownloadOnline.setOnClickListener {
            downloadOnlineCheats()
        }

        binding.btnAddCustomCheat.setOnClickListener {
            showAddCustomCheatDialog()
        }

        binding.btnToggleAll.setOnClickListener {
            val anyDisabled = allCheats.any { !it.isEnabled }
            for (c in allCheats) {
                c.isEnabled = anyDisabled
            }
            adapter.notifyDataSetChanged()
            updateSummary()
        }

        binding.editSearchCheats.doOnTextChanged { text, _, _, _ ->
            val query = text?.toString()?.trim() ?: ""
            binding.btnClearSearch.visibility = if (query.isNotEmpty()) View.VISIBLE else View.GONE
            filterCheats(query)
        }

        binding.btnClearSearch.setOnClickListener {
            binding.editSearchCheats.text?.clear()
        }

        loadCheatsFromDisk()
    }

    override fun onStart() {
        super.onStart()
        dialog?.window?.let { window ->
            val dm = resources.displayMetrics
            val isLandscape = resources.configuration.orientation == android.content.res.Configuration.ORIENTATION_LANDSCAPE
            val width = if (isLandscape) (dm.widthPixels * 0.70).toInt() else (dm.widthPixels * 0.94).toInt()
            val height = if (isLandscape) (dm.heightPixels * 0.88).toInt() else (dm.heightPixels * 0.85).toInt()
            window.setLayout(width, height)
        }
    }

    private fun getCheatsDir(): File {
        val base = File(DirectoryInitialization.userDirectory ?: YuzuApplication.appContext.filesDir.absolutePath, "cheats")
        val gameDir = File(base, game.programIdHex)
        if (!gameDir.exists()) gameDir.mkdirs()
        return gameDir
    }

    private fun loadCheatsFromDisk() {
        allCheats.clear()
        val cheatsDir = getCheatsDir()
        val prefs = PreferenceManager.getDefaultSharedPreferences(YuzuApplication.appContext)
        val enabledSet = prefs.getStringSet("cheats_enabled_${game.programIdHex}", null)

        val filesToRead = mutableListOf<File>()
        cheatsDir.listFiles()?.filter { it.isFile && it.name.endsWith(".txt", ignoreCase = true) }?.let {
            filesToRead.addAll(it)
        }

        val legacyFile = File(cheatsDir.parentFile, "${game.programIdHex}.txt")
        if (legacyFile.exists() && legacyFile.isFile) {
            filesToRead.add(legacyFile)
        }

        for (file in filesToRead) {
            try {
                var buildId = file.nameWithoutExtension.uppercase()
                if (buildId == "CHEATS" || buildId == game.programIdHex) {
                    buildId = "Все версии"
                }

                val lines = file.readLines(Charsets.UTF_8)
                var currentName = ""
                val currentCode = StringBuilder()

                for (rawLine in lines) {
                    val line = rawLine.trim()
                    if (line.isEmpty() || line.startsWith("#") || line.startsWith("//") || line.startsWith(";")) {
                        continue
                    }

                    if (line.startsWith("[") && line.contains("]")) {
                        if (currentName.isNotEmpty() && currentCode.isNotEmpty()) {
                            addCheatItem(currentName, currentCode.toString().trim(), buildId, enabledSet, file)
                        }
                        val closeIdx = line.indexOf(']')
                        currentName = line.substring(1, closeIdx).trim()
                        currentCode.clear()
                    } else {
                        currentCode.append(line).append("\n")
                    }
                }

                if (currentName.isNotEmpty() && currentCode.isNotEmpty()) {
                    addCheatItem(currentName, currentCode.toString().trim(), buildId, enabledSet, file)
                }
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }

        filterCheats(binding.editSearchCheats.text?.toString()?.trim() ?: "")
    }

    private fun addCheatItem(name: String, code: String, buildId: String, enabledSet: Set<String>?, sourceFile: File) {
        if (allCheats.any { it.name == name }) return
        val isEnabled = if (enabledSet != null) {
            enabledSet.contains(name)
        } else {
            false
        }
        allCheats.add(
            CheatModel(
                name = name,
                code = code,
                buildId = buildId,
                isEnabled = isEnabled,
                isCustom = sourceFile.name.contains("custom", ignoreCase = true),
                sourceFile = sourceFile
            )
        )
    }

    @SuppressLint("NotifyDataSetChanged")
    private fun filterCheats(query: String) {
        displayedCheats.clear()
        if (query.isEmpty()) {
            displayedCheats.addAll(allCheats)
        } else {
            displayedCheats.addAll(allCheats.filter { it.name.contains(query, ignoreCase = true) })
        }
        adapter.notifyDataSetChanged()
        binding.layoutEmptyCheats.visibility = if (displayedCheats.isEmpty()) View.VISIBLE else View.GONE
        binding.recyclerCheats.visibility = if (displayedCheats.isEmpty()) View.GONE else View.VISIBLE
        updateSummary()
    }

    private fun updateSummary() {
        val total = allCheats.size
        val enabled = allCheats.count { it.isEnabled }
        binding.textCheatsSummary.text = getString(R.string.cheats_count_summary, total, enabled)
        binding.btnToggleAll.text = if (allCheats.any { !it.isEnabled }) getString(R.string.select_all) else getString(R.string.deselect_all)
    }

    private fun saveCheats() {
        val prefs = PreferenceManager.getDefaultSharedPreferences(YuzuApplication.appContext)
        val enabledNames = allCheats.filter { it.isEnabled }.map { it.name }.toSet()
        prefs.edit().putStringSet("cheats_enabled_${game.programIdHex}", enabledNames).apply()

        try {
            val currentDisabled = NativeConfig.getDisabledAddons(game.programIdHex).toMutableList()
            currentDisabled.removeAll { s: String ->
                s.startsWith("__ENABLED__:") || allCheats.any { it.name == s }
            }
            for (cheat in allCheats) {
                if (cheat.isEnabled) {
                    currentDisabled.add("__ENABLED__:${cheat.name}")
                } else {
                    currentDisabled.add(cheat.name)
                }
            }
            NativeConfig.setDisabledAddons(game.programIdHex, currentDisabled.toTypedArray())
        } catch (e: Exception) {
            e.printStackTrace()
        }

        // Also write active cheats into cheats.txt
        val cheatsDir = getCheatsDir()
        val cheatsFile = File(cheatsDir, "cheats.txt")
        try {
            val content = StringBuilder()
            for (cheat in allCheats) {
                content.append("[${cheat.name}]\n")
                content.append(cheat.code.trim()).append("\n\n")
            }
            cheatsFile.writeText(content.toString(), Charsets.UTF_8)
        } catch (e: Exception) {
            e.printStackTrace()
        }

        if (NativeLibrary.isRunning()) {
            NativeLibrary.reloadProfiles()
            Toast.makeText(requireContext(), "⚡ Чит-коды обновлены в реальном времени!", Toast.LENGTH_SHORT).show()
        } else {
            Toast.makeText(requireContext(), "✅ Настройки чит-кодов сохранены", Toast.LENGTH_SHORT).show()
        }
    }

    private fun downloadOnlineCheats() {
        binding.layoutDownloadStatus.visibility = View.VISIBLE
        binding.btnDownloadOnline.isEnabled = false

        lifecycleScope.launch(Dispatchers.IO) {
            val tid = game.programIdHex.uppercase()
            val urls = listOf(
                "https://tinfoil.media/api/cheats/$tid",
                "https://tinfoil.io/api/cheats/$tid",
                "https://raw.githubusercontent.com/HamletDuFromage/switch-cheats-db/master/cheats/$tid.json",
                "https://cdn.jsdelivr.net/gh/HamletDuFromage/switch-cheats-db@master/cheats/$tid.json",
                "https://raw.githubusercontent.com/HamletDuFromage/switch-cheats-db/master/titles/$tid.txt"
            )

            var success = false
            var loadedCount = 0

            for (urlStr in urls) {
                try {
                    val url = URL(urlStr)
                    val conn = url.openConnection() as HttpURLConnection
                    conn.requestMethod = "GET"
                    conn.connectTimeout = 8000
                    conn.readTimeout = 8000
                    conn.setRequestProperty("User-Agent", "STORM_EDEN_Emulator/4.3.0")

                    if (conn.responseCode == 200) {
                        val reader = BufferedReader(InputStreamReader(conn.inputStream, Charsets.UTF_8))
                        val text = reader.readText().trim()
                        reader.close()

                        if (text.isNotEmpty() && text != "[]" && text != "{}") {
                            val dir = getCheatsDir()
                            if (text.startsWith("[")) {
                                // Tinfoil API format: [ { "name": "...", "build_id": "...", "source": "..." } ]
                                val jsonArr = org.json.JSONArray(text)
                                if (jsonArr.length() > 0) {
                                    val bidMap = mutableMapOf<String, StringBuilder>()
                                    val combined = StringBuilder()
                                    for (i in 0 until jsonArr.length()) {
                                        val item = jsonArr.optJSONObject(i) ?: continue
                                        var cName = item.optString("name", "").trim()
                                        val cBid = item.optString("build_id", "").trim().uppercase()
                                        val cSrc = item.optString("source", "").trim()
                                        if (cName.isEmpty() || cSrc.isEmpty()) continue
                                        if (!cName.startsWith("[")) cName = "[$cName]"
                                        if (!cName.endsWith("]")) cName = "$cName]"

                                        val entry = "$cName\n$cSrc\n\n"
                                        val targetBid = if (cBid.isNotEmpty()) cBid else tid
                                        bidMap.getOrPut(targetBid) { StringBuilder() }.append(entry)
                                        combined.append(entry)
                                        loadedCount++
                                    }

                                    for ((bidKey, content) in bidMap) {
                                        File(dir, "$bidKey.txt").writeText(content.toString(), Charsets.UTF_8)
                                    }
                                    if (combined.isNotEmpty()) {
                                        File(dir, "cheats.txt").writeText(combined.toString(), Charsets.UTF_8)
                                    }
                                    success = true
                                    break
                                }
                            } else if (text.startsWith("{")) {
                                val json = JSONObject(text)
                                val combined = StringBuilder()
                                val keys = json.keys()
                                while (keys.hasNext()) {
                                    val bidKey = keys.next().trim().uppercase()
                                    val bidObj = json.optJSONObject(bidKey)
                                    if (bidObj != null) {
                                        val bidContent = StringBuilder()
                                        val cheatNames = bidObj.keys()
                                        while (cheatNames.hasNext()) {
                                            val cName = cheatNames.next()
                                            val cCode = bidObj.optString(cName, "").trim()
                                            if (cCode.isNotEmpty()) {
                                                bidContent.append(cCode).append("\n\n")
                                                combined.append(cCode).append("\n\n")
                                                loadedCount++
                                            }
                                        }
                                        if (bidContent.isNotEmpty()) {
                                            File(dir, "$bidKey.txt").writeText(bidContent.toString(), Charsets.UTF_8)
                                        }
                                    }
                                }
                                if (combined.isNotEmpty()) {
                                    File(dir, "cheats.txt").writeText(combined.toString(), Charsets.UTF_8)
                                }
                                success = true
                                break
                            } else {
                                File(dir, "cheats.txt").writeText(text, Charsets.UTF_8)
                                success = true
                                break
                            }
                        }
                    }
                } catch (e: Exception) {
                    e.printStackTrace()
                }
            }

            withContext(Dispatchers.Main) {
                if (_binding == null) return@withContext
                binding.layoutDownloadStatus.visibility = View.GONE
                binding.btnDownloadOnline.isEnabled = true
                if (success) {
                    loadCheatsFromDisk()
                    Toast.makeText(
                        requireContext(),
                        getString(R.string.cheats_download_success, allCheats.size),
                        Toast.LENGTH_LONG
                    ).show()
                } else {
                    Toast.makeText(
                        requireContext(),
                        getString(R.string.cheats_not_found),
                        Toast.LENGTH_LONG
                    ).show()
                }
            }
        }
    }

    private fun showAddCustomCheatDialog() {
        val context = requireContext()
        val layout = android.widget.LinearLayout(context).apply {
            orientation = android.widget.LinearLayout.VERTICAL
            setPadding(40, 20, 40, 10)
        }

        val nameEdit = EditText(context).apply {
            hint = "Название чита (например: Бесконечные жизни)"
            textSize = 14f
        }
        val codeEdit = EditText(context).apply {
            hint = "Код в формате Atmosphere (04000000 12345678 ...)"
            textSize = 12f
            minLines = 4
            typeface = android.graphics.Typeface.MONOSPACE
        }

        layout.addView(nameEdit)
        layout.addView(codeEdit)

        MaterialAlertDialogBuilder(context)
            .setTitle(R.string.add_cheat)
            .setView(layout)
            .setPositiveButton(R.string.ok) { _, _ ->
                val name = nameEdit.text.toString().trim()
                val code = codeEdit.text.toString().trim()
                if (name.isNotEmpty() && code.isNotEmpty()) {
                    val cheatsDir = getCheatsDir()
                    val customFile = File(cheatsDir, "custom.txt")
                    try {
                        val currentText = if (customFile.exists()) customFile.readText(Charsets.UTF_8) else ""
                        customFile.writeText("$currentText\n\n[$name]\n$code\n", Charsets.UTF_8)
                        loadCheatsFromDisk()
                        Toast.makeText(context, "Чит добавлен!", Toast.LENGTH_SHORT).show()
                    } catch (e: Exception) {
                        e.printStackTrace()
                    }
                }
            }
            .setNegativeButton(android.R.string.cancel, null)
            .show()
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    private class CheatsAdapter(
        private val items: List<CheatModel>,
        private val isRunning: Boolean,
        private val onCheckChanged: (CheatModel, Boolean) -> Unit
    ) : RecyclerView.Adapter<CheatsAdapter.CheatViewHolder>() {

        inner class CheatViewHolder(val binding: ItemCheatBinding) : RecyclerView.ViewHolder(binding.root)

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): CheatViewHolder {
            val binding = ItemCheatBinding.inflate(LayoutInflater.from(parent.context), parent, false)
            return CheatViewHolder(binding)
        }

        override fun onBindViewHolder(holder: CheatViewHolder, position: Int) {
            val item = items[position]
            val b = holder.binding

            b.textCheatName.text = item.name
            b.checkboxCheat.setOnCheckedChangeListener(null)
            b.checkboxCheat.isChecked = item.isEnabled

            b.textBuildIdBadge.text = item.buildId
            b.textBuildIdBadge.visibility = if (item.buildId.isNotEmpty()) View.VISIBLE else View.GONE

            if (isRunning && item.isEnabled) {
                b.textCheatStatus.text = "⚡ В игре"
                b.textCheatStatus.setTextColor(0xFF00F2FE.toInt())
            } else if (item.isEnabled) {
                b.textCheatStatus.text = "✅ Включен"
                b.textCheatStatus.setTextColor(0xFF00F2FE.toInt())
            } else {
                b.textCheatStatus.text = "⚪ Выкл"
                b.textCheatStatus.setTextColor(0xFF718096.toInt())
            }

            val lines = item.code.lines().filter { it.isNotBlank() }
            val firstLine = lines.firstOrNull() ?: ""
            b.textCodePreview.text = if (lines.size > 1) "$firstLine (+${lines.size - 1} строк)" else firstLine

            b.checkboxCheat.setOnCheckedChangeListener { _, isChecked ->
                onCheckChanged(item, isChecked)
                if (isRunning && isChecked) {
                    b.textCheatStatus.text = "⚡ В игре"
                    b.textCheatStatus.setTextColor(0xFF00F2FE.toInt())
                } else if (isChecked) {
                    b.textCheatStatus.text = "✅ Включен"
                    b.textCheatStatus.setTextColor(0xFF00F2FE.toInt())
                } else {
                    b.textCheatStatus.text = "⚪ Выкл"
                    b.textCheatStatus.setTextColor(0xFF718096.toInt())
                }
            }

            b.root.setOnClickListener {
                b.checkboxCheat.isChecked = !b.checkboxCheat.isChecked
            }
        }

        override fun getItemCount(): Int = items.size
    }
}
