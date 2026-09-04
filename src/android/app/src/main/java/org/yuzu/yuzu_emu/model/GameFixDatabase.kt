// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.model

import android.content.Context
import androidx.preference.PreferenceManager
import org.yuzu.yuzu_emu.utils.DirectoryInitialization
import org.yuzu.yuzu_emu.utils.GameMetadata
import org.yuzu.yuzu_emu.utils.Log
import org.yuzu.yuzu_emu.utils.NativeConfig
import org.yuzu.yuzu_emu.features.settings.utils.SettingsFile
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
            0x01005B101DC84000L,
            "EA SPORTS FC 25",
            "• Зависание при старте игры / черный экран на заставке EA\n• Нехватка памяти Frostbite Engine (вылет при загрузке стадиона)\n• Зависание на сетевой аутентификации",
            "• Boot hang / black screen on EA splash screen\n• Frostbite Engine out-of-memory crash\n• Network handshake freeze",
            "✓ Память: 8GB DRAM (критично для движка Frostbite)\n✓ Режим полета: Включено (пропуск серверов EA Connect)\n✓ Асинхронные шейдеры: Отключено (стабильность загрузки)\n✓ Быстрая память: Включено",
            "✓ Memory Layout: 8GB DRAM (Critical for Frostbite Engine)\n✓ Airplane Mode: Enabled (Bypasses EA Connect handshake)\n✓ Asynchronous Shaders: Disabled (Prevents boot crash)\n✓ Fastmem: Enabled",
            mapOf(
                "Core\\memory_layout_mode" to "2",
                "System\\memory_layout_mode" to "2",
                "System\\airplane_mode" to "true",
                "Renderer\\use_asynchronous_shaders" to "false",
                "Cpu\\cpuopt_fastmem" to "true"
            )
        ),
        GameFixProfile(
            0x010077D0238FA000L,
            "EA SPORTS FC 26",
            "• Зависание при старте игры / черный экран на заставке EA\n• Нехватка памяти Frostbite Engine (вылет при загрузке стадиона)\n• Зависание на сетевой аутентификации",
            "• Boot hang / black screen on EA splash screen\n• Frostbite Engine out-of-memory crash\n• Network handshake freeze",
            "✓ Память: 8GB DRAM (критично для движка Frostbite)\n✓ Режим полета: Включено (пропуск серверов EA Connect)\n✓ Асинхронные шейдеры: Отключено (стабильность загрузки)\n✓ Быстрая память: Включено",
            "✓ Memory Layout: 8GB DRAM (Critical for Frostbite Engine)\n✓ Airplane Mode: Enabled (Bypasses EA Connect handshake)\n✓ Asynchronous Shaders: Disabled (Prevents boot crash)\n✓ Fastmem: Enabled",
            mapOf(
                "Core\\memory_layout_mode" to "2",
                "System\\memory_layout_mode" to "2",
                "System\\airplane_mode" to "true",
                "Renderer\\use_asynchronous_shaders" to "false",
                "Cpu\\cpuopt_fastmem" to "true"
            )
        ),
        GameFixProfile(
            0x01008CA01F186000L,
            "Leap Year",
            "• Вылет игры через некоторое время из-за утечки памяти\n• Рассинхронизация аудиопотока",
            "• Crash after prolonged gameplay due to memory accumulation\n• Audio stream desync",
            "✓ Память: 6GB DRAM (устраняет вылет через время)\n✓ Асинхронные шейдеры: Отключено\n✓ Быстрая память: Включено",
            "✓ Memory Layout: 6GB DRAM (Prevents crash over time)\n✓ Asynchronous Shaders: Disabled\n✓ Fastmem: Enabled",
            mapOf(
                "Core\\memory_layout_mode" to "1",
                "System\\memory_layout_mode" to "1",
                "Renderer\\use_asynchronous_shaders" to "false",
                "Cpu\\cpuopt_fastmem" to "true"
            )
        ),
        GameFixProfile(
            0x0100B0E020356000L,
            "The Legend of Heroes: Trails in the Sky",
            "• Стробоскопическое мерцание экрана и окружения\n• Пропадание текстур персонажей и задних планов\n• Артефакты сжатия ASTC",
            "• Screen and environment strobing flicker\n• Missing character and background textures\n• ASTC compression artifacts",
            "✓ Сжатие ASTC: Отключено (устраняет пропадание текстур)\n✓ Асинхронные шейдеры: Отключено (ликвидация мерцания)\n✓ Реактивная очистка: Отключено\n✓ Память: 6GB DRAM",
            "✓ ASTC Recompression: Uncompressed (Fixes missing textures)\n✓ Asynchronous Shaders: Disabled (Fixes flickering)\n✓ Reactive Flushing: Disabled\n✓ Memory Layout: 6GB DRAM",
            mapOf(
                "Renderer\\astc_recompression" to "0",
                "Renderer\\use_asynchronous_shaders" to "false",
                "Renderer\\use_reactive_flushing" to "false",
                "Core\\memory_layout_mode" to "1",
                "System\\memory_layout_mode" to "1",
                "Cpu\\cpuopt_fastmem" to "true"
            )
        ),
        GameFixProfile(
            0x01000B0012E4E000L,
            "Cronos: Before the Ashes",
            "• Некорректное темное изображение и сбои освещения UE4\n• Артефакты постобработки и размытия",
            "• Incorrect dark image and UE4 lighting corruption\n• Post-processing and motion blur artifacts",
            "✓ Реактивная очистка: Отключено (исправление освещения UE4)\n✓ Точность GPU: Высокая (FP16 точность шейдеров)\n✓ Сжатие ASTC: Отключено\n✓ Barrier Feedback Loops: Включено",
            "✓ Reactive Flushing: Disabled (Fixes UE4 lighting)\n✓ GPU Accuracy: High (FP16 shader precision)\n✓ ASTC Recompression: Uncompressed\n✓ Barrier Feedback Loops: Enabled",
            mapOf(
                "Renderer\\use_reactive_flushing" to "false",
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\astc_recompression" to "0",
                "Renderer\\barrier_feedback_loops" to "true"
            )
        ),
        GameFixProfile(
            0x010055700C30A000L,
            "The Outer Worlds",
            "• Некорректное изображение и черные ореолы вокруг объектов\n• Сбои TAA и темные артефакты геометрии UE4",
            "• Corrupted rendering and black halos around objects\n• TAA glitches and dark UE4 geometry artifacts",
            "✓ Реактивная очистка: Отключено (исправление черных ореолов)\n✓ Точность GPU: Высокая\n✓ Сжатие ASTC: Отключено\n✓ Barrier Feedback Loops: Включено\n✓ Память: 8GB DRAM",
            "✓ Reactive Flushing: Disabled (Fixes black halos)\n✓ GPU Accuracy: High\n✓ ASTC Recompression: Uncompressed\n✓ Barrier Feedback Loops: Enabled\n✓ Memory Layout: 8GB DRAM",
            mapOf(
                "Renderer\\use_reactive_flushing" to "false",
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\astc_recompression" to "0",
                "Renderer\\barrier_feedback_loops" to "true",
                "Core\\memory_layout_mode" to "2",
                "System\\memory_layout_mode" to "2"
            )
        ),
        GameFixProfile(
            0x01000A10041EA000L,
            "The Elder Scrolls V: Skyrim",
            "• Некорректное изображение водной глади и искажения отражений\n• Артефакты сжатия снега и текстур ландшафта",
            "• Incorrect water rendering and reflection distortion\n• Snow and terrain texture compression artifacts",
            "✓ Сжатие ASTC: Отключено (четкие текстуры снега и гор)\n✓ Реактивная очистка: Отключено (исправление отражений воды)\n✓ Точность GPU: Высокая",
            "✓ ASTC Recompression: Uncompressed (Crisp snow and terrain)\n✓ Reactive Flushing: Disabled (Fixes water reflections)\n✓ GPU Accuracy: High",
            mapOf(
                "Renderer\\astc_recompression" to "0",
                "Renderer\\use_reactive_flushing" to "false",
                "Renderer\\gpu_accuracy" to "1"
            )
        ),
        GameFixProfile(
            0x0100BCB0176D0000L,
            "Hogwarts Legacy",
            "• Некорректное изображение, сбои освещения и геометрии\n• Вылет при переходе между локациями Хогвартса из-за нехватки памяти",
            "• Corrupted rendering, lighting and geometry glitches\n• Out-of-memory crash during Hogwarts area transitions",
            "✓ Память: 8GB DRAM (критично для стабильности)\n✓ Реактивная очистка: Отключено (исправление освещения замка)\n✓ Barrier Feedback Loops: Включено\n✓ Сжатие ASTC: Отключено",
            "✓ Memory Layout: 8GB DRAM (Critical for stability)\n✓ Reactive Flushing: Disabled (Fixes castle lighting)\n✓ Barrier Feedback Loops: Enabled\n✓ ASTC Recompression: Uncompressed",
            mapOf(
                "Core\\memory_layout_mode" to "2",
                "System\\memory_layout_mode" to "2",
                "Renderer\\use_reactive_flushing" to "false",
                "Renderer\\barrier_feedback_loops" to "true",
                "Renderer\\astc_recompression" to "0"
            )
        ),

        GameFixProfile(
            0x01007EF00011E000L,
            "The Legend of Zelda: Breath of the Wild",
            "• Черный силуэт Линка из-за рассинхрона буфера освещения и трафарета\n• Белые вспышки и мерцание освещения/погоды\n• Пропадание текстур скал и земли при нехватке памяти\n• Бирюзовая сетка и артефакты Z-буфера в Святилищах",
            "• Link black silhouette caused by unsynced lighting and stencil buffers\n• White screen flashes and lighting flicker\n• Ground and terrain textures disappearing due to memory pressure\n• Shrine depth bias / cyan grid artifacts",
            "✓ Точность GPU: Высокая (исправление силуэта Линка)\n✓ Реактивная очистка: Отключено (устранение белой воды)\n✓ Сжатие ASTC: Среднее (BC3, устранение вытеснения текстур)\n✓ Быстрое время GPU: Включено (стабильные 30-32 FPS)\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено\n✓ Память: 6 ГБ DRAM (оптимально для текстур и стабильности)",
            "✓ GPU Accuracy: High (Fixes Link black silhouette)\n✓ Reactive Flushing: Disabled (Fixes white water)\n✓ ASTC Recompression: Medium (BC3, prevents texture dropping)\n✓ Fast GPU Time: Enabled (Stable 30-32 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Memory Layout: 6GB DRAM (Optimal stability)",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_reactive_flushing" to "false",
                "Renderer\\use_fast_gpu_time" to "true",
                "Renderer\\astc_recompression" to "2",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "System\\memory_layout_mode" to "1"
            )
        ),
        GameFixProfile(
            0x0100F2C0115B6000L,
            "The Legend of Zelda: Tears of the Kingdom",
            "• Черный силуэт персонажей и тени в Кавернах\n• Бирюзовая сетка и артефакты Z-буфера на водных поверхностях\n• Утечки VRAM в конструкторе Ультраруки",
            "• Character silhouette and shadow artifacts in Depths\n• Water surface and depth bias cyan grid artifacts\n• Ultrahand VRAM pressure",
            "✓ Точность GPU: Высокая (исправление теней и освещения)\n✓ Реактивная очистка: Отключено\n✓ Сжатие ASTC: Отключено\n✓ Быстрое время GPU: Отключено\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено\n✓ Память: 8GB DRAM",
            "✓ GPU Accuracy: High (Fixes character shadows and lighting)\n✓ Reactive Flushing: Disabled\n✓ ASTC Recompression: Uncompressed\n✓ Fast GPU Time: Disabled\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Memory Layout: 8GB DRAM",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_reactive_flushing" to "false",
                "Renderer\\use_fast_gpu_time" to "false",
                "Renderer\\astc_recompression" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "System\\memory_layout_mode" to "2"
            )
        ),
        GameFixProfile(
            0x01004D701742A000L,
            "Paper Mario: The Thousand-Year Door",
            "• Черный экран на катсценах в прологе\n• Сбои 2D-шрифтов диалогов и мерцание текстур",
            "• Black screen during prologue cutscenes\n• Corrupted battle text boxes and flickering textures",
            "✓ Точность GPU: Высокая\n✓ Сжатие ASTC: Отключено\n✓ Реактивная очистка: Включено",
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
            "• Вылет из-за нехватки памяти при загрузке замка Хогвартс\n• Высокое потребление ОЗУ (>8.5 ГБ) на мобильных чипах",
            "• Out of memory (OOM) crash when loading Hogwarts Castle\n• High RAM consumption (>8.5 GB) on mobile SoCs",
            "✓ Разрешение: Handheld 0.75X + FSR 80%\n✓ Сжатие текстур ASTC: Отключено\n✓ Режим памяти: 8GB DRAM",
            "✓ Resolution: Handheld 0.75X + FSR 80%\n✓ ASTC Recompression: BC1 (lowers RAM to 4.8 GB)\n✓ Memory Layout: 8GB DRAM",
            mapOf(
                "Renderer\\astc_recompression" to "0",
                "Renderer\\resolution_setup" to "1",
                "Renderer\\fsr_sharpening_slider" to "80",
                "System\\memory_layout_mode" to "2"
            )
        ),
        GameFixProfile(
            0x0100D1801648E000L,
            "Hogwarts Legacy",
            "• Вылет из-за нехватки памяти при загрузке замка Хогвартс\n• Высокое потребление ОЗУ (>8.5 ГБ) на мобильных чипах",
            "• Out of memory (OOM) crash when loading Hogwarts Castle\n• High RAM consumption (>8.5 GB) on mobile SoCs",
            "✓ Разрешение: Handheld 0.75X + FSR 80%\n✓ Сжатие текстур ASTC: Отключено\n✓ Режим памяти: 8GB DRAM",
            "✓ Resolution: Handheld 0.75X + FSR 80%\n✓ ASTC Recompression: BC1 (lowers RAM to 4.8 GB)\n✓ Memory Layout: 8GB DRAM",
            mapOf(
                "Renderer\\astc_recompression" to "0",
                "Renderer\\resolution_setup" to "1",
                "Renderer\\fsr_sharpening_slider" to "80",
                "System\\memory_layout_mode" to "2"
            )
        ),
        GameFixProfile(
            0x01002A801458A000L,
            "Diablo II: Resurrected",
            "• Вылет при продолжении игры / загрузке персонажа (OOM)\n• Зависание при опросе серверов Battle.net\n• Мерцание персонажа на экране выбора героя и графические артефакты",
            "• Character load / continue game crash (OOM)\n• Battle.net server handshake hang\n• Character flickering on selection screen and graphical artifacts",
            "✓ Память: 8GB DRAM (критично для загрузки персонажа!)\n✓ Режим полёта: Включено (пропуск Battle.net)\n✓ Быстрое время GPU: Отключено (стабильная загрузка!)\n✓ Точность GPU: Высокая\n✓ Сжатие ASTC: Отключено (устраняет полосы и квадраты)\n✓ Динамическое состояние: Отключено (исправляет артефакты декалей)\n✓ Обратная связь барьеров: Включено\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Отключено (устраняет мерцание)",
            "✓ Memory Layout: 8GB DRAM (Critical for character loading!)\n✓ Airplane Mode: Enabled (Bypasses Battle.net)\n✓ Fast GPU Time: Disabled (Stable loading!)\n✓ GPU Accuracy: High\n✓ ASTC Recompression: Uncompressed (Fixes decals and square artifacts)\n✓ Dynamic State: Disabled\n✓ Barrier Feedback Loops: Enabled\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Disabled (Fixes flickering)",
            mapOf(
                "Core\\memory_layout_mode" to "2",
                "System\\memory_layout_mode" to "2",
                "System\\airplane_mode" to "true",
                "Renderer\\use_fast_gpu_time" to "false",
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\astc_recompression" to "0",
                "Renderer\\dyna_state" to "0",
                "Renderer\\barrier_feedback_loops" to "true",
                "Renderer\\use_reactive_flushing" to "false",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "false",
                "Cpu\\cpu_accuracy" to "1"
            )
        ),
        GameFixProfile(
            0x0100916014D8C000L,
            "Diablo II: Resurrected",
            "• Вылет при продолжении игры / загрузке персонажа (OOM)\n• Зависание при опросе серверов Battle.net\n• Мерцание персонажа на экране выбора героя и графические артефакты",
            "• Character load / continue game crash (OOM)\n• Battle.net server handshake hang\n• Character flickering on selection screen and graphical artifacts",
            "✓ Память: 8GB DRAM (критично для загрузки персонажа!)\n✓ Режим полёта: Включено (пропуск Battle.net)\n✓ Быстрое время GPU: Отключено (стабильная загрузка!)\n✓ Точность GPU: Высокая\n✓ Сжатие ASTC: Отключено (устраняет полосы и квадраты)\n✓ Динамическое состояние: Отключено (исправляет артефакты декалей)\n✓ Обратная связь барьеров: Включено\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Отключено (устраняет мерцание)",
            "✓ Memory Layout: 8GB DRAM (Critical for character loading!)\n✓ Airplane Mode: Enabled (Bypasses Battle.net)\n✓ Fast GPU Time: Disabled (Stable loading!)\n✓ GPU Accuracy: High\n✓ ASTC Recompression: Uncompressed (Fixes decals and square artifacts)\n✓ Dynamic State: Disabled\n✓ Barrier Feedback Loops: Enabled\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Disabled (Fixes flickering)",
            mapOf(
                "Core\\memory_layout_mode" to "2",
                "System\\memory_layout_mode" to "2",
                "System\\airplane_mode" to "true",
                "Renderer\\use_fast_gpu_time" to "false",
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\astc_recompression" to "0",
                "Renderer\\dyna_state" to "0",
                "Renderer\\barrier_feedback_loops" to "true",
                "Renderer\\use_reactive_flushing" to "false",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "false",
                "Cpu\\cpu_accuracy" to "1"
            )
        ),
        GameFixProfile(
            0x0100726014352000L,
            "Diablo II: Resurrected",
            "• Вылет при продолжении игры / загрузке персонажа (OOM)\n• Зависание при опросе серверов Battle.net\n• Мерцание персонажа на экране выбора героя и графические артефакты",
            "• Character load / continue game crash (OOM)\n• Battle.net server handshake hang\n• Character flickering on selection screen and graphical artifacts",
            "✓ Память: 8GB DRAM (критично для загрузки персонажа!)\n✓ Режим полёта: Включено (пропуск Battle.net)\n✓ Быстрое время GPU: Отключено\n✓ Точность GPU: Высокая\n✓ Сжатие ASTC: Отключено (устраняет полосы и квадраты)\n✓ Динамическое состояние: Отключено (исправляет артефакты декалей)\n✓ Обратная связь барьеров: Включено\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Отключено (устраняет мерцание)",
            "✓ Memory Layout: 8GB DRAM (Critical for character loading!)\n✓ Airplane Mode: Enabled (Bypasses Battle.net)\n✓ Fast GPU Time: Disabled\n✓ GPU Accuracy: High\n✓ ASTC Recompression: Uncompressed (Fixes decals and square artifacts)\n✓ Dynamic State: Disabled\n✓ Barrier Feedback Loops: Enabled\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Disabled (Fixes flickering)",
            mapOf(
                "Core\\memory_layout_mode" to "2",
                "System\\memory_layout_mode" to "2",
                "System\\airplane_mode" to "true",
                "Renderer\\use_fast_gpu_time" to "false",
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\astc_recompression" to "0",
                "Renderer\\dyna_state" to "0",
                "Renderer\\barrier_feedback_loops" to "true",
                "Renderer\\use_reactive_flushing" to "false",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "false",
                "Cpu\\cpu_accuracy" to "1"
            )
        ),
        GameFixProfile(
            0x0100C6000EEA8000L,
            "Warhammer 40,000: Mechanicus",
            "• Невозможно сохранить прогресс игры (ошибка сохранения)",
            "• Unable to save game progress (infinite save loop)",
            "✓ Поддержка RenameDirectory в STORM SWITCH 4.6.0+\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ RenameDirectory support in STORM SWITCH 4.6.0+\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100923008C54000L,
            "LEGO Star Wars: The Skywalker Saga",
            "• Бесконечная загрузка на заставке\n• Зависание асинхронного таймера GPU при обращении к ресурсам\n• Блокировка сетевой телеметрии WB на Android",
            "• Infinite loading screen (TT Games loading indicator loop)\n• GPU async timer deadlock during asset initialization\n• WB telemetry network deadlock on Android",
            "✓ Режим полёта: Включено (критично для запуска на Android!)\n✓ Быстрое время GPU: Отключено (устраняет вечную загрузку!)\n✓ Динамическое состояние: Базовое\n✓ Точность GPU: Высокая\n✓ Точность CPU: Точная\n✓ Реактивная очистка: Отключено\n✓ Память: 8GB DRAM",
            "✓ Airplane Mode: Enabled (Critical for Android launch!)\n✓ Fast GPU Time: Disabled (Fixes infinite loading!)\n✓ Dynamic State: Basic (EDS1)\n✓ GPU Accuracy: High\n✓ CPU Accuracy: Accurate\n✓ Reactive Flushing: Disabled\n✓ Memory Layout: 8GB DRAM",
            mapOf(
                "System\\airplane_mode" to "true",
                "System\\memory_layout_mode" to "1",
                "Renderer\\use_fast_gpu_time" to "false",
                "Renderer\\dyna_state" to "0",
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_reactive_flushing" to "false",
                "Renderer\\astc_recompression" to "0",
                "Renderer\\use_asynchronous_shaders" to "false",
                "Cpu\\cpu_accuracy" to "0"
            )
        ),
        GameFixProfile(
            0x0100E26017E5E000L,
            "Red Dead Redemption",
            "• Хрипы и рассинхронизация звука в катсценах\n• Микрофризы физических потоков RAGE Engine",
            "• Audio crackling and desync in cutscenes\n• Micro-stutters in RAGE Engine physics threads",
            "✓ Точность CPU: Точная\n✓ Аудио-буфер Cubeb: 80 ms\n✓ Синхронизация памяти: Отключено",
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
            "✓ Расширенное динамическое состояние: Включено\n✓ Точность GPU: Высокая\n✓ Точность CPU: Точная",
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
            "✓ Контроль отсечения глубины: Включено\n✓ Точность GPU: Высокая",
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
            "✓ Эмуляция цвета BGR565: Включено\n✓ Коррекция эффектов свечения: Включено",
            "✓ Emulate BGR565: Enabled\n✓ Fix Bloom Effects: Enabled",
            mapOf(
                "Renderer\\emulate_bgr565" to "true",
                "Renderer\\fix_bloom_effects" to "true"
            )
        ),
        GameFixProfile(
            0x01006A800016E000L,
            "Super Smash Bros. Ultimate",
            "• Вылет на экране победы или в меню новостей",
            "• Crash on victory screen or news board (Web Applet)",
            "✓ Отключение веб-апплета: Включено\n✓ Mii Applet: LLE",
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
            "✓ Требуется Firmware 18.0.0+ и системные файлы Mii\n✓ Сжатие ASTC: Отключено",
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
            "✓ Точность GPU: Высокая\n✓ Анизотропная фильтрация: 16x\n✓ Декодирование ASTC на GPU: Включено",
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
            "✓ Разрешение: Handheld 0.75X + FSR 75%\n✓ Сжатие ASTC: Отключено\n✓ Ограничение VRAM: Conservative",
            "✓ Resolution: Handheld 0.75X + FSR 75%\n✓ ASTC Recompression: BC3\n✓ VRAM Usage: Conservative",
            mapOf(
                "Renderer\\astc_recompression" to "0",
                "Renderer\\resolution_setup" to "1",
                "Renderer\\vram_usage_mode" to "1"
            )
        ),
        GameFixProfile(
            0x0100B9F010DC4000L,
            "Doom Eternal",
            "• Вылет драйвера Vulkan при первом выстреле / спавне BFG",
            "• Vulkan device loss crash on weapon fire / BFG",
            "✓ Проверка границ глубины: Включено\n✓ Точность DMA: Safe",
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
            "• Вылет по нехватке памяти при погонях на Бэтмобиле",
            "• OOM crash during Batmobile chase sequences",
            "✓ Разрешение: Handheld 0.75X + FSR 80%\n✓ Сжатие ASTC: Отключено\n✓ Память: 8GB DRAM",
            "✓ Resolution: Handheld 0.75X + FSR 80%\n✓ ASTC Recompression: BC1\n✓ Memory Layout: 8GB DRAM",
            mapOf(
                "Renderer\\astc_recompression" to "0",
                "Renderer\\resolution_setup" to "1",
                "System\\memory_layout_mode" to "2"
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
                "Renderer\\fsr_sharpening_slider" to "85",
                "Renderer\\astc_recompression" to "0",
                "Renderer\\use_reactive_flushing" to "false",
                "Core\\memory_layout_mode" to "2",
                "System\\memory_layout_mode" to "2",
                "Renderer\\gpu_accuracy" to "1"
            )
        ),
        GameFixProfile(
            0x0100760012E4A000L,
            "Mario + Rabbids Sparks of Hope",
            "• Вылет движка Snowdrop при переходе в тактический бой",
            "• Snowdrop engine crash on tactical combat transition",
            "✓ Точность DMA: Safe\n✓ Точность GPU: Высокая\n✓ Barrier Feedback Loops: Включено",
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
            "✓ Разрешение: Handheld 0.75X + FSR 80%\n✓ Сжатие ASTC: Отключено\n✓ Память: 8GB DRAM",
            "✓ Resolution: Handheld 0.75X + FSR 80%\n✓ ASTC Recompression: BC1\n✓ Memory Layout: 8GB DRAM",
            mapOf(
                "Renderer\\astc_recompression" to "0",
                "Renderer\\resolution_setup" to "1",
                "System\\memory_layout_mode" to "2"
            )
        ),
        GameFixProfile(
            0x01004AB00A266000L,
            "Dark Souls: Remastered",
            "• Просадки кадровой частоты у костров и зацикливание звука баффов",
            "• Bonfire particle slowdown and weapon buff sound loop",
            "✓ Точность CPU: Точная\n✓ Точность GPU: Высокая\n✓ Аудио-движок: Cubeb",
            "✓ CPU Accuracy: Accurate\n✓ GPU Accuracy: High\n✓ Audio Engine: Cubeb",
            mapOf(
                "Cpu\\cpu_accuracy" to "0",
                "Renderer\\gpu_accuracy" to "1"
            )
        ),
        GameFixProfile(
            0x0100650017170000L,
            "Animal Well",
            "• Просадки кадровой частоты и пропадание звуковых дорожек",
            "• Frame drops and missing audio tracks on startup",
            "✓ Аудио-движок: SDL2 / Cubeb\n✓ Быстрая память (Fastmem): Безопасный режим\n✓ Асинхронные шейдеры: Включено",
            "✓ Audio Engine: SDL2 / Cubeb\n✓ Fastmem: Safe Mode\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Renderer\\use_asynchronous_shaders" to "true",
                "Cpu\\cpuopt_fastmem" to "false"
            )
        ),
        GameFixProfile(
            0x010020D01AD24000L,
            "Animal Well",
            "• Просадки кадровой частоты и пропадание звуковых дорожек",
            "• Frame drops and missing audio tracks on startup",
            "✓ Аудио-движок: SDL2 / Cubeb\n✓ Быстрая память (Fastmem): Безопасный режим\n✓ Асинхронные шейдеры: Включено",
            "✓ Audio Engine: SDL2 / Cubeb\n✓ Fastmem: Safe Mode\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Renderer\\use_asynchronous_shaders" to "true",
                "Cpu\\cpuopt_fastmem" to "false"
            )
        ),
        GameFixProfile(
            0x0100C9E01B854000L,
            "Animal Well",
            "• Просадки кадровой частоты и пропадание звуковых дорожек",
            "• Frame drops and missing audio tracks on startup",
            "✓ Аудио-движок: SDL2 / Cubeb\n✓ Быстрая память (Fastmem): Безопасный режим\n✓ Асинхронные шейдеры: Включено",
            "✓ Audio Engine: SDL2 / Cubeb\n✓ Fastmem: Safe Mode\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Renderer\\use_asynchronous_shaders" to "true",
                "Cpu\\cpuopt_fastmem" to "false"
            )
        ),
        GameFixProfile(
            0x010028600EBDA000L,
            "Super Mario 3D World + Bowser's Fury",
            "• Оптимизация физики и анимаций персонажей\n• Микрофризы при компиляции шейдеров",
            "• Optimization for physics and character animations\n• Shader compilation stutter on character animations",
            "✓ Конфигурация памяти: 4 ГБ DRAM\n✓ Точность GPU: Обычная (плавные 60 кадров/с)\n✓ Быстрое время GPU: Включено\n✓ Быстрая память Fastmem: Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ Memory Layout: 4GB DRAM\n✓ GPU Accuracy: Normal (Smooth 60 FPS)\n✓ Fast GPU Time: Enabled\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "0",
                "Renderer\\gpu_accuracy" to "0",
                "Renderer\\use_fast_gpu_time" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Cpu\\cpuopt_fastmem" to "true"
            )
        ),
        GameFixProfile(
            0x010015100B5B4000L,
            "Super Mario Bros. Wonder",
            "• Вылет при переходе уровней и активации Чудо-цветка (нехватка стандартной памяти 4GB)\n• Микрофризы при компиляции анимаций персонажей",
            "• Crash on stage transitions and Wonder Flower effects (4GB memory limit)\n• Shader compilation stutter on character animations",
            "✓ Конфигурация памяти: 6 ГБ DRAM (предотвращение переполнения памяти)\n✓ Точность GPU: Обычная (Normal 60 FPS)\n✓ Быстрое время GPU: Включено\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ Memory Layout: 6GB DRAM (Prevents memory overflow)\n✓ GPU Accuracy: Normal (Smooth 60 FPS)\n✓ Fast GPU Time: Enabled\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "1",
                "Renderer\\gpu_accuracy" to "0",
                "Renderer\\use_fast_gpu_time" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Cpu\\cpuopt_fastmem" to "true"
            )
        ),

        GameFixProfile(
            0x0100C88011246000L,
            "Disco Elysium: The Final Cut",
            "• Утечка памяти и вылеты при смене локаций\n• Размытие и мерцание текста диалогов TextMeshPro\n• Цветовые артефакты акварельных портретов и фонов",
            "• Out of memory (OOM) crash on zone transitions\n• TextMeshPro dialogue font blur and jitter\n• Color compression artifacts on painted portraits and backdrops",
            "✓ Память: 6GB DRAM (предотвращение вылетов Unity)\n✓ Сжатие ASTC: Отключено\n✓ Динамическое состояние: Базовое\n✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Реактивная очистка: Включено",
            "✓ Memory Layout: 6GB DRAM (Prevents Unity OOM crashes)\n✓ ASTC Recompression: Uncompressed (Max art fidelity)\n✓ Dynamic State: EDS1\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Reactive Flushing: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "1",
                "Renderer\\astc_recompression" to "0",
                "Renderer\\dyna_state" to "0",
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_reactive_flushing" to "true",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x01008C300F7F0000L,
            "Disco Elysium: The Final Cut (WW)",
            "• Утечка памяти и вылеты при смене локаций\n• Размытие и мерцание текста диалогов TextMeshPro\n• Цветовые артефакты акварельных портретов и фонов",
            "• Out of memory (OOM) crash on zone transitions\n• TextMeshPro dialogue font blur and jitter\n• Color compression artifacts on painted portraits and backdrops",
            "✓ Память: 6GB DRAM (предотвращение вылетов Unity)\n✓ Сжатие ASTC: Отключено\n✓ Динамическое состояние: Базовое\n✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Реактивная очистка: Включено",
            "✓ Memory Layout: 6GB DRAM (Prevents Unity OOM crashes)\n✓ ASTC Recompression: Uncompressed (Max art fidelity)\n✓ Dynamic State: EDS1\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Reactive Flushing: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "1",
                "Renderer\\astc_recompression" to "0",
                "Renderer\\dyna_state" to "0",
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_reactive_flushing" to "true",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100E26014466000L,
            "Disco Elysium: The Final Cut (Asia)",
            "• Утечка памяти и вылеты при смене локаций\n• Размытие и мерцание текста диалогов TextMeshPro\n• Цветовые артефакты акварельных портретов и фонов",
            "• Out of memory (OOM) crash on zone transitions\n• TextMeshPro dialogue font blur and jitter\n• Color compression artifacts on painted portraits and backdrops",
            "✓ Память: 6GB DRAM (предотвращение вылетов Unity)\n✓ Сжатие ASTC: Отключено\n✓ Динамическое состояние: Базовое\n✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Реактивная очистка: Включено",
            "✓ Memory Layout: 6GB DRAM (Prevents Unity OOM crashes)\n✓ ASTC Recompression: Uncompressed (Max art fidelity)\n✓ Dynamic State: EDS1\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Reactive Flushing: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "1",
                "Renderer\\astc_recompression" to "0",
                "Renderer\\dyna_state" to "0",
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_reactive_flushing" to "true",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x01003AE017DB0000L,
            "Batman: Arkham City",
            "• Просадки FPS при планировании над городом и микрофризы",
            "• FPS drops and micro-stutters while gliding across Arkham City",
            "✓ Точность CPU: Точная\n✓ Память: 6GB DRAM\n✓ Динамическое состояние: Базовое",
            "✓ CPU Accuracy: Accurate\n✓ Memory Layout: 6GB DRAM\n✓ Dynamic State: EDS1",
            mapOf(
                "Cpu\\cpu_accuracy" to "0",
                "System\\memory_layout_mode" to "1",
                "Renderer\\dyna_state" to "0"
            )
        ),
        GameFixProfile(
            0x0100FF500E34A000L,
            "Xenoblade Chronicles: Definitive Edition",
            "• Мерцание текстур открытого мира и артефакты облаков",
            "• Open world texture shimmering and cloud rendering artifacts",
            "✓ Точность GPU: Высокая\n✓ Динамическое состояние: Расширенное\n✓ Сжатие ASTC: Отключено\n✓ Память: 6GB DRAM",
            "✓ GPU Accuracy: High\n✓ Dynamic State: EDS2\n✓ ASTC Recompression: BC3\n✓ Memory Layout: 6GB DRAM",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\dyna_state" to "2",
                "Renderer\\astc_recompression" to "0",
                "System\\memory_layout_mode" to "1",
                "Renderer\\use_fast_gpu_time" to "true"
            )
        ),
        GameFixProfile(
            0x0100E95004038000L,
            "Xenoblade Chronicles 2",
            "• Просадки кадровой частоты в густонаселенных локациях (Гула, Мор Ардайн)",
            "• Heavy frame drops in dense titan areas (Gormott, Mor Ardain)",
            "✓ Динамическое состояние: Расширенное\n✓ Сжатие ASTC: Отключено\n✓ Память: 6GB DRAM\n✓ Быстрое время GPU: Включено",
            "✓ Dynamic State: EDS2\n✓ ASTC Recompression: BC3\n✓ Memory Layout: 6GB DRAM\n✓ Fast GPU Time: Enabled",
            mapOf(
                "Renderer\\dyna_state" to "2",
                "Renderer\\astc_recompression" to "0",
                "System\\memory_layout_mode" to "1",
                "Renderer\\use_fast_gpu_time" to "true"
            )
        ),
        GameFixProfile(
            0x010074F013262000L,
            "Xenoblade Chronicles 3",
            "• Утечки VRAM и микростаттеры в битвах с 7 персонажами",
            "• VRAM leaks and micro-stutters during full 7-character battle parties",
            "✓ Сжатие ASTC: Отключено\n✓ Память: 6GB DRAM\n✓ Динамическое состояние: Расширенное\n✓ Точность GPU: Высокая",
            "✓ ASTC Recompression: BC3\n✓ Memory Layout: 6GB DRAM\n✓ Dynamic State: EDS2\n✓ GPU Accuracy: High",
            mapOf(
                "Renderer\\astc_recompression" to "0",
                "System\\memory_layout_mode" to "1",
                "Renderer\\dyna_state" to "2",
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_fast_gpu_time" to "true"
            )
        ),
        GameFixProfile(
            0x0100E67012924000L,
            "The Witcher 3: Wild Hunt - Complete Edition",
            "• Вылеты по памяти и заикания физики в Новиграде и Туссенте",
            "• OOM crashes and physics stutter in Novigrad and Toussaint",
            "✓ Память: 6GB DRAM\n✓ Точность GPU: Высокая\n✓ Сжатие ASTC: Отключено\n✓ Быстрое время GPU: Включено\n✓ Реактивная очистка: Включено",
            "✓ Memory Layout: 6GB DRAM\n✓ GPU Accuracy: High\n✓ ASTC Recompression: Uncompressed\n✓ Fast GPU Time: Enabled\n✓ Reactive Flushing: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "1",
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\astc_recompression" to "0",
                "Renderer\\use_fast_gpu_time" to "true",
                "Renderer\\use_reactive_flushing" to "true"
            )
        ),
        GameFixProfile(
            0x010074600EE26000L,
            "Need for Speed: Hot Pursuit Remastered",
            "• Зависание и бесконечная загрузка при авторизации на серверах EA Autolog\n• Сетевые таймауты при запуске",
            "• Infinite loading hang during EA Autolog server authorization\n• Network connection handshake timeout",
            "✓ Режим полета (В самолете): Включено (пропуск онлайн-проверки)\n✓ Асинхронные шейдеры: Включено\n✓ Память: 8GB DRAM\n✓ Сжатие ASTC: Отключено",
            "✓ Airplane Mode: Enabled (Bypasses EA Autolog offline)\n✓ Asynchronous Shaders: Enabled\n✓ Memory Layout: 8GB DRAM\n✓ ASTC Recompression: Uncompressed",
            mapOf(
                "System\\airplane_mode" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "System\\memory_layout_mode" to "2",
                "Renderer\\astc_recompression" to "0"
            )
        ),
        GameFixProfile(
            0x010067300059A000L,
            "Mario + Rabbids: Kingdom Battle",
            "• Сильный пересвет и ослепляющий блум\n• Черные тени и артефакты освещения\n• Мерцание текстур персонажей",
            "• Severe overexposure and blinding bloom glow\n• Black shadows and lighting pass corruption\n• Character model flickering",
            "✓ Точность GPU: Высокая (устранение пересвета и черных теней)\n✓ Обратные циклы барьеров: Включено\n✓ Реактивная очистка: Включено\n✓ Сжатие ASTC: Отключено\n✓ Асинхронные шейдеры: Включено",
            "✓ GPU Accuracy: High (Fixes bloom & black shadows)\n✓ Barrier Feedback Loops: Enabled\n✓ Reactive Flushing: Enabled\n✓ ASTC Recompression: Uncompressed\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\barrier_feedback_loops" to "true",
                "Renderer\\use_reactive_flushing" to "true",
                "Renderer\\astc_recompression" to "0",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x01005CA00F966000L,
            "Mario + Rabbids: Sparks of Hope",
            "• Сбои динамического освещения на планетах\n• Черные артефакты в катсценах и битвах\n• Просадки кадров",
            "• Planet lighting pass corruption\n• Black shadow artifacts during cutscenes and tactical battles\n• Frame drops",
            "✓ Точность GPU: Высокая\n✓ Обратные циклы барьеров: Включено\n✓ Реактивная очистка: Включено\n✓ Сжатие ASTC: Отключено\n✓ Память: 8GB DRAM",
            "✓ GPU Accuracy: High\n✓ Barrier Feedback Loops: Enabled\n✓ Reactive Flushing: Enabled\n✓ ASTC Recompression: Uncompressed\n✓ Memory Layout: 8GB DRAM",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\barrier_feedback_loops" to "true",
                "Renderer\\use_reactive_flushing" to "true",
                "Renderer\\astc_recompression" to "0",
                "System\\memory_layout_mode" to "2"
            )
        ),
        GameFixProfile(
            0x01005CF01E784000L,
            "Teenage Mutant Ninja Turtles: Splintered Fate",
            "• Зависание на титульном экране из-за сетевого ожидания NIM и SSL сервисов\n• Микрофризы при спавне врагов",
            "• Title screen freeze caused by NIM and SSL network connection waiting\n• Stutter during enemy combat waves",
            "✓ Режим полета (В самолете): Включено (пропуск сетевого ожидания)\n✓ Асинхронные шейдеры: Включено\n✓ Быстрая память: Включено\n✓ Сжатие ASTC: Отключено",
            "✓ Airplane Mode: Enabled (Skips online network handshake)\n✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled\n✓ ASTC Recompression: Uncompressed",
            mapOf(
                "System\\airplane_mode" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\astc_recompression" to "0"
            )
        ),
        GameFixProfile(
            0x0100000000010000L,
            "Super Mario Odyssey",
            "• Мерцание 2D-рисунков на стенах\n• Артефакты дыма и тумана в Песчаном царстве",
            "• Flickering 2D wall drawings\n• Sand Kingdom smoke and fog rendering artifacts",
            "✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Анизотропная фильтрация: 16x\n✓ Сжатие ASTC: Отключено",
            "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ Anisotropic Filtering: 16x\n✓ ASTC Recompression: Uncompressed",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_reactive_flushing" to "true",
                "Renderer\\max_anisotropy" to "5",
                "Renderer\\astc_recompression" to "0"
            )
        ),
        GameFixProfile(
            0x0100121014688000L,
            "Metroid Prime Remastered",
            "• Статтеры при открытии дверей между отсеками\n• Мерцание эффектов визора",
            "• Door transition compilation stutter\n• Visor UI and particle flicker",
            "✓ Асинхронные шейдеры: Включено\n✓ Быстрая память: Включено\n✓ Сжатие ASTC: Отключено",
            "✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled\n✓ ASTC Recompression: Uncompressed",
            mapOf(
                "Renderer\\use_asynchronous_shaders" to "true",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\astc_recompression" to "0"
            )
        ),
        GameFixProfile(
            0x0100DCA0064A6000L,
            "Luigi's Mansion 3",
            "• Артефакты динамического света фонарика и теней\n• Падение FPS в комнатах с призраками",
            "• Dynamic flashlight beam artifacts\n• FPS drops in ghost-heavy rooms",
            "✓ Точность GPU: Высокая\n✓ Обратные циклы барьеров: Включено\n✓ Реактивная очистка: Включено\n✓ Память: 8GB DRAM",
            "✓ GPU Accuracy: High\n✓ Barrier Feedback Loops: Enabled\n✓ Reactive Flushing: Enabled\n✓ Memory Layout: 8GB DRAM",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\barrier_feedback_loops" to "true",
                "Renderer\\use_reactive_flushing" to "true",
                "System\\memory_layout_mode" to "2"
            )
        ),
        GameFixProfile(
            0x01006F8002326000L,
            "Animal Crossing: New Horizons",
            "• Размытие травы и мелких объектов\n• Мерцание теней в вечернее время",
            "• Blurry grass and ground textures\n• Evening shadow flicker",
            "✓ Анизотропная фильтрация: 16x\n✓ Точность GPU: Высокая\n✓ Асинхронные шейдеры: Включено",
            "✓ Anisotropic Filtering: 16x\n✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Renderer\\max_anisotropy" to "5",
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x01000A10041EA000L,
            "Persona 5 Royal",
            "• Мерцание 2D UI портретов и шрифтов\n• Просадки FPS в людных районах Токио",
            "• 2D UI portrait flicker and font artifacts\n• Heavy crowds FPS drops in Shibuya and Shinjuku",
            "✓ Точность GPU: Высокая\n✓ Асинхронные шейдеры: Включено\n✓ Быстрая память: Включено",
            "✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Cpu\\cpuopt_fastmem" to "true"
            )
        ),
        GameFixProfile(
            0x01006A800016E000L,
            "Super Smash Bros. Ultimate",
            "• Рассинхрон звука при загрузке 8 бойцов\n• Микрофризы при активации спецэффектов",
            "• Audio desync with 8 active fighters\n• Visual effect stutter",
            "✓ Асинхронные шейдеры: Включено\n✓ Быстрая память: Включено",
            "✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled",
            mapOf(
                "Renderer\\use_asynchronous_shaders" to "true",
                "Cpu\\cpuopt_fastmem" to "true"
            )
        ),
        GameFixProfile(
            0x0100A38018D5A000L,
            "EA SPORTS FC 24",
            "• Зависание на заставке EA Connect\n• Утечки памяти в режиме карьеры",
            "• EA Connect splash freeze\n• Career mode memory leaks",
            "✓ Режим полета (В самолете): Включено (пропуск серверов EA)\n✓ Асинхронные шейдеры: Включено\n✓ Память: 8GB DRAM",
            "✓ Airplane Mode: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Memory Layout: 8GB DRAM",
            mapOf(
                "System\\airplane_mode" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "System\\memory_layout_mode" to "2"
            )
        ),
        GameFixProfile(
            0x01004A4010F22000L,
            "Bayonetta 3",
            "• Падение FPS при призыве демонов\n• Искажение прозрачных частиц взрывов",
            "• FPS drops during demon slave summoning\n• Explosion transparency distortion",
            "✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Сжатие ASTC: Отключено\n✓ Память: 8GB DRAM",
            "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ ASTC Recompression: Uncompressed\n✓ Memory Layout: 8GB DRAM",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_reactive_flushing" to "true",
                "Renderer\\astc_recompression" to "0",
                "System\\memory_layout_mode" to "2"
            )
        ),
        GameFixProfile(
            0x01004C90141A4000L,
            "Sonic Frontiers",
            "• Просадки FPS и размытие текстур травы в открытых зонах\n• Мерцание теней Cyberspace",
            "• Heavy open-zone frame drops and blurry grass textures\n• Cyberspace shadow flickering",
            "✓ Асинхронные шейдеры: Включено\n✓ Быстрая память: Включено\n✓ Анизотропная фильтрация: 16x\n✓ Память: 8GB DRAM",
            "✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled\n✓ Anisotropic Filtering: 16x\n✓ Memory Layout: 8GB DRAM",
            mapOf(
                "Renderer\\use_asynchronous_shaders" to "true",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\max_anisotropy" to "5",
                "System\\memory_layout_mode" to "2"
            )
        ),
        GameFixProfile(
            0x01001F5010DFA000L,
            "Pokémon Legends: Arceus",
            "• Микрофризы при спавне диких покемонов в небе/траве\n• Артефакты освещения в разломах",
            "• Wild pokemon spawn micro-stutters\n• Space-time distortion lighting artifacts",
            "✓ Точность GPU: Высокая\n✓ Асинхронные шейдеры: Включено\n✓ Реактивная очистка: Включено\n✓ Память: 8GB DRAM",
            "✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled\n✓ Reactive Flushing: Enabled\n✓ Memory Layout: 8GB DRAM",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\use_reactive_flushing" to "true",
                "System\\memory_layout_mode" to "2"
            )
        ),
        GameFixProfile(
            0x0100A3D008C5C000L,
            "Pokémon Scarlet / Violet",
            "• Утечки VRAM в городах и на водных просторах\n• Падение FPS и замедление анимаций",
            "• Severe VRAM leaks in towns and lakes\n• Frame drops and slowed NPC animations",
            "✓ Точность GPU: Высокая\n✓ Сжатие ASTC: Отключено\n✓ Реактивная очистка: Включено\n✓ Память: 8GB DRAM",
            "✓ GPU Accuracy: High\n✓ ASTC Recompression: Uncompressed\n✓ Reactive Flushing: Enabled\n✓ Memory Layout: 8GB DRAM",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\astc_recompression" to "0",
                "Renderer\\use_reactive_flushing" to "true",
                "System\\memory_layout_mode" to "2"
            )
        ),
        GameFixProfile(
            0x0100ABF008968000L,
            "Pokémon Sword / Shield",
            "• Заикания в Диких землях при подгрузке погодных условий\n• Мерцание спецэффектов Dynamax",
            "• Wild Area weather transition stutters\n• Dynamax battle effect flickering",
            "✓ Асинхронные шейдеры: Включено\n✓ Быстрая память: Включено\n✓ Точность GPU: Высокая",
            "✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled\n✓ GPU Accuracy: High",
            mapOf(
                "Renderer\\use_asynchronous_shaders" to "true",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\gpu_accuracy" to "1"
            )
        ),
        GameFixProfile(
            0x01004D300C5AE000L,
            "Kirby and the Forgotten Land",
            "• Зависание анимаций врагов на 30 FPS\n• Артефакты отражений луж",
            "• Distant enemy 30 FPS stutter\n• Puddle reflection distortion",
            "✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Анизотропная фильтрация: 16x",
            "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ Anisotropic Filtering: 16x",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_reactive_flushing" to "true",
                "Renderer\\max_anisotropy" to "5"
            )
        ),
        GameFixProfile(
            0x010055D009F78000L,
            "Fire Emblem: Three Houses",
            "• Просадки FPS при масштабных битвах батальонов\n• Мерцание 2D портретов и меню",
            "• Battalion battle animation FPS drops\n• 2D UI portrait flickering",
            "✓ Точность GPU: Высокая\n✓ Асинхронные шейдеры: Включено\n✓ Быстрая память: Включено",
            "✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Cpu\\cpuopt_fastmem" to "true"
            )
        ),
        GameFixProfile(
            0x0100A6301214E000L,
            "Fire Emblem Engage",
            "• Сбои шейдеров свечения колец Emblem\n• Микрофризы в Сомниэле",
            "• Emblem ring glow shader artifacts\n• Somniel hub micro-stutters",
            "✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Асинхронные шейдеры: Включено\n✓ Память: 8GB DRAM",
            "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Memory Layout: 8GB DRAM",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_reactive_flushing" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "System\\memory_layout_mode" to "2"
            )
        ),
        GameFixProfile(
            0x01007300020FA000L,
            "Astral Chain",
            "• Падение кадров при цепных комбо-атаках легионов\n• Размытие динамических неоновых вывесок",
            "• Legion chain attack FPS drops\n• Blurry dynamic neon bloom reflections",
            "✓ Точность GPU: Высокая\n✓ Асинхронные шейдеры: Включено\n✓ Быстрое время GPU: Включено\n✓ Память: 8GB DRAM",
            "✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled\n✓ Fast GPU Time: Enabled\n✓ Memory Layout: 8GB DRAM",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\use_fast_gpu_time" to "true",
                "System\\memory_layout_mode" to "2"
            )
        ),
        GameFixProfile(
            0x01006F801BC4C000L,
            "Shin Megami Tensei V: Vengeance",
            "• Нагрев и просадки кадров в песчаных бурях Даата\n• Черные тени демонов",
            "• Da'at sandstorm thermal throttle and FPS drops\n• Demon shadow corruption",
            "✓ Точность GPU: Высокая\n✓ Сжатие ASTC: Отключено\n✓ Реактивная очистка: Включено\n✓ Память: 8GB DRAM",
            "✓ GPU Accuracy: High\n✓ ASTC Recompression: Uncompressed\n✓ Reactive Flushing: Enabled\n✓ Memory Layout: 8GB DRAM",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\astc_recompression" to "0",
                "Renderer\\use_reactive_flushing" to "true",
                "System\\memory_layout_mode" to "2"
            )
        ),
        GameFixProfile(
            0x01006C300E9F0000L,
            "Dragon Quest XI S: Echoes of an Elusive Age",
            "• Микростаттеры при смене зон и переходе между 2D/3D режимами\n• Мерцание травы",
            "• Zone transition micro-stutters\n• Grass texture flickering",
            "✓ Асинхронные шейдеры: Включено\n✓ Быстрая память: Включено\n✓ Анизотропная фильтрация: 16x",
            "✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled\n✓ Anisotropic Filtering: 16x",
            mapOf(
                "Renderer\\use_asynchronous_shaders" to "true",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\max_anisotropy" to "5"
            )
        ),
        GameFixProfile(
            0x0100CC80140F8000L,
            "Triangle Strategy",
            "• Пересвет HD-2D эффектов глубины резкости\n• Мерцание теней на изометрических картах",
            "• HD-2D depth-of-field overexposure\n• Isometric grid shadow flickering",
            "✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_reactive_flushing" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100BB70144F8000L,
            "Prince of Persia: The Lost Crown",
            "• Статтеры при активации способностей управления временем\n• Искажение фоновых слоев",
            "• Time powers activation stutter\n• Background parallax distortion",
            "✓ Асинхронные шейдеры: Включено\n✓ Быстрая память: Включено\n✓ Сжатие ASTC: Отключено",
            "✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled\n✓ ASTC Recompression: Uncompressed",
            mapOf(
                "Renderer\\use_asynchronous_shaders" to "true",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\astc_recompression" to "0"
            )
        ),
        GameFixProfile(
            0x01007820195A6000L,
            "Red Dead Redemption",
            "• Сбои освещения на закате и рассвете\n• Просадки FPS в городах Блэкуотер и Армадилло",
            "• Sunrise/sunset volumetric lighting corruption\n• Blackwater town FPS drops",
            "✓ Точность GPU: Высокая\n✓ Обратные циклы барьеров: Включено\n✓ Асинхронные шейдеры: Включено\n✓ Память: 8GB DRAM",
            "✓ GPU Accuracy: High\n✓ Barrier Feedback Loops: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Memory Layout: 8GB DRAM",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\barrier_feedback_loops" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "System\\memory_layout_mode" to "2"
            )
        ),
        GameFixProfile(
            0x010023A017E94000L,
            "Batman: Arkham Knight",
            "• Критические просадки FPS при езде на Бэтмобиле\n• Вылеты по нехватке VRAM в Готэме",
            "• Severe Batmobile driving FPS drops\n• Gotham City open-world OOM crashes",
            "✓ Память: 8GB DRAM\n✓ Сжатие ASTC: Отключено\n✓ Асинхронные шейдеры: Включено\n✓ Точность GPU: Высокая",
            "✓ Memory Layout: 8GB DRAM\n✓ ASTC Recompression: Uncompressed\n✓ Asynchronous Shaders: Enabled\n✓ GPU Accuracy: High",
            mapOf(
                "System\\memory_layout_mode" to "2",
                "Renderer\\astc_recompression" to "0",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\gpu_accuracy" to "1"
            )
        ),
        GameFixProfile(
            0x01003AE017DB0000L,
            "Batman: Arkham City",
            "• Мерцание снега и тумана над городом\n• Микрофризы при планировании с плащом",
            "• Snow particle and fog flickering\n• Glide traversal micro-stutters",
            "✓ Точность GPU: Высокая\n✓ Асинхронные шейдеры: Включено\n✓ Быстрая память: Включено",
            "✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Cpu\\cpuopt_fastmem" to "true"
            )
        ),
        GameFixProfile(
            0x010071700F8BA000L,
            "Crash Bandicoot 4: It's About Time",
            "• Рассинхрон инпут-лага в сложных платформенных секциях\n• Размытие фонов",
            "• Frame pacing latency in tight platforming sections\n• Blurry background assets",
            "✓ Асинхронные шейдеры: Включено\n✓ Быстрая память: Включено\n✓ Анизотропная фильтрация: 16x",
            "✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled\n✓ Anisotropic Filtering: 16x",
            mapOf(
                "Renderer\\use_asynchronous_shaders" to "true",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\max_anisotropy" to "5"
            )
        ),
        GameFixProfile(
            0x0100B6E00B360000L,
            "Spyro Reignited Trilogy",
            "• Заикания звука в катсценах\n• Мерцание теней на травяных холмах",
            "• Audio stutter in cutscenes\n• Shadow shimmering on grassy hills",
            "✓ Асинхронные шейдеры: Включено\n✓ Быстрая память: Включено\n✓ Точность GPU: Высокая",
            "✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled\n✓ GPU Accuracy: High",
            mapOf(
                "Renderer\\use_asynchronous_shaders" to "true",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\gpu_accuracy" to "1"
            )
        ),
        GameFixProfile(
            0x0100770008DD8000L,
            "Monster Hunter Generations Ultimate",
            "• Мерцание текстур монстров и эффектов крови\n• Просадки в мультиплеере",
            "• Monster skin texture flicker\n• Multiplayer combat frame drops",
            "✓ Точность GPU: Высокая\n✓ Анизотропная фильтрация: 16x\n✓ Быстрая память: Включено",
            "✓ GPU Accuracy: High\n✓ Anisotropic Filtering: 16x\n✓ Fastmem: Enabled",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\max_anisotropy" to "5",
                "Cpu\\cpuopt_fastmem" to "true"
            )
        ),
        GameFixProfile(
            0x010034501659E000L,
            "Crisis Core: Final Fantasy VII Reunion",
            "• Статтеры при вращении рулетки DMW (Digital Mind Wave)\n• Сбои освещения в Мидгаре",
            "• DMW reel spinning stutter\n• Midgar volumetric lighting corruption",
            "✓ Точность GPU: Высокая\n✓ Асинхронные шейдеры: Включено\n✓ Быстрая память: Включено\n✓ Память: 8GB DRAM",
            "✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled\n✓ Memory Layout: 8GB DRAM",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Cpu\\cpuopt_fastmem" to "true",
                "System\\memory_layout_mode" to "2"
            )
        ),
        GameFixProfile(
            0x010074F013262000L,
            "Xenoblade Chronicles 3",
            "• Микростаттеры и утечки VRAM в масштабных битвах\n• Артефакты частиц Ouroboros",
            "• Battle scene micro-stutters and VRAM leaks\n• Ouroboros transformation particle artifacts",
            "✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Быстрая память: Включено\n✓ Память: 8GB DRAM",
            "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ Fastmem: Enabled\n✓ Memory Layout: 8GB DRAM",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_reactive_flushing" to "true",
                "Cpu\\cpuopt_fastmem" to "true",
                "System\\memory_layout_mode" to "2"
            )
        ),
        GameFixProfile(
            0x0100FF500E34A000L,
            "Xenoblade Chronicles: Definitive Edition",
            "• Размытие текстур персонажей на расстоянии\n• Мерцание травяного покрова на равнинах Гуры",
            "• Distant character texture blur\n• Bionis Leg grass shimmering",
            "✓ Точность GPU: Высокая\n✓ Анизотропная фильтрация: 16x\n✓ Память: 8GB DRAM",
            "✓ GPU Accuracy: High\n✓ Anisotropic Filtering: 16x\n✓ Memory Layout: 8GB DRAM",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\max_anisotropy" to "5",
                "System\\memory_layout_mode" to "2"
            )
        ),
        GameFixProfile(
            0x0100B04011742000L,
            "Monster Hunter Rise",
            "• Просадки FPS при битвах с Wyvern\n• Мерцание спецэффектов Wirebug",
            "• Wyvern combat FPS drops\n• Wirebug particle flickering",
            "✓ Точность GPU: Высокая\n✓ Обратные циклы барьеров: Включено\n✓ Быстрая память: Включено\n✓ Анизотропная фильтрация: 16x",
            "✓ GPU Accuracy: High\n✓ Barrier Feedback Loops: Enabled\n✓ Fastmem: Enabled\n✓ Anisotropic Filtering: 16x",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\barrier_feedback_loops" to "true",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\max_anisotropy" to "5"
            )
        ),
        GameFixProfile(
            0x01005C9014168000L,
            "NieR:Automata The End of YoRHa Edition",
            "• Задержка отклика в секциях пулевого ада\n• Мерцание пустынного песка",
            "• Bullet-hell section frame pacing latency\n• Desert sand shimmer",
            "✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Анизотропная фильтрация: 16x\n✓ Память: 8GB DRAM",
            "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Anisotropic Filtering: 16x\n✓ Memory Layout: 8GB DRAM",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\max_anisotropy" to "5",
                "System\\memory_layout_mode" to "2"
            )
        ),
        GameFixProfile(
            0x0100224016A90000L,
            "Persona 4 Golden",
            "• Размытие текстур в ТВ-мире\n• Артефакты меню",
            "• TV World blurry textures\n• UI menu texture artifacts",
            "✓ Быстрая память: Включено\n✓ Анизотропная фильтрация: 16x\n✓ Сжатие ASTC: Отключено",
            "✓ Fastmem: Enabled\n✓ Anisotropic Filtering: 16x\n✓ ASTC Recompression: Uncompressed",
            mapOf(
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\max_anisotropy" to "5",
                "Renderer\\astc_recompression" to "0"
            )
        ),
        GameFixProfile(
            0x0100A7301646E000L,
            "Unicorn Overlord",
            "• Размытие спрайтов персонажей на глобальной карте\n• Микростаттеры битв",
            "• Overworld sprite blur\n• Battle start micro-stutters",
            "✓ Быстрая память: Включено\n✓ Сжатие ASTC: Отключено\n✓ Анизотропная фильтрация: 16x",
            "✓ Fastmem: Enabled\n✓ ASTC Recompression: Uncompressed\n✓ Anisotropic Filtering: 16x",
            mapOf(
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\astc_recompression" to "0",
                "Renderer\\max_anisotropy" to "5"
            )
        ),
        GameFixProfile(
            0x01004F3017772000L,
            "Dave the Diver",
            "• Статтеры при глубоководных погружениях\n• Артефакты эффектов пузырей",
            "• Deep sea diving stutters\n• Underwater bubble particle glitches",
            "✓ Быстрая память: Включено\n✓ Сжатие ASTC: Отключено\n✓ Асинхронные шейдеры: Включено",
            "✓ Fastmem: Enabled\n✓ ASTC Recompression: Uncompressed\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\astc_recompression" to "0",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x01006BB00C6F0000L,
            "The Legend of Zelda: Link's Awakening",
            "• Просадки FPS при размытии глубины резкости (Tilt-Shift)\n• Заикания в деревне Мэйб",
            "• Tilt-shift depth of field severe frame drops\n• Mabe Village traversal stutters",
            "✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Быстрая память: Включено\n✓ Память: 8GB DRAM",
            "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ Fastmem: Enabled\n✓ Memory Layout: 8GB DRAM",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_reactive_flushing" to "true",
                "Cpu\\cpuopt_fastmem" to "true",
                "System\\memory_layout_mode" to "2"
            )
        ),
        GameFixProfile(
            0x01008CF01BAEC000L,
            "The Legend of Zelda: Echoes of Wisdom",
            "• Падения частоты кадров при создании копий предметов (Echoes)\n• Размытие воды",
            "• Echo creation frame drops\n• Water surface reflection distortion",
            "✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Быстрая память: Включено\n✓ Память: 8GB DRAM",
            "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ Fastmem: Enabled\n✓ Memory Layout: 8GB DRAM",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_reactive_flushing" to "true",
                "Cpu\\cpuopt_fastmem" to "true",
                "System\\memory_layout_mode" to "2"
            )
        ),
        GameFixProfile(
            0x01007760086F8000L,
            "Bayonetta 2",
            "• Сбои динамического освещения и теней в катсценах\n• Просадки FPS при битвах с ангелами",
            "• Dynamic lighting and shadow glitches in cutscenes\n• Angel combat scene frame drops",
            "✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Анизотропная фильтрация: 16x",
            "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Anisotropic Filtering: 16x",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\max_anisotropy" to "5"
            )
        ),
        GameFixProfile(
            0x0100BC0018138000L,
            "Super Mario RPG",
            "• Статтеры анимаций диалогов\n• Сбои синхронизации изометрического освещения",
            "• Dialogue animation stutters\n• Isometric lighting pass desync",
            "✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Быстрая память: Включено",
            "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ Fastmem: Enabled",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_reactive_flushing" to "true",
                "Cpu\\cpuopt_fastmem" to "true"
            )
        ),
        GameFixProfile(
            0x010069200E60E000L,
            "Pikmin 4",
            "• Утечки памяти в открытых садах Unreal Engine 4\n• Микрофризы при спавне отряда Пикминов",
            "• Unreal Engine 4 open garden memory leaks\n• Pikmin squad spawn micro-stutters",
            "✓ Память: 8GB DRAM\n✓ Точность GPU: Высокая\n✓ Асинхронные шейдеры: Включено\n✓ Реактивная очистка: Включено",
            "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled\n✓ Reactive Flushing: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "2",
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\use_reactive_flushing" to "true"
            )
        ),
        GameFixProfile(
            0x0100FA701A7CA000L,
            "Princess Peach: Showtime!",
            "• Просадки FPS при смене театральных декораций и костюмов\n• Мерцание теней сцены",
            "• Theater stage transition frame drops\n• Stage lighting shadow flicker",
            "✓ Память: 8GB DRAM\n✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "2",
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100C2A01A03A000L,
            "Luigi's Mansion 2 HD",
            "• Мерцание луча фонарика Dark-Light\n• Артефакты призрачных следов",
            "• Dark-Light flashlight beam flickering\n• Ghost trail rendering artifacts",
            "✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Быстрая память: Включено",
            "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ Fastmem: Enabled",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_reactive_flushing" to "true",
                "Cpu\\cpuopt_fastmem" to "true"
            )
        ),
        GameFixProfile(
            0x01000BC0000A0000L,
            "Hollow Knight",
            "• Задержка ввода (input lag) в босс-файтах\n• Микростаттеры при смене комнат",
            "• Boss fight frame latency\n• Room transition compilation stutters",
            "✓ Асинхронные шейдеры: Включено\n✓ Быстрая память: Включено\n✓ Сжатие ASTC: Отключено",
            "✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled\n✓ ASTC Recompression: Uncompressed",
            mapOf(
                "Renderer\\use_asynchronous_shaders" to "true",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\astc_recompression" to "0"
            )
        ),
        GameFixProfile(
            0x0100735010B46000L,
            "Hades",
            "• Микрофризы при комнатах с большим числом снарядов и спецэффектов",
            "• High particle and projectile chamber micro-stutters",
            "✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено\n✓ Анизотропная фильтрация: 16x",
            "✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Anisotropic Filtering: 16x",
            mapOf(
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\max_anisotropy" to "5"
            )
        ),
        GameFixProfile(
            0x010041B01211E000L,
            "Sea of Stars",
            "• Размытие пиксель-арта\n• Рассинхронизация динамического освещения солнца/луны",
            "• Pixel art sprite blur\n• Sun/moon eclipse dynamic lighting desync",
            "✓ Точность GPU: Высокая\n✓ Асинхронные шейдеры: Включено\n✓ Сжатие ASTC: Отключено",
            "✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled\n✓ ASTC Recompression: Uncompressed",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\astc_recompression" to "0"
            )
        ),
        GameFixProfile(
            0x0100656017E9C000L,
            "Octopath Traveler II",
            "• Пересвет и размытие глубины резкости в HD-2D\n• Микростаттеры при смене дня и ночи",
            "• HD-2D depth-of-field overexposure\n• Day/night transition micro-stutters",
            "✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_reactive_flushing" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x010052C00BAB8000L,
            "Bravely Default II",
            "• Утечки памяти в Unreal Engine 4 на карте мира\n• Просадки FPS в битвах",
            "• Unreal Engine 4 overworld memory leaks\n• Battle start FPS drops",
            "✓ Память: 8GB DRAM\n✓ Точность GPU: Высокая\n✓ Быстрая память: Включено",
            "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "2",
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true"
            )
        ),
        GameFixProfile(
            0x01003D200BAA2000L,
            "Dead Cells",
            "• Задержка ввода в динамичных боевых секциях\n• Статтеры процедурной генерации",
            "• Combat frame latency\n• Procedural level generation stutters",
            "✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено\n✓ Сжатие ASTC: Отключено",
            "✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ ASTC Recompression: Uncompressed",
            mapOf(
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\astc_recompression" to "0"
            )
        ),
        GameFixProfile(
            0x01001B60133A2000L,
            "Outer Wilds",
            "• Просадки FPS при физическом расчете орбит планет\n• Утечки VRAM в туманностях",
            "• Solar system physics calculation slowdown\n• Space nebulae VRAM leaks",
            "✓ Память: 8GB DRAM\n✓ Точность CPU: Точная\n✓ Асинхронные шейдеры: Включено",
            "✓ Memory Layout: 8GB DRAM\n✓ CPU Accuracy: Accurate\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "2",
                "Cpu\\cpu_accuracy" to "0",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x010067600DF2A000L,
            "Subnautica",
            "• Задержка прогрузки чанков морского дна\n• Микрофризы при управлении батискафом",
            "• Ocean floor chunk loading delays\n• Seamoth traversal stutters",
            "✓ Память: 8GB DRAM\n✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "2",
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x01009850119BC000L,
            "Ori and the Will of the Wisps",
            "• Просадки FPS при скоростном перемещении по локациям\n• Мерцание фонового света",
            "• Fast traversal frame drops\n• Volumetric light shimmering",
            "✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Быстрая память: Включено\n✓ Анизотропная фильтрация: 16x",
            "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ Fastmem: Enabled\n✓ Anisotropic Filtering: 16x",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_reactive_flushing" to "true",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\max_anisotropy" to "5"
            )
        ),
        GameFixProfile(
            0x01000CD00DE1E000L,
            "Alien: Isolation",
            "• Артефакты динамических теней на станции «Севастополь»\n• Шум отражений",
            "• Dynamic shadow artifacts in dark corridors\n• Specular reflection noise",
            "✓ Точность GPU: Высокая\n✓ Обратные циклы барьеров: Включено\n✓ Анизотропная фильтрация: 16x\n✓ Память: 8GB DRAM",
            "✓ GPU Accuracy: High\n✓ Barrier Feedback Loops: Enabled\n✓ Anisotropic Filtering: 16x\n✓ Memory Layout: 8GB DRAM",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\barrier_feedback_loops" to "true",
                "Renderer\\max_anisotropy" to "5",
                "System\\memory_layout_mode" to "2"
            )
        ),
        GameFixProfile(
            0x01005AF00BA7A000L,
            "Metroid Dread",
            "• Микрофризы при скоростном скольжении Самус\n• Рассинхронизация счетчика кадров в катсценах E.M.M.I.",
            "• Samus speed booster micro-stutters\n• E.M.M.I. cutscene frame rate desync",
            "✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено\n✓ Анизотропная фильтрация: 16x",
            "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Anisotropic Filtering: 16x",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\max_anisotropy" to "5"
            )
        ),
        GameFixProfile(
            0x010077200889E000L,
            "Donkey Kong Country: Tropical Freeze",
            "• Сбои полупрозрачности воды и шерсти Донки Конга\n• Микростаттеры при полетах на бочках",
            "• Water transparency and Kong fur shader glitches\n• Barrel blast transition micro-stutters",
            "✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Сжатие ASTC: Отключено",
            "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ ASTC Recompression: Uncompressed",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_reactive_flushing" to "true",
                "Renderer\\astc_recompression" to "0"
            )
        ),
        GameFixProfile(
            0x0100483017770000L,
            "Kirby's Return to Dream Land Deluxe",
            "• Размытие контуров сел-шейдинга персонажей\n• Просадки FPS при суперспособностях Кирби",
            "• Cel-shaded outline blurring\n• Super ability particle frame drops",
            "✓ Точность GPU: Высокая\n✓ Асинхронные шейдеры: Включено\n✓ Анизотропная фильтрация: 16x",
            "✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled\n✓ Anisotropic Filtering: 16x",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\max_anisotropy" to "5"
            )
        ),
        GameFixProfile(
            0x010085C0084FA000L,
            "Captain Toad: Treasure Tracker",
            "• Мерцание теней на трехмерных диорамах уровней\n• Артефакты глубины резкости",
            "• Shadow flickering on 3D diorama puzzles\n• Depth-of-field blur artifacts",
            "✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Сжатие ASTC: Отключено",
            "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ ASTC Recompression: Uncompressed",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_reactive_flushing" to "true",
                "Renderer\\astc_recompression" to "0"
            )
        ),
        GameFixProfile(
            0x01006000040C2000L,
            "Yoshi's Crafted World",
            "• Утечки памяти в Unreal Engine 4 на картонных декорациях\n• Размытие текстур заднего плана",
            "• Unreal Engine 4 cardboard diorama VRAM leaks\n• Background blur texture shimmering",
            "✓ Память: 8GB DRAM\n✓ Точность GPU: Высокая\n✓ Асинхронные шейдеры: Включено",
            "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "2",
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x010019401051C000L,
            "Mario Strikers: Battle League",
            "• Рассинхронизация анимации гиперударов (Hyper Strikes)\n• Статтеры при взрывах спецэффектов",
            "• Hyper Strike comic animation desync\n• Stadium particle burst stutters",
            "✓ Точность CPU: Точная\n✓ Точность GPU: Высокая\n✓ Быстрая память: Включено",
            "✓ CPU Accuracy: Accurate\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled",
            mapOf(
                "Cpu\\cpu_accuracy" to "0",
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true"
            )
        ),
        GameFixProfile(
            0x0100E2B017DAE000L,
            "Super Bomberman R 2",
            "• Просадки FPS при массовых взрывах в режиме «Замок»\n• Рассинхронизация сетевого таймера и зависание на проверке серверов Konami",
            "• Castle mode multi-explosion slowdowns\n• Konami server handshake stall and local multiplayer sync latency",
            "✓ Режим полёта: Включено (пропуск ожидания серверов Konami)\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено\n✓ Точность GPU: Высокая",
            "✓ Airplane Mode: Enabled (Bypasses Konami server check)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ GPU Accuracy: High",
            mapOf(
                "System\\airplane_mode" to "true",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\gpu_accuracy" to "1"
            )
        ),
        GameFixProfile(
            0x01002390146AC000L,
            "Advance Wars 1+2: Re-Boot Camp",
            "• Задержки загрузки анимаций командиров (CO Powers)\n• Размытие 2D-спрайтов техники",
            "• CO Power full-screen anime animation delay\n• Tactical map sprite blur",
            "✓ Точность GPU: Высокая\n✓ Сжатие ASTC: Отключено\n✓ Быстрая память: Включено",
            "✓ GPU Accuracy: High\n✓ ASTC Recompression: Uncompressed\n✓ Fastmem: Enabled",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\astc_recompression" to "0",
                "Cpu\\cpuopt_fastmem" to "true"
            )
        ),
        GameFixProfile(
            0x01002D801A1DE000L,
            "WarioWare: Move It!",
            "• Задержка отклика в микроиграх с быстрой сменой позы\n• Пропуск кадров на переходах",
            "• Microgame form change input latency\n• Transition animation frame skips",
            "✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено\n✓ Реактивная очистка: Включено",
            "✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Reactive Flushing: Enabled",
            mapOf(
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\use_reactive_flushing" to "true"
            )
        ),
        GameFixProfile(
            0x0100D6F015F70000L,
            "No Man's Sky",
            "• Вылеты по нехватке памяти при входе в атмосферу планет\n• Артефакты процедурной генерации",
            "• Planetary entry OOM memory crashes\n• Procedural voxel terrain artifacting",
            "✓ Память: 8GB DRAM\n✓ Точность GPU: Высокая\n✓ Обратные циклы барьеров: Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Barrier Feedback Loops: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "2",
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\barrier_feedback_loops" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x01000BD016550000L,
            "Portal: Companion Collection",
            "• Сбои рекурсивного рендеринга порталов\n• Рассинхронизация физики кубов в Source Engine",
            "• Recursive portal view rendering glitches\n• Source Engine physics stutter",
            "✓ Точность GPU: Высокая\n✓ Обратные циклы барьеров: Включено\n✓ Быстрая память: Включено\n✓ Анизотропная фильтрация: 16x",
            "✓ GPU Accuracy: High\n✓ Barrier Feedback Loops: Enabled\n✓ Fastmem: Enabled\n✓ Anisotropic Filtering: 16x",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\barrier_feedback_loops" to "true",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\max_anisotropy" to "5"
            )
        ),
        GameFixProfile(
            0x010058C01570E000L,
            "Persona 4 Golden",
            "• Микростаттеры при перемещении по туманной Инабе\n• Рассинхронизация звука аниме-вставок",
            "• Foggy Inaba traversal micro-stutters\n• Anime video cutscene audio desync",
            "✓ Быстрая память: Включено\n✓ Точность GPU: Высокая\n✓ Анизотропная фильтрация: 16x\n✓ Сжатие ASTC: Отключено",
            "✓ Fastmem: Enabled\n✓ GPU Accuracy: High\n✓ Anisotropic Filtering: 16x\n✓ ASTC Recompression: Uncompressed",
            mapOf(
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\max_anisotropy" to "5",
                "Renderer\\astc_recompression" to "0"
            )
        ),
        GameFixProfile(
            0x010072001570A000L,
            "Persona 3 Portable",
            "• Размытие 2D-портретов и визуальной новеллы\n• Просадки FPS на верхних этажах Тартара",
            "• Visual novel 2D portrait blur\n• Tartarus upper floor frame drops",
            "✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x01004C80170A6000L,
            "Tunic",
            "• Сбои страниц руководства в изометрическом виде\n• Шум объемного освещения",
            "• Instruction manual overlay rendering glitches\n• Isometric volumetric light noise",
            "✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_reactive_flushing" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100DDF01A03A000L,
            "Dave the Diver",
            "• Задержка прогрузки глубоководных биомов\n• Микрофризы в суши-ресторане Банчо",
            "• Deep sea biome transition latency\n• Bancho Sushi rush hour micro-stutters",
            "✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено\n✓ Сжатие ASTC: Отключено",
            "✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ ASTC Recompression: Uncompressed",
            mapOf(
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\astc_recompression" to "0"
            )
        ),
        GameFixProfile(
            0x01002BE01584E000L,
            "Cult of the Lamb",
            "• Просадки FPS при большом числе последователей в поселении\n• Сбои динамических теней",
            "• Cult camp high follower count slowdown\n• Ritual dynamic shadow flickering",
            "✓ Быстрая память: Включено\n✓ Точность GPU: Высокая\n✓ Асинхронные шейдеры: Включено",
            "✓ Fastmem: Enabled\n✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100913018F08000L,
            "Unicorn Overlord",
            "• Рассинхронизация 2D-анимаций Vanillaware в масштабных битвах\n• Размытие шрифтов интерфейса",
            "• Vanillaware 2D battle animation desync\n• Tactical interface font blur",
            "✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Анизотропная фильтрация: 16x\n✓ Быстрая память: Включено",
            "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ Anisotropic Filtering: 16x\n✓ Fastmem: Enabled",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_reactive_flushing" to "true",
                "Renderer\\max_anisotropy" to "5",
                "Cpu\\cpuopt_fastmem" to "true"
            )
        ),
        GameFixProfile(
            0x01002DA013484000L,
            "The Legend of Zelda: Skyward Sword HD",
            "• Артефакты меча и курсора при управлении движением\n• Размытие текстур облачного моря Небоземи",
            "• Motion control sword and pointer jitter\n• Skyloft cloud sea texture shimmering",
            "✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Анизотропная фильтрация: 16x",
            "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Anisotropic Filtering: 16x",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\max_anisotropy" to "5"
            )
        ),
        GameFixProfile(
            0x010028600EBDA000L,
            "Super Mario 3D World + Bowser's Fury",
            "• Просадки FPS при появлении Яростного Боузера в открытом море\n• Мерцание шейдеров дождя и лавы",
            "• Bowser's Fury open sea stormy weather FPS drops\n• Rain splash and lava dynamic shader flickering",
            "✓ Память: 8GB DRAM\n✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "1",
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x010036601D380000L,
            "Super Mario Party Jamboree",
            "• Микрофризы в мини-играх на 20 игроков Koopathlon\n• Рассинхронизация счетчика очков",
            "• 20-player Koopathlon minigame stutters\n• Live board score counter desync",
            "✓ Память: 8GB DRAM\n✓ Точность GPU: Высокая\n✓ Асинхронные шейдеры: Включено\n✓ Реактивная очистка: Включено",
            "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled\n✓ Reactive Flushing: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "1",
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\use_reactive_flushing" to "true"
            )
        ),
        GameFixProfile(
            0x010091801E8B2000L,
            "Mario & Luigi: Brothership",
            "• Задержка отклика в совместных Brother Attacks\n• Просадки FPS на морских островах Конкордии",
            "• Brother Attacks timing lag\n• Concordia ocean sailing FPS dips",
            "✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено\n✓ Сжатие ASTC: Отключено",
            "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ ASTC Recompression: Uncompressed",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\astc_recompression" to "0"
            )
        ),
        GameFixProfile(
            0x01000A10041EA000L,
            "Super Mario Party",
            "• Микрозадержки анимаций кубиков и персонажей\n• Сбои полупрозрачности воды в речных сплавах",
            "• Dice roll animation micro-stutters\n• River Survival water transparency glitches",
            "✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x01006BB00C6F0000L,
            "Mario Party Superstars",
            "• Мерцание теней на классических досках N64\n• Размытие миниатюр правил мини-игр",
            "• Retro N64 board shadow flickering\n• Minigame instruction modal blur",
            "✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Анизотропная фильтрация: 16x",
            "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ Anisotropic Filtering: 16x",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_reactive_flushing" to "true",
                "Renderer\\max_anisotropy" to "5"
            )
        ),
        GameFixProfile(
            0x0100F7701140E000L,
            "Mario Golf: Super Rush",
            "• Размытие сетки рельефа грина при прицеливании\n• Просадки FPS при массовом забеге Speed Golf",
            "• Green terrain grid blur\n• Speed Golf stamina sprint slowdown",
            "✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100BDE00862A000L,
            "Mario Tennis Aces",
            "• Инпут-лаг в замедлении времени Zone Speed\n• Артефакты свечения ракетки при спец-ударах",
            "• Zone Speed slow-motion input lag\n• Special Shot racket glow artifacts",
            "✓ Точность CPU: Высокая (Accurate)\n✓ Точность GPU: Высокая\n✓ Быстрая память: Включено",
            "✓ CPU Accuracy: Accurate\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled",
            mapOf(
                "Cpu\\cpu_accuracy" to "0",
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true"
            )
        ),
        GameFixProfile(
            0x01003C700009C000L,
            "Splatoon 2",
            "• Мерцание отражений чернил на металлических поверхностях\n• Микрофризы в хабе площади Инкополиса",
            "• Metallic surface ink reflection flicker\n• Inkopolis Square hub micro-stutters",
            "✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено\n✓ Анизотропная фильтрация: 16x",
            "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Anisotropic Filtering: 16x",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\max_anisotropy" to "5"
            )
        ),
        GameFixProfile(
            0x0100C2500FC20000L,
            "Splatoon 3",
            "• Утечки памяти в хабе Плюхграда и в матчах Залива Самонид\n• Просадки FPS при обильном залитии карты краской и сетевые задержки лобби",
            "• Splatsville hub VRAM leaks\n• Salmon Run heavy ink coverage slowdown & match lobby delay",
            "✓ Режим полёта: Включено (офлайн-кампания без сетевых задержек)\n✓ Память: 8GB DRAM\n✓ Точность GPU: Высокая\n✓ Асинхронные шейдеры: Включено\n✓ Реактивная очистка: Включено",
            "✓ Airplane Mode: Enabled (Instant offline Hero mode)\n✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled\n✓ Reactive Flushing: Enabled",
            mapOf(
                "System\\airplane_mode" to "true",
                "System\\memory_layout_mode" to "1",
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\use_reactive_flushing" to "true"
            )
        ),
        GameFixProfile(
            0x01004D300C5AE000L,
            "Paper Mario: The Origami King",
            "• Сбои отрисовки кольцевой арены в битвах\n• Артефакты конфетти и бумажных складок",
            "• Ring puzzle battle arena glitches\n• Confetti paper fold texture artifacts",
            "✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Быстрая память: Включено",
            "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ Fastmem: Enabled",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_reactive_flushing" to "true",
                "Cpu\\cpuopt_fastmem" to "true"
            )
        ),
        GameFixProfile(
            0x01002B00111A2000L,
            "Hyrule Warriors: Age of Calamity",
            "• Тяжелые просадки FPS при массовом скоплении монстров на экране\n• Утечки VRAM в битвах Чудищ",
            "• Massive enemy swarm heavy slowdown\n• Divine Beast battle VRAM exhaustion",
            "✓ Память: 8GB DRAM\n✓ Точность GPU: Высокая\n✓ Асинхронные шейдеры: Включено\n✓ Быстрая память: Включено",
            "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "1",
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Cpu\\cpuopt_fastmem" to "true"
            )
        ),
        GameFixProfile(
            0x0100AE0009A80000L,
            "Hyrule Warriors: Definitive Edition",
            "• Размытие спецэффектов комбо Focus Spirit\n• Микрофризы при спавне отрядов офицеров",
            "• Focus Spirit attack bloom blur\n• Officer squad spawn micro-stutters",
            "✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x01007E3015C3E000L,
            "Fire Emblem Warriors: Three Hopes",
            "• Падение производительности в масштабных боях на картах Фодлана\n• Артефакты теней полководцев",
            "• Large scale Fodlan battlefield FPS dips\n• Commander shadow map rendering errors",
            "✓ Память: 8GB DRAM\n✓ Точность GPU: Высокая\n✓ Асинхронные шейдеры: Включено",
            "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "1",
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100000011D90000L,
            "Pokemon Brilliant Diamond",
            "• Мерцание чиби-отражений в лужах и окнах Синно\n• Задержка открытия меню покедекса",
            "• Chibi puddle & window reflection flicker\n• Pokedex animation transition lag",
            "✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено\n✓ Анизотропная фильтрация: 16x",
            "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Anisotropic Filtering: 16x",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\max_anisotropy" to "5"
            )
        ),
        GameFixProfile(
            0x010018E011D92000L,
            "Pokemon Shining Pearl",
            "• Мерцание чиби-отражений в лужах и окнах Синно\n• Задержка открытия меню покедекса",
            "• Chibi puddle & window reflection flicker\n• Pokedex animation transition lag",
            "✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено\n✓ Анизотропная фильтрация: 16x",
            "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Anisotropic Filtering: 16x",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\max_anisotropy" to "5"
            )
        ),
        GameFixProfile(
            0x010003F003A34000L,
            "Pokemon Let's Go, Pikachu!",
            "• Задержка круга прицеливания при броске покебола\n• Мерцание травы на маршрутах Канто",
            "• Pokeball throw capture ring input lag\n• Kanto route grass shader flickering",
            "✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100152000022000L,
            "Pokemon Let's Go, Eevee!",
            "• Задержка круга прицеливания при броске покебола\n• Мерцание травы на маршрутах Канто",
            "• Pokeball throw capture ring input lag\n• Kanto route grass shader flickering",
            "✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100B870097D6000L,
            "Shin Megami Tensei V",
            "• Просадки FPS на песчаных барханах пустыни Да'ат в Unreal Engine 4\n• Размытие магических заклинаний Нахобино",
            "• Da'at desert sand dunes UE4 slowdown\n• Nahobino Magatsuhi skill blur",
            "✓ Память: 8GB DRAM\n✓ Точность GPU: Высокая\n✓ Асинхронные шейдеры: Включено\n✓ Быстрая память: Включено",
            "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "1",
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Cpu\\cpuopt_fastmem" to "true"
            )
        ),
        GameFixProfile(
            0x0100C48008890000L,
            "Xenoblade Chronicles 2: Torna ~ The Golden Country",
            "• Утечки памяти на гигантских просторах Титана Торны\n• Размытие облачного покрова и листвы",
            "• Torna Titan open terrain VRAM leaks\n• Cloud sea and foliage shimmering",
            "✓ Память: 8GB DRAM\n✓ Точность GPU: Высокая\n✓ Асинхронные шейдеры: Включено\n✓ Быстрая память: Включено",
            "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "1",
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Cpu\\cpuopt_fastmem" to "true"
            )
        ),
        GameFixProfile(
            0x010049900F546000L,
            "Super Mario 3D All-Stars",
            "• Мерцание текстур воды в Mario Sunshine\n• Рассинхронизация звуковых дорожек в Mario Galaxy",
            "• Sunshine water reflection texture flickering\n• Galaxy orchestrated audio drift",
            "✓ Точность CPU: Высокая (Accurate)\n✓ Точность GPU: Высокая\n✓ Быстрая память: Включено",
            "✓ CPU Accuracy: Accurate\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled",
            mapOf(
                "Cpu\\cpu_accuracy" to "0",
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true"
            )
        ),
        GameFixProfile(
            0x010065B00AE8E000L,
            "Cuphead",
            "• Инпут-лаг при скоростных уклонениях и парированиях розовых снарядов\n• Размытие пленочного зерна 1930-х",
            "• Precision parry / dash input lag\n• 1930s film grain shader blurring",
            "✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено\n✓ Сжатие ASTC: Отключено",
            "✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ ASTC Recompression: Uncompressed",
            mapOf(
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\astc_recompression" to "0"
            )
        ),
        GameFixProfile(
            0x0100D5700DC34000L,
            "Ori and the Blind Forest: Definitive Edition",
            "• Сбои изометрических источников света в лесу Нибель\n• Микрофризы при скоростном Bash-прыжке",
            "• Nibel forest dynamic volumetric light glitches\n• High-speed Bash chain micro-stutters",
            "✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Быстрая память: Включено\n✓ Анизотропная фильтрация: 16x",
            "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ Fastmem: Enabled\n✓ Anisotropic Filtering: 16x",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_reactive_flushing" to "true",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\max_anisotropy" to "5"
            )
        ),
        GameFixProfile(
            0x0100216008E88000L,
            "Slay the Spire",
            "• Микрозадержки анимаций розыгрыша карт и реликвий\n• Размытие текстовых описаний баффов",
            "• Card play and relic particle animation delay\n• Buff description text blur",
            "✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено\n✓ Сжатие ASTC: Отключено",
            "✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ ASTC Recompression: Uncompressed",
            mapOf(
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\astc_recompression" to "0"
            )
        ),
        GameFixProfile(
            0x01008C300B4D2000L,
            "Into the Breach",
            "• Задержка прорисовки изометрической тактической сетки 8x8\n• Размытие анимаций атаки Мехов",
            "• 8x8 tactical grid UI latency\n• Mech attack pixel-art animation blur",
            "✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено\n✓ Сжатие ASTC: Отключено",
            "✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ ASTC Recompression: Uncompressed",
            mapOf(
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\astc_recompression" to "0"
            )
        ),
        GameFixProfile(
            0x0100C4D01D026000L,
            "Tales of Series (Berseria / Graces f / Symphonia)",
            "• Зависание на 0 FPS и черный экран при смене локаций\n• Рассинхронизация видео-переходов NVDEC",
            "• 0 FPS freeze and black screen on location transitions\n• NVDEC video transition desync",
            "✓ Сжатие ASTC: Отключено (устраняет зависание при смене локаций)\n✓ Реактивная очистка: Включено\n✓ Эмуляция NVDEC: ГПУ видеоядро (NVDEC)\n✓ Асинхронные шейдеры: Включено\n✓ Быстрая память (Fastmem): Включено",
            "✓ ASTC Recompression: Uncompressed (Fixes location transition freeze!)\n✓ Reactive Flushing: Enabled\n✓ NVDEC Emulation: GPU Video Core\n✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled",
            mapOf(
                "Renderer\\astc_recompression" to "0",
                "Renderer\\use_reactive_flushing" to "true",
                "Renderer\\nvdec_emulation" to "2",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\gpu_accuracy" to "1"
            )
        ),
        GameFixProfile(
            0x01007300020FA000L,
            "Astral Chain",
            "• Просадки FPS при вызове Легионов и комбо-атаках в Арке\n• Утечки VRAM при длительной игре",
            "• Legion summon and chain sync combo FPS drops\n• Long session VRAM exhaustion in the Ark",
            "✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "1",
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x01005FA00BAFA000L,
            "Fire Emblem: Three Houses",
            "• Утечки памяти при перемещении по монастырю Гаррег Мах\n• Просадки FPS на тактической сетке боев",
            "• Garreg Mach Monastery roaming memory leaks\n• Tactical grid battle FPS dips",
            "✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Быстрая память (Fastmem): Включено",
            "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ Fastmem: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "1",
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_reactive_flushing" to "true",
                "Cpu\\cpuopt_fastmem" to "true"
            )
        ),
        GameFixProfile(
            0x0100F4C009322000L,
            "Pikmin 3 Deluxe",
            "• Размытие текстур фруктов и сока\n• Мерцание теней в заданиях Олимара",
            "• Fruit texture and juice rendering blur\n• Olimar side-story shadow shimmering",
            "✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Анизотропная фильтрация: 16x\n✓ Асинхронные шейдеры: Включено",
            "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Anisotropic Filtering: 16x\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\max_anisotropy" to "5",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x01007E3006DDA000L,
            "Kirby Star Allies",
            "• Рассинхронизация спецэффектов сердец дружбы Friend Heart\n• Микрофризы в битвах четверки героев",
            "• Friend Heart dynamic glow desync\n• 4-player team battle micro-stutters",
            "✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Анизотропная фильтрация: 16x",
            "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Anisotropic Filtering: 16x",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\max_anisotropy" to "5"
            )
        ),
        GameFixProfile(
            0x0100ABF008968000L,
            "Pokemon Sword & Shield",
            "• Просадки FPS при динамической смене погоды в Диких землях Галара\n• Мерцание свечения Гигантамакса",
            "• Galar Wild Area weather dynamic lighting drops\n• Dynamax aura rendering flicker",
            "✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Анизотропная фильтрация: 16x",
            "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Anisotropic Filtering: 16x",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\max_anisotropy" to "5"
            )
        ),
        GameFixProfile(
            0x01006A800016E000L,
            "Super Smash Bros. Ultimate",
            "• Инпут-лаг и просадки кадров в боях на 8 игроков\n• Зависание катсцен и задержки проверки Духов онлайн",
            "• 8-fighter intense brawl input latency\n• Spirit Board online latency & cutscene sync pause",
            "✓ Режим полёта: Включено (пропуск онлайн-опроса Духов)\n✓ Точность CPU: Точная (Accurate)\n✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ Airplane Mode: Enabled (Bypasses Spirit Board network check)\n✓ CPU Accuracy: Accurate\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "System\\airplane_mode" to "true",
                "Cpu\\cpu_accuracy" to "0",
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x01004C00000CE000L,
            "Dragon Quest XI S: Echoes of an Elusive Age",
            "• Микрофризы при быстрой скачке на лошади по королевству Гелиодор в UE4\n• Размытие волос и брони",
            "• Heliodor kingdom high-speed horse ride micro-stutters\n• Character hair and armor texture blur",
            "✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "1",
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x01007D401662A000L,
            "NieR:Automata The End of YoRHa Edition",
            "• Просадки FPS при скоростном полете на поде по Руинам Города\n• Артефакты частиц bullet-hell сфер",
            "• Ruined City flight unit sequence FPS drops\n• Bullet-hell glowing sphere particle artifacts",
            "✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Реактивная очистка: Включено",
            "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Reactive Flushing: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "1",
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\use_reactive_flushing" to "true"
            )
        ),
        GameFixProfile(
            0x0100778006E88000L,
            "Bayonetta 1 & 2",
            "• Инпут-лаг в замедлении времени Witch Time\n• Пересвет эффектов Umbran Climax",
            "• Witch Time slow-motion input lag\n• Umbran Climax overexposed bloom",
            "✓ Точность CPU: Точная (Accurate)\n✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ CPU Accuracy: Accurate\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Cpu\\cpu_accuracy" to "0",
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x01005CA0157CA000L,
            "Persona 5 Royal",
            "• Рассинхронизация комикс-переходов и анимаций All-Out Attack\n• Микрофризы во Дворцах Метаверсума",
            "• All-Out Attack comic transition desync\n• Metaverse Palace shadow infiltration micro-stutters",
            "✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Анизотропная фильтрация: 16x\n✓ Сжатие ASTC: Отключено",
            "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Anisotropic Filtering: 16x\n✓ ASTC Recompression: Uncompressed",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\max_anisotropy" to "5",
                "Renderer\\astc_recompression" to "0"
            )
        ),
        GameFixProfile(
            0x0100780016140000L,
            "Live A Live",
            "• Размытие пиксельных спрайтов при переходе между эпохами\n• Микрофризы в боях на шахматной сетке",
            "• HD-2D era chapter transition sprite blur\n• Grid battle attack sequence stutters",
            "✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Сжатие ASTC: Отключено",
            "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ ASTC Recompression: Uncompressed",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\astc_recompression" to "0"
            )
        ),
        GameFixProfile(
            0x01000BD00E756000L,
            "Tony Hawk's Pro Skater 1 + 2",
            "• Зависание на заставке и бесконечный спиннер авторизации Activision\n• Сетевой таймаут при старте",
            "• Infinite Activision authorization spinner on startup\n• Online handshake deadlock",
            "✓ Режим полёта: Включено (пропуск онлайн-авторизации Activision)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Память: 8GB DRAM",
            "✓ Airplane Mode: Enabled (Bypasses Activision online login)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Memory Layout: 8GB DRAM",
            mapOf(
                "System\\airplane_mode" to "true",
                "System\\memory_layout_mode" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100F7A00B704000L,
            "Crash Team Racing Nitro-Fueled",
            "• Зависание в главном меню при опросе серверов Pit Stop\n• Микрофризы анимаций подиума",
            "• Pit Stop server telemetry handshake freeze\n• Podium animation micro-stutters",
            "✓ Режим полёта: Включено (пропуск серверов Pit Stop)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Анизотропная фильтрация: 16x",
            "✓ Airplane Mode: Enabled (Bypasses Pit Stop online check)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Anisotropic Filtering: 16x",
            mapOf(
                "System\\airplane_mode" to "true",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\max_anisotropy" to "5"
            )
        ),
                GameFixProfile(
            0x01006560184E6000L,
            "Mortal Kombat 1",
            "• Зависание на заставке WB Games при онлайн-синхронизации\n• Сбои Extended Dynamic State в шейдерах арены",
            "• WB Games intro online sync freeze\n• Extended Dynamic State arena shader crashes",
            "✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Режим полёта: Включено (пропуск серверов WB Play)\n✓ Быстрое время GPU: Отключено (устраняет deadlock UE4)\n✓ Динамическое состояние: Базовое (EDS1)\n✓ Точность CPU: Авто (безопасные мониторы потоков)\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено\n✓ Точность GPU: Обычная",
            "✓ Memory Layout: 8GB DRAM\n✓ Airplane Mode: Enabled (Bypasses WB Play online check)\n✓ Fast GPU Time: Disabled (Fixes UE4 deadlock)\n✓ Dynamic State: Basic (EDS1)\n✓ CPU Accuracy: Auto (Safe thread monitors)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ GPU Accuracy: Normal",
            mapOf(
                "Core\\memory_layout_mode" to "2",
                "System\\memory_layout_mode" to "2",
                "System\\airplane_mode" to "true",
                "Renderer\\use_fast_gpu_time" to "false",
                "Renderer\\dyna_state" to "1",
                "Cpu\\cpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\astc_recompression" to "0",
                "Renderer\\gpu_accuracy" to "0",
                "System\\use_docked_mode" to "0"
            )
        ),
        GameFixProfile(
            0x0100D2800D5C2000L,
            "Mortal Kombat 1",
            "• Зависание на заставке WB Games при онлайн-синхронизации\n• Сбои Extended Dynamic State в шейдерах арены",
            "• WB Games intro online sync freeze\n• Extended Dynamic State arena shader crashes",
            "✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Режим полёта: Включено (пропуск серверов WB Play)\n✓ Быстрое время GPU: Отключено (устраняет deadlock UE4)\n✓ Динамическое состояние: Базовое (EDS1)\n✓ Точность CPU: Авто (безопасные мониторы потоков)\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено\n✓ Точность GPU: Обычная",
            "✓ Memory Layout: 8GB DRAM\n✓ Airplane Mode: Enabled (Bypasses WB Play online check)\n✓ Fast GPU Time: Disabled (Fixes UE4 deadlock)\n✓ Dynamic State: Basic (EDS1)\n✓ CPU Accuracy: Auto (Safe thread monitors)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ GPU Accuracy: Normal",
            mapOf(
                "Core\\memory_layout_mode" to "2",
                "System\\memory_layout_mode" to "2",
                "System\\airplane_mode" to "true",
                "Renderer\\use_fast_gpu_time" to "false",
                "Renderer\\dyna_state" to "1",
                "Cpu\\cpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\astc_recompression" to "0",
                "Renderer\\gpu_accuracy" to "0",
                "System\\use_docked_mode" to "0"
            )
        ),
        GameFixProfile(
            0x0100B1100C4D0000L,
            "Mortal Kombat 11",
            "• Зависание на титульном экране при синхронизации WB Play / Башен Времени\n• Утечки VRAM в кинематографичных фаталити",
            "• WB Play / Towers of Time server sync freeze on title screen\n• Cinematic Fatalities VRAM spikes",
            "✓ Режим полёта: Включено (пропуск ожидания WB Play)\n✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ Airplane Mode: Enabled (Bypasses WB Play online check)\n✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "System\\airplane_mode" to "true",
                "Core\\memory_layout_mode" to "2",
                "System\\memory_layout_mode" to "2",
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x01007E300B70C000L,
            "Borderlands Legendary Collection (1, 2, TPS)",
            "• Задержка старта на 60+ секунд и зависание при сетевом опросе SHiFT\n• Просадки FPS при взрывах стихий",
            "• 60+ second boot freeze during SHiFT network ping\n• Elemental explosion particle slowdowns",
            "✓ Режим полёта: Включено (мгновенный старт без ожидания SHiFT)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Точность GPU: Высокая",
            "✓ Airplane Mode: Enabled (Instant boot without SHiFT timeout)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ GPU Accuracy: High",
            mapOf(
                "System\\airplane_mode" to "true",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\gpu_accuracy" to "1"
            )
        ),
        GameFixProfile(
            0x01001B700A654000L,
            "Diablo III: Eternal Collection",
            "• Задержка и зависание на экране сезонов при проверке Battle.net\n• Микрофризы при спавне элитных паков",
            "• Battle.net seasonal handshake timeout freeze\n• Elite mob pack spawn stutter",
            "✓ Режим полёта: Включено (пропуск серверов Battle.net)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Точность GPU: Высокая",
            "✓ Airplane Mode: Enabled (Skips Battle.net server check)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ GPU Accuracy: High",
            mapOf(
                "System\\airplane_mode" to "true",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\gpu_accuracy" to "1"
            )
        ),
        GameFixProfile(
            0x01005E4017C7A000L,
            "Demon Slayer: Kimetsu no Yaiba - The Hinokami Chronicles",
            "• Зависание на заставке при онлайн-верификации CyberConnect2\n• Просадки кадров в спецприемах дыхания",
            "• Title screen freeze during CyberConnect2 online verification loop\n• Breathing form ultimate attack FPS drops",
            "✓ Режим полёта: Включено (пропуск онлайн-верификации)\n✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ Airplane Mode: Enabled (Skips online verification)\n✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "System\\airplane_mode" to "true",
                "System\\memory_layout_mode" to "1",
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100830007780000L,
            "Monster Hunter Rise / Sunbreak",
            "• Микрофризы при опросе сетевого лобби Hunter Search\n• Утечки VRAM в локациях Джунглей и Цитадели",
            "• Hunter Search lobby broadcast micro-freezes\n• Jungle and Citadel VRAM leaks",
            "✓ Режим полёта: Включено (устранение микрофризов Hunter Search)\n✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ Airplane Mode: Enabled (Fixes Hunter Search lobby lag)\n✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "System\\airplane_mode" to "true",
                "System\\memory_layout_mode" to "1",
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x010052900FA62000L,
            "Burnout Paradise Remastered",
            "• Зависание на заставке при обращении к серверам новостей EA Paradise City\n• Микрофризы в авариях",
            "• EA Paradise City news server connection freeze\n• Crash sequence micro-stutters",
            "✓ Режим полёта: Включено (пропуск серверов EA)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Сжатие ASTC: Отключено",
            "✓ Airplane Mode: Enabled (Bypasses EA online news)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ ASTC Recompression: Uncompressed",
            mapOf(
                "System\\airplane_mode" to "true",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\astc_recompression" to "0"
            )
        ),
        GameFixProfile(
            0x0100650012270000L,
            "Overcooked! All You Can Eat",
            "• Зависание на этапе инициализации Team17 Cross-Play сервисов на Android\n• Рассинхронизация физики кухни",
            "• Team17 Cross-Play service handshake freeze on Android\n• Kitchen item physics desync",
            "✓ Режим полёта: Включено (пропуск Team17 Cross-Play)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ Airplane Mode: Enabled (Bypasses Team17 Cross-Play)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "System\\airplane_mode" to "true",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x01001FB012014000L,
            "Plants vs. Zombies: Battle for Neighborville",
            "• Зависание на стартовом экране из-за онлайн-требований Frostbite EA\n• Утечки памяти в хабе Беспечного парка",
            "• Frostbite EA online login requirement deadlock on title screen\n• Giddy Park hub memory leaks",
            "✓ Режим полёта: Включено (пропуск онлайн-авторизации EA)\n✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ Airplane Mode: Enabled (Bypasses EA online login requirement)\n✓ Memory Layout: 8GB DRAM\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "System\\airplane_mode" to "true",
                "System\\memory_layout_mode" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100D5600DE44000L,
            "Risk of Rain 2",
            "• Сетевой таймаут PlayFab в главном меню\n• Просадки FPS при спавне сотен монстров на 5+ стадии",
            "• PlayFab matchmaking network timeout\n• High-stage horde particle slowdowns",
            "✓ Режим полёта: Включено (пропуск PlayFab таймаутов)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Точность GPU: Высокая",
            "✓ Airplane Mode: Enabled (Bypasses PlayFab timeout)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ GPU Accuracy: High",
            mapOf(
                "System\\airplane_mode" to "true",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\gpu_accuracy" to "1"
            )
        ),
        GameFixProfile(
            0x0100E2600AE5C000L,
            "Minecraft Dungeons",
            "• Зацикливание авторизации учетной записи Microsoft / Xbox Live\n• Микрофризы процедурных подземелий",
            "• Microsoft / Xbox Live telemetry sign-in loop\n• Procedural dungeon generation stutters",
            "✓ Режим полёта: Включено (пропуск авторизации Microsoft)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ Airplane Mode: Enabled (Bypasses Microsoft telemetry)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "System\\airplane_mode" to "true",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x01006B4009B74000L,
            "Dragon Ball FighterZ",
            "• Зависание на старте при поиске сетевого лобби Bandai Namco\n• Инпут-лаг в комбо",
            "• Bandai Namco online lobby connection loop on title screen\n• Combo input latency",
            "✓ Режим полёта: Включено (пропуск сетевого лобби)\n✓ Точность CPU: Точная (Accurate)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ Airplane Mode: Enabled (Bypasses online lobby search)\n✓ CPU Accuracy: Accurate\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "System\\airplane_mode" to "true",
                "Cpu\\cpu_accuracy" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x010077C000B90000L,
            "Dragon Ball Xenoverse 2",
            "• Зависание в городе Контон при опросе сетевых серверов\n• Просадки FPS в рейдах на 6 игроков",
            "• Conton City server polling freeze\n• 6-player raid boss FPS drops",
            "✓ Режим полёта: Включено (пропуск сетевого сервера Контона)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Память: 8GB DRAM",
            "✓ Airplane Mode: Enabled (Bypasses Conton City server)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Memory Layout: 8GB DRAM",
            mapOf(
                "System\\airplane_mode" to "true",
                "System\\memory_layout_mode" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100B1A01B20A000L,
            "NBA 2K24 / 2K23",
            "• Зависание на экране обновления ростеров (2K Sports server handshake)\n• Утечки памяти в режиме MyCAREER",
            "• 2K Sports roster server handshake freeze\n• MyCAREER mode memory leaks",
            "✓ Режим полёта: Включено (пропуск серверов 2K Sports)\n✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ Airplane Mode: Enabled (Bypasses 2K server handshake)\n✓ Memory Layout: 8GB DRAM\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "System\\airplane_mode" to "true",
                "System\\memory_layout_mode" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100827018DEE000L,
            "Just Dance Series (2020-2024)",
            "• Зависание на заставке при подключении к Ubisoft Connect\n• Рассинхронизация видеопотока клипов",
            "• Ubisoft Connect server connection loop on boot\n• Video stream audio sync delay",
            "✓ Режим полёта: Включено (пропуск серверов Ubisoft Connect)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ Airplane Mode: Enabled (Bypasses Ubisoft Connect)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "System\\airplane_mode" to "true",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x01005B900C09A000L,
            "Clubhouse Games: 51 Worldwide Classics",
            "• Задержка и зависание при локальном сетевом поиске столов\n• Размытие текстур игровых досок",
            "• Local lobby discovery loop freeze on startup\n• Board texture and piece blur",
            "✓ Режим полёта: Включено (мгновенная загрузка офлайн)\n✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ Airplane Mode: Enabled (Instant offline boot)\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "System\\airplane_mode" to "true",
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100BDE00862A000L,
            "Mario Tennis Aces",
            "• Зависание на проверке онлайн-турниров при старте\n• Просадки FPS при Zone Shot спецэффектах",
            "• Tournament online leaderboard check hang\n• Zone Shot particle slowdowns",
            "✓ Режим полёта: Включено (пропуск онлайн-проверки турниров)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Точность GPU: Высокая",
            "✓ Airplane Mode: Enabled (Skips tournament online check)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ GPU Accuracy: High",
            mapOf(
                "System\\airplane_mode" to "true",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\gpu_accuracy" to "1"
            )
        ),
        GameFixProfile(
            0x0100874011EE6000L,
            "Mario Golf: Super Rush",
            "• Задержка и зависание при сетевом поиске ранговых матчей\n• Мерцание дальних флажков и травы",
            "• Ranked match network polling hang\n• Course grass and flag shadow flickering",
            "✓ Режим полёта: Включено (мгновенный старт офлайн)\n✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ Airplane Mode: Enabled (Instant offline start)\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "System\\airplane_mode" to "true",
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x01007E3019808000L,
            "Shin Megami Tensei V: Vengeance",
            "• Вылеты из-за нехватки памяти (OOM) на открытых картах Да'ат в UE4\n• Розовые артефакты текстур",
            "• Unreal Engine 4 Da'at open-world Out-Of-Memory crashes\n• Corrupted/pink ASTC texture tiles",
            "✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "1",
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100121014688000L,
            "Metroid Prime Remastered",
            "• Артефакты визора и HUD-интерфейса\n• Микрофризы при открытии переходных дверей между комнатами",
            "• Combat/Scan visor HUD graphical glitches\n• Room transition door loading stutters",
            "✓ Точность GPU: Высокая\n✓ Сжатие ASTC: Отключено (исправление визора)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ GPU Accuracy: High\n✓ ASTC Recompression: Uncompressed (Fixes visor artifacts)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\astc_recompression" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x01005E3017B46000L,
            "Crisis Core -Final Fantasy VII- Reunion",
            "• Микрозадержки при срабатывании барабана рулетки DMW в бою\n• Мерцание динамического освещения в UE4",
            "• DMW combat reel digital mind wave hitches\n• UE4 dynamic lighting and shadow flickering",
            "✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Быстрая память (Fastmem): Включено",
            "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ Fastmem: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "1",
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_reactive_flushing" to "true",
                "Cpu\\cpuopt_fastmem" to "true"
            )
        ),
        GameFixProfile(
            0x0100D15017D3E000L,
            "Octopath Traveler II",
            "• Размытие пиксель-арт спрайтов персонажей и HD-2D окружения\n• Швы на текстурах воды",
            "• HD-2D sprite blur and character outline softening\n• Water surface shader seam lines",
            "✓ Точность GPU: Высокая\n✓ Сжатие ASTC: Отключено\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ GPU Accuracy: High\n✓ ASTC Recompression: Uncompressed\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\astc_recompression" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100508013AE0000L,
            "Sonic Frontiers",
            "• Просадки FPS при разгоне на открытых островах Звездопада\n• Подгрузка геометрии рейлов и колец",
            "• Starfall Islands high-speed boost framerate drops\n• Rail and collectible geometry pop-in",
            "✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "1",
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100570017106000L,
            "Tactics Ogre: Reborn",
            "• Микрозадержки при исполнении спец-ударов и магии\n• Сглаживание пиксельных шрифтов диалогов",
            "• Finishing moves particle stutters\n• Retro dialogue font smoothing",
            "✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100AE600BA12000L,
            "Dragon's Dogma: Dark Arisen",
            "• Зависание на старте при сетевой верификации серверов Пешек (Pawns)\n• Просадки FPS в битвах с химерами и грифонами",
            "• Pawn server network verification hang on boot\n• Chimera and Griffin boss particle slowdowns",
            "✓ Режим полёта: Включено (пропуск серверов Пешек)\n✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ Airplane Mode: Enabled (Bypasses Pawn server checks)\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "System\\airplane_mode" to "true",
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x01000B300B250000L,
            "Warframe",
            "• Бесконечный цикл ожидания подключения к серверам при старте\n• Утечки памяти в реле",
            "• Infinite server login connection loop on title screen\n• Relay hub memory pressure",
            "✓ Режим полёта: Включено (пропуск сетевого ожидания)\n✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено",
            "✓ Airplane Mode: Enabled (Bypasses network connection retry)\n✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled",
            mapOf(
                "System\\airplane_mode" to "true",
                "System\\memory_layout_mode" to "1",
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true"
            )
        ),
        GameFixProfile(
            0x010077001A8D4000L,
            "Brotato",
            "• Просадки FPS ниже 60 при спавне больших волн врагов и сотен пуль\n• Избыточная нагрузка на GPU при точности High",
            "• Framerate drops below 60 during large monster waves and bullet storms\n• Excessive GPU overhead with High GPU accuracy",
            "✓ Точность CPU: Авто / Небезопасная (максимальная скорость для сотен мобов)\n✓ Точность GPU: Обычная (Normal, стабильные 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Реактивная очистка: Отключено",
            "✓ CPU Accuracy: Auto / Unsafe (Maximum CPU throughput for 200+ mobs)\n✓ GPU Accuracy: Normal (Locked 60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Reactive Flushing: Disabled",
            mapOf(
                "Cpu\\cpu_accuracy" to "1",
                "Renderer\\gpu_accuracy" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\use_reactive_flushing" to "false",
                "Renderer\\astc_recompression" to "0"
            )
        ),
        GameFixProfile(
            0x010085C019864000L,
            "Vampire Survivors",
            "• Задержки физики и анимаций при заполнении экрана тысячами монстров",
            "• Physics and animation slowdowns during screen-filling enemy swarms",
            "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Cpu\\cpu_accuracy" to "1",
                "Renderer\\gpu_accuracy" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\use_reactive_flushing" to "false"
            )
        ),
        GameFixProfile(
            0x0100FF500E34A000L,
            "Xenoblade Chronicles: Definitive Edition",
            "• Просадки FPS и утечки VRAM в открытых локациях (Bionis Leg)\n• Микрофризы при подгрузке текстур высокого разрешения",
            "• Framerate drops and VRAM pressure in large open zones (Bionis Leg)\n• Texture streaming micro-stutters",
            "✓ Точность GPU: Обычная (Normal, стабильная производительность)\n✓ Сжатие ASTC: Отключено (чистые текстуры без задержек)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Память: 8GB DRAM",
            "✓ GPU Accuracy: Normal (Smooth performance)\n✓ ASTC Recompression: Uncompressed\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Memory Layout: 8GB DRAM",
            mapOf(
                "Renderer\\gpu_accuracy" to "0",
                "Renderer\\astc_recompression" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "System\\memory_layout_mode" to "1"
            )
        ),
        GameFixProfile(
            0x0100E95004038000L,
            "Xenoblade Chronicles 2",
            "• Размытие динамического разрешения и нехватка VRAM в провинции Гормотт\n• Просадки частоты кадров в битвах",
            "• Dynamic resolution blur and VRAM memory buildup in Gormott\n• Combat framerate drops",
            "✓ Точность GPU: Обычная (Normal)\n✓ Сжатие ASTC: Отключено\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Память: 8GB DRAM",
            "✓ GPU Accuracy: Normal\n✓ ASTC Recompression: Uncompressed\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Memory Layout: 8GB DRAM",
            mapOf(
                "Renderer\\gpu_accuracy" to "0",
                "Renderer\\astc_recompression" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "System\\memory_layout_mode" to "1"
            )
        ),
        GameFixProfile(
            0x01008D3017B4C000L,
            "Super Mario RPG",
            "• Микрофризы при переходе между локациями и в диалогах\n• Сбои глубины теней персонажей",
            "• Micro-stutters during scene transitions and battle dialogs\n• Character shadow depth inaccuracies",
            "✓ Точность GPU: Обычная (Normal)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ GPU Accuracy: Normal\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Renderer\\gpu_accuracy" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x010006701B4B2000L,
            "Princess Peach: Showtime!",
            "• Падение FPS на движке Unreal Engine 4 при трансформациях\n• Мерцание геометрии и теней",
            "• Framerate drops on Unreal Engine 4 during transformations\n• Geometry and shadow flickering",
            "✓ Точность GPU: Обычная (Normal)\n✓ Память: 8GB DRAM\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ GPU Accuracy: Normal\n✓ Memory Layout: 8GB DRAM\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Renderer\\gpu_accuracy" to "0",
                "System\\memory_layout_mode" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100C8901844A000L,
            "Luigi's Mansion 2 HD",
            "• Компиляционные задержки шейдеров в темных комнатах особняка\n• Мерцание динамического освещения фонарика",
            "• Shader compilation stutters in dark mansion corridors\n• Flashlight dynamic lighting flickering",
            "✓ Точность GPU: Обычная (Normal)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Память: 8GB DRAM",
            "✓ GPU Accuracy: Normal\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Memory Layout: 8GB DRAM",
            mapOf(
                "Renderer\\gpu_accuracy" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "System\\memory_layout_mode" to "1"
            )
        ),
        GameFixProfile(
            0x0100B9301A4A0000L,
            "F-Zero 99",
            "• Зависание на стартовом экране при попытке подключения к серверам Nintendo\n• Бесконечная синхронизация онлайна",
            "• Hang on title screen attempting to connect to Nintendo servers\n• Infinite online network sync loop",
            "✓ Режим полёта: Включено (пропуск сетевого зависания)\n✓ Точность CPU: Авто / Небезопасная (Unsafe JIT, 60 FPS)\n✓ Точность GPU: Обычная (Normal)",
            "✓ Airplane Mode: Enabled (Bypasses network connection hang)\n✓ CPU Accuracy: Auto / Unsafe (60 FPS)\n✓ GPU Accuracy: Normal",
            mapOf(
                "System\\airplane_mode" to "true",
                "Cpu\\cpu_accuracy" to "1",
                "Renderer\\gpu_accuracy" to "0",
                "Cpu\\cpuopt_fastmem" to "true"
            )
        ),
        GameFixProfile(
            0x0100427010476000L,
            "Super Mario 3D All-Stars",
            "• Накладные расходы двойной эмуляции в Super Mario Sunshine и Super Mario Galaxy\n• Потрескивания звука при нехватке CPU",
            "• Double-emulation overhead in Super Mario Sunshine and Galaxy\n• Audio crackling from CPU thread starvation",
            "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ CPU Accuracy: Auto / Unsafe (Maximum JIT performance)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Cpu\\cpu_accuracy" to "1",
                "Renderer\\gpu_accuracy" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x01001F5010DFA000L,
            "Pokemon Legends: Arceus",
            "• Микрофризы и просадки FPS при спавне диких Покемонов\n• Утечки памяти в деревне Джубилайф",
            "• Micro-stutters and framerate drops during wild Pokemon spawns\n• Memory pressure in Jubilife Village",
            "✓ Точность GPU: Обычная (Normal, стабильные 30 FPS)\n✓ Память: 8GB DRAM\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ GPU Accuracy: Normal (Smooth 30 FPS)\n✓ Memory Layout: 8GB DRAM\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Renderer\\gpu_accuracy" to "0",
                "System\\memory_layout_mode" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100A3D008C5C000L,
            "Pokemon Scarlet",
            "• Тяжелые утечки памяти в открытом мире Палдеи (>9 ГБ VRAM)\n• Падения FPS и рывки камеры",
            "• Massive open world memory leaks in Paldea (>9 GB VRAM)\n• Framerate drops and camera stuttering",
            "✓ Точность GPU: Обычная (Normal)\n✓ Реактивная очистка: Отключено\n✓ Память: 8GB DRAM\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ GPU Accuracy: Normal\n✓ Reactive Flushing: Disabled\n✓ Memory Layout: 8GB DRAM\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Renderer\\gpu_accuracy" to "0",
                "Renderer\\use_reactive_flushing" to "false",
                "System\\memory_layout_mode" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x01008F6008C5E000L,
            "Pokemon Violet",
            "• Тяжелые утечки памяти в открытом мире Палдеи (>9 ГБ VRAM)\n• Падения FPS и рывки камеры",
            "• Massive open world memory leaks in Paldea (>9 GB VRAM)\n• Framerate drops and camera stuttering",
            "✓ Точность GPU: Обычная (Normal)\n✓ Реактивная очистка: Отключено\n✓ Память: 8GB DRAM\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ GPU Accuracy: Normal\n✓ Reactive Flushing: Disabled\n✓ Memory Layout: 8GB DRAM\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Renderer\\gpu_accuracy" to "0",
                "Renderer\\use_reactive_flushing" to "false",
                "System\\memory_layout_mode" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100D1AB10000000L,
            "Diablo I + Hellfire (DevilutionX)",
            "• Порт DevilutionX для Diablo I и дополнения Hellfire\n• Поддержка MPQ (diabdat, hellfire, hfmonk, ru)\n• Оптимизация 2D Software/Hardware SDL2 рендера",
            "• DevilutionX port for Diablo I and Hellfire expansion\n• MPQ support (diabdat, hellfire, hfmonk, ru)\n• Optimized 2D SDL2 rendering",
            "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Cpu\\cpu_accuracy" to "1",
                "Renderer\\gpu_accuracy" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\use_reactive_flushing" to "false"
            )
        ),
        GameFixProfile(
            0x0100B00B51230000L,
            "Grand Theft Auto V (GTA V Homebrew Port)",
            "• Кастомный Homebrew-порт GTA V для Nintendo Switch\n• Поддержка RPF архивов, аудио и DLC-паков в romfs/\n• Высокое потребление VRAM и тяжелая геометрия Лос-Сантоса",
            "• GTA V Custom Homebrew Switch port\n• RPF archives, audio and DLC packs support in romfs/\n• Heavy Los Santos geometry and VRAM pressure",
            "✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal)\n✓ Сжатие ASTC: Отключено\n✓ Быстрая память (Fastmem): Включено",
            "✓ Memory Layout: 8GB DRAM\n✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal\n✓ ASTC Recompression: Uncompressed\n✓ Fastmem: Enabled",
            mapOf(
                "Core\\memory_layout_mode" to "2",
                "System\\memory_layout_mode" to "2",
                "Cpu\\cpu_accuracy" to "1",
                "Renderer\\gpu_accuracy" to "0",
                "Renderer\\astc_recompression" to "0",
                "Renderer\\use_fast_gpu_time" to "true",
                "Renderer\\gpu_fence_behavior" to "2",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100671000000000L,
            "Grand Theft Auto: San Andreas (re3-sa)",
            "• Порт открытого движка re3-sa для GTA San Andreas\n• Загрузка ресурсов main.scm, models, data, txd, dff\n• Стабильные 60 FPS на открытой карте штата Сан-Андреас",
            "• re3-sa open engine port for GTA San Andreas\n• main.scm, models, data, txd, dff assets loading\n• Locked 60 FPS across San Andreas",
            "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Cpu\\cpu_accuracy" to "1",
                "Renderer\\gpu_accuracy" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100672000000000L,
            "Grand Theft Auto: Vice City (reVC)",
            "• Порт открытого движка reVC для GTA Vice City\n• Загрузка ресурсов gxt, audio, txd, dff\n• Стабильные 60 FPS в Вайс-Сити",
            "• reVC open engine port for GTA Vice City\n• gxt, audio, txd, dff assets loading\n• Locked 60 FPS in Vice City",
            "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Cpu\\cpu_accuracy" to "1",
                "Renderer\\gpu_accuracy" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100673000000000L,
            "Grand Theft Auto III (re3)",
            "• Порт открытого движка re3 для GTA III\n• Загрузка ресурсов Либерти-Сити, коллизий и текстур\n• Стабильные 60 FPS",
            "• re3 open engine port for GTA III\n• Liberty City collision and texture streaming\n• Locked 60 FPS",
            "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Cpu\\cpu_accuracy" to "1",
                "Renderer\\gpu_accuracy" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100000000000018L,
            "Half-Life 1 / Black Mesa Classic (Xash3D FWGS)",
            "• Движок Xash3D FWGS для Half-Life 1, Blue Shift, Opposing Force, Counter-Strike\n• Загрузка PAK/WAD ресурсов и GoldSrc карт\n• Максимальная производительность и отзывчивость управления",
            "• Xash3D FWGS engine for Half-Life 1, Blue Shift, Opposing Force, CS\n• PAK/WAD resources and GoldSrc maps loading\n• Maximum responsiveness and framerate",
            "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Cpu\\cpu_accuracy" to "1",
                "Renderer\\gpu_accuracy" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100D00100000000L,
            "DOOM 1, 2, Final, Plutonia, TNT (GZDoom / PrBoom / Crispy)",
            "• Порты GZDoom, PrBoom, Crispy Doom для классической серии DOOM\n• Поддержка WAD файлов, DeHackEd патчей и модов Sigil/Plutonia/TNT\n• 60 FPS с чистым виброоткликом",
            "• GZDoom, PrBoom, Crispy Doom ports for classic DOOM series\n• WAD files, DeHackEd patches and Sigil/Plutonia/TNT mods\n• Smooth 60 FPS with rumble",
            "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Cpu\\cpu_accuracy" to "1",
                "Renderer\\gpu_accuracy" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100D00300000000L,
            "DOOM 3 (dhewm3 Switch)",
            "• Порт dhewm3 (id Tech 4) для DOOM 3 и Resurrection of Evil\n• Поддержка динамического освещения и попиксельных теней id Tech 4\n• 8 ГБ DRAM для кэширования PK4",
            "• dhewm3 (id Tech 4) port for DOOM 3 and Resurrection of Evil\n• id Tech 4 dynamic lighting and per-pixel shadow rendering\n• 8GB DRAM for PK4 caching",
            "✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено",
            "✓ Memory Layout: 8GB DRAM\n✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "1",
                "Cpu\\cpu_accuracy" to "1",
                "Renderer\\gpu_accuracy" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100000000000071L,
            "Quake I / Quake II / Quake III (Quakespasm / Yamagi / ioquake3)",
            "• Порты движков Quakespasm (Quake 1), Yamagi Quake II, ioquake3\n• Загрузка PAK и PK3 файлов, GL-рендеринг\n• Идеальные 60 FPS с гироскопом",
            "• Quakespasm (Quake 1), Yamagi Quake II, ioquake3 ports\n• PAK and PK3 loading, hardware GL rendering\n• Locked 60 FPS with gyro support",
            "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Cpu\\cpu_accuracy" to "1",
                "Renderer\\gpu_accuracy" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x010000000000003DL,
            "Duke Nukem 3D / Shadow Warrior / Blood (EDuke32 / VoidSW / NBlood)",
            "• Порты Build Engine (EDuke32, VoidSW, NBlood) для культовых 2.5D шутеров\n• Загрузка GRP и RFF ресурсов, вокселей и полигонального рендера\n• Стабильные 60 FPS",
            "• Build Engine ports (EDuke32, VoidSW, NBlood) for classic 2.5D shooters\n• GRP and RFF assets, voxels and polygonal render\n• Locked 60 FPS",
            "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Cpu\\cpu_accuracy" to "1",
                "Renderer\\gpu_accuracy" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100000000000077L,
            "Return to Castle Wolfenstein / Wolf3D (iortcw / ECWolf)",
            "• Порты движков id Tech 3 (iortcw) и ECWolf для Wolfenstein 3D и RtCW\n• Поддержка PK3 и WL6 карт\n• 60 FPS с аппаратным освещением",
            "• id Tech 3 (iortcw) and ECWolf ports for Wolfenstein 3D and RtCW\n• PK3 and WL6 map support\n• Smooth 60 FPS with hardware lighting",
            "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Cpu\\cpu_accuracy" to "1",
                "Renderer\\gpu_accuracy" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x01000000000000F1L,
            "Fallout 1 & Fallout 2 (fallout-ce / fallout2-ce)",
            "• Порты Community Edition для Fallout 1 и Fallout 2\n• Загрузка master.dat, critter.dat и кастомных шрифтов\n• Стабильная работа мыши/тача и 60 FPS",
            "• Community Edition ports for Fallout 1 and Fallout 2\n• master.dat, critter.dat and custom fonts loading\n• Smooth touch/mouse input and 60 FPS",
            "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Cpu\\cpu_accuracy" to "1",
                "Renderer\\gpu_accuracy" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100000000000030L,
            "Heroes of Might and Magic II & III (fheroes2 / VCMI Switch)",
            "• Порты движков fheroes2 и VCMI для Героев Меча и Магии II и III (In the Wake of Gods / Horn of the Abyss)\n• Загрузка LOD, AGG, SND ресурсов и модов",
            "• fheroes2 and VCMI ports for Heroes of Might and Magic II and III (WoG / HotA)\n• LOD, AGG, SND assets and mods loading",
            "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Cpu\\cpu_accuracy" to "1",
                "Renderer\\gpu_accuracy" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x010000000000004DL,
            "Max Payne 1 (Max Payne Switch)",
            "• Нативный порт Max Payne для Nintendo Switch\n• Загрузка архивов RAS (x_data.ras, x_russian.ras)\n• Плавный Bullet-Time и 60 FPS",
            "• Native Max Payne port for Nintendo Switch\n• RAS archives loading (x_data.ras, x_english.ras)\n• Smooth Bullet-Time and locked 60 FPS",
            "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Cpu\\cpu_accuracy" to "1",
                "Renderer\\gpu_accuracy" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x010000000000006DL,
            "The Elder Scrolls III: Morrowind (OpenMW Switch)",
            "• Порт современного 3D-движка OpenMW для TES III: Morrowind\n• Загрузка ESM, BSA, текстурных паков и модов\n• 8 ГБ DRAM для открытого мира Вварденфелла",
            "• OpenMW modern 3D engine port for TES III: Morrowind\n• ESM, BSA, texture packs and mods loading\n• 8GB DRAM for Vvardenfell open world",
            "✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal)\n✓ Быстрая память (Fastmem): Включено",
            "✓ Memory Layout: 8GB DRAM\n✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal\n✓ Fastmem: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "1",
                "Cpu\\cpu_accuracy" to "1",
                "Renderer\\gpu_accuracy" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x010000000000004AL,
            "Star Wars: Jedi Outcast & Academy (OpenJK Switch)",
            "• Порт OpenJK для Star Wars Jedi Knight II: Jedi Outcast и Jedi Academy\n• Стабильные 60 FPS в дуэлях на световых мечах",
            "• OpenJK port for Star Wars Jedi Knight II: Jedi Outcast and Academy\n• Smooth 60 FPS in lightsaber duels",
            "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Cpu\\cpu_accuracy" to "1",
                "Renderer\\gpu_accuracy" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100000000000053L,
            "S.T.A.L.K.E.R.: Shadow of Chernobyl (OpenXRay Switch)",
            "• Порт движка X-Ray 1.6 (OpenXRay) для S.T.A.L.K.E.R.: Тень Чернобыля\n• Загрузка DB-архивов и gamedata\n• 8 ГБ DRAM для локаций Зоны",
            "• OpenXRay engine port for S.T.A.L.K.E.R.: Shadow of Chernobyl\n• DB archives and gamedata loading\n• 8GB DRAM for Zone locations",
            "✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal)\n✓ Быстрая память (Fastmem): Включено",
            "✓ Memory Layout: 8GB DRAM\n✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal\n✓ Fastmem: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "1",
                "Cpu\\cpu_accuracy" to "1",
                "Renderer\\gpu_accuracy" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x010000000000005CL,
            "Sonic CD / Sonic 1 / Sonic 2 / Mania (RSDK Decompilation)",
            "• Порты Retro Engine (RSDKv3, RSDKv4, RSDKv5) для классических игр Sonic\n• Загрузка Data.rsdk и модов\n• Идеальные 60 FPS",
            "• Retro Engine (RSDKv3, RSDKv4, RSDKv5) decompilations for classic Sonic games\n• Data.rsdk and mods loading\n• Flawless 60 FPS",
            "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Cpu\\cpu_accuracy" to "1",
                "Renderer\\gpu_accuracy" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100000000000064L,
            "Super Mario 64 (SM64-NX / Render96)",
            "• Порт SM64-NX и Render96 для Super Mario 64 с широкоформатным рендером и HD-моделями\n• Стабильные 60 FPS",
            "• SM64-NX and Render96 port for Super Mario 64 with widescreen and HD models\n• Locked 60 FPS",
            "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Cpu\\cpu_accuracy" to "1",
                "Renderer\\gpu_accuracy" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100000000000074L,
            "Zelda: Ocarina of Time / Majora's Mask (Ship of Harkinian / 2S2H)",
            "• Порты Ship of Harkinian (OoT) и 2 Ship 2 Harkinian (MM)\n• Загрузка OTR архивов, поддержка 60 FPS, свободная камера и HD текстуры",
            "• Ship of Harkinian (OoT) and 2 Ship 2 Harkinian (MM) ports\n• OTR archives loading, 60 FPS support, free camera and HD textures",
            "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Cpu\\cpu_accuracy" to "1",
                "Renderer\\gpu_accuracy" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100000000000022L,
            "Cave Story / Cave Story+ / AM2R (NXEngine-evo / AM2R)",
            "• Порты NXEngine-evo для Cave Story и нативный порт AM2R (Another Metroid 2 Remake)\n• Чистый 2D-рендеринг и 60 FPS",
            "• NXEngine-evo ports for Cave Story and native AM2R port\n• Crisp 2D rendering and 60 FPS",
            "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Cpu\\cpu_accuracy" to "1",
                "Renderer\\gpu_accuracy" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100000000000068L,
            "Theme Hospital / Caesar III (CorsixTH / Julius / Augustus)",
            "• Порты CorsixTH (Theme Hospital) и Julius/Augustus (Caesar III)\n• Высокое разрешение и сенсорное управление",
            "• CorsixTH (Theme Hospital) and Julius/Augustus (Caesar III) ports\n• High resolution and touch controls",
            "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Cpu\\cpu_accuracy" to "1",
                "Renderer\\gpu_accuracy" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100000000000091L,
            "Command & Conquer / Red Alert / Dune II (OpenRA / Dune Legacy)",
            "• Порты OpenRA (C&C, Red Alert, Dune 2000) и Dune Legacy (Dune II)\n• Быстрый расчет AI юнитов на Unsafe JIT",
            "• OpenRA (C&C, Red Alert, Dune 2000) and Dune Legacy (Dune II) ports\n• Fast RTS AI unit simulation on Unsafe JIT",
            "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Cpu\\cpu_accuracy" to "1",
                "Renderer\\gpu_accuracy" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100000000001000L,
            "RetroArch & Emulators (PPSSPP, Flycast, ScummVM, MelonDS, mGBA, DuckStation)",
            "• Мультисистемные эмуляторы и оболочки для запуска ретро-платформ на Switch\n• 8 ГБ DRAM для тяжелых ядер (Flycast, PPSSPP, DuckStation)\n• Максимальная скорость JIT компиляции",
            "• Multi-system emulators and frontends for retro platforms\n• 8GB DRAM for memory-heavy cores (Flycast, PPSSPP, DuckStation)\n• Maximum JIT compiler performance",
            "✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено",
            "✓ Memory Layout: 8GB DRAM\n✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "1",
                "Cpu\\cpu_accuracy" to "1",
                "Renderer\\gpu_accuracy" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100000000003000L,
            "Homebrew Utilities & Overlays (NX-Shell, DBI, Goldleaf, JKSV, Checkpoint, EdiZon, Tesla)",
            "• Системные Homebrew-утилиты Switch для управления сейвами, файлами и оверлеями\n• Мгновенный отклик файловой системы и стабильная работа",
            "• Switch Homebrew system utilities for saves, files and overlays\n• Instant filesystem responsiveness and stability",
            "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Cpu\\cpu_accuracy" to "1",
                "Renderer\\gpu_accuracy" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100000000000055L,
            "Grand Theft Auto V (GTA V Homebrew Port / PC Wrapper)",
            "• Высокие требования к памяти потоков и текстурам открытого мира Лос-Сантоса\n• Зависание сетевых сокетов и многопоточных вызовов IPC",
            "• High thread memory pressure and Los Santos open-world texture bandwidth\n• Network socket handshake and multithreaded IPC freezes",
            "✓ Память: 8GB DRAM (критично для стабильной работы открытого мира)\n✓ Точность CPU: Авто / Небезопасная (Unsafe JIT, максимальный FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Режим полёта: Включено (пропуск сетевых хуков)\n✓ Авто-заглушки: Включено",
            "✓ Memory Layout: 8GB DRAM (Critical for open-world stability)\n✓ CPU Accuracy: Auto / Unsafe (Max FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Airplane Mode: Enabled\n✓ Auto Stub: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "2",
                "Cpu\\cpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\gpu_accuracy" to "0",
                "Renderer\\astc_recompression" to "0",
                "System\\airplane_mode" to "true",
                "Debugging\\use_auto_stub" to "true"
            )
        ),
        GameFixProfile(
            0x0100000000000034L,
            "Grand Theft Auto: The Trilogy / GTA III / VC / SA (re3 / reVC / Homebrew Ports)",
            "• Просадки частоты кадров при рендере геометрии города\n• Рассинхронизация аудио-потоков радио",
            "• City geometry rendering framerate dips\n• Radio station audio stream desynchronization",
            "✓ Конфигурация памяти: 6 ГБ DRAM\n✓ Точность CPU: Авто / Unsafe JIT\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ Memory Layout: 6GB DRAM\n✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "1",
                "Cpu\\cpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\gpu_accuracy" to "0",
                "Renderer\\astc_recompression" to "0"
            )
        ),
        GameFixProfile(
            0x01002EF01A316000L,
            "Brotato",
            "• Просадки FPS при спавне волн врагов\n• Микрофризы расчёта физики снарядов",
            "• Framerate drops during massive horde waves\n• Projectile physics calculation micro-stutters",
            "✓ Конфигурация памяти: 6 ГБ DRAM\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Точность GPU: Обычная (Normal)",
            "✓ Memory Layout: 6GB DRAM\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ GPU Accuracy: Normal",
            mapOf(
                "System\\memory_layout_mode" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\gpu_accuracy" to "0"
            )
        ),
        GameFixProfile(
            0x010089A0197E4000L,
            "Vampire Survivors",
            "• Микрофризы и просадки кадров на 25+ минуте при тысячах спрайтов на экране\n• Утечки памяти движка Phaser",
            "• Micro-stutters and framerate drops at 25+ min with thousands of sprites\n• Phaser engine memory leaks",
            "✓ Конфигурация памяти: 6 ГБ DRAM\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Точность GPU: Обычная (Normal)",
            "✓ Memory Layout: 6GB DRAM\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ GPU Accuracy: Normal",
            mapOf(
                "System\\memory_layout_mode" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\gpu_accuracy" to "0"
            )
        ),
        GameFixProfile(
            0x010097F018538000L,
            "Dave the Diver",
            "• Просадки кадровой частоты во время ночной охоты и шторма\n• Утечки VRAM в суши-баре Bancho Sushi",
            "• Framerate drops during stormy dives and night hunting\n• VRAM memory spikes in Bancho Sushi restaurant",
            "✓ Конфигурация памяти: 6 ГБ DRAM\n✓ Точность GPU: Высокая (High)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ Memory Layout: 6GB DRAM\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "1",
                "Renderer\\gpu_accuracy" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x010093801237C000L,
            "Metroid Dread",
            "• Микрофризы при входе в зоны E.M.M.I.\n• Сбои размытия в катсценах и шейдеров тепловизора",
            "• E.M.M.I. zone transition micro-stutters\n• Cutscene motion blur and thermal vision shader glitches",
            "✓ Точность GPU: Высокая (High)\n✓ Асинхронные шейдеры: Включено\n✓ Быстрая память (Fastmem): Включено",
            "✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Cpu\\cpuopt_fastmem" to "true"
            )
        ),
        GameFixProfile(
            0x010042D00D900000L,
            "LEGO Star Wars: The Skywalker Saga",
            "• Падение FPS на открытых планетах (Корусант, Татуин)\n• Утечки видеопамяти при смене планет",
            "• Open-world planet performance drops (Coruscant, Tatooine)\n• Hyperdrive transition VRAM spikes",
            "✓ Память: 8GB DRAM\n✓ Точность GPU: Обычная (Normal)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: Normal\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "2",
                "Renderer\\gpu_accuracy" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100307018934000L,
            "Signalis",
            "• Сбои кинематографичных ретро-шейдеров ЭЛТ и дизеринга\n• Зависание инвентаря",
            "• CRT retro-filter and dithering shader glitches\n• Inventory UI freeze",
            "✓ Точность GPU: Высокая (High)\n✓ Асинхронные шейдеры: Включено\n✓ Быстрая память (Fastmem): Включено",
            "✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Cpu\\cpuopt_fastmem" to "true"
            )
        ),
        GameFixProfile(
            0x0100EC9010258000L,
            "Streets of Rage 4",
            "• Рассинхронизация кадров в битвах с боссами\n• Разрывы спрайтовой анимации",
            "• Boss fight frame pacing desync\n• Sprite animation tearing",
            "✓ Точность GPU: Высокая (High)\n✓ Асинхронные шейдеры: Включено\n✓ Быстрое время GPU: Включено\n✓ Быстрая память (Fastmem): Включено\n✓ Игнорировать прерывания памяти: Включено",
            "✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled\n✓ Fast GPU Time: Enabled\n✓ Fastmem: Enabled\n✓ Ignore Memory Aborts: Enabled",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\use_fast_gpu_time" to "true",
                "Cpu\\cpuopt_fastmem" to "true",
                "Cpu\\cpuopt_ignore_memory_aborts" to "true"
            )
        ),
        GameFixProfile(
            0x010085800E33E000L,
            "Streets of Rage 4",
            "• Рассинхронизация кадров в битвах с боссами\n• Разрывы спрайтовой анимации",
            "• Boss fight frame pacing desync\n• Sprite animation tearing",
            "✓ Точность GPU: Высокая (High)\n✓ Асинхронные шейдеры: Включено\n✓ Быстрое время GPU: Включено\n✓ Быстрая память (Fastmem): Включено\n✓ Игнорировать прерывания памяти: Включено",
            "✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled\n✓ Fast GPU Time: Enabled\n✓ Fastmem: Enabled\n✓ Ignore Memory Aborts: Enabled",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\use_fast_gpu_time" to "true",
                "Cpu\\cpuopt_fastmem" to "true",
                "Cpu\\cpuopt_ignore_memory_aborts" to "true"
            )
        ),
        GameFixProfile(
            0x0100BA700E340000L,
            "Streets of Rage 4",
            "• Рассинхронизация кадров в битвах с боссами\n• Разрывы спрайтовой анимации",
            "• Boss fight frame pacing desync\n• Sprite animation tearing",
            "✓ Точность GPU: Высокая (High)\n✓ Асинхронные шейдеры: Включено\n✓ Быстрое время GPU: Включено\n✓ Быстрая память (Fastmem): Включено\n✓ Игнорировать прерывания памяти: Включено",
            "✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled\n✓ Fast GPU Time: Enabled\n✓ Fastmem: Enabled\n✓ Ignore Memory Aborts: Enabled",
            mapOf(
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\use_fast_gpu_time" to "true",
                "Cpu\\cpuopt_fastmem" to "true",
                "Cpu\\cpuopt_ignore_memory_aborts" to "true"
            )
        ),
        GameFixProfile(
            0x0100E65002BB8000L,
            "Stardew Valley",
            "• Микрофризы при смене дней и сохранении на ферме\n• Просадки FPS во время дождя и фестивалей",
            "• Day transition and farm autosave micro-stutters\n• Rain particles and festival FPS drops",
            "✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Точность GPU: Обычная (Normal)",
            "✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ GPU Accuracy: Normal",
            mapOf(
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\gpu_accuracy" to "0"
            )
        ),
        GameFixProfile(
            0x01002FC00412C000L,
            "Little Nightmares: Complete Edition",
            "• Вылеты на движке Unreal Engine 4 в Чреве\n• Сбои динамических теней фонарика",
            "• Unreal Engine 4 Maw transition crashes\n• Flashlight dynamic shadow artifacts",
            "✓ Конфигурация памяти: 6 ГБ DRAM\n✓ Точность GPU: Высокая (High)\n✓ Динамическое состояние: Базовое (EDS1)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ Memory Layout: 6GB DRAM\n✓ GPU Accuracy: High\n✓ Dynamic State: Basic (EDS1)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "1",
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\dyna_state" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x010097100EDD6000L,
            "Little Nightmares II",
            "• Вылеты из-за нехватки памяти (OOM) в Бледном городе\n• Артефакты тумана и объемного света",
            "• Pale City memory pressure (OOM) crashes\n• Volumetric fog and lighting artifacts",
            "✓ Память: 8GB DRAM\n✓ Точность GPU: Высокая (High)\n✓ Динамическое состояние: Базовое (EDS1)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Dynamic State: Basic (EDS1)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "2",
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\dyna_state" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x010066101A55A000L,
            "Little Nightmares III",
            "• Высокие требования к DRAM и шейдерам спирали\n• Сбои многопоточности Unreal Engine 5",
            "• High DRAM and Spiral shader complexity\n• Unreal Engine 5 multithreading synchronization",
            "✓ Память: 8GB DRAM\n✓ Точность GPU: Высокая (High)\n✓ Динамическое состояние: Базовое (EDS1)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Dynamic State: Basic (EDS1)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "2",
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\dyna_state" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x01000B900D8B0000L,
            "Cadence of Hyrule: Crypt of the NecroDancer",
            "• Рассинхронизация ритмического аудио-движка\n• Задержка обработки ввода стрелок",
            "• Rhythm audio engine timing desynchronization\n• Beat input delay",
            "✓ Аудио-движок: Cubeb\n✓ Точность CPU: Точная (Accurate)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ Audio Engine: Cubeb\n✓ CPU Accuracy: Accurate\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Cpu\\cpu_accuracy" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100CEA007D08000L,
            "Crypt of the NecroDancer: Nintendo Switch Edition",
            "• Рассинхронизация такта ударов и музыки в подземелье\n• Задержка аудио-буфера",
            "• Beat synchronization jitter in procedural dungeons\n• Audio buffer latency",
            "✓ Аудио-движок: Cubeb\n✓ Точность CPU: Точная (Accurate)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ Audio Engine: Cubeb\n✓ CPU Accuracy: Accurate\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Cpu\\cpu_accuracy" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100BDA01AABC000L,
            "Rift of the NecroDancer",
            "• Рассинхронизация дорожек ритм-битв в мини-играх\n• Инпут-лаг комбо",
            "• Rhythm lane timing desync during minigames\n• Combo input response latency",
            "✓ Аудио-движок: Cubeb\n✓ Точность CPU: Точная (Accurate)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ Audio Engine: Cubeb\n✓ CPU Accuracy: Accurate\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
            mapOf(
                "Cpu\\cpu_accuracy" to "0",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true"
            )
        ),
        GameFixProfile(
            0x0100D59022590000L,
            "Scott Pilgrim vs. The World: The Game - Complete Edition",
            "• Зависание на заставке Ubisoft Connect\n• Рассинхронизация спрайтов 4 игроков",
            "• Ubisoft Connect handshake freeze on boot\n• 4-player sprite synchronization jitter",
            "✓ Режим полёта: Включено (пропуск сетевого опроса Ubisoft)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Точность GPU: Высокая (High)",
            "✓ Airplane Mode: Enabled (Bypasses Ubisoft Connect)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ GPU Accuracy: High",
            mapOf(
                "System\\airplane_mode" to "true",
                "Cpu\\cpuopt_fastmem" to "true",
                "Renderer\\use_asynchronous_shaders" to "true",
                "Renderer\\gpu_accuracy" to "1"
            )
        ),
        GameFixProfile(
            0x010094D023A28000L,
            "Drill Core",
            "• Зависание при процедурной генерации буровых платформ\n• Утечки памяти при спавне сотен монстров",
            "• Procedural platform generation freeze\n• Swarm particle memory buildup",
            "✓ Конфигурация памяти: 6 ГБ DRAM\n✓ Точность GPU: Высокая (High)\n✓ Динамическое состояние: Базовое (EDS1)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
            "✓ Memory Layout: 6GB DRAM\n✓ GPU Accuracy: High\n✓ Dynamic State: Basic (EDS1)\n✓ Fastmem: Enabled\n✓ Auto Stub: Enabled",
            mapOf(
                "System\\memory_layout_mode" to "1",
                "Renderer\\gpu_accuracy" to "1",
                "Renderer\\dyna_state" to "1",
                "Cpu\\cpuopt_fastmem" to "true",
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
                nameLower.contains("disco elysium") -> listOf("disco elysium", "elysium", "0100c88011246000", "01008c300f7f0000", "0100e26014466000")
                nameLower.contains("xenoblade") -> listOf("xenoblade", "0100ff500e34a000", "0100e95004038000", "010074f013262000")
                nameLower.contains("witcher") -> listOf("witcher", "wild hunt", "0100e67012924000", "01003d100e9c6000")
                nameLower.contains("arkham city") -> listOf("arkham city", "01003ae017db0000")
                nameLower.contains("breath of the wild") -> listOf("breath of the wild", "botw", "01007ef00011e000")
                nameLower.contains("tears of the kingdom") -> listOf("tears of the kingdom", "totk", "0100f2c0115b6000")
                nameLower.contains("paper mario") -> listOf("paper mario", "thousand-year", "01004d701742a000")
                nameLower.contains("hogwarts legacy") -> listOf("hogwarts", "0100b5b0112f8000", "0100d1801648e000")
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
                nameLower.contains("animal well") -> listOf("animal well", "010020d01ad24000", "010092c01d9f8000", "0100c9e01b854000")
                nameLower.contains("mortal kombat 1") || nameLower.contains("mk1") -> listOf("mortal kombat 1", "mk1", "01006560184e6000", "0100d2800d5c2000", "010066b019e0e000")
                nameLower.contains("mortal kombat") -> listOf("mortal kombat", "mk11", "0100b1100c4d0000")
                nameLower.contains("hot pursuit") || nameLower.contains("need for speed") -> listOf("need for speed", "hot pursuit", "nfs", "010074600ee26000", "0100b9000d000000")
                nameLower.contains("kingdom battle") -> listOf("kingdom battle", "mario + rabbids", "mario rabbids", "010067300059a000")
                nameLower.contains("sparks of hope") -> listOf("sparks of hope", "01005ca00f966000")
                nameLower.contains("splintered fate") || nameLower.contains("tmnt") -> listOf("splintered fate", "tmnt", "ninja turtles", "01005cf01e784000")
                nameLower.contains("mario odyssey") -> listOf("odyssey", "mario odyssey", "0100000000010000")
                nameLower.contains("wonder") -> listOf("wonder", "mario wonder", "010015100b5b4000")
                nameLower.contains("metroid prime") -> listOf("metroid prime", "remastered", "0100121014688000")
                nameLower.contains("animal crossing") -> listOf("animal crossing", "horizons", "01006f8002326000")
                nameLower.contains("persona 5") -> listOf("persona 5", "p5r", "01000a10041ea000")
                nameLower.contains("smash bros") -> listOf("smash bros", "ssbu", "01006a800016e000")
                nameLower.contains("fc 24") || nameLower.contains("fifa") -> listOf("ea sports fc", "fc 24", "fifa", "0100a38018d5a000", "0100346016ee8000")
                nameLower.contains("sonic frontiers") -> listOf("sonic frontiers", "frontiers", "01004c90141a4000")
                nameLower.contains("arceus") -> listOf("legends: arceus", "arceus", "01001f5010dfa000")
                nameLower.contains("sword") || nameLower.contains("shield") -> listOf("sword", "shield", "0100abf008968000", "01008db008c2c000")
                nameLower.contains("kirby") -> listOf("kirby", "forgotten land", "01004d300c5ae000")
                nameLower.contains("three houses") -> listOf("three houses", "010055d009f78000")
                nameLower.contains("engage") -> listOf("engage", "0100a6301214e000")
                nameLower.contains("astral chain") -> listOf("astral chain", "01007300020fa000")
                nameLower.contains("dragon quest") -> listOf("dragon quest", "dq xi", "dq11", "01006c300e9f0000")
                nameLower.contains("triangle strategy") -> listOf("triangle strategy", "0100cc80140f8000")
                nameLower.contains("red dead") -> listOf("red dead", "rdr", "01007820195a6000")
                nameLower.contains("crash") -> listOf("crash bandicoot", "crash 4", "010071700f8ba000")
                nameLower.contains("spyro") -> listOf("spyro", "reignited", "0100b6e00b360000")
                nameLower.contains("monster hunter") -> listOf("monster hunter", "mhgu", "gen ultimate", "0100770008dd8000", "0100b04011742000")
                nameLower.contains("crisis core") -> listOf("crisis core", "ffvii reunion", "010034501659e000")
                nameLower.contains("pikmin") -> listOf("pikmin", "010069200e60e000")
                nameLower.contains("peach") -> listOf("princess peach", "showtime", "0100fa701a7ca000")
                nameLower.contains("hollow knight") -> listOf("hollow knight", "01000bc0000a0000")
                nameLower.contains("hades") -> listOf("hades", "0100735010b46000")
                nameLower.contains("dead cells") -> listOf("dead cells", "01003d200baa2000")
                nameLower.contains("outer wilds") -> listOf("outer wilds", "01001b60133a2000")
                nameLower.contains("subnautica") -> listOf("subnautica", "010067600df2a000")
                nameLower.contains("alien") -> listOf("alien", "isolation", "01000cd00de1e000")
                nameLower.contains("wisps") || nameLower.contains("ori") -> listOf("ori", "wisps", "01009850119bc000")
                nameLower.contains("sea of stars") -> listOf("sea of stars", "010041b01211e000")
                nameLower.contains("octopath") -> listOf("octopath", "0100656017e9c000")
                nameLower.contains("bravely") -> listOf("bravely default", "010052c00bab8000")
                nameLower.contains("mario rpg") -> listOf("mario rpg", "0100bc0018138000")
                nameLower.contains("metroid dread") -> listOf("metroid dread", "dread", "01005af00ba7a000")
                nameLower.contains("tropical freeze") -> listOf("tropical freeze", "donkey kong", "010077200889e000")
                nameLower.contains("dream land") -> listOf("dream land", "0100483017770000")
                nameLower.contains("captain toad") -> listOf("captain toad", "treasure tracker", "010085c0084fa000")
                nameLower.contains("crafted world") -> listOf("crafted world", "yoshi", "01006000040c2000")
                nameLower.contains("strikers") -> listOf("strikers", "battle league", "010019401051c000")
                nameLower.contains("bomberman") -> listOf("bomberman", "0100e2b017dae000")
                nameLower.contains("advance wars") -> listOf("advance wars", "re-boot", "01002390146ac000")
                nameLower.contains("warioware") -> listOf("warioware", "move it", "01002d801a1de000")
                nameLower.contains("no man's sky") -> listOf("no man's sky", "no man", "nms", "0100d6f015f70000")
                nameLower.contains("portal") -> listOf("portal", "01000bd016550000")
                nameLower.contains("persona 4") -> listOf("persona 4", "p4g", "010058c01570e000")
                nameLower.contains("persona 3") -> listOf("persona 3", "p3p", "010072001570a000")
                nameLower.contains("tunic") -> listOf("tunic", "01004c80170a6000")
                nameLower.contains("dave the diver") -> listOf("dave the diver", "dave_the_diver", "0100ddf01a03a000")
                nameLower.contains("cult of the lamb") -> listOf("cult of the lamb", "cult_of_the_lamb", "01002be01584e000")
                nameLower.contains("unicorn overlord") -> listOf("unicorn overlord", "unicorn_overlord", "0100913018f08000")
                nameLower.contains("skyward sword") -> listOf("skyward sword", "skyward_sword", "01002da013484000")
                nameLower.contains("bowser's fury") -> listOf("bowser", "3d world", "010028600ebda000")
                nameLower.contains("jamboree") -> listOf("jamboree", "010036601d380000")
                nameLower.contains("brothership") -> listOf("brothership", "mario & luigi", "010091801e8b2000")
                nameLower.contains("splatoon 2") -> listOf("splatoon 2", "01003c700009c000")
                nameLower.contains("splatoon 3") -> listOf("splatoon 3", "0100c2500fc20000")
                nameLower.contains("origami king") -> listOf("origami", "01004d300c5ae000")
                nameLower.contains("age of calamity") -> listOf("calamity", "01002b00111a2000")
                nameLower.contains("cuphead") -> listOf("cuphead", "010065b00ae8e000")
                nameLower.contains("blind forest") -> listOf("blind forest", "0100d5700dc34000")
                nameLower.contains("slay the spire") -> listOf("slay the spire", "slay_the_spire", "0100216008e88000")
                nameLower.contains("into the breach") -> listOf("into the breach", "into_the_breach", "01008c300b4d2000")
                nameLower.contains("tales of") -> listOf("tales", "berseria", "symphonia", "vesperia", "graces", "0100c4d01d026000", "010049401732a000", "010060900b976000")
                nameLower.contains("astral chain") -> listOf("astral chain", "astral_chain", "01007300020fa000")
                nameLower.contains("three houses") -> listOf("three houses", "fe3h", "01005fa00bafa000")
                nameLower.contains("smash bros") -> listOf("smash", "ssbu", "01006a800016e000")
                nameLower.contains("dragon quest") -> listOf("dragon quest", "dq11", "01004c00000ce000")
                nameLower.contains("nier") -> listOf("nier", "automata", "01007d401662a000")
                nameLower.contains("bayonetta") -> listOf("bayonetta", "0100778006e88000", "01007a5004656000")
                nameLower.contains("persona 5") -> listOf("persona 5", "p5r", "01005ca0157ca000")
                nameLower.contains("live a live") -> listOf("live a live", "live_a_live", "0100780016140000")
                nameLower.contains("tony hawk") -> listOf("tony hawk", "thps", "01000bd00e756000")
                nameLower.contains("nitro-fueled") -> listOf("nitro", "ctr", "0100f7a00b704000")
                nameLower.contains("mortal kombat 1") || nameLower.contains("mk1") -> listOf("mortal kombat 1", "mk1", "0100d2800d5c2000")
                nameLower.contains("mortal kombat") -> listOf("mortal kombat", "mk11", "0100b1100c4d0000")
                nameLower.contains("gothic") -> listOf("gothic", "010041201a5ec000", "01007e101bb46000")
                nameLower.contains("prince of persia") || nameLower.contains("lost crown") -> listOf("prince of persia", "lost crown", "01008c1019972000")
                nameLower.contains("echoes of wisdom") -> listOf("echoes of wisdom", "wisdom", "01008cf01ba04000")
                nameLower.contains("luigi's mansion 3") || nameLower.contains("lm3") -> listOf("luigi's mansion 3", "lm3", "0100dca0064a6000")
                nameLower.contains("no man's sky") || nameLower.contains("nms") -> listOf("no man's sky", "nms", "0100de801648e000")
                nameLower.contains("batman") || nameLower.contains("arkham") -> listOf("batman", "arkham", "010039b0182da000")
                nameLower.contains("kingdom come") || nameLower.contains("kcd") -> listOf("kingdom come", "kcd", "010062c015792000")
                nameLower.contains("tomb raider") -> listOf("tomb raider", "0100c4d018a0e000")
                nameLower.contains("borderlands") -> listOf("borderlands", "01007e300b70c000")
                nameLower.contains("demon slayer") -> listOf("demon slayer", "hinokami", "01005e4017c7a000")
                nameLower.contains("gta v") || nameLower.contains("gta 5") || nameLower.contains("gtav") -> listOf("gta v", "gta 5", "grand theft auto v", "0100000000000055")
                nameLower.contains("gta") || nameLower.contains("grand theft auto") || nameLower.contains("re3") || nameLower.contains("revc") -> listOf("gta", "grand theft auto", "san andreas", "vice city", "re3", "revc", "0100000000000034")
                nameLower.contains("monster hunter") -> listOf("monster hunter", "mhr", "sunbreak", "0100830007780000")
                nameLower.contains("burnout paradise") -> listOf("burnout", "paradise", "010052900fa62000")
                nameLower.contains("overcooked") -> listOf("overcooked", "0100650012270000")
                nameLower.contains("neighborville") -> listOf("neighborville", "pvz", "01001fb012014000")
                nameLower.contains("risk of rain") -> listOf("risk of rain", "ror2", "0100d5600de44000")
                nameLower.contains("minecraft dungeons") -> listOf("dungeons", "mcd", "0100e2600ae5c000")
                nameLower.contains("dragon ball fighterz") -> listOf("fighterz", "dbfz", "01006b4009b74000")
                nameLower.contains("xenoverse 2") -> listOf("xenoverse", "dbxv2", "010077c000b90000")
                nameLower.contains("nba 2k") -> listOf("nba 2k", "0100b1a01b20a000")
                nameLower.contains("just dance") -> listOf("just dance", "0100827018dee000")
                nameLower.contains("51 worldwide") -> listOf("clubhouse", "51 worldwide", "01005b900c09a000")
                nameLower.contains("tennis aces") -> listOf("tennis aces", "mario tennis", "0100bde00862a000")
                nameLower.contains("super rush") -> listOf("super rush", "mario golf", "0100874011ee6000")
                nameLower.contains("shin megami tensei") -> listOf("megami tensei", "smt5", "smtv", "vengeance", "01007e3019808000")
                nameLower.contains("metroid prime") -> listOf("metroid prime", "prime remastered", "0100121014688000")
                nameLower.contains("crisis core") -> listOf("crisis core", "ffvii", "reunion", "01005e3017b46000")
                nameLower.contains("octopath traveler") -> listOf("octopath", "octopath2", "0100d15017d3e000")
                nameLower.contains("sonic frontiers") -> listOf("sonic frontiers", "frontiers", "0100508013ae0000")
                nameLower.contains("tactics ogre") -> listOf("tactics ogre", "reborn", "0100570017106000")
                nameLower.contains("dragon's dogma") -> listOf("dragon's dogma", "dragons dogma", "dark arisen", "0100ae600ba12000")
                nameLower.contains("warframe") -> listOf("warframe", "01000b300b250000")
                nameLower.contains("brotato") -> listOf("brotato", "010077001a8d4000")
                nameLower.contains("vampire survivors") -> listOf("vampire survivors", "vampire", "survivors", "010085c019864000")
                nameLower.contains("definitive edition") && nameLower.contains("xenoblade") -> listOf("xenoblade", "definitive", "0100ff500e34a000")
                nameLower.contains("xenoblade chronicles 2") -> listOf("xenoblade 2", "0100e95004038000")
                nameLower.contains("super mario rpg") -> listOf("mario rpg", "01008d3017b4c000")
                nameLower.contains("showtime") -> listOf("showtime", "princess peach", "010006701b4b2000")
                nameLower.contains("mansion 2") -> listOf("mansion 2", "dark moon", "0100c8901844a000")
                nameLower.contains("f-zero") -> listOf("f-zero", "fzero", "0100b9301a4a0000")
                nameLower.contains("3d all-stars") -> listOf("3d all-stars", "all stars", "sunshine", "galaxy", "0100427010476000")
                nameLower.contains("legends: arceus") || nameLower.contains("arceus") -> listOf("arceus", "01001f5010dfa000")
                nameLower.contains("scarlet") -> listOf("scarlet", "0100a3d008c5c000")
                nameLower.contains("violet") -> listOf("violet", "01008f6008c5e000")
                nameLower.contains("devilutionx") -> listOf("devilutionx", "diablo", "hellfire", "diabdat", "0100d1ab10000000")
                nameLower.contains("gta v") -> listOf("gta v", "gta 5", "grand theft auto v", "0100b00b51230000")
                nameLower.contains("re3-sa") -> listOf("re3-sa", "san andreas", "gtasa", "0100671000000000")
                nameLower.contains("revc") -> listOf("revc", "vice city", "gtavc", "0100672000000000")
                nameLower.contains("re3") -> listOf("re3", "gta 3", "gta iii", "gta3", "0100673000000000")
                nameLower.contains("xash3d") -> listOf("xash3d", "half-life", "halflife", "valve.wad", "0100000000000018")
                nameLower.contains("gzdoom") -> listOf("gzdoom", "prboom", "crispy", "doom.wad", "doom2.wad", "sigil", "plutonia", "0100d00100000000")
                nameLower.contains("dhewm3") -> listOf("dhewm3", "doom 3", "doom3", "0100d00300000000")
                nameLower.contains("quakespasm") -> listOf("quake", "quakespasm", "yamagi", "ioquake3", "0100000000000071")
                nameLower.contains("eduke32") -> listOf("duke", "duke3d", "voidsw", "shadow warrior", "nblood", "blood.rff", "010000000000003d")
                nameLower.contains("iortcw") -> listOf("iortcw", "ecwolf", "wolfenstein", "wolf3d", "0100000000000077")
                nameLower.contains("fallout-ce") -> listOf("fallout", "fallout-ce", "fallout2-ce", "01000000000000f1")
                nameLower.contains("fheroes2") -> listOf("heroes", "fheroes2", "vcmi", "homm", "0100000000000030")
                nameLower.contains("max payne") -> listOf("max payne", "maxpayne", "010000000000004d")
                nameLower.contains("openmw") -> listOf("openmw", "morrowind", "010000000000006d")
                nameLower.contains("openjk") -> listOf("openjk", "jedi outcast", "jedi academy", "010000000000004a")
                nameLower.contains("openxray") -> listOf("openxray", "stalker", "s.t.a.l.k.e.r", "0100000000000053")
                nameLower.contains("rsdk") -> listOf("rsdk", "sonic cd", "data.rsdk", "010000000000005c")
                nameLower.contains("sm64-nx") -> listOf("sm64", "render96", "0100000000000064")
                nameLower.contains("ship of harkinian") -> listOf("harkinian", "soh", "2s2h", "ocarina of time", "majora", "0100000000000074")
                nameLower.contains("nxengine") -> listOf("nxengine", "cave story", "am2r", "0100000000000022")
                nameLower.contains("corsixth") -> listOf("corsixth", "theme hospital", "julius", "augustus", "caesar", "0100000000000068")
                nameLower.contains("openra") -> listOf("openra", "dune legacy", "command & conquer", "red alert", "0100000000000091")
                nameLower.contains("retroarch") -> listOf("retroarch", "ppsspp", "flycast", "scummvm", "melonds", "mgba", "duckstation", "0100000000001000")
                nameLower.contains("homebrew utilities") -> listOf("nx-shell", "dbi", "goldleaf", "jksv", "checkpoint", "edizon", "awoo", "tesla", "0100000000003000")
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

    const val TEMPORARY_FIX_HEADER = "# STORM_AUTO_GAME_FIX_TEMPORARY"

    private val temporaryFixGames = java.util.Collections.synchronizedSet(mutableSetOf<String>())

    @Volatile
    private var activeSessionProgramIdHex: String? = null

    @Volatile
    private var activeSessionFix: GameFixProfile? = null

    fun isTemporaryFixFile(file: java.io.File): Boolean {
        if (!file.exists() || !file.isFile) return false
        return try {
            file.bufferedReader().use { reader ->
                val firstLine = reader.readLine()
                firstLine != null && firstLine.startsWith(TEMPORARY_FIX_HEADER)
            }
        } catch (_: Exception) {
            false
        }
    }

    fun isUserCustomConfig(game: Game): Boolean {
        return try {
            val file = SettingsFile.getCustomSettingsFile(game)
            file.exists() && file.length() > 0 && !isTemporaryFixFile(file)
        } catch (_: Exception) {
            false
        }
    }

    fun activateSessionFix(game: Game) {
        val fix = getFix(game)
        val hex = getProgramIdHex(game)
        activeSessionProgramIdHex = hex
        activeSessionFix = fix
        if (hex.isNotEmpty()) {
            temporaryFixGames.add(hex.uppercase())
        }
        Log.info("[GameFixDatabase] Activated session fix for ${game.title} ($hex)")
    }

    fun clearActiveSessionFix(game: Game? = null) {
        try {
            if (game != null) {
                val hex = getProgramIdHex(game)
                if (hex.isNotEmpty()) {
                    temporaryFixGames.remove(hex.uppercase())
                }
                val file = SettingsFile.getCustomSettingsFile(game)
                if (file.exists() && isTemporaryFixFile(file)) {
                    file.delete()
                    Log.info("[GameFixDatabase] Deleted temporary GameFix config for ${game.title}")
                }
            }
            activeSessionProgramIdHex = null
            activeSessionFix = null
            Log.info("[GameFixDatabase] Cleared active fix for game")
        } catch (_: Exception) {}
    }

    fun cleanupSession(game: Game? = null) {
        clearActiveSessionFix(game)
    }

    fun isSessionFixActive(game: Game): Boolean {
        val hex = getProgramIdHex(game)
        if (hex.isNotEmpty() && temporaryFixGames.contains(hex.uppercase())) {
            return true
        }
        return try {
            val file = SettingsFile.getCustomSettingsFile(game)
            file.exists() && isTemporaryFixFile(file)
        } catch (_: Exception) {
            false
        }
    }

    fun isFixApplied(game: Game): Boolean {
        return isSessionFixActive(game)
    }

    fun getActiveSessionFix(game: Game): GameFixProfile? {
        val hex = getProgramIdHex(game)
        if (hex.isNotEmpty() && hex.equals(activeSessionProgramIdHex, ignoreCase = true)) {
            return activeSessionFix
        }
        return null
    }

    fun applyFix(game: Game, forceOverwrite: Boolean = false): Boolean {
        val fix = getFix(game) ?: return false
        activateSessionFix(game)

        // If the user already has their own personal manual configuration, do not overwrite it unless forced
        if (!forceOverwrite && isUserCustomConfig(game)) {
            Log.info("[GameFixDatabase] Game ${game.title} already has personal custom config; keeping user settings.")
            return true
        }

        try {
            val file = SettingsFile.getCustomSettingsFile(game)
            val parent = file.parentFile
            if (parent != null && !parent.exists()) {
                parent.mkdirs()
            }

            val sections = mutableMapOf<String, MutableMap<String, String>>()
            for ((fullKey, value) in fix.settingsMap) {
                val sectionName = if (fullKey.contains("\\")) fullKey.substringBefore("\\") else "Core"
                val keyName = if (fullKey.contains("\\")) fullKey.substringAfterLast("\\") else fullKey
                val section = sections.getOrPut(sectionName) { mutableMapOf() }
                section[keyName] = value
                if (keyName == "memory_layout_mode") {
                    sections.getOrPut("Core") { mutableMapOf() }[keyName] = value
                    sections.getOrPut("System") { mutableMapOf() }[keyName] = value
                }
            }

            val sb = StringBuilder()
            sb.append(TEMPORARY_FIX_HEADER).append("\n")
            sb.append("# Auto-generated by STORM SWITCH GameFix. Automatically deleted on game exit.\n\n")
            for ((sectionName, map) in sections) {
                sb.append("[$sectionName]\n")
                for ((k, v) in map) {
                    sb.append("$k = $v\n")
                    sb.append("$k\\use_global = false\n")
                }
                sb.append("\n")
            }

            file.writeText(sb.toString())
            Log.info("[GameFixDatabase] Successfully wrote temporary custom config for ${game.title} at ${file.absolutePath}")
            return true
        } catch (e: Exception) {
            Log.error("[GameFixDatabase] Failed to write custom config: ${e.message}")
            return false
        }
    }

    fun cleanupAllTemporaryFixes() {
        try {
            val userDir = DirectoryInitialization.userDirectory ?: return
            val customDir = java.io.File(userDir, "config/custom")
            if (customDir.exists() && customDir.isDirectory) {
                customDir.listFiles()?.forEach { file ->
                    if (file.isFile && file.name.endsWith(".ini", ignoreCase = true)) {
                        if (isTemporaryFixFile(file) || isCorruptedStaleFixFile(file)) {
                            file.delete()
                            Log.info("[GameFixDatabase] Cleaned up stale temporary fix file: ${file.name}")
                        }
                    }
                }
            }
            temporaryFixGames.clear()
        } catch (e: Exception) {
            Log.error("[GameFixDatabase] Error cleaning up temporary fixes: ${e.message}")
        }
    }

    private fun isCorruptedStaleFixFile(file: java.io.File): Boolean {
        return try {
            val text = file.readText()
            text.contains("enable_compute_pipelines = false") ||
            text.contains("use_vulkan_driver_pipeline_cache = false") ||
            text.contains("use_asynchronous_shaders = false")
        } catch (_: Exception) {
            false
        }
    }
}
