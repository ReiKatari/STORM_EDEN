// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.fragments

import android.os.Bundle
import androidx.fragment.app.Fragment
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.isVisible
import androidx.core.view.updatePadding
import androidx.fragment.app.activityViewModels
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import com.fasterxml.jackson.databind.JsonNode
import com.fasterxml.jackson.module.kotlin.jacksonObjectMapper
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.transition.MaterialSharedAxis
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.databinding.FragmentDriverFetcherBinding
import org.yuzu.yuzu_emu.features.fetcher.DriverGroupAdapter
import org.yuzu.yuzu_emu.model.DriverViewModel
import org.yuzu.yuzu_emu.model.HomeViewModel
import org.yuzu.yuzu_emu.utils.GpuDriverHelper
import org.yuzu.yuzu_emu.utils.ViewUtils.updateMargins
import java.io.IOException
import java.net.URL
import java.time.Instant
import java.time.LocalDateTime
import java.time.ZoneId
import kotlin.getValue

class DriverFetcherFragment : Fragment() {
    private var _binding: FragmentDriverFetcherBinding? = null
    private val binding get() = _binding!!

    private val gpuModel: String?
        get() = GpuDriverHelper.hookLibPath?.let { GpuDriverHelper.getGpuModel(hookLibPath = it) }

    private val adrenoModel: Int
        get() = parseAdrenoModel()

    private val recommendedDriver: String
        get() = driverMap.firstOrNull { adrenoModel in it.first }?.second ?: "Unsupported"

    private val activeDriverInfo: String
        get() {
            val customData = GpuDriverHelper.customDriverSettingData
            val path = org.yuzu.yuzu_emu.features.settings.model.StringSetting.DRIVER_PATH.getString()
            return if (path.isEmpty() || customData == org.yuzu.yuzu_emu.utils.GpuDriverMetadata()) {
                val sys = GpuDriverHelper.getSystemDriverInfo()
                val sysVer = org.yuzu.yuzu_emu.NativeLibrary.getVulkanDriverVersion().takeIf { !it.isNullOrEmpty() }
                    ?: sys?.get(0) ?: getString(R.string.system_gpu_driver)
                "${getString(R.string.system_gpu_driver)} ($sysVer)"
            } else {
                val vendor = customData.vendor?.takeIf { it.isNotBlank() } ?: "Custom"
                val name = customData.name?.takeIf { it.isNotBlank() } ?: java.io.File(path).name
                val version = customData.version?.takeIf { it.isNotBlank() } ?: ""
                "$name $version [$vendor]"
            }
        }

    enum class SortMode {
        Default, PublishTime,
    }

    private data class DriverRepo(
        val name: String = "",
        val path: String = "",
        val sort: Int = 0,
        val useTagName: Boolean = false,
        val sortMode: SortMode = SortMode.Default
    )

    private val repoList: List<DriverRepo> = listOf(
        DriverRepo("STORM DRIVER", "ReiKatari/STORM_DRIVER", 0, true, SortMode.PublishTime),
        DriverRepo("Balemuni Turnip (Apex)", "Balemuni/Balemunis-Aurora", 1),
        DriverRepo("KIMCHI Turnip Drivers", "K11MCH1/AdrenoToolsDrivers", 2, true, SortMode.PublishTime),
        DriverRepo("Mr. Purple Turnip", "MrPurple666/purple-turnip", 3),
        DriverRepo("Weab-Chan Freedreno CI", "Weab-chan/freedreno_turnip-CI", 4),
        DriverRepo("Whitebelyash Turnip Hub", "whitebelyash/freedreno_turnip-CI", 5, false, SortMode.PublishTime),
        DriverRepo("GameHub Adreno 8xx", "crueter/GameHub-8Elite-Drivers", 6),
    )

    private val driverMap = listOf(
        IntRange(Integer.MIN_VALUE, 9) to "Unsupported",
        IntRange(10, 99) to "STORM DRIVER / KIMCHI Turnip", // Special case for Adreno Axx
        IntRange(100, 599) to "Unsupported",
        IntRange(600, 639) to "Mr. Purple EOL-24.3.4",
        IntRange(640, 699) to "STORM DRIVER / KIMCHI",
        IntRange(700, 799) to "STORM DRIVER / Balemuni Apex",
        IntRange(800, 899) to "STORM DRIVER / Balemuni Apex v2",
        IntRange(900, Int.MAX_VALUE) to "STORM DRIVER / Turnip Latest"
    )

    private lateinit var driverGroupAdapter: DriverGroupAdapter
    private val driverViewModel: DriverViewModel by activityViewModels()
    private val homeViewModel: HomeViewModel by activityViewModels()

    private fun parseAdrenoModel(): Int {
        if (gpuModel == null) {
            return 0
        }

        val modelList = gpuModel!!.split(" ")

        // format: Adreno (TM) <ModelNumber>
        if (modelList.size < 3 || modelList[0] != "Adreno") {
            return 0
        }

        val model = modelList[2]

        try {
            // special case for Axx GPUs (e.g. AYANEO Pocket S2)
            // driverMap has specific ranges for this
            if (model.startsWith("A")) {
                return model.substring(1).toInt()
            }

            return model.toInt()
        } catch (e: Exception) {
            // Model parse error, just say unsupported
            e.printStackTrace()
            return 0
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enterTransition = MaterialSharedAxis(MaterialSharedAxis.X, true)
        returnTransition = MaterialSharedAxis(MaterialSharedAxis.X, false)
        reenterTransition = MaterialSharedAxis(MaterialSharedAxis.X, false)
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentDriverFetcherBinding.inflate(inflater)
        binding.badgeRecommendedDriver.text = recommendedDriver
        binding.badgeGpuModel.text = gpuModel ?: "Qualcomm Adreno"
        binding.badgeActiveDriver.text = activeDriverInfo

        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        homeViewModel.setStatusBarShadeVisibility(visible = false)
        binding.toolbarDrivers.setNavigationOnClickListener {
            requireActivity().onBackPressedDispatcher.onBackPressed()
        }

        binding.listDrivers.layoutManager = LinearLayoutManager(context)
        driverGroupAdapter = DriverGroupAdapter(requireActivity(), driverViewModel)
        binding.listDrivers.adapter = driverGroupAdapter

        setInsets()

        viewLifecycleOwner.lifecycleScope.launch {
            viewLifecycleOwner.repeatOnLifecycle(androidx.lifecycle.Lifecycle.State.STARTED) {
                driverViewModel.driverList.collect {
                    binding.badgeActiveDriver.text = activeDriverInfo
                }
            }
        }

        fetchDrivers()
    }

    override fun onResume() {
        super.onResume()
        binding.badgeActiveDriver.text = activeDriverInfo
    }

    private val client = OkHttpClient.Builder()
        .connectTimeout(15, java.util.concurrent.TimeUnit.SECONDS)
        .readTimeout(20, java.util.concurrent.TimeUnit.SECONDS)
        .followRedirects(true)
        .followSslRedirects(true)
        .build()

    private fun fetchDrivers() {
        binding.loadingIndicator.isVisible = true

        val driverGroups = arrayListOf<DriverGroup>()

        repoList.forEach { driver ->
            val name = driver.name
            val path = driver.path
            val useTagName = driver.useTagName
            val sortMode = driver.sortMode
            val sort = driver.sort

            CoroutineScope(Dispatchers.Main).launch {
                withContext(Dispatchers.IO) {
                    var releases = ArrayList<Release>()
                    
                    val urlsToTry = listOf(
                        "https://api.github.com/repos/$path/releases",
                        "https://api.githubfast.com/repos/$path/releases",
                        "https://ghproxy.net/https://api.github.com/repos/$path/releases",
                        "https://gh-proxy.com/https://api.github.com/repos/$path/releases"
                    )

                    for (url in urlsToTry) {
                        try {
                            val request = Request.Builder()
                                .url(url)
                                .header("User-Agent", "Mozilla/5.0 (Linux; Android 14; Mobile) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/128.0.0.0 StormEden/4.4.3")
                                .header("Accept", "application/vnd.github.v3+json, application/json, text/plain, */*")
                                .build()

                            client.newCall(request).execute().use { response ->
                                if (response.isSuccessful) {
                                    val body = response.body?.string()
                                    if (!body.isNullOrBlank()) {
                                        val parsed = Release.fromJsonArray(body, useTagName, sortMode)
                                        if (parsed.isNotEmpty()) {
                                            releases = parsed
                                            return@use
                                        }
                                    }
                                }
                            }
                        } catch (e: Exception) {
                            // Continue to next mirror
                        }
                        if (releases.isNotEmpty()) break
                    }

                    val group = DriverGroup(
                        name,
                        releases,
                        sort
                    )

                    synchronized(driverGroups) {
                        driverGroups.add(group)
                        driverGroups.sortBy {
                            it.sort
                        }
                    }

                    withContext(Dispatchers.Main) {
                        driverGroupAdapter.updateDriverGroups(driverGroups)

                        if (driverGroups.size >= repoList.size) {
                            binding.loadingIndicator.isVisible = false
                        }
                    }
                }
            }
        }
    }

    private fun setInsets() = ViewCompat.setOnApplyWindowInsetsListener(
        binding.root
    ) { _: View, windowInsets: WindowInsetsCompat ->
        val barInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
        val cutoutInsets = windowInsets.getInsets(WindowInsetsCompat.Type.displayCutout())

        val leftInsets = barInsets.left + cutoutInsets.left
        val rightInsets = barInsets.right + cutoutInsets.right

        binding.toolbarDrivers.updateMargins(left = leftInsets, right = rightInsets)
        binding.listDrivers.updateMargins(left = leftInsets, right = rightInsets)

        binding.listDrivers.updatePadding(
            bottom = barInsets.bottom + resources.getDimensionPixelSize(
                R.dimen.spacing_bottom_list_fab
            )
        )

        windowInsets
    }

    data class Artifact(val url: URL, val name: String)

    data class Release(
        var tagName: String = "",
        var titleName: String = "",
        var title: String = "",
        var body: String = "",
        var artifacts: List<Artifact> = ArrayList(),
        var prerelease: Boolean = false,
        var latest: Boolean = false,
        var publishTime: LocalDateTime = LocalDateTime.now()
    ) {
        companion object {
            fun fromJsonArray(
                jsonString: String,
                useTagName: Boolean,
                sortMode: SortMode
            ): ArrayList<Release> {
                val mapper = jacksonObjectMapper()

                try {
                    val rootNode = mapper.readTree(jsonString)

                    val releases = ArrayList<Release>()

                    var latestRelease: Release? = null

                    if (rootNode.isArray) {
                        rootNode.forEach { node ->
                            val release = fromJson(node, useTagName)

                            if (latestRelease == null && !release.prerelease) {
                                latestRelease = release
                                release.latest = true
                            }

                            releases.add(release)
                        }
                    }

                    when (sortMode) {
                        SortMode.PublishTime -> releases.sortByDescending {
                            it.publishTime
                        }

                        else -> {}
                    }

                    return releases
                } catch (e: Exception) {
                    e.printStackTrace()
                    return ArrayList()
                }
            }

            private fun fromJson(node: JsonNode, useTagName: Boolean): Release {
                try {
                    val tagName = node.get("tag_name").toString().removeSurrounding("\"")
                    val body = node.get("body").toString().removeSurrounding("\"")
                    val prerelease = node.get("prerelease").toString().toBoolean()
                    val titleName = node.get("name").toString().removeSurrounding("\"")

                    val published = node.get("published_at").toString().removeSurrounding("\"")
                    val instantTime: Instant? = Instant.parse(published)
                    val localTime = instantTime?.atZone(ZoneId.systemDefault())?.toLocalDateTime() ?: LocalDateTime.now()

                    val title = if (useTagName) tagName else titleName

                    val assets = node.get("assets")
                    val artifacts = ArrayList<Artifact>()
                    if (assets?.isArray == true) {
                        assets.forEach { subNode ->
                            val urlStr = subNode.get("browser_download_url").toString()
                                .removeSurrounding("\"")

                            val url = URL(urlStr)
                            val name = subNode.get("name").toString().removeSurrounding("\"")

                            val artifact = Artifact(url, name)
                            artifacts.add(artifact)
                        }
                    }

                    return Release(
                        tagName,
                        titleName,
                        title,
                        body,
                        artifacts,
                        prerelease,
                        false,
                        localTime
                    )
                } catch (e: Exception) {
                    // TODO: handle malformed input.
                    e.printStackTrace()
                }

                return Release()
            }
        }
    }

    data class DriverGroup(
        val name: String,
        val releases: ArrayList<Release>,
        val sort: Int
    )
}
