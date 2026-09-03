// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.utils

import android.graphics.SurfaceTexture
import android.net.Uri
import android.os.Build
import android.view.Surface
import java.io.File
import java.io.IOException
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.features.settings.model.StringSetting
import java.io.FileNotFoundException
import java.util.zip.ZipException
import java.util.zip.ZipFile

object GpuDriverHelper {
    private const val META_JSON_FILENAME = "meta.json"
    private var fileRedirectionPath: String? = null
    var driverInstallationPath: String? = null
    internal var hookLibPath: String? = null

    val driverStoragePath get() = DirectoryInitialization.userDirectory!! + "/gpu_drivers/"

    fun initializeFreedrenoConfigEarly() {
        NativeFreedrenoConfig.setFreedrenoBasePath(YuzuApplication.appContext.cacheDir.absolutePath)
        NativeFreedrenoConfig.initializeFreedrenoConfig()
        NativeFreedrenoConfig.reloadFreedrenoConfig()
    }

    fun initializeDriverParameters() {
        try {
            // Initialize the file redirection directory.
            fileRedirectionPath = YuzuApplication.appContext
                .getExternalFilesDir(null)!!.canonicalPath + "/gpu/vk_file_redirect/"

            // Initialize the driver installation directory.
            driverInstallationPath = YuzuApplication.appContext
                .filesDir.canonicalPath + "/gpu_driver/"
        } catch (e: IOException) {
            throw RuntimeException(e)
        }

        initializeDirectories()
        hookLibPath = YuzuApplication.appContext.applicationInfo.nativeLibraryDir + "/"
        NativeFreedrenoConfig.reloadFreedrenoConfig()

        // Auto-restore selected driver from config if installation folder is missing files
        val activeDriverZipPath = try { StringSetting.DRIVER_PATH.getString() } catch (_: Exception) { "" }
        if (installedCustomDriverData.libraryName.isNullOrEmpty() && activeDriverZipPath.isNotEmpty()) {
            val activeDriverFile = File(activeDriverZipPath)
            if (activeDriverFile.exists()) {
                val metadata = getMetadataFromZip(activeDriverFile)
                if (metadata.name != null && metadata.minApi <= Build.VERSION.SDK_INT) {
                    try {
                        FileUtil.unzipToInternalStorage(
                            activeDriverFile.path,
                            File(driverInstallationPath!!)
                        )
                    } catch (_: Exception) {
                    }
                }
            }
        }

        val isA830Device = android.os.Build.MODEL.contains("S938", ignoreCase = true) ||
                           android.os.Build.HARDWARE.contains("sun", ignoreCase = true) ||
                           android.os.Build.BOARD.contains("sun", ignoreCase = true) ||
                           android.os.Build.DEVICE.contains("sun", ignoreCase = true) ||
                           android.os.Build.PRODUCT.contains("sun", ignoreCase = true) ||
                           (android.os.Build.VERSION.SDK_INT >= 31 && android.os.Build.SOC_MODEL.contains("8750", ignoreCase = true))

        val customLib = installedCustomDriverData.libraryName
        if (!customLib.isNullOrEmpty()) {
            val installDir = File(driverInstallationPath!!)
            val drircFile = File(installDir, "drirc.xml")
            val drircConfFile = File(installDir, "00-storm.conf")
            val fallbackConf = if (drircConfFile.exists()) drircConfFile else if (drircFile.exists()) drircFile else null

            if (fallbackConf != null && fallbackConf.exists()) {
                val configBytes = fallbackConf.readBytes()
                try {
                    val redirDir = File(fileRedirectionPath!!)
                    if (!redirDir.exists()) redirDir.mkdirs()
                    File(redirDir, "drirc.xml").writeBytes(configBytes)
                    File(redirDir, "drirc").writeBytes(configBytes)
                    File(redirDir, "00-storm.conf").writeBytes(configBytes)
                    File(redirDir, "drirc.conf").writeBytes(configBytes)

                    File(installDir, "00-storm.conf").writeBytes(configBytes)
                    File(installDir, "drirc.conf").writeBytes(configBytes)
                    File(installDir, "drirc").writeBytes(configBytes)

                    // Also write to $HOME/.drirc for Mesa driconf loader
                    val homeDir = YuzuApplication.appContext.filesDir
                    File(homeDir, ".drirc").writeBytes(configBytes)
                } catch (_: Throwable) {}

                NativeFreedrenoConfig.setFreedrenoEnv("DRIRC_CONFIGDIR", driverInstallationPath!!)
                NativeFreedrenoConfig.setFreedrenoEnv("MESA_DRIRC_DIR", driverInstallationPath!!)
                NativeFreedrenoConfig.setFreedrenoEnv("MESA_DRIRC_FILE", fallbackConf.absolutePath)
                NativeFreedrenoConfig.setFreedrenoEnv("HOME", YuzuApplication.appContext.filesDir.absolutePath)
            }
            
            // 4GB Monolithic Shader Cache for Mesa Turnip and PanVK
            NativeFreedrenoConfig.setFreedrenoEnv("MESA_SHADER_CACHE_MAX_SIZE", "4294967296")
            NativeFreedrenoConfig.setFreedrenoEnv("MESA_DISK_CACHE_SINGLE_FILE", "1")

            var model = ""
            try {
                model = hookLibPath?.let { getGpuModel(hookLibPath = it) } ?: ""
            } catch (e: Throwable) {
                // Ignore fallback
            }

            // ARM Mali (PanVK/Panfrost) early Z and geometry optimizations
            if (model.contains("Mali", ignoreCase = true)) {
                NativeFreedrenoConfig.setFreedrenoEnv("PAN_MESA_DEBUG", "fpk,early_z,opt")
            }
        }

        NativeLibrary.initializeGpuDriver(
            hookLibPath,
            driverInstallationPath,
            customLib,
            fileRedirectionPath
        )
    }

    fun getDrivers(): MutableList<Pair<String, GpuDriverMetadata>> {
        val driverZips = File(driverStoragePath).listFiles()
        val drivers: MutableList<Pair<String, GpuDriverMetadata>> =
            driverZips
                ?.mapNotNull {
                    val metadata = getMetadataFromZip(it)
                    metadata.name?.let { _ -> Pair(it.path, metadata) }
                }
                ?.sortedByDescending { it: Pair<String, GpuDriverMetadata> -> it.second.name }
                ?.distinct()
                ?.toMutableList() ?: mutableListOf()
        return drivers
    }

    fun installDefaultDriver() {
        // Removing the installed driver will result in the backend using the default system driver.
        File(driverInstallationPath!!).deleteRecursively()
        initializeDriverParameters()
    }

    fun copyDriverToInternalStorage(driverUri: Uri): Boolean {
        // Ensure we have directories.
        initializeDirectories()

        // Copy the zip file URI to user data
        val copiedFile =
            FileUtil.copyUriToInternalStorage(driverUri, driverStoragePath) ?: return false

        // Validate driver
        val metadata = getMetadataFromZip(copiedFile)
        if (metadata.name == null) {
            copiedFile.delete()
            return false
        }

        if (metadata.minApi > Build.VERSION.SDK_INT) {
            copiedFile.delete()
            return false
        }
        return true
    }

    /**
     * Copies driver zip into user data directory so that it can be exported along with
     * other user data and also unzipped into the installation directory
     */
    fun installCustomDriver(driverUri: Uri): Boolean {
        // Revert to system default in the event the specified driver is bad.
        installDefaultDriver()

        // Ensure we have directories.
        initializeDirectories()

        // Copy the zip file URI to user data
        val copiedFile =
            FileUtil.copyUriToInternalStorage(driverUri, driverStoragePath) ?: return false

        // Validate driver
        val metadata = getMetadataFromZip(copiedFile)
        if (metadata.name == null) {
            copiedFile.delete()
            return false
        }

        if (metadata.minApi > Build.VERSION.SDK_INT) {
            copiedFile.delete()
            return false
        }

        // Unzip the driver.
        try {
            FileUtil.unzipToInternalStorage(
                copiedFile.path,
                File(driverInstallationPath!!)
            )
        } catch (e: SecurityException) {
            return false
        }

        // Initialize the driver parameters.
        initializeDriverParameters()

        return true
    }

    /**
     * Unzips driver into installation directory
     */
    fun installCustomDriver(driver: File): Boolean {
        // Revert to system default in the event the specified driver is bad.
        installDefaultDriver()

        // Ensure we have directories.
        initializeDirectories()

        // Validate driver
        val metadata = getMetadataFromZip(driver)
        if (metadata.name == null) {
            driver.delete()
            return false
        }

        // Unzip the driver to the private installation directory
        try {
            FileUtil.unzipToInternalStorage(
                driver.path,
                File(driverInstallationPath!!)
            )
        } catch (e: SecurityException) {
            return false
        }

        // Initialize the driver parameters.
        initializeDriverParameters()

        return true
    }

    /**
     * Takes in a zip file and reads the meta.json file for presentation to the UI
     *
     * @param driver Zip containing driver and meta.json file
     * @return A non-null [GpuDriverMetadata] instance that may have null members
     */
    fun getMetadataFromZip(driver: File): GpuDriverMetadata {
        if (!driver.exists()) {
            return GpuDriverMetadata()
        }

        try {
            ZipFile(driver).use { zf ->
                val entries = zf.entries()
                while (entries.hasMoreElements()) {
                    val entry = entries.nextElement()
                    val entryName = entry.name.substringAfterLast('/')
                    if (!entry.isDirectory && (entryName.equals("meta.json", ignoreCase = true) || (entryName.endsWith(".json", ignoreCase = true) && !entryName.contains(".metadata.")))) {
                        zf.getInputStream(entry).use {
                            val meta = GpuDriverMetadata(it, entry.size)
                            if (meta.name != null) {
                                return meta
                            }
                        }
                    }
                }
            }
        } catch (_: ZipException) {
        } catch (_: FileNotFoundException) {
        }
        return GpuDriverMetadata()

    }

    external fun supportsCustomDriverLoading(): Boolean

    external fun getSystemDriverInfo(
        surface: Surface = Surface(SurfaceTexture(true)),
        hookLibPath: String = GpuDriverHelper.hookLibPath!!
    ): Array<String>?

    external fun getGpuModel(
        surface: Surface = Surface(SurfaceTexture(true)),
        hookLibPath: String
    ): String?

    fun isAdrenoGpu(): Boolean {
        return try {
            supportsCustomDriverLoading()
        } catch (e: Exception) {
            false
        }
    }

    // Parse the custom driver metadata to retrieve the name.
    val installedCustomDriverData: GpuDriverMetadata
        get() = GpuDriverMetadata(File(driverInstallationPath + META_JSON_FILENAME))

    val customDriverSettingData: GpuDriverMetadata
        get() = getMetadataFromZip(File(StringSetting.DRIVER_PATH.getString()))

    fun initializeDirectories() {
        // Ensure the file redirection directory exists.
        val fileRedirectionDir = File(fileRedirectionPath!!)
        if (!fileRedirectionDir.exists()) {
            fileRedirectionDir.mkdirs()
        }
        // Ensure the driver installation directory exists.
        val driverInstallationDir = File(driverInstallationPath!!)
        if (!driverInstallationDir.exists()) {
            driverInstallationDir.mkdirs()
        }
        // Ensure the driver storage directory exists
        val driverStorageDirectory = File(driverStoragePath)
        if (!driverStorageDirectory.exists()) {
            driverStorageDirectory.mkdirs()
        }
    }

    /**
     * Checks if a driver zip with the given filename is already present and valid in the
     * internal driver storage directory. Validation requires a readable meta.json with a name.
     */
    fun isDriverZipInstalledByName(fileName: String): Boolean {
        // Normalize separators in case upstream sent a path
        val baseName = fileName.substringAfterLast('/')
            .substringAfterLast('\\')
        val candidate = File("$driverStoragePath$baseName")
        if (!candidate.exists() || candidate.length() == 0L) return false
        val metadata = getMetadataFromZip(candidate)
        return metadata.name != null
    }
}
