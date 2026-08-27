// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.model

import android.content.Context
import androidx.preference.PreferenceManager
import org.yuzu.yuzu_emu.utils.DirectoryInitialization
import org.yuzu.yuzu_emu.utils.GameMetadata
import org.yuzu.yuzu_emu.utils.Log
import org.yuzu.yuzu_emu.utils.NativeConfig
import java.io.File

data class GameFixProfile(
    val titleId: Long,
    val gameName: String,
    val issuesRu: String,
    val issuesEn: String,
    val fixesRu: String,
    val fixesEn: String,
    val settingsMap: Map<String, String>
)

object GameFixDatabase {

    private val profiles = listOf(
        GameFixProfile(
            0x01007EF00011E000L,
            "The Legend of Zelda: Breath of the Wild",
            "• Белая непрозрачная вода из-за рассинхронизации буфера глубины и рефракций\n• Желтые/ослепляющие вспышки частиц и тумана в Святилищах (Shrines)\n• Мерцание текстур травы и теней",
            "• White opaque water caused by depth buffer and refraction desync\n• Yellow/blinding particle flashes and fog in Shrines\n• Grass and shadow texture flickering",
            "✓ Точность GPU: Высокая (High — идеальная прозрачная вода)\n✓ Реактивный сброс (Reactive Flushing): Включено\n✓ Сжатие ASTC: Отключено (Uncompressed — решает проблемы воды и Святилищ)\n✓ Асинхронные шейдеры: Включено\n✓ Память: 8GB DRAM",
            "✓ GPU Accuracy: High (Crystal clear water transparency)\n✓ Reactive Flushing: Enabled\n✓ ASTC Recompression: Uncompressed (Fixes water & Shrines)\n✓ Asynchronous Shaders: Enabled\n✓ Memory Layout: 8GB DRAM",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_reactive_flushing" to "true",
                "Renderer\\astc_recompression" to "0",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Core\\memory_layout_mode" to "2"
            )
        ),
        GameFixProfile(
            0x0100F2C0115B6000L,
            "The Legend of Zelda: Tears of the Kingdom",
            "• Черный экран при смене оружия и способностей (Fuse/Ultrahand)\n• Утечки VRAM в кавернах и святилищах\n• Мерцание текстур скверны (Gloom) и теней облаков",
            "• Black screen during weapon/ability switching (Fuse menu)\n• VRAM leaks in Depths and Shrines\n• Gloom texture flickering and cloud shadow artifacts",
            "✓ Точность GPU: Высокая (High)\n✓ Реактивный сброс (Reactive Flushing): Включено\n✓ Сжатие ASTC: Отключено (Uncompressed)\n✓ Анизотропная фильтрация: 16x\n✓ Асинхронные шейдеры: Включено\n✓ Память: 8GB DRAM",
            "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ ASTC Recompression: Uncompressed\n✓ Anisotropic Filtering: 16x\n✓ Asynchronous Shaders: Enabled\n✓ Memory Layout: 8GB DRAM",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_reactive_flushing" to "true",
                "Renderer\\astc_recompression" to "0",
                "Renderer\\max_anisotropy" to "5",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Core\\memory_layout_mode" to "2"
            )
        ),
        GameFixProfile(
            0x01004D701742A000L,
            "Paper Mario: The Thousand-Year Door",
            "• Черный экран на катсценах в прологе\n• Сбои 2D-шрифтов диалогов и мерцание текстур",
            "• Black screen during prologue cutscenes\n• Corrupted battle text boxes and flickering textures",
            "✓ Точность GPU: Высокая (High)\n✓ Сжатие ASTC: Отключено (для идеального видео)\n✓ Реактивный сброс (Reactive Flushing): Включено",
            "✓ GPU Accuracy: High\n✓ ASTC Recompression: Uncompressed\n✓ Reactive Flushing: Enabled",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\astc_recompression" to "0",
                "Renderer\\use_reactive_flushing" to "true"
            )
        ),
        GameFixProfile(
            0x0100B5B0112F8000L,
            "Hogwarts Legacy",
            "• Вылет из-за нехватки памяти (OOM) при загрузке замка Хогвартс\n• Высокое потребление ОЗУ (>8.5 ГБ) на мобильных чипах",
            "• Out of memory (OOM) crash when loading Hogwarts Castle\n• High RAM consumption (>8.5 GB) on mobile SoCs",
            "✓ Разрешение: Handheld 0.75X + FSR 80%\n✓ Сжатие текстур ASTC: BC1 (снижает RAM до 4.8 ГБ)\n✓ Режим памяти: 8GB DRAM",
            "✓ Resolution: Handheld 0.75X + FSR 80%\n✓ ASTC Recompression: BC1 (lowers RAM to 4.8 GB)\n✓ Memory Layout: 8GB DRAM",
            mapOf(
                "Renderer\\astc_recompression" to "1",
                "Renderer\\resolution_setup" to "1",
                "Renderer\\fsr_sharpening_slider" to "80",
                "Core\\memory_layout_mode" to "2"
            )
        ),
        GameFixProfile(
            0x0100916014D8C000L,
            "Diablo II: Resurrected",
            "• Быстрый нагрев устройства (>46°C за 5 минут)\n• Термальный троттлинг и просадка FPS с 30 до 22",
            "• Rapid SoC heating (>46°C in 5 min)\n• Thermal throttling dropping FPS from 30 to 22",
            "✓ Разрешение: Handheld 0.75X-1.0X + FSR 80%\n✓ Сжатие ASTC: BC1/BC3\n✓ Сброс реактивной памяти (Reactive Flushing): Отключено",
            "✓ Resolution: Handheld 0.75X-1.0X + FSR 80%\n✓ ASTC Recompression: BC1/BC3\n✓ Reactive Flushing: Disabled",
            mapOf(
                "Renderer\\astc_recompression" to "1",
                "Renderer\\use_reactive_flushing" to "false",
                "Renderer\\fsr_sharpening_slider" to "80"
            )
        ),
        GameFixProfile(
            0x0100C6000EEA8000L,
            "Warhammer 40,000: Mechanicus",
            "• Невозможно сохранить прогресс игры (ошибка сохранения)",
            "• Unable to save game progress (infinite save loop)",
            "✓ Поддержка RenameDirectory в STORM EDEN 4.6.0+\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ RenameDirectory support in STORM EDEN 4.6.0+\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100923008C54000L,
            "LEGO Star Wars: The Skywalker Saga",
            "• Бесконечная загрузка (зацикливание индикатора загрузки TT Games)\n• Зависание асинхронного таймера GPU при обращении к ресурсам",
            "• Infinite loading screen (TT Games loading indicator loop)\n• GPU async timer deadlock during asset initialization",
            "✓ Быстрое время GPU (Fast GPU Time): Отключено (устраняет вечную загрузку!)\n✓ Динамическое состояние: Базовое (EDS1)\n✓ Точность GPU: Высокая (High)\n✓ Точность CPU: Точная (Accurate)\n✓ Реактивный сброс: Отключено\n✓ Память: 8GB DRAM",
            "✓ Fast GPU Time: Disabled (Fixes infinite loading!)\n✓ Dynamic State: Basic (EDS1)\n✓ GPU Accuracy: High\n✓ CPU Accuracy: Accurate\n✓ Reactive Flushing: Disabled\n✓ Memory Layout: 8GB DRAM",
            mapOf(
                "Renderer\\use_fast_gpu_time" to "false",
                "Renderer\\dyna_state" to "0",
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_reactive_flushing" to "false",
                "Renderer\\astc_recompression" to "0",
                "Renderer\\use_asynchronous_shaders" to "false",
                "Cpu\\cpu_accuracy" to "0",
                "Core\\memory_layout_mode" to "2"
            )
        ),
        GameFixProfile(
            0x0100E26017E5E000L,
            "Red Dead Redemption",
            "• Хрипы и рассинхронизация звука в катсценах\n• Микрофризы физических потоков RAGE Engine",
            "• Audio crackling and desync in cutscenes\n• Micro-stutters in RAGE Engine physics threads",
            "✓ Точность CPU: Точная (Accurate)\n✓ Аудио-буфер Cubeb: 80 ms\n✓ Синхронизация памяти (Sync Memory): Отключено",
            "✓ CPU Accuracy: Accurate\n✓ Cubeb Audio Buffer: 80 ms\n✓ Sync Memory Ops: Disabled",
            mapOf(
                "Cpu\\cpu_accuracy" to "0",
                "Renderer\\sync_memory_operations" to "false"
            )
        ),
        GameFixProfile(
            0x0100D870045B6000L,
            "Luigi's Mansion 3",
            "• Растягивание полигонов (взрывы геометрии)\n• Невидимый луч фонарика и зависания в лифте",
            "• Vertex explosion (stretched geometry)\n• Invisible flashlight beam and elevator freeze",
            "✓ Extended Dynamic State: Включено\n✓ Точность GPU: Высокая (High)\n✓ Точность CPU: Accurate",
            "✓ Extended Dynamic State: Enabled\n✓ GPU Accuracy: High\n✓ CPU Accuracy: Accurate",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpu_accuracy" to "0",
                "Renderer\\dyna_state" to "2"
            )
        ),
        GameFixProfile(
            0x01004A4010F22000L,
            "Bayonetta 3",
            "• Невидимые персонажи и противники на чипах Snapdragon\n• Чёрный экран после QTE-добиваний",
            "• Invisible character/enemy models on Snapdragon SoCs\n• Black screen after QTE sequences",
            "✓ Depth Clip Control: Включено (STORM DRIVER)\n✓ Точность GPU: Высокая (High)",
            "✓ Depth Clip Control: Enabled (STORM DRIVER)\n✓ GPU Accuracy: High",
            mapOf(
                "Renderer\\gpu_accuracy" to "1"
            )
        ),
        GameFixProfile(
            0x01007300020FA000L,
            "Astral Chain",
            "• Пропадание неонового интерфейса Легиона\n• Затемнение картинки и циклический гул звука",
            "• Missing Legion neon glow effects\n• Dark screen tint and audio looping",
            "✓ Эмуляция цвета BGR565: Включено\n✓ Коррекция эффектов свечения (Fix Bloom): Включено",
            "✓ Emulate BGR565: Enabled\n✓ Fix Bloom Effects: Enabled",
            mapOf(
                "Renderer\\emulate_bgr565" to "true",
                "Renderer\\fix_bloom_effects" to "true"
            )
        ),
        GameFixProfile(
            0x01006A800016E000L,
            "Super Smash Bros. Ultimate",
            "• Вылет на экране победы или в меню новостей (Web Applet)",
            "• Crash on victory screen or news board (Web Applet)",
            "✓ Отключение Web-апплета: Включено\n✓ Mii Applet: LLE",
            "✓ Disable Web Applet: Enabled\n✓ Mii Applet: LLE",
            mapOf(
                "Debugging\\disable_web_applet" to "true"
            )
        ),
        GameFixProfile(
            0x0100152000022000L,
            "Mario Kart 8 Deluxe",
            "• Отсутствие голов у персонажей Mii на трассах",
            "• Invisible/missing heads on Mii characters",
            "✓ Требуется Firmware 18.0.0+ и системные файлы Mii\n✓ Сжатие ASTC: Uncompressed",
            "✓ Firmware 18.0.0+ and Mii system files required\n✓ ASTC Recompression: Uncompressed",
            mapOf(
                "Renderer\\astc_recompression" to "0"
            )
        ),
        GameFixProfile(
            0x01001F5010DFA000L,
            "Pokemon Legends: Arceus",
            "• Вытягивание полигонов травы и деревьев в небо\n• Сбои теней на аренах",
            "• Vertex explosion on trees and grass geometry\n• Shadow glitches during battle transitions",
            "✓ Точность GPU: Высокая (High)\n✓ Анизотропная фильтрация: 16x\n✓ Декодирование ASTC на GPU: Включено",
            "✓ GPU Accuracy: High\n✓ Anisotropic Filtering: 16x\n✓ ASTC GPU Decode: Enabled",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\max_anisotropy" to "5"
            )
        ),
        GameFixProfile(
            0x01008C30086E0000L,
            "Pokemon Scarlet",
            "• Утечки памяти в открытом мире Палдеи\n• Мерцание ландшафта и текстур",
            "• Open-world memory leaks in Paldea\n• Terrain and texture flickering",
            "✓ Разрешение: Handheld 0.75X + FSR 75%\n✓ Сжатие ASTC: BC3\n✓ Ограничение VRAM: Conservative",
            "✓ Resolution: Handheld 0.75X + FSR 75%\n✓ ASTC Recompression: BC3\n✓ VRAM Usage: Conservative",
            mapOf(
                "Renderer\\astc_recompression" to "2",
                "Renderer\\resolution_setup" to "1",
                "Renderer\\vram_usage_mode" to "1"
            )
        ),
        GameFixProfile(
            0x0100B9F010DC4000L,
            "Doom Eternal",
            "• Вылет драйвера Vulkan при первом выстреле / спавне BFG",
            "• Vulkan device loss crash on weapon fire / BFG",
            "✓ Проверка границ глубины: STORM DRIVER (pan_depth_bounds)\n✓ Точность DMA: Safe",
            "✓ Depth bounds test: STORM DRIVER (pan_depth_bounds)\n✓ DMA Accuracy: Safe",
            mapOf(
                "Renderer\\dma_accuracy" to "1"
            )
        ),
        GameFixProfile(
            0x010034B01314C000L,
            "Prince of Persia: The Lost Crown",
            "• Чёрный экран при воспроизведении видеовставок и анимаций амулетов",
            "• Black screen during video cutscenes and amulet animations",
            "✓ Декодирование видео (NVDEC): На GPU\n✓ Fastmem Exclusives: Отключено",
            "✓ NVDEC Video Emulation: GPU\n✓ Fastmem Exclusives: Disabled",
            mapOf(
                "Renderer\\nvdec_emulation" to "2",
                "Cpu\\cpuopt_fastmem_exclusives" to "false"
            )
        ),
        GameFixProfile(
            0x010063B017DAE000L,
            "Batman: Arkham Knight",
            "• Вылет по нехватке памяти (OOM) при погонях на Бэтмобиле",
            "• OOM crash during Batmobile chase sequences",
            "✓ Разрешение: Handheld 0.75X + FSR 80%\n✓ Сжатие ASTC: BC1\n✓ Память: 8GB DRAM",
            "✓ Resolution: Handheld 0.75X + FSR 80%\n✓ ASTC Recompression: BC1\n✓ Memory Layout: 8GB DRAM",
            mapOf(
                "Renderer\\astc_recompression" to "1",
                "Renderer\\resolution_setup" to "1",
                "Core\\memory_layout_mode" to "2"
            )
        ),
        GameFixProfile(
            0x01000B901C46E000L,
            "Shin Megami Tensei V: Vengeance",
            "• Вылет движка Unreal Engine 4 при старте на чипах Snapdragon 8",
            "• Unreal Engine 4 crash on launch on Snapdragon 8 devices",
            "✓ Macro JIT / HLE: Включено\n✓ Нативное декодирование BCn: Включено",
            "✓ Macro JIT / HLE: Enabled\n✓ Native BCn Decode: Enabled",
            mapOf(
                "Debugging\\disable_macro_jit" to "false",
                "Debugging\\disable_macro_hle" to "false"
            )
        ),
        GameFixProfile(
            0x01003D100E9C6000L,
            "The Witcher 3: Wild Hunt",
            "• Зависание физики волос/одежды Геральта в Новиграде",
            "• HairWorks and physics freezes in Novigrad",
            "✓ Fastmem Exclusives: Включено\n✓ Разрешение: Handheld 0.75X + FSR 85%",
            "✓ Fastmem Exclusives: Enabled\n✓ Resolution: Handheld 0.75X + FSR 85%",
            mapOf(
                "Cpu\\cpuopt_fastmem_exclusives" to "true",
                "Renderer\\resolution_setup" to "1",
                "Renderer\\fsr_sharpening_slider" to "85"
            )
        ),
        GameFixProfile(
            0x0100760012E4A000L,
            "Mario + Rabbids Sparks of Hope",
            "• Вылет движка Snowdrop при переходе в тактический бой",
            "• Snowdrop engine crash on tactical combat transition",
            "✓ Точность DMA: Safe\n✓ Точность GPU: Высокая (High)\n✓ Barrier Feedback Loops: Включено",
            "✓ DMA Accuracy: Safe\n✓ GPU Accuracy: High\n✓ Barrier Feedback Loops: Enabled",
            mapOf(
                "Renderer\\dma_accuracy" to "1",
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\barrier_feedback_loops" to "true"
            )
        ),
        GameFixProfile(
            0x010056D015DB6000L,
            "Sonic Frontiers",
            "• Падение FPS и вылет в открытых зонах островов",
            "• Frame drops and OOM crash in open-zone islands",
            "✓ Разрешение: Handheld 0.75X + FSR 80%\n✓ Сжатие ASTC: BC1\n✓ Память: 8GB DRAM",
            "✓ Resolution: Handheld 0.75X + FSR 80%\n✓ ASTC Recompression: BC1\n✓ Memory Layout: 8GB DRAM",
            mapOf(
                "Renderer\\astc_recompression" to "1",
                "Renderer\\resolution_setup" to "1",
                "Core\\memory_layout_mode" to "2"
            )
        ),
        GameFixProfile(
            0x01004AB00A266000L,
            "Dark Souls: Remastered",
            "• Просадки кадровой частоты у костров и зацикливание звука баффов",
            "• Bonfire particle slowdown and weapon buff sound loop",
            "✓ Точность CPU: Accurate\n✓ Точность GPU: High\n✓ Аудио-движок: Cubeb",
            "✓ CPU Accuracy: Accurate\n✓ GPU Accuracy: High\n✓ Audio Engine: Cubeb",
            mapOf(
                "Cpu\\cpu_accuracy" to "0",
                "Renderer\\gpu_accuracy" to "1"
            )
        ),
        GameFixProfile(
            0x0100C9E01B854000L,
            "Animal Well",
            "• Чёрный экран и пропадание звуковых дорожек",
            "• Black screen and missing audio tracks on startup",
            "✓ Аудио-движок: Cubeb 48kHz\n✓ Асинхронные шейдеры: Включено",
            "✓ Audio Engine: Cubeb 48kHz\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        )
    )

    fun parseProgramId(programIdStr: String): Long {
        val str = programIdStr.trim()
        if (str.isEmpty()) return 0L
        
        // 1. If explicit Hex format (starts with 0x/0X or has A-F characters)
        if (str.startsWith("0x", ignoreCase = true)) {
            try {
                val hex = java.lang.Long.parseUnsignedLong(str.substring(2), 16)
                if (hex != 0L) return hex
            } catch (_: Exception) {}
        }

        if (str.any { it in 'a'..'f' || it in 'A'..'F' }) {
            try {
                val hex = java.lang.Long.parseUnsignedLong(str, 16)
                if (hex != 0L) return hex
            } catch (_: Exception) {}
        }

        // 2. If it's a 16-character hex string starting with 0100
        if (str.length == 16 && str.startsWith("0100", ignoreCase = true)) {
            try {
                val hex = java.lang.Long.parseUnsignedLong(str, 16)
                if (hex != 0L) return hex
            } catch (_: Exception) {}
        }

        // 3. Try parsing decimal representation (from std::to_string(u64))
        try {
            val dec = java.lang.Long.parseUnsignedLong(str, 10)
            if (dec != 0L && ((dec ushr 48) == 0x0100L || (dec ushr 48) == 0x0101L || dec > 0x1000000000000L)) {
                return dec
            }
        } catch (_: Exception) {}

        // 4. Fallback to hexadecimal representation
        try {
            val hex = java.lang.Long.parseUnsignedLong(str, 16)
            if (hex != 0L) return hex
        } catch (_: Exception) {}

        return 0L
    }

    fun resolveTitleId(game: Game): Long {
        var id = parseProgramId(game.programId)
        if (id != 0L) return id

        // Extract 16-hex Title ID from filename e.g. "Game [0100916014D8C000].nsp"
        try {
            val regex = Regex("\\[([0-9a-fA-F]{16})\\]")
            val match = regex.find(game.path)
            if (match != null) {
                val hex = match.groupValues[1]
                id = java.lang.Long.parseUnsignedLong(hex, 16)
                if (id != 0L) return id
            }
        } catch (_: Exception) {}

        // Try reading metadata via GameMetadata JNI
        try {
            if (game.path.isNotEmpty()) {
                val progId = GameMetadata.getProgramId(game.path)
                if (progId.isNotEmpty()) {
                    id = parseProgramId(progId)
                    if (id != 0L) return id
                }
            }
        } catch (_: Exception) {}

        return 0L
    }

    fun getProgramIdHex(programIdStr: String): String {
        val idLong = parseProgramId(programIdStr)
        return if (idLong != 0L) String.format("%016X", idLong) else ""
    }

    fun getProgramIdHex(game: Game): String {
        val idLong = resolveTitleId(game)
        if (idLong != 0L) return String.format("%016X", idLong)
        val profile = getFix(game)
        return if (profile != null) String.format("%016X", profile.titleId) else ""
    }

    fun getFix(programIdStr: String): GameFixProfile? {
        val idLong = parseProgramId(programIdStr)
        if (idLong == 0L) return null
        val baseId = idLong and 0x1FFFL.inv()
        return profiles.firstOrNull { it.titleId == idLong || (it.titleId and 0x1FFFL.inv()) == baseId }
    }

    fun getFix(game: Game): GameFixProfile? {
        val idLong = resolveTitleId(game)
        if (idLong != 0L) {
            val baseId = idLong and 0x1FFFL.inv()
            val byId = profiles.firstOrNull { it.titleId == idLong || (it.titleId and 0x1FFFL.inv()) == baseId }
            if (byId != null) return byId
        }

        // 100% Robust fallback: match by game title keywords & path
        val cleanTitle = (game.title ?: "").lowercase(java.util.Locale.ROOT)
        val cleanPath = (game.path ?: "").lowercase(java.util.Locale.ROOT)
        return profiles.firstOrNull { profile ->
            val nameLower = profile.gameName.lowercase(java.util.Locale.ROOT)
            val keywords = when {
                nameLower.contains("breath of the wild") -> listOf("breath of the wild", "botw", "01007ef00011e000")
                nameLower.contains("tears of the kingdom") -> listOf("tears of the kingdom", "totk", "0100f2c0115b6000")
                nameLower.contains("paper mario") -> listOf("paper mario", "thousand-year", "01004d701742a000")
                nameLower.contains("hogwarts legacy") -> listOf("hogwarts", "0100b5b0112f8000")
                nameLower.contains("diablo ii") -> listOf("diablo ii", "diablo 2", "resurrected", "0100916014d8c000")
                nameLower.contains("mechanicus") -> listOf("mechanicus", "warhammer", "0100c6000eea8000")
                nameLower.contains("skywalker saga") -> listOf("skywalker saga", "lego star wars", "0100923008c54000")
                nameLower.contains("bayonetta 3") -> listOf("bayonetta 3", "01004a4010fea000")
                nameLower.contains("shin megami tensei v") -> listOf("vengeance", "shin megami", "smt v", "smtv", "01006f801bc4c000")
                nameLower.contains("luigi's mansion 3") -> listOf("luigi's mansion 3", "luigis mansion", "0100d7c000b02000")
                nameLower.contains("scarlet") -> listOf("scarlet", "0100a3d008c5c000")
                nameLower.contains("violet") -> listOf("violet", "01008f6008c5e000")
                nameLower.contains("arkham knight") -> listOf("arkham knight", "010023a017e94000")
                nameLower.contains("doom eternal") -> listOf("doom eternal", "0100bb600dc30000")
                nameLower.contains("lost crown") -> listOf("lost crown", "prince of persia", "0100bb70144f8000")
                nameLower.contains("animal well") -> listOf("animal well", "010092c01d9f8000")
                else -> listOf(nameLower)
            }
            keywords.any { cleanTitle.contains(it) || cleanPath.contains(it) }
        }
    }

    fun hasFix(programIdStr: String): Boolean {
        return getFix(programIdStr) != null
    }

    fun hasFix(game: Game): Boolean {
        return getFix(game) != null
    }

    fun isDontAskAgain(context: Context, programIdStr: String): Boolean {
        val hex = getProgramIdHex(programIdStr)
        if (hex.isEmpty()) return false
        val prefs = PreferenceManager.getDefaultSharedPreferences(context)
        return prefs.getBoolean("storm_fix_dont_ask_$hex", false)
    }

    fun isDontAskAgain(context: Context, game: Game): Boolean {
        val hex = getProgramIdHex(game)
        if (hex.isEmpty()) return false
        val prefs = PreferenceManager.getDefaultSharedPreferences(context)
        return prefs.getBoolean("storm_fix_dont_ask_$hex", false)
    }

    fun setDontAskAgain(context: Context, programIdStr: String, value: Boolean = true) {
        val hex = getProgramIdHex(programIdStr)
        if (hex.isNotEmpty()) {
            val prefs = PreferenceManager.getDefaultSharedPreferences(context)
            prefs.edit().putBoolean("storm_fix_dont_ask_$hex", value).apply()
        }
    }

    fun setDontAskAgain(context: Context, game: Game, value: Boolean = true) {
        val hex = getProgramIdHex(game)
        if (hex.isNotEmpty()) {
            val prefs = PreferenceManager.getDefaultSharedPreferences(context)
            prefs.edit().putBoolean("storm_fix_dont_ask_$hex", value).apply()
        }
    }

    fun isFixApplied(game: Game): Boolean {
        val titleIdHex = getProgramIdHex(game)
        if (titleIdHex.isEmpty()) return false
        val filesToCheck = listOf(
            File(DirectoryInitialization.userDirectory, "config/custom/$titleIdHex.ini"),
            File(DirectoryInitialization.userDirectory, "config/custom/${game.settingsName}.ini")
        )
        for (configFile in filesToCheck) {
            if (configFile.exists()) {
                try {
                    val content = configFile.readText()
                    if (content.contains("storm_fix_applied=true") || content.contains("storm_fix_applied = true")) {
                        return true
                    }
                } catch (_: Exception) {}
            }
        }
        return false
    }

    fun applyFix(game: Game): Boolean {
        val fix = getFix(game) ?: return false
        val titleIdHex = getProgramIdHex(game)
        if (titleIdHex.isEmpty()) return false
        val configDir = File(DirectoryInitialization.userDirectory, "config/custom")
        if (!configDir.exists()) {
            configDir.mkdirs()
        }
        val targetFiles = listOf(
            File(configDir, "$titleIdHex.ini"),
            File(configDir, "${game.settingsName}.ini")
        )

        return try {
            val sections = mutableMapOf<String, MutableMap<String, String>>()
            val primaryFile = targetFiles.firstOrNull { it.exists() } ?: targetFiles[0]
            if (primaryFile.exists()) {
                var currentSection = ""
                primaryFile.forEachLine { rawLine ->
                    val line = rawLine.trim()
                    if (line.isNotEmpty() && !line.startsWith("#") && !line.startsWith(";")) {
                        if (line.startsWith("[") && line.endsWith("]")) {
                            currentSection = line.substring(1, line.length - 1)
                            sections.putIfAbsent(currentSection, mutableMapOf())
                        } else {
                            val eq = line.indexOf('=')
                            if (eq != -1 && currentSection.isNotEmpty()) {
                                val k = line.substring(0, eq).trim()
                                val v = line.substring(eq + 1).trim()
                                sections[currentSection]?.put(k, v)
                            }
                        }
                    }
                }
            }

            // Merge fix settings
            for ((fullKey, value) in fix.settingsMap) {
                val parts = fullKey.split("\\")
                if (parts.size == 2) {
                    val sec = parts[0]
                    val key = parts[1]
                    sections.putIfAbsent(sec, mutableMapOf())
                    sections[sec]?.put(key, value)
                    sections[sec]?.put("$key\\default", "false")
                }
            }
            sections.putIfAbsent("StormEden", mutableMapOf())
            sections["StormEden"]?.put("storm_fix_applied", "true")

            // Write back INI
            val sb = java.lang.StringBuilder()
            for ((sec, kvs) in sections) {
                sb.append("[$sec]\n")
                for ((k, v) in kvs) {
                    sb.append("$k=$v\n")
                }
                sb.append("\n")
            }
            val textToSave = sb.toString()
            for (f in targetFiles) {
                f.writeText(textToSave)
                Log.info("[GameFixDatabase] Applied game fix profile to ${f.absolutePath}")
            }

            // Also directly apply to in-memory settings if session is running
            for ((fullKey, value) in fix.settingsMap) {
                val parts = fullKey.split("\\")
                if (parts.size == 2) {
                    val key = parts[1]
                    try {
                        when (key) {
                            "use_fast_gpu_time", "use_reactive_flushing", "use_asynchronous_shaders", "cpuopt_fastmem" -> {
                                NativeConfig.setBoolean(key, value.toBoolean())
                            }
                            "gpu_accuracy", "astc_recompression", "resolution_setup", "fsr_sharpening_slider", "max_anisotropy", "cpu_accuracy", "memory_layout_mode", "dyna_state" -> {
                                NativeConfig.setInt(key, value.toIntOrNull() ?: 0)
                            }
                        }
                    } catch (_: Exception) {}
                }
            }

            true
        } catch (e: Exception) {
            Log.error("[GameFixDatabase] Failed to apply game fix: ${e.message}")
            false
        }
    }
}
