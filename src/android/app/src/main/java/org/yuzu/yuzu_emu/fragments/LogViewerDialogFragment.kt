// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.fragments

import android.app.Dialog
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.net.Uri
import android.os.Bundle
import android.text.SpannableStringBuilder
import android.text.style.ForegroundColorSpan
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.Window
import android.widget.Toast
import androidx.core.content.FileProvider
import androidx.core.widget.doOnTextChanged
import androidx.fragment.app.DialogFragment
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.databinding.DialogLogViewerBinding
import org.yuzu.yuzu_emu.model.Game
import org.yuzu.yuzu_emu.utils.DirectoryInitialization
import java.io.File

class LogViewerDialogFragment : DialogFragment() {

    private var _binding: DialogLogViewerBinding? = null
    private val binding get() = _binding!!

    private var game: Game? = null
    private var rawLogLines = listOf<String>()
    private var filteredLogLines = listOf<String>()

    private var currentFilterType = FILTER_ALL

    companion object {
        const val TAG = "LogViewerDialogFragment"
        private const val ARG_GAME = "game"

        const val FILTER_ALL = 0
        const val FILTER_ERRORS = 1
        const val FILTER_WARNINGS = 2
        const val FILTER_INFO = 3

        fun newInstance(game: Game?): LogViewerDialogFragment {
            val fragment = LogViewerDialogFragment()
            val args = Bundle().apply {
                if (game != null) {
                    putParcelable(ARG_GAME, game)
                }
            }
            fragment.arguments = args
            return fragment
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        game = arguments?.getParcelable(ARG_GAME)
        setStyle(STYLE_NO_TITLE, 0)
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val dialog = super.onCreateDialog(savedInstanceState)
        dialog.requestWindowFeature(Window.FEATURE_NO_TITLE)
        dialog.window?.let { window ->
            window.setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))
        }
        return dialog
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = DialogLogViewerBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        val isRunning = NativeLibrary.isRunning()
        val gameName = game?.title?.ifBlank { null } ?: game?.programIdHex ?: "STORM EDEN"
        val progId = game?.programIdHex ?: ""
        binding.textLogSubtitle.text = "$gameName ${if (progId.isNotEmpty()) "[$progId]" else ""} | ${if (isRunning) "⚡ В игре" else "⚪ Оффлайн"}"

        binding.btnClose.setOnClickListener { dismiss() }
        binding.btnRefreshLog.setOnClickListener { loadLogFile(true) }
        binding.btnCopyLog.setOnClickListener { copyLogToClipboard() }
        binding.btnShareLog.setOnClickListener { shareLogFile() }
        binding.btnShareMessenger.setOnClickListener { shareLogFile() }

        binding.chipFilterAll.setOnClickListener { setFilter(FILTER_ALL) }
        binding.chipFilterErrors.setOnClickListener { setFilter(FILTER_ERRORS) }
        binding.chipFilterWarnings.setOnClickListener { setFilter(FILTER_WARNINGS) }
        binding.chipFilterInfo.setOnClickListener { setFilter(FILTER_INFO) }

        binding.editSearchLog.doOnTextChanged { text, _, _, _ ->
            val query = text?.toString()?.trim() ?: ""
            binding.btnClearSearch.visibility = if (query.isNotEmpty()) View.VISIBLE else View.GONE
            applyFilter()
        }

        binding.btnClearSearch.setOnClickListener {
            binding.editSearchLog.text?.clear()
        }

        loadLogFile(false)
    }

    private fun setFilter(filterType: Int) {
        currentFilterType = filterType
        applyFilter()
    }

    override fun onStart() {
        super.onStart()
        dialog?.window?.let { window ->
            val dm = resources.displayMetrics
            val isLandscape = resources.configuration.orientation == android.content.res.Configuration.ORIENTATION_LANDSCAPE
            val width = if (isLandscape) (dm.widthPixels * 0.94).toInt().coerceIn(600, 1600) else (dm.widthPixels * 0.96).toInt()
            val height = if (isLandscape) (dm.heightPixels * 0.94).toInt().coerceIn(360, 950) else (dm.heightPixels * 0.92).toInt()
            window.setLayout(width, height)
            window.setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))
            window.setGravity(android.view.Gravity.CENTER)
        }
    }

    private fun getActiveLogFile(): File? {
        val rootDir = DirectoryInitialization.userDirectory ?: requireContext().filesDir.absolutePath
        val logDir = File(rootDir, "log")
        return listOf(
            File(logDir, "storm_eden_log.txt"),
            File(logDir, "eden_log.txt"),
            File(logDir, "yuzu_log.txt")
        ).firstOrNull { it.exists() && it.length() > 0 } ?: File(logDir, "storm_eden_log.txt")
    }

    private fun loadLogFile(scrollToBottom: Boolean) {
        lifecycleScope.launch(Dispatchers.IO) {
            val logFile = getActiveLogFile()
            val lines = if (logFile != null && logFile.exists()) {
                try {
                    logFile.readLines(Charsets.UTF_8).takeLast(2000)
                } catch (_: Exception) {
                    emptyList()
                }
            } else {
                emptyList()
            }

            rawLogLines = lines
            val sizeKb = if (logFile != null && logFile.exists()) logFile.length() / 1024 else 0

            withContext(Dispatchers.Main) {
                if (_binding == null) return@withContext
                binding.textLogStats.text = "Всего строк: ${lines.size} | Размер: ${sizeKb} КБ"
                applyFilter()

                if (scrollToBottom) {
                    binding.scrollLog.post {
                        binding.scrollLog.fullScroll(View.FOCUS_DOWN)
                    }
                }
            }
        }
    }

    private fun applyFilter() {
        val searchQuery = binding.editSearchLog.text?.toString()?.trim() ?: ""

        filteredLogLines = rawLogLines.filter { line ->
            val matchesCategory = when (currentFilterType) {
                FILTER_ERRORS -> line.contains("<Error>", ignoreCase = true) || line.contains("<Critical>", ignoreCase = true)
                FILTER_WARNINGS -> line.contains("<Warning>", ignoreCase = true)
                FILTER_INFO -> line.contains("<Info>", ignoreCase = true)
                else -> true
            }

            val matchesSearch = if (searchQuery.isEmpty()) true else line.contains(searchQuery, ignoreCase = true)

            matchesCategory && matchesSearch
        }

        val ssb = SpannableStringBuilder()
        for (line in filteredLogLines) {
            val start = ssb.length
            ssb.append(line).append("\n")
            val end = ssb.length

            val color = when {
                line.contains("<Error>", ignoreCase = true) || line.contains("<Critical>", ignoreCase = true) -> 0xFFFF6B6B.toInt()
                line.contains("<Warning>", ignoreCase = true) -> 0xFFFFD166.toInt()
                line.contains("<Info>", ignoreCase = true) -> 0xFF4FD1C5.toInt()
                line.contains("<Debug>", ignoreCase = true) -> 0xFFA0AEC0.toInt()
                else -> 0xFFC9D1D9.toInt()
            }
            ssb.setSpan(ForegroundColorSpan(color), start, end, SpannableStringBuilder.SPAN_EXCLUSIVE_EXCLUSIVE)
        }

        val filterLabel = when (currentFilterType) {
            FILTER_ERRORS -> "Ошибки"
            FILTER_WARNINGS -> "Предупреждения"
            FILTER_INFO -> "Информация"
            else -> "Все"
        }
        binding.textLogStats.text = "Отобрано ($filterLabel): ${filteredLogLines.size} из ${rawLogLines.size}"

        if (filteredLogLines.isEmpty()) {
            binding.textLogContent.text = "По выбранному фильтру ничего не найдено"
        } else {
            binding.textLogContent.text = ssb
        }
    }

    private fun copyLogToClipboard() {
        val text = if (filteredLogLines.isNotEmpty()) {
            filteredLogLines.joinToString("\n")
        } else {
            rawLogLines.joinToString("\n")
        }
        if (text.isBlank()) {
            Toast.makeText(requireContext(), "Лог пуст", Toast.LENGTH_SHORT).show()
            return
        }

        val clipboard = requireContext().getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        val clip = ClipData.newPlainText("STORM EDEN Log", text)
        clipboard.setPrimaryClip(clip)
        Toast.makeText(requireContext(), "📋 Лог скопирован в буфер обмена", Toast.LENGTH_SHORT).show()
    }

    private fun shareLogFile() {
        val textToShare = if (filteredLogLines.isNotEmpty()) {
            filteredLogLines.joinToString("\n")
        } else {
            rawLogLines.joinToString("\n")
        }

        if (textToShare.isBlank()) {
            Toast.makeText(requireContext(), "Лог пуст", Toast.LENGTH_SHORT).show()
            return
        }

        lifecycleScope.launch(Dispatchers.IO) {
            try {
                val cacheDir = requireContext().cacheDir
                val shareFile = File(cacheDir, "storm_eden_log.txt")
                shareFile.writeText(textToShare, Charsets.UTF_8)

                val uri: Uri = FileProvider.getUriForFile(
                    requireContext(),
                    "${requireContext().packageName}.fileprovider",
                    shareFile
                )

                withContext(Dispatchers.Main) {
                    val filterSuffix = when (currentFilterType) {
                        FILTER_ERRORS -> " (Только ошибки)"
                        FILTER_WARNINGS -> " (Только предупреждения)"
                        else -> ""
                    }
                    val intent = Intent(Intent.ACTION_SEND).apply {
                        type = "text/plain"
                        putExtra(Intent.EXTRA_STREAM, uri)
                        putExtra(Intent.EXTRA_SUBJECT, "STORM EDEN Log - ${game?.title ?: "Switch"}$filterSuffix")
                        putExtra(Intent.EXTRA_TEXT, textToShare.take(5000))
                        addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
                    }
                    val chooser = Intent.createChooser(intent, "Направить лог в Telegram / MAX / Мессенджеры")
                    chooser.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                    startActivity(chooser)
                }
            } catch (e: Exception) {
                withContext(Dispatchers.Main) {
                    val intent = Intent(Intent.ACTION_SEND).apply {
                        type = "text/plain"
                        putExtra(Intent.EXTRA_TEXT, textToShare)
                    }
                    val chooser = Intent.createChooser(intent, "Направить лог в Telegram / MAX / Мессенджеры")
                    chooser.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                    startActivity(chooser)
                }
            }
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}
