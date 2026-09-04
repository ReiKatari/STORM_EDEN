// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.utils

import android.content.SharedPreferences
import android.net.Uri
import android.provider.DocumentsContract
import androidx.preference.PreferenceManager
import kotlinx.serialization.encodeToString
import kotlinx.serialization.json.Json
import java.io.File
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.model.Game
import org.yuzu.yuzu_emu.model.GameDir
import org.yuzu.yuzu_emu.model.MinimalDocumentFile
import androidx.core.content.edit
import androidx.core.net.toUri
import org.yuzu.yuzu_emu.features.settings.model.BooleanSetting

object GameHelper {
    private const val KEY_OLD_GAME_PATH = "game_path"
    const val KEY_GAMES = "Games"

    var cachedGameList = mutableListOf<Game>()

    private lateinit var preferences: SharedPreferences

    fun getGames(): List<Game> {
        val games = mutableListOf<Game>()
        val context = YuzuApplication.appContext
        preferences = PreferenceManager.getDefaultSharedPreferences(context)

        val gameDirs = mutableListOf<GameDir>()
        val oldGamesDir = preferences.getString(KEY_OLD_GAME_PATH, "") ?: ""
        if (oldGamesDir.isNotEmpty()) {
            gameDirs.add(GameDir(oldGamesDir, true))
            preferences.edit() { remove(KEY_OLD_GAME_PATH) }
        }
        NativeConfig.getGameDirs().forEach { dir ->
            if (gameDirs.none { it.uriString == dir.uriString }) {
                gameDirs.add(dir)
            }
        }

        if (gameDirs.isEmpty()) {
            val backupDirs = preferences.getStringSet("game_directories_backup", null)
            if (!backupDirs.isNullOrEmpty()) {
                val restored = backupDirs.map { GameDir(it, true) }
                restored.forEach { gameDirs.add(it) }
                NativeConfig.setGameDirs(restored.toTypedArray())
            }
        } else {
            val dirSet = gameDirs.map { it.uriString }.toSet()
            preferences.edit().putStringSet("game_directories_backup", dirSet).apply()
        }

        if (cachedGameList.isEmpty()) {
            val stored = preferences.getStringSet(KEY_GAMES, emptySet()) ?: emptySet()
            for (item in stored) {
                try {
                    cachedGameList.add(Json.decodeFromString(item))
                } catch (_: Exception) {}
            }
        }

        // Ensure keys are loaded so that ROM metadata can be decrypted.
        NativeLibrary.reloadKeys()

        // Reset metadata so we don't use stale information
        GameMetadata.resetMetadata()

        // Remove previous filesystem provider information so we can get up to date version info
        NativeLibrary.clearFilesystemProvider()

        val mountedContainerUris = mutableSetOf<String>()
        mountExternalContentDirectories(mountedContainerUris)

        // Stage 1: Pre-mount ALL containers across ALL game directories and subdirectories
        gameDirs.forEach { gameDir ->
            val gameDirUri = gameDir.uriString.toUri()
            if (FileUtil.isTreeUriValid(gameDirUri)) {
                val scanDepth = if (gameDir.deepScan) 3 else 1
                scanContentContainersRecursive(FileUtil.listFiles(gameDirUri), scanDepth) {
                    val filePath = it.uri.toString()
                    if (mountedContainerUris.add(filePath)) {
                        NativeLibrary.addGameFolderFileToFilesystemProvider(filePath)
                    }
                }
            }
        }

        // Stage 2: Load games with all content/updates/DLCs already registered in ContentProvider
        val badDirs = mutableListOf<Int>()
        gameDirs.forEachIndexed { index: Int, gameDir: GameDir ->
            val gameDirUri = gameDir.uriString.toUri()
            val isValid = FileUtil.isTreeUriValid(gameDirUri)
            if (isValid) {
                val scanDepth = if (gameDir.deepScan) 3 else 1

                addGamesRecursive(
                    games,
                    FileUtil.listFiles(gameDirUri),
                    scanDepth
                )
            } else {
                badDirs.add(index)
            }
        }

        // Remove all game dirs with insufficient permissions from config
        if (badDirs.isNotEmpty()) {
            var offset = 0
            badDirs.forEach {
                gameDirs.removeAt(it - offset)
                offset++
            }
            NativeConfig.setGameDirs(gameDirs.toTypedArray())
        }

        // Group games by file path so all distinct ROM dumps/versions are properly preserved and displayed
        val uniqueGamesMap = linkedMapOf<String, Game>()
        games.forEach { game ->
            val key = game.path
            val existing = uniqueGamesMap[key]
            if (existing == null) {
                uniqueGamesMap[key] = game
            } else {
                val existingVer = existing.version.removePrefix("v").removePrefix("V").trim()
                val currentVer = game.version.removePrefix("v").removePrefix("V").trim()
                val existingIntVer = existing.internalVersion.toLongOrNull() ?: 0L
                val currentIntVer = game.internalVersion.toLongOrNull() ?: 0L

                if (currentIntVer > existingIntVer) {
                    uniqueGamesMap[key] = game
                } else if (currentIntVer == existingIntVer && currentVer != "1.0.0" && existingVer == "1.0.0") {
                    uniqueGamesMap[key] = game
                } else if (game.addonCount > existing.addonCount) {
                    uniqueGamesMap[key] = game
                }
            }
        }
        val finalGames = uniqueGamesMap.values.toList()

        if (finalGames.isNotEmpty()) {
            // Cache list of games found on disk
            val serializedGames = mutableSetOf<String>()
            finalGames.forEach {
                serializedGames.add(Json.encodeToString(it))
            }
            preferences.edit() {
                remove(KEY_GAMES)
                    .putStringSet(KEY_GAMES, serializedGames)
            }
            cachedGameList = finalGames.toMutableList()
            return finalGames
        } else if (cachedGameList.isNotEmpty()) {
            // Protection: never wipe cached games if a background reload or driver switch temporarily returned 0 files
            return cachedGameList
        }

        cachedGameList = finalGames.toMutableList()
        return finalGames
    }

    fun restoreContentForGame(game: Game) {
        NativeLibrary.reloadKeys()

        val mountedContainerUris = mutableSetOf<String>()
        mountExternalContentDirectories(mountedContainerUris)
        mountGameFolderContent(Uri.parse(game.path), mountedContainerUris)
        NativeLibrary.addFileToFilesystemProvider(game.path)
    }

    // File extensions considered as external content, buuut should
    // be done better imo.
    private val externalContentExtensions = setOf("nsp", "xci", "nsz", "xcz")

    private fun scanContentContainersRecursive(
        files: Array<MinimalDocumentFile>,
        depth: Int,
        onContainerFound: (MinimalDocumentFile) -> Unit
    ) {
        if (depth <= 0) {
            return
        }

        files.forEach {
            if (it.isDirectory) {
                scanContentContainersRecursive(
                    FileUtil.listFiles(it.uri),
                    depth - 1,
                    onContainerFound
                )
            } else {
                val extension = FileUtil.getExtension(it.uri).lowercase()
                if (externalContentExtensions.contains(extension)) {
                    onContainerFound(it)
                }
            }
        }
    }

    private fun addGamesRecursive(
        games: MutableList<Game>,
        files: Array<MinimalDocumentFile>,
        depth: Int
    ) {
        if (depth <= 0) {
            return
        }

        files.forEach {
            if (it.isDirectory) {
                addGamesRecursive(
                    games,
                    FileUtil.listFiles(it.uri),
                    depth - 1
                )
            } else {
                val extension = FileUtil.getExtension(it.uri).lowercase()
                if (Game.extensions.contains(extension)) {
                    val game = getGame(it.uri, true, false)
                    if (game != null) {
                        games.add(game)
                    }
                }
            }
        }
    }

    private fun mountExternalContentDirectories(mountedContainerUris: MutableSet<String>) {
        val uniqueExternalContentDirs = linkedSetOf<String>()
        NativeConfig.getExternalContentDirs().forEach { externalDir ->
            if (externalDir.isNotEmpty()) {
                uniqueExternalContentDirs.add(externalDir)
            }
        }

        for (externalDir in uniqueExternalContentDirs) {
            val externalDirUri = externalDir.toUri()
            if (FileUtil.isTreeUriValid(externalDirUri)) {
                scanContentContainersRecursive(FileUtil.listFiles(externalDirUri), 3) {
                    val containerUri = it.uri.toString()
                    if (mountedContainerUris.add(containerUri)) {
                        NativeLibrary.addFileToFilesystemProvider(containerUri)
                    }
                }
            }
        }
    }

    private fun mountGameFolderContent(gameUri: Uri, mountedContainerUris: MutableSet<String>) {
        if (!BooleanSetting.EXT_CONTENT_FROM_GAME_DIRS.getBoolean()) {
            return
        }
        if (gameUri.scheme == "content") {
            val parentUri = getParentDocumentUri(gameUri) ?: return
            scanContentContainersRecursive(FileUtil.listFiles(parentUri), 1) {
                val containerUri = it.uri.toString()
                if (mountedContainerUris.add(containerUri)) {
                    NativeLibrary.addGameFolderFileToFilesystemProvider(containerUri)
                }
            }
            return
        }

        val gameFile = File(gameUri.path ?: gameUri.toString())
        val parentDir = gameFile.parentFile ?: return
        parentDir.listFiles()?.forEach { sibling ->
            if (!sibling.isFile) {
                return@forEach
            }

            val extension = sibling.extension.lowercase()
            if (externalContentExtensions.contains(extension)) {
                val containerUri = Uri.fromFile(sibling).toString()
                if (mountedContainerUris.add(containerUri)) {
                    NativeLibrary.addGameFolderFileToFilesystemProvider(containerUri)
                }
            }
        }
    }

    private fun getParentDocumentUri(uri: Uri): Uri? {
        return try {
            val documentId = DocumentsContract.getDocumentId(uri)
            val separatorIndex = documentId.lastIndexOf('/')
            if (separatorIndex == -1) {
                null
            } else {
                val parentDocumentId = documentId.substring(0, separatorIndex)
                DocumentsContract.buildDocumentUriUsingTree(uri, parentDocumentId)
            }
        } catch (_: Exception) {
            null
        }
    }

    fun cleanGameTitle(rawTitle: String): String {
        var clean = rawTitle.trim()
        clean = clean.replace(Regex("\\.(nsp|nsz|xci|xcz|zip|7z)$", RegexOption.IGNORE_CASE), "").trim()
        val firstBracket = clean.indexOfAny(charArrayOf('[', '('))
        if (firstBracket > 0) {
            clean = clean.substring(0, firstBracket).trim()
        } else if (firstBracket == 0) {
            clean = clean.replace(Regex("\\[.*?\\]"), " ").replace(Regex("\\(.*?\\)"), " ").trim()
        }
        clean = clean.replace(Regex("[-_:]+$"), "").trim()
        return clean.ifEmpty { rawTitle }
    }

    fun getGame(
        uri: Uri,
        addedToLibrary: Boolean = false,
        registerFilesystemProvider: Boolean = true
    ): Game? {
        val filePath = uri.toString()
        if (!GameMetadata.getIsValid(filePath)) {
            return null
        }
        if (!GameMetadata.isBaseGame(filePath)) {
            return null
        }

        if (registerFilesystemProvider) {
            // Needed to update installed content information
            NativeLibrary.addFileToFilesystemProvider(filePath)
        }

        val nacpTitle = GameMetadata.getTitle(filePath).trim()
        val filename = FileUtil.getFilename(uri)
        val useFilename = BooleanSetting.SHOW_FILENAME_AS_TITLE.getBoolean()
        val name = if (useFilename) {
            cleanGameTitle(filename)
        } else {
            if (nacpTitle.isNotEmpty()) nacpTitle else cleanGameTitle(filename)
        }

        var programId = GameMetadata.getProgramId(filePath)

        val pIdLong = programId.toULongOrNull(16)
        if (pIdLong != null && (pIdLong and 0xFFFuL) != 0uL) {
            return null // Exclude standalone Updates and DLCs from game list
        }

        // If the game's ID field is empty, use the filename without extension.
        if (programId.isEmpty()) {
            programId = if (filename.contains(".")) filename.substring(0, filename.lastIndexOf(".")) else filename
        }

        val rawVersion = GameMetadata.getVersion(filePath, false)
        var cleanVersion = rawVersion.trim().removePrefix("v").removePrefix("V").ifEmpty { "1.0.0" }
        var rawInternalVersion = GameMetadata.getInternalVersion(filePath).trim().removePrefix("v").removePrefix("V")
        var cleanInternalVersion = rawInternalVersion.ifEmpty { "0" }

        // If version is default 1.0.0 or internal version is 0, extract paired or standalone version from filename
        val decodedFilename = runCatching { Uri.decode(filePath) }.getOrDefault(filePath)
        val pairMatch = Regex("""\(([0-9]+\.[0-9]+(?:\.[0-9]+)*)\s*-\s*([0-9]+)""", RegexOption.IGNORE_CASE).find(decodedFilename)
        if (pairMatch != null) {
            val pVer = pairMatch.groupValues[1].trim()
            val pIntVer = pairMatch.groupValues[2].trim()
            if (pVer.isNotEmpty() && (cleanVersion == "1.0.0" || cleanVersion.isEmpty())) {
                cleanVersion = pVer
            }
            if (pIntVer.isNotEmpty() && (cleanInternalVersion == "0" || cleanInternalVersion.isEmpty())) {
                cleanInternalVersion = pIntVer
            }
        } else if (cleanVersion == "1.0.0" || cleanVersion.isEmpty()) {
            val fullMatch = Regex("""(?:[\(\[\s_]v?|\b)([0-9]+\.[0-9]+(?:\.[0-9]+)*)(?!\s*(?:GB|MB|KB|TB|ГБ|МБ|КБ|Б|B)\b)""", RegexOption.IGNORE_CASE).find(decodedFilename)
            if (fullMatch != null) {
                val parsedVer = fullMatch.groupValues[1].trim()
                if (parsedVer.isNotEmpty() && parsedVer != "1.0.0") {
                    cleanVersion = parsedVer
                }
            }
        }

        if (cleanInternalVersion.isEmpty() || cleanInternalVersion == "0") {
            val match = Regex("[\\[\\(_]v?(\\d{5,})[\\]\\)]", RegexOption.IGNORE_CASE).find(decodedFilename)
            if (match != null) {
                cleanInternalVersion = match.groupValues[1]
            }
        }

        val addonCount = GameMetadata.getAddonCount(filePath)
        val finalAddonCount = if (addonCount > 0) {
            addonCount
        } else {
            cachedGameList.firstOrNull { it.path == filePath || it.programId == programId }?.addonCount ?: 0
        }

        val newGame = Game(
            name,
            filePath,
            programId,
            GameMetadata.getDeveloper(filePath),
            cleanVersion,
            cleanInternalVersion,
            GameMetadata.getIsHomebrew(filePath),
            finalAddonCount
        )


        if (addedToLibrary) {
            val addedTime = preferences.getLong(newGame.keyAddedToLibraryTime, 0L)
            if (addedTime == 0L) {
                preferences.edit()
                    .putLong(newGame.keyAddedToLibraryTime, System.currentTimeMillis())
                    .apply()
            }
        }

        return newGame
    }
}
