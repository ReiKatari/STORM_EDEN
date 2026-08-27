// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.features.fetcher

import android.animation.LayoutTransition
import android.content.res.ColorStateList
import android.text.Html
import android.text.Html.FROM_HTML_MODE_COMPACT
import android.text.TextUtils
import android.view.LayoutInflater
import android.view.ViewGroup
import android.widget.Toast
import androidx.core.view.isVisible
import androidx.fragment.app.FragmentActivity
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.button.MaterialButton
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.databinding.ItemReleaseBinding
import org.yuzu.yuzu_emu.fragments.DriverFetcherFragment.Release
import androidx.core.net.toUri
import androidx.transition.ChangeBounds
import androidx.transition.Fade
import androidx.transition.TransitionManager
import androidx.transition.TransitionSet
import com.google.android.material.color.MaterialColors
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request
import org.commonmark.parser.Parser
import org.commonmark.renderer.html.HtmlRenderer
import org.yuzu.yuzu_emu.databinding.DialogProgressBinding
import org.yuzu.yuzu_emu.model.DriverViewModel
import org.yuzu.yuzu_emu.utils.FileUtil
import org.yuzu.yuzu_emu.utils.GpuDriverHelper
import java.io.File
import java.io.FileOutputStream
import java.io.IOException

class ReleaseAdapter(
    private val releases: List<Release>,
    private val activity: FragmentActivity,
    private val driverViewModel: DriverViewModel
) : RecyclerView.Adapter<ReleaseAdapter.ReleaseViewHolder>() {

    inner class ReleaseViewHolder(
        private val binding: ItemReleaseBinding
    ) : RecyclerView.ViewHolder(binding.root) {
        private var isPreview: Boolean = true
        private val client = OkHttpClient()
        private val markdownParser = Parser.builder().build()
        private val htmlRenderer = HtmlRenderer.builder().build()

        init {
            binding.root.let { root ->
                val layoutTransition = root.layoutTransition ?: LayoutTransition().apply {
                    enableTransitionType(LayoutTransition.CHANGING)
                    setDuration(125)
                }
                root.layoutTransition = layoutTransition
            }

            (binding.textBody.parent as ViewGroup).isTransitionGroup = false
            binding.containerDownloads.isTransitionGroup = false
        }

        fun bind(release: Release) {
            binding.textReleaseName.text = release.title
            binding.badgeLatest.isVisible = release.latest

            // truncates to 150 chars so it does not take up too much space.
            var bodyPreview = release.body.take(150)
            bodyPreview = bodyPreview.replace("#", "").removeSurrounding(" ")

            val body =
                bodyPreview.replace("\\r\\n", "\n").replace("\\n", "\n").replace("\n", "<br>")

            binding.textBody.text = Html.fromHtml(body, FROM_HTML_MODE_COMPACT)

            binding.textBody.setOnClickListener {
                TransitionManager.beginDelayedTransition(
                    binding.root,
                    TransitionSet().addTransition(Fade()).addTransition(ChangeBounds())
                        .setDuration(100)
                )

                isPreview = !isPreview
                if (isPreview) {
                    val body = bodyPreview.replace("\\r\\n", "\n").replace("\\n", "\n")
                        .replace("\n", "<br>")

                    binding.textBody.text = Html.fromHtml(body, FROM_HTML_MODE_COMPACT)
                    binding.textBody.maxLines = 3
                    binding.textBody.ellipsize = TextUtils.TruncateAt.END
                } else {
                    val body = release.body.replace("\\r\\n", "\n\n").replace("\\n", "\n\n")

                    try {
                        val doc = markdownParser.parse(body)
                        val html = htmlRenderer.render(doc)
                        binding.textBody.text = Html.fromHtml(html, Html.FROM_HTML_MODE_COMPACT)
                    } catch (e: Exception) {
                        e.printStackTrace()
                        binding.textBody.text = body
                    }

                    binding.textBody.maxLines = Integer.MAX_VALUE
                    binding.textBody.ellipsize = null
                }
            }

            val onDownloadsClick = {
                val isVisible = binding.containerDownloads.isVisible
                TransitionManager.beginDelayedTransition(
                    binding.root,
                    TransitionSet().addTransition(Fade()).addTransition(ChangeBounds())
                        .setDuration(100)
                )

                binding.containerDownloads.isVisible = !isVisible

                binding.imageDownloadsArrow.rotation = if (isVisible) 0f else 180f
                binding.buttonToggleDownloads.text =
                    if (isVisible) {
                        activity.getString(R.string.show_downloads)
                    } else {
                        activity.getString(R.string.hide_downloads)
                    }
            }

            binding.buttonToggleDownloads.setOnClickListener {
                onDownloadsClick()
            }

            binding.imageDownloadsArrow.setOnClickListener {
                onDownloadsClick()
            }

            binding.containerDownloads.removeAllViews()

            release.artifacts.forEach { artifact ->
                val currentDriverPath = org.yuzu.yuzu_emu.features.settings.model.StringSetting.DRIVER_PATH.getString()
                val customMeta = GpuDriverHelper.customDriverSettingData

                val installedDriverEntry = try {
                    driverViewModel.driverData.firstOrNull {
                        File(it.first).name.equals(artifact.name, ignoreCase = true) ||
                        (!it.second.name.isNullOrEmpty() && artifact.name.contains(it.second.name!!, ignoreCase = true) &&
                         !it.second.version.isNullOrEmpty() && artifact.name.contains(it.second.version!!, ignoreCase = true))
                    }
                } catch (_: Exception) {
                    null
                }

                val installedPath = installedDriverEntry?.first ?: if (GpuDriverHelper.isDriverZipInstalledByName(artifact.name)) {
                    "${GpuDriverHelper.driverStoragePath}${artifact.name}"
                } else {
                    null
                }

                val alreadyInstalled = installedPath != null

                val isCurrentlyActive = if (currentDriverPath.isNotEmpty() && alreadyInstalled) {
                    currentDriverPath.equals(installedPath, ignoreCase = true) ||
                    File(currentDriverPath).name.equals(artifact.name, ignoreCase = true) ||
                    (!customMeta.name.isNullOrEmpty() && artifact.name.contains(customMeta.name!!, ignoreCase = true) &&
                     !customMeta.version.isNullOrEmpty() && artifact.name.contains(customMeta.version!!, ignoreCase = true))
                } else {
                    false
                }

                val button = MaterialButton(binding.root.context).apply {
                    text = when {
                        isCurrentlyActive -> context.getString(R.string.in_use_label, artifact.name)
                        alreadyInstalled -> context.getString(R.string.installed_label, artifact.name)
                        else -> artifact.name
                    }
                    setTextAppearance(
                        com.google.android.material.R.style.TextAppearance_Material3_LabelLarge
                    )
                    textAlignment = MaterialButton.TEXT_ALIGNMENT_VIEW_START
                    setBackgroundColor(
                        context.getColor(
                            com.google.android.material.R.color.m3_button_background_color_selector
                        )
                    )
                    setIconResource(if (isCurrentlyActive || alreadyInstalled) R.drawable.ic_check else R.drawable.ic_import)
                    iconTint = ColorStateList.valueOf(
                        MaterialColors.getColor(
                            this,
                            if (isCurrentlyActive) com.google.android.material.R.attr.colorPrimary else com.google.android.material.R.attr.colorPrimary
                        )
                    )

                    elevation = 6f
                    layoutParams = ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT,
                        ViewGroup.LayoutParams.WRAP_CONTENT
                    )
                    isEnabled = true

                    setOnClickListener {
                        if (isCurrentlyActive) {
                            Toast.makeText(
                                context,
                                "⚡ " + context.getString(R.string.in_use_label, artifact.name),
                                Toast.LENGTH_SHORT
                            ).show()
                            return@setOnClickListener
                        }

                        if (alreadyInstalled && installedPath != null) {
                            MaterialAlertDialogBuilder(context)
                                .setTitle(context.getString(R.string.active_driver))
                                .setMessage(context.getString(R.string.apply_driver_confirm, artifact.name))
                                .setPositiveButton(R.string.apply_driver_now) { _, _ ->
                                    org.yuzu.yuzu_emu.features.settings.model.StringSetting.DRIVER_PATH.setString(installedPath)
                                    driverViewModel.updateDriverList()
                                    Toast.makeText(
                                        context,
                                        "⚡ " + context.getString(R.string.in_use_label, artifact.name),
                                        Toast.LENGTH_SHORT
                                    ).show()
                                    notifyDataSetChanged()
                                }
                                .setNegativeButton(android.R.string.cancel, null)
                                .show()
                            return@setOnClickListener
                        }

                        val dialogBinding =
                            DialogProgressBinding.inflate(LayoutInflater.from(context))
                        dialogBinding.progressBar.isIndeterminate = true
                        dialogBinding.title.text = context.getString(R.string.installing_driver)
                        dialogBinding.status.text = context.getString(R.string.downloading)

                        val progressDialog = MaterialAlertDialogBuilder(context)
                            .setView(dialogBinding.root)
                            .setCancelable(false)
                            .create()

                        progressDialog.show()

                        CoroutineScope(Dispatchers.Main).launch {
                            try {
                                val cacheDir = context.externalCacheDir ?: context.cacheDir
                                cacheDir.mkdirs()
                                val file = File(cacheDir, artifact.name)

                                val originalUrl = artifact.url.toString()
                                val downloadMirrors = mutableListOf(originalUrl)
                                if (originalUrl.startsWith("https://github.com/")) {
                                    downloadMirrors.add("https://ghproxy.net/$originalUrl")
                                    downloadMirrors.add("https://gh-proxy.com/$originalUrl")
                                    downloadMirrors.add(originalUrl.replace("https://github.com/", "https://githubfast.com/"))
                                }

                                var downloadSuccess = false
                                var lastException: Exception? = null

                                val downloadClient = OkHttpClient.Builder()
                                    .connectTimeout(45, java.util.concurrent.TimeUnit.SECONDS)
                                    .readTimeout(180, java.util.concurrent.TimeUnit.SECONDS)
                                    .followRedirects(true)
                                    .followSslRedirects(true)
                                    .retryOnConnectionFailure(true)
                                    .build()

                                withContext(Dispatchers.IO) {
                                    for ((index, dlUrl) in downloadMirrors.withIndex()) {
                                        try {
                                            withContext(Dispatchers.Main) {
                                                dialogBinding.status.text = if (index > 0) {
                                                    "Загрузка через зеркало $index..."
                                                } else {
                                                    context.getString(R.string.downloading)
                                                }
                                            }

                                            val req = Request.Builder()
                                                .url(dlUrl)
                                                .header("User-Agent", "Mozilla/5.0 (Linux; Android 14; Mobile) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/128.0.0.0 StormEden/4.4.3")
                                                .header("Accept", "application/octet-stream, */*")
                                                .build()

                                            downloadClient.newCall(req).execute().use { response ->
                                                if (!response.isSuccessful) {
                                                    throw IOException("HTTP ${response.code}")
                                                }

                                                val body = response.body ?: throw IOException(context.getString(R.string.empty_response_body))
                                                val contentLength = body.contentLength()
                                                var totalBytesRead = 0L

                                                body.byteStream().use { input ->
                                                    FileOutputStream(file).use { output ->
                                                        val buffer = ByteArray(64 * 1024)
                                                        var bytesRead: Int
                                                        var lastUpdate = System.currentTimeMillis()

                                                        while (input.read(buffer).also { bytesRead = it } != -1) {
                                                            output.write(buffer, 0, bytesRead)
                                                            totalBytesRead += bytesRead
                                                            val now = System.currentTimeMillis()
                                                            if (now - lastUpdate > 250) {
                                                                lastUpdate = now
                                                                withContext(Dispatchers.Main) {
                                                                    val mbRead = String.format("%.1f", totalBytesRead / (1024.0 * 1024.0))
                                                                    if (contentLength > 0) {
                                                                        val mbTotal = String.format("%.1f", contentLength / (1024.0 * 1024.0))
                                                                        dialogBinding.status.text = "Загрузка: $mbRead / $mbTotal МБ"
                                                                    } else {
                                                                        dialogBinding.status.text = "Загружено: $mbRead МБ"
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }

                                            if (file.exists() && file.length() > 1024L) {
                                                downloadSuccess = true
                                                break
                                            } else {
                                                file.delete()
                                            }
                                        } catch (e: Exception) {
                                            lastException = e
                                            file.delete()
                                        }
                                    }
                                }

                                if (!downloadSuccess) {
                                    throw lastException ?: IOException(context.getString(R.string.driver_empty))
                                }

                                dialogBinding.status.text = context.getString(R.string.installing)

                                val driverData = GpuDriverHelper.getMetadataFromZip(file)
                                val driverPath =
                                    "${GpuDriverHelper.driverStoragePath}${FileUtil.getFilename(
                                        file.toUri()
                                    )}"

                                if (GpuDriverHelper.copyDriverToInternalStorage(file.toUri())) {
                                    driverViewModel.onDriverAdded(Pair(driverPath, driverData))
                                    progressDialog.dismiss()

                                    MaterialAlertDialogBuilder(context)
                                        .setTitle(context.getString(R.string.successfully_installed, driverData.name?.ifEmpty { artifact.name } ?: artifact.name))
                                        .setMessage(context.getString(R.string.apply_driver_confirm, artifact.name))
                                        .setPositiveButton(R.string.apply_driver_now) { _, _ ->
                                            org.yuzu.yuzu_emu.features.settings.model.StringSetting.DRIVER_PATH.setString(driverPath)
                                            driverViewModel.updateDriverList()
                                            notifyDataSetChanged()
                                        }
                                        .setNegativeButton(R.string.close) { _, _ ->
                                            notifyDataSetChanged()
                                        }
                                        .show()
                                } else {
                                    throw IOException(
                                        context.getString(
                                            R.string.failed_install_driver,
                                            artifact.name
                                        )
                                    )
                                }
                            } catch (e: Exception) {
                                progressDialog.dismiss()

                                MaterialAlertDialogBuilder(context)
                                    .setTitle(context.getString(R.string.driver_failed_title))
                                    .setMessage(e.message)
                                    .setPositiveButton(R.string.ok) { dialog, _ ->
                                        dialog.cancel()
                                    }
                                    .show()
                            }
                        }
                    }
                }
                binding.containerDownloads.addView(button)
            }
        }
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ReleaseViewHolder {
        val binding = ItemReleaseBinding.inflate(
            LayoutInflater.from(parent.context),
            parent,
            false
        )
        return ReleaseViewHolder(binding)
    }

    override fun onBindViewHolder(holder: ReleaseViewHolder, position: Int) {
        holder.bind(releases[position])
    }

    override fun getItemCount(): Int = releases.size
}
