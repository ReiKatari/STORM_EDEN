// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.fragments

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.recyclerview.widget.GridLayoutManager
import com.google.android.material.transition.MaterialSharedAxis
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.adapters.InstallableAdapter
import org.yuzu.yuzu_emu.databinding.FragmentInstallablesBinding
import org.yuzu.yuzu_emu.model.AddonViewModel
import org.yuzu.yuzu_emu.model.DriverViewModel
import org.yuzu.yuzu_emu.model.GamesViewModel
import org.yuzu.yuzu_emu.model.HomeViewModel
import org.yuzu.yuzu_emu.model.Installable
import org.yuzu.yuzu_emu.model.TaskState
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.Environment
import android.provider.DocumentsContract
import org.yuzu.yuzu_emu.utils.DirectoryInitialization
import org.yuzu.yuzu_emu.utils.FileUtil
import org.yuzu.yuzu_emu.utils.InstallableActions
import org.yuzu.yuzu_emu.utils.NativeConfig
import org.yuzu.yuzu_emu.utils.ViewUtils.updateMargins
import org.yuzu.yuzu_emu.utils.collect
import java.io.BufferedOutputStream
import java.io.File
import java.math.BigInteger
import java.time.LocalDateTime
import java.time.format.DateTimeFormatter

open class OpenDocumentWithInitialPath(private val getInitialPath: () -> String) :
    ActivityResultContracts.OpenDocument() {
    override fun createIntent(context: Context, input: Array<String>): Intent {
        val intent = super.createIntent(context, input)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            try {
                val path = getInitialPath()
                val file = File(path)
                if (!file.exists()) {
                    file.mkdirs()
                }
                val externalRoot = Environment.getExternalStorageDirectory().canonicalPath
                val relativePath = if (file.canonicalPath.startsWith(externalRoot)) {
                    file.canonicalPath.removePrefix(externalRoot).trimStart('/', '\\')
                } else {
                    path.trimStart('/', '\\')
                }
                val uri = DocumentsContract.buildDocumentUri(
                    "com.android.externalstorage.documents",
                    "primary:$relativePath"
                )
                intent.putExtra(DocumentsContract.EXTRA_INITIAL_URI, uri)
            } catch (_: Exception) {}
        }
        return intent
    }
}

open class CreateDocumentWithInitialPath(
    mimeType: String,
    private val getInitialPath: () -> String
) : ActivityResultContracts.CreateDocument(mimeType) {
    override fun createIntent(context: Context, input: String): Intent {
        val intent = super.createIntent(context, input)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            try {
                val path = getInitialPath()
                val file = File(path)
                if (!file.exists()) {
                    file.mkdirs()
                }
                val externalRoot = Environment.getExternalStorageDirectory().canonicalPath
                val relativePath = if (file.canonicalPath.startsWith(externalRoot)) {
                    file.canonicalPath.removePrefix(externalRoot).trimStart('/', '\\')
                } else {
                    path.trimStart('/', '\\')
                }
                val uri = DocumentsContract.buildDocumentUri(
                    "com.android.externalstorage.documents",
                    "primary:$relativePath"
                )
                intent.putExtra(DocumentsContract.EXTRA_INITIAL_URI, uri)
            } catch (_: Exception) {}
        }
        return intent
    }
}

class InstallableFragment : Fragment() {
    private var _binding: FragmentInstallablesBinding? = null
    private val binding get() = _binding!!

    private val homeViewModel: HomeViewModel by activityViewModels()
    private val gamesViewModel: GamesViewModel by activityViewModels()
    private val addonViewModel: AddonViewModel by activityViewModels()
    private val driverViewModel: DriverViewModel by activityViewModels()

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
        _binding = FragmentInstallablesBinding.inflate(layoutInflater)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        homeViewModel.setStatusBarShadeVisibility(visible = false)

        binding.toolbarInstallables.setNavigationOnClickListener {
            requireActivity().onBackPressedDispatcher.onBackPressed()
        }

        homeViewModel.openImportSaves.collect(viewLifecycleOwner) {
            if (it) {
                importSaves.launch(arrayOf("application/zip"))
                homeViewModel.setOpenImportSaves(false)
            }
        }

        val installables = listOf(
            Installable(
                R.string.user_data,
                R.string.user_data_description,
                install = { importUserDataLauncher.launch(arrayOf("application/zip")) },
                export = { exportUserDataLauncher.launch("export.zip") }
            ),
            Installable(
                R.string.manage_save_data,
                R.string.manage_save_data_description,
                install = {
                    MessageDialogFragment.newInstance(
                        requireActivity(),
                        titleId = R.string.import_save_warning,
                        descriptionId = R.string.import_save_warning_description,
                        positiveAction = { homeViewModel.setOpenImportSaves(true) }
                    ).show(parentFragmentManager, MessageDialogFragment.TAG)
                },
                export = {
                    val oldSaveDataFolder = File(
                        NativeConfig.getSaveDir() +
                            NativeLibrary.getDefaultProfileSaveDataRoot(false)
                    )
                    val futureSaveDataFolder = File(
                        NativeConfig.getSaveDir() +
                            NativeLibrary.getDefaultProfileSaveDataRoot(true)
                    )
                    if (!oldSaveDataFolder.exists() && !futureSaveDataFolder.exists()) {
                        Toast.makeText(
                            YuzuApplication.appContext,
                            R.string.no_save_data_found,
                            Toast.LENGTH_SHORT
                        ).show()
                        return@Installable
                    } else {
                        exportSaves.launch(
                            "${getString(R.string.save_data)} " +
                                LocalDateTime.now().format(
                                    DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm")
                                )
                        )
                    }
                }
            ),
            Installable(
                R.string.install_game_content,
                R.string.install_game_content_description,
                install = { installGameUpdateLauncher.launch(arrayOf("*/*")) }
            ),
            Installable(
                R.string.install_firmware,
                R.string.install_firmware_description,
                install = { getFirmwareLauncher.launch(arrayOf("application/zip")) }
            ),
            Installable(
                R.string.uninstall_firmware,
                R.string.uninstall_firmware_description,
                install = {
                    InstallableActions.uninstallFirmware(
                        activity = requireActivity(),
                        fragmentManager = parentFragmentManager,
                        homeViewModel = homeViewModel
                    )
                }
            ),
            Installable(
                R.string.install_prod_keys,
                R.string.install_prod_keys_description,
                install = { getProdKeyLauncher.launch(arrayOf("*/*")) }
            ),
            Installable(
                R.string.install_amiibo_keys,
                R.string.install_amiibo_keys_description,
                install = { getAmiiboKeyLauncher.launch(arrayOf("*/*")) }
            )
        )

        binding.listInstallables.apply {
            layoutManager = GridLayoutManager(
                requireContext(),
                resources.getInteger(R.integer.grid_columns)
            )
            adapter = InstallableAdapter(installables)
        }

        setInsets()
    }

    private fun setInsets() =
        ViewCompat.setOnApplyWindowInsetsListener(
            binding.root
        ) { _: View, windowInsets: WindowInsetsCompat ->
            val barInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            val cutoutInsets = windowInsets.getInsets(WindowInsetsCompat.Type.displayCutout())

            val leftInsets = barInsets.left + cutoutInsets.left
            val rightInsets = barInsets.right + cutoutInsets.right

            binding.toolbarInstallables.updateMargins(left = leftInsets, right = rightInsets)
            binding.listInstallables.updateMargins(left = leftInsets, right = rightInsets)

            binding.listInstallables.updatePadding(bottom = barInsets.bottom)

            windowInsets
        }

    private val getProdKeyLauncher =
        registerForActivityResult(ActivityResultContracts.OpenDocument()) { result ->
            if (result != null) {
                InstallableActions.processKey(
                    activity = requireActivity(),
                    fragmentManager = parentFragmentManager,
                    gamesViewModel = gamesViewModel,
                    result = result,
                    extension = "keys"
                )
            }
        }

    private val getAmiiboKeyLauncher =
        registerForActivityResult(OpenDocumentWithInitialPath {
            "${DirectoryInitialization.userDirectory ?: File(Environment.getExternalStorageDirectory(), "STORM SWITCH").path}/amiibo"
        }) { result ->
            if (result != null) {
                InstallableActions.processKey(
                    activity = requireActivity(),
                    fragmentManager = parentFragmentManager,
                    gamesViewModel = gamesViewModel,
                    result = result,
                    extension = "bin"
                )
            }
        }

    private val getFirmwareLauncher =
        registerForActivityResult(ActivityResultContracts.OpenDocument()) { result ->
            if (result != null) {
                InstallableActions.processFirmware(
                    activity = requireActivity(),
                    fragmentManager = parentFragmentManager,
                    homeViewModel = homeViewModel,
                    result = result
                )
            }
        }

    private val installGameUpdateLauncher =
        registerForActivityResult(ActivityResultContracts.OpenMultipleDocuments()) { documents ->
            InstallableActions.verifyAndInstallContent(
                activity = requireActivity(),
                fragmentManager = parentFragmentManager,
                addonViewModel = addonViewModel,
                documents = documents,
                programId = addonViewModel.game?.programId
            )
        }

    private val importUserDataLauncher =
        registerForActivityResult(ActivityResultContracts.OpenDocument()) { result ->
            if (result != null) {
                InstallableActions.importUserData(
                    activity = requireActivity(),
                    fragmentManager = parentFragmentManager,
                    gamesViewModel = gamesViewModel,
                    driverViewModel = driverViewModel,
                    result = result
                )
            }
        }

    private val exportUserDataLauncher =
        registerForActivityResult(ActivityResultContracts.CreateDocument("application/zip")) { result ->
            if (result != null) {
                InstallableActions.exportUserData(
                    activity = requireActivity(),
                    fragmentManager = parentFragmentManager,
                    result = result
                )
            }
        }

    private val importSaves =
        registerForActivityResult(OpenDocumentWithInitialPath {
            "${DirectoryInitialization.userDirectory ?: File(Environment.getExternalStorageDirectory(), "STORM SWITCH").path}/nand/user/save"
        }) { result ->
            if (result == null) {
                return@registerForActivityResult
            }

            val cacheSaveDir = File("${requireContext().cacheDir.path}/saves/")
            cacheSaveDir.mkdir()

            ProgressDialogFragment.newInstance(
                requireActivity(),
                R.string.save_files_importing,
                false
            ) { progressCallback, _ ->
                try {
                    FileUtil.unzipToInternalStorage(
                        result.toString(),
                        cacheSaveDir,
                        progressCallback
                    )
                    val files = cacheSaveDir.listFiles()
                    var successfulImports = 0
                    var failedImports = 0
                    if (files != null) {
                        for (file in files) {
                            if (file.isDirectory) {
                                val progId = try {
                                    BigInteger(file.name, 16).toString()
                                } catch (_: Exception) {
                                    failedImports++
                                    continue
                                }
                                val baseSaveDir = NativeLibrary.getSavePath(progId)
                                if (baseSaveDir.isEmpty()) {
                                    failedImports++
                                    continue
                                }

                                val internalSaveFolder = File(
                                    "${NativeConfig.getSaveDir()}$baseSaveDir"
                                )
                                internalSaveFolder.deleteRecursively()
                                internalSaveFolder.mkdir()
                                file.copyRecursively(target = internalSaveFolder, overwrite = true)
                                successfulImports++
                            }
                        }
                    }

                    withContext(Dispatchers.Main) {
                        if (successfulImports == 0) {
                            MessageDialogFragment.newInstance(
                                requireActivity(),
                                titleId = R.string.save_file_invalid_zip_structure,
                                descriptionId = R.string.save_file_invalid_zip_structure_description
                            ).show(parentFragmentManager, MessageDialogFragment.TAG)
                            return@withContext
                        }
                        val successString = if (failedImports > 0) {
                            """
                            ${
                            requireContext().resources.getQuantityString(
                                R.plurals.saves_import_success,
                                successfulImports,
                                successfulImports
                            )
                            }
                            ${
                            requireContext().resources.getQuantityString(
                                R.plurals.saves_import_failed,
                                failedImports,
                                failedImports
                            )
                            }
                            """
                        } else {
                            requireContext().resources.getQuantityString(
                                R.plurals.saves_import_success,
                                successfulImports,
                                successfulImports
                            )
                        }
                        MessageDialogFragment.newInstance(
                            requireActivity(),
                            titleId = R.string.import_complete,
                            descriptionString = successString
                        ).show(parentFragmentManager, MessageDialogFragment.TAG)
                    }

                    cacheSaveDir.deleteRecursively()
                } catch (e: Exception) {
                    Toast.makeText(
                        YuzuApplication.appContext,
                        getString(R.string.fatal_error),
                        Toast.LENGTH_LONG
                    ).show()
                }
            }.show(parentFragmentManager, ProgressDialogFragment.TAG)
        }

    private val exportSaves = registerForActivityResult(
        CreateDocumentWithInitialPath("application/zip") {
            "${DirectoryInitialization.userDirectory ?: File(Environment.getExternalStorageDirectory(), "STORM SWITCH").path}/nand/user/save"
        }
    ) { result ->
        if (result == null) {
            return@registerForActivityResult
        }

        ProgressDialogFragment.newInstance(
            requireActivity(),
            R.string.save_files_exporting,
            false
        ) { _, _ ->
            val cacheSaveDir = File("${requireContext().cacheDir.path}/saves/")
            cacheSaveDir.mkdir()

            val oldSaveDataFolder = File(
                NativeConfig.getSaveDir() +
                    NativeLibrary.getDefaultProfileSaveDataRoot(false)
            )
            if (oldSaveDataFolder.exists()) {
                oldSaveDataFolder.copyRecursively(cacheSaveDir)
            }

            val futureSaveDataFolder = File(
                NativeConfig.getSaveDir() +
                    NativeLibrary.getDefaultProfileSaveDataRoot(true)
            )
            if (futureSaveDataFolder.exists()) {
                futureSaveDataFolder.copyRecursively(cacheSaveDir)
            }

            val saveFilesTotal = cacheSaveDir.listFiles()?.size ?: 0
            if (saveFilesTotal == 0) {
                cacheSaveDir.deleteRecursively()
                return@newInstance getString(R.string.no_save_data_found)
            }

            val zipResult = FileUtil.zipFromInternalStorage(
                cacheSaveDir,
                cacheSaveDir.path,
                BufferedOutputStream(requireContext().contentResolver.openOutputStream(result))
            )
            cacheSaveDir.deleteRecursively()

            return@newInstance when (zipResult) {
                TaskState.Completed -> getString(R.string.export_success)
                TaskState.Cancelled, TaskState.Failed -> getString(R.string.export_failed)
            }
        }.show(parentFragmentManager, ProgressDialogFragment.TAG)
    }
}
