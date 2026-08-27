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
        setStyle(STYLE_NO_TITLE, 0)
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val dialog = super.onCreateDialog(savedInstanceState)
        dialog.requestWindowFeature(android.view.Window.FEATURE_NO_TITLE)
        dialog.window?.let { window ->
            window.setBackgroundDrawable(android.graphics.drawable.ColorDrawable(android.graphics.Color.TRANSPARENT))
        }
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
        val versionStr = game.version.ifEmpty { "" }

        binding.textGameInfo.text = buildString {
            append("ID: ")
            append(game.programIdHex)
            if (versionStr.isNotEmpty()) {
                append(" | Версия: ")
                append(versionStr)
            }
            append(if (isRunning) " | ⚡ В игре" else " | ⚪ Оффлайн")
            append("\n⚠️ Применение читов временно отключено разработчиком для стабильности игр")
        }

        adapter = CheatsAdapter(
            displayedCheats,
            isRunning,
            onCheckChanged = { cheat, isChecked ->
                cheat.isEnabled = isChecked
                updateSummary()
                saveCheatsSilently()
            },
            onEditValueClicked = { cheat ->
                showEditCheatValueDialog(cheat)
            },
            onLongClicked = { cheat ->
                showCheatActionMenu(cheat)
            }
        )

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
            val width = if (isLandscape) (dm.widthPixels * 0.94).toInt().coerceIn(600, 1600) else (dm.widthPixels * 0.96).toInt()
            val height = if (isLandscape) (dm.heightPixels * 0.94).toInt().coerceIn(360, 950) else (dm.heightPixels * 0.92).toInt()
            window.setLayout(width, height)
            window.setBackgroundDrawable(android.graphics.drawable.ColorDrawable(android.graphics.Color.TRANSPARENT))
            window.setGravity(android.view.Gravity.CENTER)

            val lp = window.attributes
            lp.width = width
            lp.height = height
            lp.gravity = android.view.Gravity.CENTER
            window.attributes = lp
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
        
        val enabledFile = File(cheatsDir, "enabled_cheats.txt")
        val enabledSet = if (enabledFile.exists()) {
            try {
                enabledFile.readLines(Charsets.UTF_8).map { it.trim() }.filter { it.isNotEmpty() }.toSet()
            } catch (_: Exception) {
                prefs.getStringSet("cheats_enabled_${game.programIdHex}", null)
            }
        } else {
            prefs.getStringSet("cheats_enabled_${game.programIdHex}", null)
        }

        val filesToRead = mutableListOf<File>()
        cheatsDir.listFiles()?.filter { it.isFile && it.name.endsWith(".txt", ignoreCase = true) && !it.name.equals("enabled_cheats.txt", ignoreCase = true) }?.let { list ->
            // Sort so specific Build ID files come before generic cheats.txt
            val sorted = list.sortedBy { f ->
                if (f.name.equals("cheats.txt", ignoreCase = true) || f.name.equals("custom.txt", ignoreCase = true)) 1 else 0
            }
            filesToRead.addAll(sorted)
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

    private fun saveCheatsSilently() {
        val prefs = PreferenceManager.getDefaultSharedPreferences(YuzuApplication.appContext)
        val enabledNames = allCheats.filter { it.isEnabled }.map { it.name }.toSet()
        prefs.edit().putStringSet("cheats_enabled_${game.programIdHex}", enabledNames).apply()

        val rootDir = DirectoryInitialization.userDirectory ?: YuzuApplication.appContext.filesDir.absolutePath
        val cheatsDir = getCheatsDir()
        val loadCheatsDir = File(rootDir, "load/${game.programIdHex}/cheats")
        val atmoCheatsDir = File(rootDir, "sdmc/atmosphere/contents/${game.programIdHex}/cheats")
        val atmoCheatsLowerDir = File(rootDir, "sdmc/atmosphere/contents/${game.programIdHex.lowercase()}/cheats")

        val targetDirs = listOf(cheatsDir, loadCheatsDir, atmoCheatsDir, atmoCheatsLowerDir)
        for (d in targetDirs) {
            if (!d.exists()) d.mkdirs()
        }

        // 1. Write enabled_cheats.txt for direct C++ PatchManager synchronization
        try {
            val enabledLines = mutableListOf<String>()
            for (name in enabledNames) {
                enabledLines.add(name)
                enabledLines.add("[$name]")
            }
            val textToWrite = enabledLines.joinToString("\n")
            for (d in targetDirs) {
                File(d, "enabled_cheats.txt").writeText(textToWrite, Charsets.UTF_8)
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }

        // 2. Also write full list into cheats.txt and per-buildId files
        try {
            val content = StringBuilder()
            val buildMap = mutableMapOf<String, StringBuilder>()

            for (cheat in allCheats) {
                content.append("[${cheat.name}]\n")
                content.append(cheat.code.trim()).append("\n\n")

                if (cheat.buildId.isNotBlank()) {
                    val bsb = buildMap.getOrPut(cheat.buildId) { StringBuilder() }
                    bsb.append("[${cheat.name}]\n").append(cheat.code.trim()).append("\n\n")
                }
            }

            for (d in targetDirs) {
                File(d, "cheats.txt").writeText(content.toString(), Charsets.UTF_8)
                for ((bId, bContent) in buildMap) {
                    File(d, "$bId.txt").writeText(bContent.toString(), Charsets.UTF_8)
                    File(d, "${bId.lowercase()}.txt").writeText(bContent.toString(), Charsets.UTF_8)
                }
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }

        // 3. Reload in-game memory cheats in real-time if emulation is running
        if (NativeLibrary.isRunning()) {
            try {
                NativeLibrary.reloadCheats()
                NativeLibrary.reloadProfiles()
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }
    }

    private fun saveCheats() {
        saveCheatsSilently()
        if (NativeLibrary.isRunning()) {
            Toast.makeText(requireContext(), "⚡ Чит-коды активированы в игре!", Toast.LENGTH_SHORT).show()
        } else {
            Toast.makeText(requireContext(), "✅ Чит-коды успешно сохранены", Toast.LENGTH_SHORT).show()
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
                "https://fastly.jsdelivr.net/gh/HamletDuFromage/switch-cheats-db@master/cheats/$tid.json",
                "https://raw.githubusercontent.com/HamletDuFromage/switch-cheats-db/master/titles/$tid.txt",
                "https://raw.githubusercontent.com/ibnux/switch-cheat/master/contents/$tid/cheats.txt",
                "https://raw.githubusercontent.com/astranvg/Cheats-Atmosphere/master/cheats/$tid.txt",
                "https://raw.githubusercontent.com/mrdude2478/Breeze/master/cheats/$tid.txt"
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
                    conn.setRequestProperty("User-Agent", "STORM_EDEN_Emulator/4.3.1")

                    if (conn.responseCode == 200) {
                        val reader = BufferedReader(InputStreamReader(conn.inputStream, Charsets.UTF_8))
                        val text = reader.readText().trim()
                        reader.close()

                        if (text.isNotEmpty() && text != "[]" && text != "{}") {
                            val rootDir = DirectoryInitialization.userDirectory ?: YuzuApplication.appContext.filesDir.absolutePath
                            val dir = getCheatsDir()
                            val loadCheatsDir = File(rootDir, "load/$tid/cheats")
                            val atmoCheatsDir = File(rootDir, "sdmc/atmosphere/contents/$tid/cheats")
                            val atmoCheatsLowerDir = File(rootDir, "sdmc/atmosphere/contents/${tid.lowercase()}/cheats")
                            val allDirs = listOf(dir, loadCheatsDir, atmoCheatsDir, atmoCheatsLowerDir)
                            for (d in allDirs) {
                                if (!d.exists()) d.mkdirs()
                            }

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
                                        for (d in allDirs) {
                                            File(d, "$bidKey.txt").writeText(content.toString(), Charsets.UTF_8)
                                            File(d, "${bidKey.lowercase()}.txt").writeText(content.toString(), Charsets.UTF_8)
                                        }
                                    }
                                    if (combined.isNotEmpty()) {
                                        for (d in allDirs) {
                                            File(d, "cheats.txt").writeText(combined.toString(), Charsets.UTF_8)
                                        }
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
                                            for (d in allDirs) {
                                                File(d, "$bidKey.txt").writeText(bidContent.toString(), Charsets.UTF_8)
                                                File(d, "${bidKey.lowercase()}.txt").writeText(bidContent.toString(), Charsets.UTF_8)
                                            }
                                        }
                                    }
                                }
                                if (combined.isNotEmpty()) {
                                    for (d in allDirs) {
                                        File(d, "cheats.txt").writeText(combined.toString(), Charsets.UTF_8)
                                    }
                                }
                                success = true
                                break
                            } else {
                                for (d in allDirs) {
                                    File(d, "cheats.txt").writeText(text, Charsets.UTF_8)
                                }
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

    private fun showEditCheatValueDialog(item: CheatModel) {
        val context = requireContext()
        val layout = android.widget.LinearLayout(context).apply {
            orientation = android.widget.LinearLayout.VERTICAL
            setPadding(48, 24, 48, 12)
        }

        val lines = item.code.lines().filter { it.isNotBlank() }
        var targetLineIdx = -1
        var existingHex = ""

        for (i in lines.indices) {
            val tokens = lines[i].trim().split("\\s+".toRegex())
            if (tokens.size >= 3 && (tokens[0].startsWith("04") || tokens[0].startsWith("08") || tokens[0].startsWith("02") || tokens[0].startsWith("01") || tokens[0].startsWith("58") || tokens[0].startsWith("78"))) {
                targetLineIdx = i
                existingHex = tokens.last()
                break
            } else if (tokens.size == 2) {
                targetLineIdx = i
                existingHex = tokens.last()
                break
            }
        }

        val existingDec = try {
            existingHex.toLong(16)
        } catch (_: Exception) {
            0L
        }

        val titleTv = com.google.android.material.textview.MaterialTextView(context).apply {
            text = "Текущее значение: $existingDec (0x$existingHex)"
            textSize = 13f
            setTextColor(0xFF38BDF8.toInt())
            setPadding(0, 0, 0, 16)
        }
        layout.addView(titleTv)

        val inputLayout = com.google.android.material.textfield.TextInputLayout(context).apply {
            hint = "Количество / Новое значение"
            boxBackgroundMode = com.google.android.material.textfield.TextInputLayout.BOX_BACKGROUND_OUTLINE
        }
        val inputEdit = com.google.android.material.textfield.TextInputEditText(context).apply {
            inputType = android.text.InputType.TYPE_CLASS_NUMBER
            setText(if (existingDec > 0) existingDec.toString() else "999999")
            textSize = 15f
        }
        inputLayout.addView(inputEdit)
        layout.addView(inputLayout)

        val presetsLayout = android.widget.LinearLayout(context).apply {
            orientation = android.widget.LinearLayout.HORIZONTAL
            setPadding(0, 16, 0, 0)
        }
        val presets = listOf("1000", "99999", "999999", "99999999")
        for (p in presets) {
            val btn = com.google.android.material.button.MaterialButton(context, null, com.google.android.material.R.attr.materialButtonOutlinedStyle).apply {
                text = p
                textSize = 10f
                setPadding(12, 0, 12, 0)
                layoutParams = android.widget.LinearLayout.LayoutParams(0, android.widget.LinearLayout.LayoutParams.WRAP_CONTENT, 1f).apply {
                    setMargins(4, 0, 4, 0)
                }
                setOnClickListener {
                    inputEdit.setText(p)
                }
            }
            presetsLayout.addView(btn)
        }
        layout.addView(presetsLayout)

        MaterialAlertDialogBuilder(context)
            .setTitle("✏️ Изменить кол-во: ${item.name}")
            .setView(layout)
            .setPositiveButton(R.string.apply_driver_now) { _, _ ->
                val entered = inputEdit.text.toString().trim().toLongOrNull() ?: 0L
                val hexLen = if (existingHex.isNotEmpty()) existingHex.length else 8
                val newHex = java.lang.Long.toHexString(entered).uppercase().padStart(hexLen, '0')

                val updatedLines = lines.toMutableList()
                if (targetLineIdx in updatedLines.indices) {
                    val tokens = updatedLines[targetLineIdx].trim().split("\\s+".toRegex()).toMutableList()
                    if (tokens.isNotEmpty()) {
                        tokens[tokens.size - 1] = newHex
                        updatedLines[targetLineIdx] = tokens.joinToString(" ")
                    }
                } else if (updatedLines.isNotEmpty()) {
                    val tokens = updatedLines[0].trim().split("\\s+".toRegex()).toMutableList()
                    tokens[tokens.size - 1] = newHex
                    updatedLines[0] = tokens.joinToString(" ")
                }

                val newCode = updatedLines.joinToString("\n")
                item.code = newCode

                updateCheatInFile(item)
                saveCheats()
                adapter.notifyDataSetChanged()
                Toast.makeText(context, "Значение обновлено: $entered (0x$newHex)", Toast.LENGTH_SHORT).show()
            }
            .setNegativeButton(android.R.string.cancel, null)
            .show()
    }

    private fun updateCheatInFile(item: CheatModel) {
        try {
            val file = item.sourceFile
            if (file != null && file.exists()) {
                val lines = file.readLines(Charsets.UTF_8)
                val out = StringBuilder()
                var inTarget = false
                for (line in lines) {
                    val trimmed = line.trim()
                    if (trimmed.startsWith("[") && trimmed.contains("]")) {
                        val name = trimmed.substring(1, trimmed.indexOf(']')).trim()
                        if (name == item.name) {
                            inTarget = true
                            out.append("[$name]\n").append(item.code).append("\n\n")
                            continue
                        } else {
                            inTarget = false
                        }
                    }
                    if (!inTarget) {
                        out.append(line).append("\n")
                    }
                }
                file.writeText(out.toString().trim() + "\n", Charsets.UTF_8)
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    private fun showCheatActionMenu(item: CheatModel) {
        val options = arrayOf("✏️ Изменить кол-во / значение", "📋 Копировать код", "🗑️ Удалить чит")
        MaterialAlertDialogBuilder(requireContext())
            .setTitle(item.name)
            .setItems(options) { _, which ->
                when (which) {
                    0 -> showEditCheatValueDialog(item)
                    1 -> {
                        val cm = requireContext().getSystemService(android.content.Context.CLIPBOARD_SERVICE) as android.content.ClipboardManager
                        cm.setPrimaryClip(android.content.ClipData.newPlainText("Cheat", "[${item.name}]\n${item.code}"))
                        Toast.makeText(requireContext(), "Код скопирован", Toast.LENGTH_SHORT).show()
                    }
                    2 -> {
                        deleteCheat(item)
                    }
                }
            }
            .show()
    }

    private fun deleteCheat(item: CheatModel) {
        allCheats.remove(item)
        displayedCheats.remove(item)
        try {
            val file = item.sourceFile
            if (file != null && file.exists()) {
                val lines = file.readLines(Charsets.UTF_8)
                val out = StringBuilder()
                var inTarget = false
                for (line in lines) {
                    val trimmed = line.trim()
                    if (trimmed.startsWith("[") && trimmed.contains("]")) {
                        val name = trimmed.substring(1, trimmed.indexOf(']')).trim()
                        inTarget = (name == item.name)
                    }
                    if (!inTarget) {
                        out.append(line).append("\n")
                    }
                }
                file.writeText(out.toString().trim() + "\n", Charsets.UTF_8)
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
        saveCheats()
        adapter.notifyDataSetChanged()
        updateSummary()
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    private class CheatsAdapter(
        private val items: List<CheatModel>,
        private val isRunning: Boolean,
        private val onCheckChanged: (CheatModel, Boolean) -> Unit,
        private val onEditValueClicked: (CheatModel) -> Unit,
        private val onLongClicked: (CheatModel) -> Unit
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

            val localizedDesc = getLocalizedCheatDescription(item.name, item.code)
            b.textCheatDescription.text = localizedDesc
            b.textCheatDescription.visibility = if (localizedDesc.isNotEmpty()) View.VISIBLE else View.GONE

            val lines = item.code.lines().filter { it.isNotBlank() }
            val firstLine = lines.firstOrNull() ?: ""
            b.textCodePreview.text = if (lines.size > 1) "$firstLine (+${lines.size - 1} строк)" else firstLine

            // Show value editor button ONLY for numeric quantity cheats (money, items, ammo, stats, etc.)
            val isEditable = isNumericQuantityCheat(item.name, item.code)
            b.btnEditValue.visibility = if (isEditable && lines.isNotEmpty()) View.VISIBLE else View.GONE
            b.btnEditValue.setOnClickListener {
                onEditValueClicked(item)
            }

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

            b.root.setOnLongClickListener {
                onLongClicked(item)
                true
            }
        }

        override fun getItemCount(): Int = items.size

        private fun getLocalizedCheatDescription(name: String, code: String): String {
            val lower = name.lowercase().trim()
            val baseDesc = when {
                lower.contains("60fps") || lower.contains("60 fps") -> "Режим 60 кадров в секунду (60 FPS)"
                lower.contains("30fps") || lower.contains("30 fps") -> "Фиксация 30 кадров в секунду (30 FPS)"
                lower.contains("120fps") || lower.contains("120 fps") -> "Режим 120 кадров в секунду (120 FPS)"
                lower.contains("unlock fps") || lower.contains("uncap fps") || lower.contains("no vsync") -> "Разблокировка частоты кадров"
                lower.contains("dynamic res") || lower.contains("dynamic resolution") || lower.contains("drs") -> "Отключение динамического разрешения"
                lower.contains("lod") || lower.contains("draw distance") || lower.contains("render distance") -> "Увеличенная дальность прорисовки (LOD)"
                lower.contains("god mode") || lower.contains("godmode") || lower.contains("invincib") || lower.contains("no damage") -> "Бессмертие (Режим Бога)"
                lower.contains("inf health") || lower.contains("inf hp") || lower.contains("infinite health") || lower.contains("infinite hp") || lower.contains("max hp") -> "Бесконечное здоровье (HP)"
                lower.contains("inf stam") || lower.contains("infinite stam") || lower.contains("max stam") || lower.contains("stamina") -> "Бесконечная выносливость"
                lower.contains("inf mana") || lower.contains("infinite mana") || lower.contains("inf mp") || lower.contains("infinite mp") || lower.contains("max mp") -> "Бесконечная мана (MP)"
                lower.contains("shield") || lower.contains("armor") || lower.contains("defense") || lower.contains("def") -> "Максимальная броня / Защита"
                lower.contains("money") || lower.contains("gold") || lower.contains("cash") || lower.contains("coins") || lower.contains("rupees") || lower.contains("zenny") || lower.contains("credits") || lower.contains("currency") -> "Деньги / Золото"
                lower.contains("ammo") || lower.contains("bullet") || lower.contains("arrows") || lower.contains("shots") -> "Патроны / Боеприпасы"
                lower.contains("all items") || lower.contains("inf item") || lower.contains("item x99") || lower.contains("material") || lower.contains("crafting") -> "Бесконечные предметы и ресурсы"
                lower.contains("durability") || lower.contains("inf weapon") || lower.contains("never break") -> "Неразрушимое оружие (Бесконечная прочность)"
                lower.contains("moon jump") || lower.contains("super jump") || lower.contains("high jump") || lower.contains("inf jump") -> "Лунный / Супер прыжок"
                lower.contains("no clip") || lower.contains("noclip") || lower.contains("walk through") || lower.contains("fly") || lower.contains("flight") -> "Прохождение сквозь стены / Полет"
                lower.contains("speed") || lower.contains("fast move") || lower.contains("movement") || lower.contains("fast run") -> "Увеличенная скорость движения"
                lower.contains("one hit") || lower.contains("1 hit") || lower.contains("instant kill") || lower.contains("one shot") -> "Убийство с одного удара"
                lower.contains("attack") || lower.contains("damage") || lower.contains("dmg") || lower.contains("power") || lower.contains("strength") -> "Увеличенный урон / Сила атаки"
                lower.contains("cooldown") || lower.contains("no cd") || lower.contains("instant skill") -> "Без перезарядки способностей (No Cooldown)"
                lower.contains("skill point") || lower.contains("sp") || lower.contains("ap") || lower.contains("talent") -> "Очки навыков / Способности (SP/AP)"
                lower.contains("level") || lower.contains("lvl") || lower.contains("max level") || lower.contains("exp") || lower.contains("experience") -> "Уровень / Опыт"
                lower.contains("fog") || lower.contains("disable fog") || lower.contains("remove fog") || lower.contains("no fog") -> "Отключение тумана и дымки"
                lower.contains("widescreen") || lower.contains("21:9") || lower.contains("ultrawide") -> "Поддержка широкого экрана 21:9"
                lower.contains("fov") || lower.contains("camera") || lower.contains("zoom") -> "Настройка угла обзора и камеры (FOV)"
                lower.contains("drop rate") || lower.contains("100% drop") || lower.contains("max drop") || lower.contains("lucky") -> "100% шанс выпадения редких предметов"
                lower.contains("unlock") || lower.contains("all characters") || lower.contains("costumes") || lower.contains("gallery") -> "Разблокировка всего контента"
                lower.contains("timer") || lower.contains("freeze time") || lower.contains("inf time") -> "Заморозка таймера (Бесконечное время)"
                lower.contains("master") || lower.contains("main code") -> "Главный мастер-код (Master Code)"
                else -> {
                    val firstLine = code.lines().firstOrNull { it.isNotBlank() }?.trim() ?: ""
                    when {
                        firstLine.startsWith("04") -> "Патч памяти: прямая запись 32-бит"
                        firstLine.startsWith("08") -> "Патч памяти: прямая запись 64-бит"
                        firstLine.startsWith("01") || firstLine.startsWith("02") -> "Патч регистров памяти"
                        firstLine.startsWith("58") -> "Патч указателя / смещения памяти"
                        else -> "Пользовательский чит-код Atmosphere"
                    }
                }
            }

            if (isNumericQuantityCheat(name, code)) {
                val value = extractNumericValue(code)
                if (value > 0) {
                    val prefix = if (baseDesc.isNotEmpty()) baseDesc else name
                    return "$prefix (Кол-во: $value)"
                }
            }

            return baseDesc
        }

        private fun extractNumericValue(code: String): Long {
            val lines = code.lines().filter { it.isNotBlank() }
            for (line in lines) {
                val tokens = line.trim().split("\\s+".toRegex())
                if (tokens.size >= 2) {
                    val lastToken = tokens.last()
                    val dec = try { lastToken.toLong(16) } catch (_: Exception) { 0L }
                    if (dec > 0) return dec
                }
            }
            return 0L
        }

        private fun isNumericQuantityCheat(name: String, code: String): Boolean {
            val lowerName = name.lowercase()

            val toggleKeywords = listOf(
                "fps", "framerate", "frame rate", "60fps", "30fps", "120fps", "uncap", "speed limit",
                "resolution", "dynamic res", "widescreen", "aspect", "hdr", "bloom", "fog", "vsync",
                "invincible", "god mode", "godmode", "no damage", "infinite", "unlimited",
                "moon jump", "noclip", "no clip", "freeze", "unlock all", "all items", "100%", "skip"
            )
            if (toggleKeywords.any { lowerName.contains(it) }) {
                return false
            }

            val quantityKeywords = listOf(
                "money", "gold", "cash", "coin", "rupee", "dollar", "zenny", "gil", "bell", "credit",
                "token", "point", "gem", "shard", "cap", "currency", "деньги", "золото", "монет",
                "hp", "health", "mp", "sp", "mana", "stamina", "exp", "level", "lvl", "stat",
                "atk", "def", "speed", "опыт", "здоровье", "мана", "уровен",
                "item", "amount", "quantity", "count", "ammo", "bullet", "arrow", "bomb", "potion",
                "предмет", "патрон", "стрел", "количеств", "score", "life", "lives"
            )
            if (quantityKeywords.any { lowerName.contains(it) }) {
                return true
            }

            val lines = code.lines().filter { it.isNotBlank() }
            if (lines.size == 1) {
                val tokens = lines[0].trim().split("\\s+".toRegex())
                if (tokens.size >= 3 && (tokens[0].startsWith("04") || tokens[0].startsWith("02") || tokens[0].startsWith("01"))) {
                    return true
                }
            }
            return false
        }
    }
}
