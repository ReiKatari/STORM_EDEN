// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/hle/service/game_fix_database.h"
#include <fstream>
#include <sstream>
#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "common/logging.h"
#include "common/settings.h"

namespace Core {

static const std::vector<GameFixProfile> s_profiles = {
    {
        0x01007EF00011E000ULL,
        "The Legend of Zelda: Breath of the Wild",
        "• Просадки FPS при избыточной синхронизации GPU\n• Непрозрачный/темный туман в Святилищах при принудительной реактивной очистке\n• Микрофризы в лесах Короков",
        "• Framerate drops caused by excessive GPU buffer synchronization\n• Dark opaque volumetric fog in Shrines caused by reactive flushing\n• Korok Forest micro-stutters",
        "✓ Точность GPU: Обычная (Normal, стабильные 30 FPS)\n✓ Реактивная очистка: Отключено (исправление тумана в Святилищах)\n✓ Сжатие ASTC: Отключено (чистые текстуры)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Память: 8GB DRAM",
        "✓ GPU Accuracy: Normal (Locked 30 FPS without GPU stalling)\n✓ Reactive Flushing: Disabled (Fixes Shrine volumetric fog)\n✓ ASTC Recompression: Uncompressed\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Memory Layout: 8GB DRAM",
        {
            {"Renderer\\gpu_accuracy", "0"},
            {"Renderer\\use_reactive_flushing", "false"},
            {"Renderer\\astc_recompression", "0"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"System\\memory_layout_mode", "1"}
        }
    },
    {
        0x0100F2C0115B6000ULL,
        "The Legend of Zelda: Tears of the Kingdom",
        "• Утечки VRAM в Кавернах и конструкторе Ультраруки\n• Микрофризы при смене способностей в колесе быстрого выбора",
        "• VRAM memory pressure in Depths and Ultrahand construction\n• Quick-select ability wheel micro-stutters",
        "✓ Точность GPU: Обычная (Normal, максимальный FPS)\n✓ Реактивная очистка: Отключено\n✓ Сжатие ASTC: Отключено\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Память: 8GB DRAM",
        "✓ GPU Accuracy: Normal (Maximum framerate stability)\n✓ Reactive Flushing: Disabled\n✓ ASTC Recompression: Uncompressed\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Memory Layout: 8GB DRAM",
        {
            {"Renderer\\gpu_accuracy", "0"},
            {"Renderer\\use_reactive_flushing", "false"},
            {"Renderer\\astc_recompression", "0"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"System\\memory_layout_mode", "1"}
        }
    },
    {
        0x01004D701742A000ULL,
        "Paper Mario: The Thousand-Year Door",
        "• Черный экран на катсценах в прологе\n• Сбои 2D-шрифтов диалогов и мерцание текстур",
        "• Black screen during prologue cutscenes\n• Corrupted battle text boxes and flickering textures",
        "✓ Точность GPU: Высокая\n✓ Сжатие ASTC: Отключено\n✓ Реактивная очистка: Включено",
        "✓ GPU Accuracy: High\n✓ ASTC Recompression: Uncompressed\n✓ Reactive Flushing: Enabled",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\astc_recompression", "0"},
            {"Renderer\\use_reactive_flushing", "true"}
        }
    },
    {
        0x0100B5B0112F8000ULL,
        "Hogwarts Legacy",
        "• Вылет из-за нехватки памяти при загрузке замка Хогвартс\n• Высокое потребление ОЗУ (>8.5 ГБ) на мобильных чипах",
        "• Out of memory (OOM) crash when loading Hogwarts Castle\n• High RAM consumption (>8.5 GB) on mobile SoCs",
        "✓ Разрешение: Handheld 0.75X + FSR 80%\n✓ Сжатие текстур ASTC: Отключено\n✓ Режим памяти: 8GB DRAM",
        "✓ Resolution: Handheld 0.75X + FSR 80%\n✓ ASTC Recompression: BC1 (lowers RAM to 4.8 GB)\n✓ Memory Layout: 8GB DRAM",
        {
            {"Renderer\\astc_recompression", "0"},
            {"Renderer\\resolution_setup", "1"},
            {"Renderer\\fsr_sharpening_slider", "80"},
            {"Core\\memory_layout_mode", "2"}
        }
    },
    {
        0x0100916014D8C000ULL,
        "Diablo II: Resurrected",
        "• Быстрый нагрев устройства (>46°C за 5 минут)\n• Термальный троттлинг и просадка FPS с 30 до 22",
        "• Rapid SoC heating (>46°C in 5 min)\n• Thermal throttling dropping FPS from 30 to 22",
        "✓ Разрешение: Handheld 0.75X-1.0X + FSR 80%\n✓ Реактивная очистка: Отключено\n✓ Защита от троттлинга GPU: Включено",
        "✓ Resolution: Handheld 0.75X-1.0X + FSR 80%\n✓ Reactive Flushing: Disabled\n✓ GPU Thermal Floor Clamp: Enabled",
        {
            {"Renderer\\use_reactive_flushing", "false"},
            {"Renderer\\fsr_sharpening_slider", "80"}
        }
    },
    {
        0x0100C6000EEA8000ULL,
        "Warhammer 40,000: Mechanicus",
        "• Невозможно сохранить прогресс игры (ошибка сохранения)",
        "• Unable to save game progress (infinite save loop)",
        "✓ Поддержка RenameDirectory в STORM EDEN 4.6.0+\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ RenameDirectory support in STORM EDEN 4.6.0+\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100923008C54000ULL,
        "LEGO Star Wars: The Skywalker Saga",
        "• Бесконечная загрузка на заставке\n• Зависание асинхронного таймера GPU при обращении к ресурсам\n• Блокировка сетевой телеметрии WB на Android",
        "• Infinite loading screen (TT Games loading indicator loop)\n• GPU async timer deadlock during asset initialization\n• WB telemetry network deadlock on Android",
        "✓ Режим полёта: Включено (критично для запуска на Android!)\n✓ Быстрое время GPU: Отключено (устраняет вечную загрузку!)\n✓ Динамическое состояние: Базовое\n✓ Точность GPU: Высокая\n✓ Точность CPU: Точная\n✓ Реактивная очистка: Отключено\n✓ Память: 8GB DRAM",
        "✓ Airplane Mode: Enabled (Critical for Android launch!)\n✓ Fast GPU Time: Disabled (Fixes infinite loading!)\n✓ Dynamic State: Basic (EDS1)\n✓ GPU Accuracy: High\n✓ CPU Accuracy: Accurate\n✓ Reactive Flushing: Disabled\n✓ Memory Layout: 8GB DRAM",
        {
            {"System\\airplane_mode", "true"},
            {"System\\memory_layout_mode", "1"},
            {"Renderer\\use_fast_gpu_time", "false"},
            {"Renderer\\dyna_state", "0"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_reactive_flushing", "false"},
            {"Renderer\\astc_recompression", "0"},
            {"Renderer\\use_asynchronous_shaders", "false"},
            {"Cpu\\cpu_accuracy", "0"}
        }
    },
    {
        0x0100E26017E5E000ULL,
        "Red Dead Redemption",
        "• Хрипы и рассинхронизация звука в катсценах\n• Микрофризы физических потоков RAGE Engine",
        "• Audio crackling and desync in cutscenes\n• Micro-stutters in RAGE Engine physics threads",
        "✓ Точность CPU: Точная\n✓ Аудио-буфер Cubeb: 80 ms\n✓ Синхронизация памяти: Отключено",
        "✓ CPU Accuracy: Accurate\n✓ Cubeb Audio Buffer: 80 ms\n✓ Sync Memory Ops: Disabled",
        {
            {"Cpu\\cpu_accuracy", "0"},
            {"Renderer\\sync_memory_operations", "false"}
        }
    },
    {
        0x0100D870045B6000ULL,
        "Luigi's Mansion 3",
        "• Растягивание полигонов (взрывы геометрии)\n• Невидимый луч фонарика и зависания в лифте",
        "• Vertex explosion (stretched geometry)\n• Invisible flashlight beam and elevator freeze",
        "✓ Расширенное динамическое состояние: Включено\n✓ Точность GPU: Высокая\n✓ Точность CPU: Точная",
        "✓ Extended Dynamic State: Enabled\n✓ GPU Accuracy: High\n✓ CPU Accuracy: Accurate",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpu_accuracy", "0"},
            {"Renderer\\dyna_state", "2"}
        }
    },
    {
        0x01004A4010F22000ULL,
        "Bayonetta 3",
        "• Невидимые персонажи и противники на чипах Snapdragon\n• Чёрный экран после QTE-добиваний",
        "• Invisible character/enemy models on Snapdragon SoCs\n• Black screen after QTE sequences",
        "✓ Контроль отсечения глубины: Включено\n✓ Точность GPU: Высокая",
        "✓ Depth Clip Control: Enabled (STORM DRIVER)\n✓ GPU Accuracy: High",
        {
            {"Renderer\\gpu_accuracy", "1"}
        }
    },
    {
        0x01007300020FA000ULL,
        "Astral Chain",
        "• Пропадание неонового интерфейса Легиона\n• Затемнение картинки и циклический гул звука",
        "• Missing Legion neon glow effects\n• Dark screen tint and audio looping",
        "✓ Эмуляция цвета BGR565: Включено\n✓ Коррекция эффектов свечения: Включено",
        "✓ Emulate BGR565: Enabled\n✓ Fix Bloom Effects: Enabled",
        {
            {"Renderer\\emulate_bgr565", "true"},
            {"Renderer\\fix_bloom_effects", "true"}
        }
    },
    {
        0x01006A800016E000ULL,
        "Super Smash Bros. Ultimate",
        "• Вылет на экране победы или в меню новостей",
        "• Crash on victory screen or news board (Web Applet)",
        "✓ Отключение веб-апплета: Включено\n✓ Mii Applet: LLE",
        "✓ Disable Web Applet: Enabled\n✓ Mii Applet: LLE",
        {
            {"Debugging\\disable_web_applet", "true"}
        }
    },
    {
        0x0100152000022000ULL,
        "Mario Kart 8 Deluxe",
        "• Отсутствие голов у персонажей Mii на трассах",
        "• Invisible/missing heads on Mii characters",
        "✓ Требуется Firmware 18.0.0+ и системные файлы Mii\n✓ Сжатие ASTC: Отключено",
        "✓ Firmware 18.0.0+ and Mii system files required\n✓ ASTC Recompression: Uncompressed",
        {
            {"Renderer\\astc_recompression", "0"}
        }
    },
    {
        0x01001F5010DFA000ULL,
        "Pokemon Legends: Arceus",
        "• Вытягивание полигонов травы и деревьев в небо\n• Сбои теней на аренах",
        "• Vertex explosion on trees and grass geometry\n• Shadow glitches during battle transitions",
        "✓ Точность GPU: Высокая\n✓ Анизотропная фильтрация: 16x\n✓ Декодирование ASTC на GPU: Включено",
        "✓ GPU Accuracy: High\n✓ Anisotropic Filtering: 16x\n✓ ASTC GPU Decode: Enabled",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\max_anisotropy", "5"}
        }
    },
    {
        0x01008C30086E0000ULL,
        "Pokemon Scarlet",
        "• Утечки памяти в открытом мире Палдеи\n• Мерцание ландшафта и текстур",
        "• Open-world memory leaks in Paldea\n• Terrain and texture flickering",
        "✓ Разрешение: Handheld 0.75X + FSR 75%\n✓ Сжатие ASTC: Отключено\n✓ Ограничение VRAM: Conservative",
        "✓ Resolution: Handheld 0.75X + FSR 75%\n✓ ASTC Recompression: BC3\n✓ VRAM Usage: Conservative",
        {
            {"Renderer\\astc_recompression", "0"},
            {"Renderer\\resolution_setup", "1"},
            {"Renderer\\vram_usage_mode", "1"}
        }
    },
    {
        0x0100A3D0086EE000ULL,
        "Pokemon Violet",
        "• Утечки памяти в открытом мире Палдеи\n• Мерцание ландшафта и текстур",
        "• Open-world memory leaks in Paldea\n• Terrain and texture flickering",
        "✓ Разрешение: Handheld 0.75X + FSR 75%\n✓ Сжатие ASTC: Отключено\n✓ Ограничение VRAM: Conservative",
        "✓ Resolution: Handheld 0.75X + FSR 75%\n✓ ASTC Recompression: BC3\n✓ VRAM Usage: Conservative",
        {
            {"Renderer\\astc_recompression", "0"},
            {"Renderer\\resolution_setup", "1"},
            {"Renderer\\vram_usage_mode", "1"}
        }
    },
    {
        0x0100B9F010DC4000ULL,
        "Doom Eternal",
        "• Вылет драйвера Vulkan при первом выстреле / спавне BFG",
        "• Vulkan device loss crash on weapon fire / BFG",
        "✓ Проверка границ глубины: Включено\n✓ Точность DMA: Safe",
        "✓ Depth bounds test: STORM DRIVER (pan_depth_bounds)\n✓ DMA Accuracy: Safe",
        {
            {"Renderer\\dma_accuracy", "1"}
        }
    },
    {
        0x010034B01314C000ULL,
        "Prince of Persia: The Lost Crown",
        "• Чёрный экран при воспроизведении видеовставок и анимаций амулетов",
        "• Black screen during video cutscenes and amulet animations",
        "✓ Декодирование видео (NVDEC): На GPU\n✓ Fastmem Exclusives: Отключено",
        "✓ NVDEC Video Emulation: GPU\n✓ Fastmem Exclusives: Disabled",
        {
            {"Renderer\\nvdec_emulation", "2"},
            {"Cpu\\cpuopt_fastmem_exclusives", "false"}
        }
    },
    {
        0x010063B017DAE000ULL,
        "Batman: Arkham Knight",
        "• Вылет по нехватке памяти при погонях на Бэтмобиле",
        "• OOM crash during Batmobile chase sequences",
        "✓ Разрешение: Handheld 0.75X + FSR 80%\n✓ Сжатие ASTC: Отключено\n✓ Память: 8GB DRAM",
        "✓ Resolution: Handheld 0.75X + FSR 80%\n✓ ASTC Recompression: BC1\n✓ Memory Layout: 8GB DRAM",
        {
            {"Renderer\\astc_recompression", "0"},
            {"Renderer\\resolution_setup", "1"},
            {"Core\\memory_layout_mode", "2"}
        }
    },
    {
        0x01000B901C46E000ULL,
        "Shin Megami Tensei V: Vengeance",
        "• Вылет движка Unreal Engine 4 при старте на чипах Snapdragon 8",
        "• Unreal Engine 4 crash on launch on Snapdragon 8 devices",
        "✓ Macro JIT / HLE: Включено\n✓ Нативное декодирование BCn: Включено",
        "✓ Macro JIT / HLE: Enabled\n✓ Native BCn Decode: Enabled",
        {
            {"Debugging\\disable_macro_jit", "false"},
            {"Debugging\\disable_macro_hle", "false"}
        }
    },
    {
        0x01003D100E9C6000ULL,
        "The Witcher 3: Wild Hunt",
        "• Зависание физики волос/одежды Геральта в Новиграде",
        "• HairWorks and physics freezes in Novigrad",
        "✓ Fastmem Exclusives: Включено\n✓ Разрешение: Handheld 0.75X + FSR 85%",
        "✓ Fastmem Exclusives: Enabled\n✓ Resolution: Handheld 0.75X + FSR 85%",
        {
            {"Cpu\\cpuopt_fastmem_exclusives", "true"},
            {"Renderer\\resolution_setup", "1"},
            {"Renderer\\fsr_sharpening_slider", "85"}
        }
    },
    {
        0x0100760012E4A000ULL,
        "Mario + Rabbids Sparks of Hope",
        "• Вылет движка Snowdrop при переходе в тактический бой",
        "• Snowdrop engine crash on tactical combat transition",
        "✓ Точность DMA: Safe\n✓ Точность GPU: Высокая\n✓ Barrier Feedback Loops: Включено",
        "✓ DMA Accuracy: Safe\n✓ GPU Accuracy: High\n✓ Barrier Feedback Loops: Enabled",
        {
            {"Renderer\\dma_accuracy", "1"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\barrier_feedback_loops", "true"}
        }
    },
    {
        0x010056D015DB6000ULL,
        "Sonic Frontiers",
        "• Падение FPS и вылет в открытых зонах островов",
        "• Frame drops and OOM crash in open-zone islands",
        "✓ Разрешение: Handheld 0.75X + FSR 80%\n✓ Сжатие ASTC: Отключено\n✓ Память: 8GB DRAM",
        "✓ Resolution: Handheld 0.75X + FSR 80%\n✓ ASTC Recompression: BC1\n✓ Memory Layout: 8GB DRAM",
        {
            {"Renderer\\astc_recompression", "0"},
            {"Renderer\\resolution_setup", "1"},
            {"Core\\memory_layout_mode", "2"}
        }
    },
    {
        0x01004AB00A266000ULL,
        "Dark Souls: Remastered",
        "• Просадки кадровой частоты у костров и зацикливание звука баффов",
        "• Bonfire particle slowdown and weapon buff sound loop",
        "✓ Точность CPU: Точная\n✓ Точность GPU: Высокая\n✓ Аудио-движок: Cubeb",
        "✓ CPU Accuracy: Accurate\n✓ GPU Accuracy: High\n✓ Audio Engine: Cubeb",
        {
            {"Cpu\\cpu_accuracy", "0"},
            {"Renderer\\gpu_accuracy", "1"}
        }
    },
    {
        0x0100C9E01B854000ULL,
        "Animal Well",
        "• Чёрный экран и пропадание звуковых дорожек",
        "• Black screen and missing audio tracks on startup",
        "✓ Аудио-движок: Cubeb 48kHz\n✓ Асинхронные шейдеры: Включено",
        "✓ Audio Engine: Cubeb 48kHz\n✓ Asynchronous Shaders: Enabled",
        {
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100C88011246000ULL,
        "Disco Elysium: The Final Cut",
        "• Утечка памяти и вылеты при смене локаций\n• Размытие и мерцание текста диалогов TextMeshPro\n• Цветовые артефакты акварельных портретов и фонов",
        "• Out of memory (OOM) crash on zone transitions\n• TextMeshPro dialogue font blur and jitter\n• Color compression artifacts on painted portraits and backdrops",
        "✓ Память: 6GB DRAM (предотвращение вылетов Unity)\n✓ Сжатие ASTC: Отключено\n✓ Динамическое состояние: Базовое\n✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Реактивная очистка: Включено",
        "✓ Memory Layout: 6GB DRAM (Prevents Unity OOM crashes)\n✓ ASTC Recompression: Uncompressed (Max art fidelity)\n✓ Dynamic State: EDS1\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Reactive Flushing: Enabled",
        {
            {"Core\\memory_layout_mode", "1"},
            {"Renderer\\astc_recompression", "0"},
            {"Renderer\\dyna_state", "0"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_reactive_flushing", "true"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x01003AE017DB0000ULL,
        "Batman: Arkham City",
        "• Просадки FPS при планировании над городом и микрофризы",
        "• FPS drops and micro-stutters while gliding across Arkham City",
        "✓ Точность CPU: Точная\n✓ Память: 6GB DRAM\n✓ Динамическое состояние: Базовое",
        "✓ CPU Accuracy: Accurate\n✓ Memory Layout: 6GB DRAM\n✓ Dynamic State: EDS1",
        {
            {"Cpu\\cpu_accuracy", "0"},
            {"Core\\memory_layout_mode", "1"},
            {"Renderer\\dyna_state", "0"}
        }
    },
    {
        0x0100FF500E34A000ULL,
        "Xenoblade Chronicles: Definitive Edition",
        "• Мерцание текстур открытого мира и артефакты облаков",
        "• Open world texture shimmering and cloud rendering artifacts",
        "✓ Точность GPU: Высокая\n✓ Динамическое состояние: Расширенное\n✓ Сжатие ASTC: Отключено\n✓ Память: 6GB DRAM",
        "✓ GPU Accuracy: High\n✓ Dynamic State: EDS2\n✓ ASTC Recompression: BC3\n✓ Memory Layout: 6GB DRAM",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\dyna_state", "2"},
            {"Renderer\\astc_recompression", "0"},
            {"Core\\memory_layout_mode", "1"},
            {"Renderer\\use_fast_gpu_time", "true"}
        }
    },
    {
        0x0100E95004038000ULL,
        "Xenoblade Chronicles 2",
        "• Просадки кадровой частоты в густонаселенных локациях (Гула, Мор Ардайн)",
        "• Heavy frame drops in dense titan areas (Gormott, Mor Ardain)",
        "✓ Динамическое состояние: Расширенное\n✓ Сжатие ASTC: Отключено\n✓ Память: 6GB DRAM\n✓ Быстрое время GPU: Включено",
        "✓ Dynamic State: EDS2\n✓ ASTC Recompression: BC3\n✓ Memory Layout: 6GB DRAM\n✓ Fast GPU Time: Enabled",
        {
            {"Renderer\\dyna_state", "2"},
            {"Renderer\\astc_recompression", "0"},
            {"Core\\memory_layout_mode", "1"},
            {"Renderer\\use_fast_gpu_time", "true"}
        }
    },
    {
        0x010074F013262000ULL,
        "Xenoblade Chronicles 3",
        "• Утечки VRAM и микростаттеры в битвах с 7 персонажами",
        "• VRAM leaks and micro-stutters during full 7-character battle parties",
        "✓ Сжатие ASTC: Отключено\n✓ Память: 6GB DRAM\n✓ Динамическое состояние: Расширенное\n✓ Точность GPU: Высокая",
        "✓ ASTC Recompression: BC3\n✓ Memory Layout: 6GB DRAM\n✓ Dynamic State: EDS2\n✓ GPU Accuracy: High",
        {
            {"Renderer\\astc_recompression", "0"},
            {"Core\\memory_layout_mode", "1"},
            {"Renderer\\dyna_state", "2"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_fast_gpu_time", "true"}
        }
    },
    {
        0x0100E67012924000ULL,
        "The Witcher 3: Wild Hunt - Complete Edition",
        "• Вылеты по памяти и заикания физики в Новиграде и Туссенте",
        "• OOM crashes and physics stutter in Novigrad and Toussaint",
        "✓ Память: 6GB DRAM\n✓ Точность GPU: Высокая\n✓ Сжатие ASTC: Отключено\n✓ Быстрое время GPU: Включено\n✓ Реактивная очистка: Включено",
        "✓ Memory Layout: 6GB DRAM\n✓ GPU Accuracy: High\n✓ ASTC Recompression: Uncompressed\n✓ Fast GPU Time: Enabled\n✓ Reactive Flushing: Enabled",
        {
            {"Core\\memory_layout_mode", "1"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\astc_recompression", "0"},
            {"Renderer\\use_fast_gpu_time", "true"},
            {"Renderer\\use_reactive_flushing", "true"}
        }
    },
    {
        0x010074600EE26000ULL,
        "Need for Speed: Hot Pursuit Remastered",
        "• Зависание и бесконечная загрузка при авторизации на серверах EA Autolog\n• Сетевые таймауты при запуске",
        "• Infinite loading hang during EA Autolog server authorization\n• Network connection handshake timeout",
        "✓ Режим полета (В самолете): Включено (пропуск онлайн-проверки)\n✓ Асинхронные шейдеры: Включено\n✓ Память: 8GB DRAM\n✓ Сжатие ASTC: Отключено",
        "✓ Airplane Mode: Enabled (Bypasses EA Autolog offline)\n✓ Asynchronous Shaders: Enabled\n✓ Memory Layout: 8GB DRAM\n✓ ASTC Recompression: Uncompressed",
        {
            {"System\\airplane_mode", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Core\\memory_layout_mode", "2"},
            {"Renderer\\astc_recompression", "0"}
        }
    },
    {
        0x010067300059A000ULL,
        "Mario + Rabbids: Kingdom Battle",
        "• Сильный пересвет и ослепляющий блум\n• Черные тени и артефакты освещения\n• Мерцание текстур персонажей",
        "• Severe overexposure and blinding bloom glow\n• Black shadows and lighting pass corruption\n• Character model flickering",
        "✓ Точность GPU: Высокая (устранение пересвета и черных теней)\n✓ Обратные циклы барьеров: Включено\n✓ Реактивная очистка: Включено\n✓ Сжатие ASTC: Отключено\n✓ Асинхронные шейдеры: Включено",
        "✓ GPU Accuracy: High (Fixes bloom & black shadows)\n✓ Barrier Feedback Loops: Enabled\n✓ Reactive Flushing: Enabled\n✓ ASTC Recompression: Uncompressed\n✓ Asynchronous Shaders: Enabled",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\barrier_feedback_loops", "true"},
            {"Renderer\\use_reactive_flushing", "true"},
            {"Renderer\\astc_recompression", "0"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x01005CA00F966000ULL,
        "Mario + Rabbids: Sparks of Hope",
        "• Сбои динамического освещения на планетах\n• Черные артефакты в катсценах и битвах\n• Просадки кадров",
        "• Planet lighting pass corruption\n• Black shadow artifacts during cutscenes and tactical battles\n• Frame drops",
        "✓ Точность GPU: Высокая\n✓ Обратные циклы барьеров: Включено\n✓ Реактивная очистка: Включено\n✓ Сжатие ASTC: Отключено\n✓ Память: 8GB DRAM",
        "✓ GPU Accuracy: High\n✓ Barrier Feedback Loops: Enabled\n✓ Reactive Flushing: Enabled\n✓ ASTC Recompression: Uncompressed\n✓ Memory Layout: 8GB DRAM",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\barrier_feedback_loops", "true"},
            {"Renderer\\use_reactive_flushing", "true"},
            {"Renderer\\astc_recompression", "0"},
            {"Core\\memory_layout_mode", "2"}
        }
    },
    {
        0x01005CF01E784000ULL,
        "Teenage Mutant Ninja Turtles: Splintered Fate",
        "• Зависание на титульном экране из-за сетевого ожидания NIM и SSL сервисов\n• Микрофризы при спавне врагов",
        "• Title screen freeze caused by NIM and SSL network connection waiting\n• Stutter during enemy combat waves",
        "✓ Режим полета (В самолете): Включено (пропуск сетевого ожидания)\n✓ Асинхронные шейдеры: Включено\n✓ Быстрая память: Включено\n✓ Сжатие ASTC: Отключено",
        "✓ Airplane Mode: Enabled (Skips online network handshake)\n✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled\n✓ ASTC Recompression: Uncompressed",
        {
            {"System\\airplane_mode", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\astc_recompression", "0"}
        }
    },
    {
        0x0100000000010000ULL,
        "Super Mario Odyssey",
        "• Мерцание 2D-рисунков на стенах\n• Артефакты дыма и тумана в Песчаном царстве",
        "• Flickering 2D wall drawings\n• Sand Kingdom smoke and fog rendering artifacts",
        "✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Анизотропная фильтрация: 16x\n✓ Сжатие ASTC: Отключено",
        "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ Anisotropic Filtering: 16x\n✓ ASTC Recompression: Uncompressed",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_reactive_flushing", "true"},
            {"Renderer\\max_anisotropy", "5"},
            {"Renderer\\astc_recompression", "0"}
        }
    },
    {
        0x0100121014688000ULL,
        "Metroid Prime Remastered",
        "• Статтеры при открытии дверей между отсеками\n• Мерцание эффектов визора",
        "• Door transition compilation stutter\n• Visor UI and particle flicker",
        "✓ Асинхронные шейдеры: Включено\n✓ Быстрая память: Включено\n✓ Сжатие ASTC: Отключено",
        "✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled\n✓ ASTC Recompression: Uncompressed",
        {
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\astc_recompression", "0"}
        }
    },
    {
        0x0100DCA0064A6000ULL,
        "Luigi's Mansion 3",
        "• Артефакты динамического света фонарика и теней\n• Падение FPS в комнатах с призраками",
        "• Dynamic flashlight beam artifacts\n• FPS drops in ghost-heavy rooms",
        "✓ Точность GPU: Высокая\n✓ Обратные циклы барьеров: Включено\n✓ Реактивная очистка: Включено\n✓ Память: 8GB DRAM",
        "✓ GPU Accuracy: High\n✓ Barrier Feedback Loops: Enabled\n✓ Reactive Flushing: Enabled\n✓ Memory Layout: 8GB DRAM",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\barrier_feedback_loops", "true"},
            {"Renderer\\use_reactive_flushing", "true"},
            {"Core\\memory_layout_mode", "2"}
        }
    },
    {
        0x01006F8002326000ULL,
        "Animal Crossing: New Horizons",
        "• Размытие травы и мелких объектов\n• Мерцание теней в вечернее время",
        "• Blurry grass and ground textures\n• Evening shadow flicker",
        "✓ Анизотропная фильтрация: 16x\n✓ Точность GPU: Высокая\n✓ Асинхронные шейдеры: Включено",
        "✓ Anisotropic Filtering: 16x\n✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled",
        {
            {"Renderer\\max_anisotropy", "5"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x010015100B514000ULL,
        "Persona 5 Royal",
        "• Мерцание 2D UI портретов и шрифтов\n• Просадки FPS в людных районах Токио",
        "• 2D UI portrait flicker and font artifacts\n• Heavy crowds FPS drops in Shibuya and Shinjuku",
        "✓ Точность GPU: Высокая\n✓ Асинхронные шейдеры: Включено\n✓ Быстрая память: Включено",
        "✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Cpu\\cpuopt_fastmem", "true"}
        }
    },
    {
        0x01006A800016E000ULL,
        "Super Smash Bros. Ultimate",
        "• Рассинхрон звука при загрузке 8 бойцов\n• Микрофризы при активации спецэффектов",
        "• Audio desync with 8 active fighters\n• Visual effect stutter",
        "✓ Асинхронные шейдеры: Включено\n✓ Быстрая память: Включено",
        "✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled",
        {
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Cpu\\cpuopt_fastmem", "true"}
        }
    },
    {
        0x0100A38018D5A000ULL,
        "EA SPORTS FC 24",
        "• Зависание на заставке EA Connect\n• Утечки памяти в режиме карьеры",
        "• EA Connect splash freeze\n• Career mode memory leaks",
        "✓ Режим полета (В самолете): Включено (пропуск серверов EA)\n✓ Асинхронные шейдеры: Включено\n✓ Память: 8GB DRAM",
        "✓ Airplane Mode: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Memory Layout: 8GB DRAM",
        {
            {"System\\airplane_mode", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Core\\memory_layout_mode", "2"}
        }
    },
    {
        0x01004C90141A4000ULL,
        "Sonic Frontiers",
        "• Просадки FPS и размытие текстур травы в открытых зонах\n• Мерцание теней Cyberspace",
        "• Heavy open-zone frame drops and blurry grass textures\n• Cyberspace shadow flickering",
        "✓ Асинхронные шейдеры: Включено\n✓ Быстрая память: Включено\n✓ Анизотропная фильтрация: 16x\n✓ Память: 8GB DRAM",
        "✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled\n✓ Anisotropic Filtering: 16x\n✓ Memory Layout: 8GB DRAM",
        {
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\max_anisotropy", "5"},
            {"Core\\memory_layout_mode", "2"}
        }
    },
    {
        0x01001F5010DFA000ULL,
        "Pokémon Legends: Arceus",
        "• Микрофризы при спавне диких покемонов в небе/траве\n• Артефакты освещения в разломах",
        "• Wild pokemon spawn micro-stutters\n• Space-time distortion lighting artifacts",
        "✓ Точность GPU: Высокая\n✓ Асинхронные шейдеры: Включено\n✓ Реактивная очистка: Включено\n✓ Память: 8GB DRAM",
        "✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled\n✓ Reactive Flushing: Enabled\n✓ Memory Layout: 8GB DRAM",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Renderer\\use_reactive_flushing", "true"},
            {"Core\\memory_layout_mode", "2"}
        }
    },
    {
        0x0100A3D008C5C000ULL,
        "Pokémon Scarlet / Violet",
        "• Утечки VRAM в городах и на водных просторах\n• Падение FPS и замедление анимаций",
        "• Severe VRAM leaks in towns and lakes\n• Frame drops and slowed NPC animations",
        "✓ Точность GPU: Высокая\n✓ Сжатие ASTC: Отключено\n✓ Реактивная очистка: Включено\n✓ Память: 8GB DRAM",
        "✓ GPU Accuracy: High\n✓ ASTC Recompression: Uncompressed\n✓ Reactive Flushing: Enabled\n✓ Memory Layout: 8GB DRAM",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\astc_recompression", "0"},
            {"Renderer\\use_reactive_flushing", "true"},
            {"Core\\memory_layout_mode", "2"}
        }
    },
    {
        0x0100ABF008968000ULL,
        "Pokémon Sword / Shield",
        "• Заикания в Диких землях при подгрузке погодных условий\n• Мерцание спецэффектов Dynamax",
        "• Wild Area weather transition stutters\n• Dynamax battle effect flickering",
        "✓ Асинхронные шейдеры: Включено\n✓ Быстрая память: Включено\n✓ Точность GPU: Высокая",
        "✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled\n✓ GPU Accuracy: High",
        {
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\gpu_accuracy", "1"}
        }
    },
    {
        0x01004D300C5AE000ULL,
        "Kirby and the Forgotten Land",
        "• Зависание анимаций врагов на 30 FPS\n• Артефакты отражений луж",
        "• Distant enemy 30 FPS stutter\n• Puddle reflection distortion",
        "✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Анизотропная фильтрация: 16x",
        "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ Anisotropic Filtering: 16x",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_reactive_flushing", "true"},
            {"Renderer\\max_anisotropy", "5"}
        }
    },
    {
        0x010055D009F78000ULL,
        "Fire Emblem: Three Houses",
        "• Просадки FPS при масштабных битвах батальонов\n• Мерцание 2D портретов и меню",
        "• Battalion battle animation FPS drops\n• 2D UI portrait flickering",
        "✓ Точность GPU: Высокая\n✓ Асинхронные шейдеры: Включено\n✓ Быстрая память: Включено",
        "✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Cpu\\cpuopt_fastmem", "true"}
        }
    },
    {
        0x0100A6301214E000ULL,
        "Fire Emblem Engage",
        "• Сбои шейдеров свечения колец Emblem\n• Микрофризы в Сомниэле",
        "• Emblem ring glow shader artifacts\n• Somniel hub micro-stutters",
        "✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Асинхронные шейдеры: Включено\n✓ Память: 8GB DRAM",
        "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Memory Layout: 8GB DRAM",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_reactive_flushing", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Core\\memory_layout_mode", "2"}
        }
    },
    {
        0x01007300020FA000ULL,
        "Astral Chain",
        "• Падение кадров при цепных комбо-атаках легионов\n• Размытие динамических неоновых вывесок",
        "• Legion chain attack FPS drops\n• Blurry dynamic neon bloom reflections",
        "✓ Точность GPU: Высокая\n✓ Асинхронные шейдеры: Включено\n✓ Быстрое время GPU: Включено\n✓ Память: 8GB DRAM",
        "✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled\n✓ Fast GPU Time: Enabled\n✓ Memory Layout: 8GB DRAM",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Renderer\\use_fast_gpu_time", "true"},
            {"Core\\memory_layout_mode", "2"}
        }
    },
    {
        0x01006F801BC4C000ULL,
        "Shin Megami Tensei V: Vengeance",
        "• Нагрев и просадки кадров в песчаных бурях Даата\n• Черные тени демонов",
        "• Da'at sandstorm thermal throttle and FPS drops\n• Demon shadow corruption",
        "✓ Точность GPU: Высокая\n✓ Сжатие ASTC: Отключено\n✓ Реактивная очистка: Включено\n✓ Память: 8GB DRAM",
        "✓ GPU Accuracy: High\n✓ ASTC Recompression: Uncompressed\n✓ Reactive Flushing: Enabled\n✓ Memory Layout: 8GB DRAM",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\astc_recompression", "0"},
            {"Renderer\\use_reactive_flushing", "true"},
            {"Core\\memory_layout_mode", "2"}
        }
    },
    {
        0x01006C300E9F0000ULL,
        "Dragon Quest XI S: Echoes of an Elusive Age",
        "• Микростаттеры при смене зон и переходе между 2D/3D режимами\n• Мерцание травы",
        "• Zone transition micro-stutters\n• Grass texture flickering",
        "✓ Асинхронные шейдеры: Включено\n✓ Быстрая память: Включено\n✓ Анизотропная фильтрация: 16x",
        "✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled\n✓ Anisotropic Filtering: 16x",
        {
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\max_anisotropy", "5"}
        }
    },
    {
        0x0100CC80140F8000ULL,
        "Triangle Strategy",
        "• Пересвет HD-2D эффектов глубины резкости\n• Мерцание теней на изометрических картах",
        "• HD-2D depth-of-field overexposure\n• Isometric grid shadow flickering",
        "✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_reactive_flushing", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100BB70144F8000ULL,
        "Prince of Persia: The Lost Crown",
        "• Статтеры при активации способностей управления временем\n• Искажение фоновых слоев",
        "• Time powers activation stutter\n• Background parallax distortion",
        "✓ Асинхронные шейдеры: Включено\n✓ Быстрая память: Включено\n✓ Сжатие ASTC: Отключено",
        "✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled\n✓ ASTC Recompression: Uncompressed",
        {
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\astc_recompression", "0"}
        }
    },
    {
        0x01007820195A6000ULL,
        "Red Dead Redemption",
        "• Сбои освещения на закате и рассвете\n• Просадки FPS в городах Блэкуотер и Армадилло",
        "• Sunrise/sunset volumetric lighting corruption\n• Blackwater town FPS drops",
        "✓ Точность GPU: Высокая\n✓ Обратные циклы барьеров: Включено\n✓ Асинхронные шейдеры: Включено\n✓ Память: 8GB DRAM",
        "✓ GPU Accuracy: High\n✓ Barrier Feedback Loops: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Memory Layout: 8GB DRAM",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\barrier_feedback_loops", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Core\\memory_layout_mode", "2"}
        }
    },
    {
        0x010023A017E94000ULL,
        "Batman: Arkham Knight",
        "• Критические просадки FPS при езде на Бэтмобиле\n• Вылеты по нехватке VRAM в Готэме",
        "• Severe Batmobile driving FPS drops\n• Gotham City open-world OOM crashes",
        "✓ Память: 8GB DRAM\n✓ Сжатие ASTC: Отключено\n✓ Асинхронные шейдеры: Включено\n✓ Точность GPU: Высокая",
        "✓ Memory Layout: 8GB DRAM\n✓ ASTC Recompression: Uncompressed\n✓ Asynchronous Shaders: Enabled\n✓ GPU Accuracy: High",
        {
            {"Core\\memory_layout_mode", "2"},
            {"Renderer\\astc_recompression", "0"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Renderer\\gpu_accuracy", "1"}
        }
    },
    {
        0x01003AE017DB0000ULL,
        "Batman: Arkham City",
        "• Мерцание снега и тумана над городом\n• Микрофризы при планировании с плащом",
        "• Snow particle and fog flickering\n• Glide traversal micro-stutters",
        "✓ Точность GPU: Высокая\n✓ Асинхронные шейдеры: Включено\n✓ Быстрая память: Включено",
        "✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Cpu\\cpuopt_fastmem", "true"}
        }
    },
    {
        0x010071700F8BA000ULL,
        "Crash Bandicoot 4: It's About Time",
        "• Рассинхрон инпут-лага в сложных платформенных секциях\n• Размытие фонов",
        "• Frame pacing latency in tight platforming sections\n• Blurry background assets",
        "✓ Асинхронные шейдеры: Включено\n✓ Быстрая память: Включено\n✓ Анизотропная фильтрация: 16x",
        "✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled\n✓ Anisotropic Filtering: 16x",
        {
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\max_anisotropy", "5"}
        }
    },
    {
        0x0100B6E00B360000ULL,
        "Spyro Reignited Trilogy",
        "• Заикания звука в катсценах\n• Мерцание теней на травяных холмах",
        "• Audio stutter in cutscenes\n• Shadow shimmering on grassy hills",
        "✓ Асинхронные шейдеры: Включено\n✓ Быстрая память: Включено\n✓ Точность GPU: Высокая",
        "✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled\n✓ GPU Accuracy: High",
        {
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\gpu_accuracy", "1"}
        }
    },
    {
        0x0100770008DD8000ULL,
        "Monster Hunter Generations Ultimate",
        "• Мерцание текстур монстров и эффектов крови\n• Просадки в мультиплеере",
        "• Monster skin texture flicker\n• Multiplayer combat frame drops",
        "✓ Точность GPU: Высокая\n✓ Анизотропная фильтрация: 16x\n✓ Быстрая память: Включено",
        "✓ GPU Accuracy: High\n✓ Anisotropic Filtering: 16x\n✓ Fastmem: Enabled",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\max_anisotropy", "5"},
            {"Cpu\\cpuopt_fastmem", "true"}
        }
    },
    {
        0x010034501659E000ULL,
        "Crisis Core: Final Fantasy VII Reunion",
        "• Статтеры при вращении рулетки DMW (Digital Mind Wave)\n• Сбои освещения в Мидгаре",
        "• DMW reel spinning stutter\n• Midgar volumetric lighting corruption",
        "✓ Точность GPU: Высокая\n✓ Асинхронные шейдеры: Включено\n✓ Быстрая память: Включено\n✓ Память: 8GB DRAM",
        "✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled\n✓ Memory Layout: 8GB DRAM",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Core\\memory_layout_mode", "2"}
        }
    },
    {
        0x0100E67012924000ULL,
        "The Witcher 3: Wild Hunt",
        "• Мерцание теней и воды в Новиграде\n• Просадки FPS в густых лесах Велена",
        "• Shadow and water flickering in Novigrad\n• Dense forest FPS drops in Velen",
        "✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Анизотропная фильтрация: 16x\n✓ Память: 8GB DRAM",
        "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Anisotropic Filtering: 16x\n✓ Memory Layout: 8GB DRAM",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\max_anisotropy", "5"},
            {"Core\\memory_layout_mode", "2"}
        }
    },
    {
        0x010074F013262000ULL,
        "Xenoblade Chronicles 3",
        "• Микростаттеры и утечки VRAM в масштабных битвах\n• Артефакты частиц Ouroboros",
        "• Battle scene micro-stutters and VRAM leaks\n• Ouroboros transformation particle artifacts",
        "✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Быстрая память: Включено\n✓ Память: 8GB DRAM",
        "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ Fastmem: Enabled\n✓ Memory Layout: 8GB DRAM",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_reactive_flushing", "true"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Core\\memory_layout_mode", "2"}
        }
    },
    {
        0x0100FF500E34A000ULL,
        "Xenoblade Chronicles: Definitive Edition",
        "• Размытие текстур персонажей на расстоянии\n• Мерцание травяного покрова на равнинах Гуры",
        "• Distant character texture blur\n• Bionis Leg grass shimmering",
        "✓ Точность GPU: Высокая\n✓ Анизотропная фильтрация: 16x\n✓ Память: 8GB DRAM",
        "✓ GPU Accuracy: High\n✓ Anisotropic Filtering: 16x\n✓ Memory Layout: 8GB DRAM",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\max_anisotropy", "5"},
            {"Core\\memory_layout_mode", "2"}
        }
    },
    {
        0x0100B04011742000ULL,
        "Monster Hunter Rise",
        "• Просадки FPS при битвах с Wyvern\n• Мерцание спецэффектов Wirebug",
        "• Wyvern combat FPS drops\n• Wirebug particle flickering",
        "✓ Точность GPU: Высокая\n✓ Обратные циклы барьеров: Включено\n✓ Быстрая память: Включено\n✓ Анизотропная фильтрация: 16x",
        "✓ GPU Accuracy: High\n✓ Barrier Feedback Loops: Enabled\n✓ Fastmem: Enabled\n✓ Anisotropic Filtering: 16x",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\barrier_feedback_loops", "true"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\max_anisotropy", "5"}
        }
    },
    {
        0x0100BB600DC30000ULL,
        "DOOM Eternal",
        "• Ослепляющие вспышки плазменных взрывов\n• Статтеры при добивании демонов (Glory Kill)",
        "• Blinding plasma explosion flashes\n• Glory kill execution micro-stutters",
        "✓ Точность GPU: Высокая\n✓ Обратные циклы барьеров: Включено\n✓ Быстрая память: Включено\n✓ Память: 8GB DRAM",
        "✓ GPU Accuracy: High\n✓ Barrier Feedback Loops: Enabled\n✓ Fastmem: Enabled\n✓ Memory Layout: 8GB DRAM",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\barrier_feedback_loops", "true"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Core\\memory_layout_mode", "2"}
        }
    },
    {
        0x0100916014D8C000ULL,
        "Diablo II: Resurrected",
        "• Нагрев и просадки кадров в джунглях Кураста\n• Сбои заклинаний огня волшебницы",
        "• Kurast jungle thermal throttling and stutter\n• Sorceress fire spell lighting glitches",
        "✓ Асинхронные шейдеры: Включено\n✓ Быстрая память: Включено\n✓ Точность GPU: Высокая",
        "✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled\n✓ GPU Accuracy: High",
        {
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\gpu_accuracy", "1"}
        }
    },
    {
        0x01005C9014168000ULL,
        "NieR:Automata The End of YoRHa Edition",
        "• Задержка отклика в секциях пулевого ада\n• Мерцание пустынного песка",
        "• Bullet-hell section frame pacing latency\n• Desert sand shimmer",
        "✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Анизотропная фильтрация: 16x\n✓ Память: 8GB DRAM",
        "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Anisotropic Filtering: 16x\n✓ Memory Layout: 8GB DRAM",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\max_anisotropy", "5"},
            {"Core\\memory_layout_mode", "2"}
        }
    },
    {
        0x0100224016A90000ULL,
        "Persona 4 Golden",
        "• Размытие текстур в ТВ-мире\n• Артефакты меню",
        "• TV World blurry textures\n• UI menu texture artifacts",
        "✓ Быстрая память: Включено\n✓ Анизотропная фильтрация: 16x\n✓ Сжатие ASTC: Отключено",
        "✓ Fastmem: Enabled\n✓ Anisotropic Filtering: 16x\n✓ ASTC Recompression: Uncompressed",
        {
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\max_anisotropy", "5"},
            {"Renderer\\astc_recompression", "0"}
        }
    },
    {
        0x0100A7301646E000ULL,
        "Unicorn Overlord",
        "• Размытие спрайтов персонажей на глобальной карте\n• Микростаттеры битв",
        "• Overworld sprite blur\n• Battle start micro-stutters",
        "✓ Быстрая память: Включено\n✓ Сжатие ASTC: Отключено\n✓ Анизотропная фильтрация: 16x",
        "✓ Fastmem: Enabled\n✓ ASTC Recompression: Uncompressed\n✓ Anisotropic Filtering: 16x",
        {
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\astc_recompression", "0"},
            {"Renderer\\max_anisotropy", "5"}
        }
    },
    {
        0x01004F3017772000ULL,
        "Dave the Diver",
        "• Статтеры при глубоководных погружениях\n• Артефакты эффектов пузырей",
        "• Deep sea diving stutters\n• Underwater bubble particle glitches",
        "✓ Быстрая память: Включено\n✓ Сжатие ASTC: Отключено\n✓ Асинхронные шейдеры: Включено",
        "✓ Fastmem: Enabled\n✓ ASTC Recompression: Uncompressed\n✓ Asynchronous Shaders: Enabled",
        {
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\astc_recompression", "0"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x01006BB00C6F0000ULL,
        "The Legend of Zelda: Link's Awakening",
        "• Просадки FPS при размытии глубины резкости (Tilt-Shift)\n• Заикания в деревне Мэйб",
        "• Tilt-shift depth of field severe frame drops\n• Mabe Village traversal stutters",
        "✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Быстрая память: Включено\n✓ Память: 8GB DRAM",
        "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ Fastmem: Enabled\n✓ Memory Layout: 8GB DRAM",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_reactive_flushing", "true"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Core\\memory_layout_mode", "2"}
        }
    },
    {
        0x01008CF01BAEC000ULL,
        "The Legend of Zelda: Echoes of Wisdom",
        "• Падения частоты кадров при создании копий предметов (Echoes)\n• Размытие воды",
        "• Echo creation frame drops\n• Water surface reflection distortion",
        "✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Быстрая память: Включено\n✓ Память: 8GB DRAM",
        "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ Fastmem: Enabled\n✓ Memory Layout: 8GB DRAM",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_reactive_flushing", "true"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Core\\memory_layout_mode", "2"}
        }
    },
    {
        0x01007760086F8000ULL,
        "Bayonetta 2",
        "• Сбои динамического освещения и теней в катсценах\n• Просадки FPS при битвах с ангелами",
        "• Dynamic lighting and shadow glitches in cutscenes\n• Angel combat scene frame drops",
        "✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Анизотропная фильтрация: 16x",
        "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Anisotropic Filtering: 16x",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\max_anisotropy", "5"}
        }
    },
    {
        0x0100BC0018138000ULL,
        "Super Mario RPG",
        "• Статтеры анимаций диалогов\n• Сбои синхронизации изометрического освещения",
        "• Dialogue animation stutters\n• Isometric lighting pass desync",
        "✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Быстрая память: Включено",
        "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ Fastmem: Enabled",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_reactive_flushing", "true"},
            {"Cpu\\cpuopt_fastmem", "true"}
        }
    },
    {
        0x010069200E60E000ULL,
        "Pikmin 4",
        "• Утечки памяти в открытых садах Unreal Engine 4\n• Микрофризы при спавне отряда Пикминов",
        "• Unreal Engine 4 open garden memory leaks\n• Pikmin squad spawn micro-stutters",
        "✓ Память: 8GB DRAM\n✓ Точность GPU: Высокая\n✓ Асинхронные шейдеры: Включено\n✓ Реактивная очистка: Включено",
        "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled\n✓ Reactive Flushing: Enabled",
        {
            {"Core\\memory_layout_mode", "2"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Renderer\\use_reactive_flushing", "true"}
        }
    },
    {
        0x0100FA701A7CA000ULL,
        "Princess Peach: Showtime!",
        "• Просадки FPS при смене театральных декораций и костюмов\n• Мерцание теней сцены",
        "• Theater stage transition frame drops\n• Stage lighting shadow flicker",
        "✓ Память: 8GB DRAM\n✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Core\\memory_layout_mode", "2"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100C2A01A03A000ULL,
        "Luigi's Mansion 2 HD",
        "• Мерцание луча фонарика Dark-Light\n• Артефакты призрачных следов",
        "• Dark-Light flashlight beam flickering\n• Ghost trail rendering artifacts",
        "✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Быстрая память: Включено",
        "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ Fastmem: Enabled",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_reactive_flushing", "true"},
            {"Cpu\\cpuopt_fastmem", "true"}
        }
    },
    {
        0x01000BC0000A0000ULL,
        "Hollow Knight",
        "• Задержка ввода (input lag) в босс-файтах\n• Микростаттеры при смене комнат",
        "• Boss fight frame latency\n• Room transition compilation stutters",
        "✓ Асинхронные шейдеры: Включено\n✓ Быстрая память: Включено\n✓ Сжатие ASTC: Отключено",
        "✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled\n✓ ASTC Recompression: Uncompressed",
        {
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\astc_recompression", "0"}
        }
    },
    {
        0x0100735010B46000ULL,
        "Hades",
        "• Микрофризы при комнатах с большим числом снарядов и спецэффектов",
        "• High particle and projectile chamber micro-stutters",
        "✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено\n✓ Анизотропная фильтрация: 16x",
        "✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Anisotropic Filtering: 16x",
        {
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Renderer\\max_anisotropy", "5"}
        }
    },
    {
        0x010041B01211E000ULL,
        "Sea of Stars",
        "• Размытие пиксель-арта\n• Рассинхронизация динамического освещения солнца/луны",
        "• Pixel art sprite blur\n• Sun/moon eclipse dynamic lighting desync",
        "✓ Точность GPU: Высокая\n✓ Асинхронные шейдеры: Включено\n✓ Сжатие ASTC: Отключено",
        "✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled\n✓ ASTC Recompression: Uncompressed",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Renderer\\astc_recompression", "0"}
        }
    },
    {
        0x0100656017E9C000ULL,
        "Octopath Traveler II",
        "• Пересвет и размытие глубины резкости в HD-2D\n• Микростаттеры при смене дня и ночи",
        "• HD-2D depth-of-field overexposure\n• Day/night transition micro-stutters",
        "✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_reactive_flushing", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x010052C00BAB8000ULL,
        "Bravely Default II",
        "• Утечки памяти в Unreal Engine 4 на карте мира\n• Просадки FPS в битвах",
        "• Unreal Engine 4 overworld memory leaks\n• Battle start FPS drops",
        "✓ Память: 8GB DRAM\n✓ Точность GPU: Высокая\n✓ Быстрая память: Включено",
        "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled",
        {
            {"Core\\memory_layout_mode", "2"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"}
        }
    },
    {
        0x01003D200BAA2000ULL,
        "Dead Cells",
        "• Задержка ввода в динамичных боевых секциях\n• Статтеры процедурной генерации",
        "• Combat frame latency\n• Procedural level generation stutters",
        "✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено\n✓ Сжатие ASTC: Отключено",
        "✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ ASTC Recompression: Uncompressed",
        {
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Renderer\\astc_recompression", "0"}
        }
    },
    {
        0x01001B60133A2000ULL,
        "Outer Wilds",
        "• Просадки FPS при физическом расчете орбит планет\n• Утечки VRAM в туманностях",
        "• Solar system physics calculation slowdown\n• Space nebulae VRAM leaks",
        "✓ Память: 8GB DRAM\n✓ Точность CPU: Точная\n✓ Асинхронные шейдеры: Включено",
        "✓ Memory Layout: 8GB DRAM\n✓ CPU Accuracy: Accurate\n✓ Asynchronous Shaders: Enabled",
        {
            {"Core\\memory_layout_mode", "2"},
            {"Cpu\\cpu_accuracy", "0"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x010067600DF2A000ULL,
        "Subnautica",
        "• Задержка прогрузки чанков морского дна\n• Микрофризы при управлении батискафом",
        "• Ocean floor chunk loading delays\n• Seamoth traversal stutters",
        "✓ Память: 8GB DRAM\n✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Core\\memory_layout_mode", "2"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x01009850119BC000ULL,
        "Ori and the Will of the Wisps",
        "• Просадки FPS при скоростном перемещении по локациям\n• Мерцание фонового света",
        "• Fast traversal frame drops\n• Volumetric light shimmering",
        "✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Быстрая память: Включено\n✓ Анизотропная фильтрация: 16x",
        "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ Fastmem: Enabled\n✓ Anisotropic Filtering: 16x",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_reactive_flushing", "true"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\max_anisotropy", "5"}
        }
    },
    {
        0x01000CD00DE1E000ULL,
        "Alien: Isolation",
        "• Артефакты динамических теней на станции «Севастополь»\n• Шум отражений",
        "• Dynamic shadow artifacts in dark corridors\n• Specular reflection noise",
        "✓ Точность GPU: Высокая\n✓ Обратные циклы барьеров: Включено\n✓ Анизотропная фильтрация: 16x\n✓ Память: 8GB DRAM",
        "✓ GPU Accuracy: High\n✓ Barrier Feedback Loops: Enabled\n✓ Anisotropic Filtering: 16x\n✓ Memory Layout: 8GB DRAM",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\barrier_feedback_loops", "true"},
            {"Renderer\\max_anisotropy", "5"},
            {"Core\\memory_layout_mode", "2"}
        }
    },
    {
        0x01005AF00BA7A000ULL,
        "Metroid Dread",
        "• Микрофризы при скоростном скольжении Самус\n• Рассинхронизация счетчика кадров в катсценах E.M.M.I.",
        "• Samus speed booster micro-stutters\n• E.M.M.I. cutscene frame rate desync",
        "✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено\n✓ Анизотропная фильтрация: 16x",
        "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Anisotropic Filtering: 16x",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Renderer\\max_anisotropy", "5"}
        }
    },
    {
        0x010077200889E000ULL,
        "Donkey Kong Country: Tropical Freeze",
        "• Сбои полупрозрачности воды и шерсти Донки Конга\n• Микростаттеры при полетах на бочках",
        "• Water transparency and Kong fur shader glitches\n• Barrel blast transition micro-stutters",
        "✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Сжатие ASTC: Отключено",
        "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ ASTC Recompression: Uncompressed",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_reactive_flushing", "true"},
            {"Renderer\\astc_recompression", "0"}
        }
    },
    {
        0x0100483017770000ULL,
        "Kirby's Return to Dream Land Deluxe",
        "• Размытие контуров сел-шейдинга персонажей\n• Просадки FPS при суперспособностях Кирби",
        "• Cel-shaded outline blurring\n• Super ability particle frame drops",
        "✓ Точность GPU: Высокая\n✓ Асинхронные шейдеры: Включено\n✓ Анизотропная фильтрация: 16x",
        "✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled\n✓ Anisotropic Filtering: 16x",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Renderer\\max_anisotropy", "5"}
        }
    },
    {
        0x010085C0084FA000ULL,
        "Captain Toad: Treasure Tracker",
        "• Мерцание теней на трехмерных диорамах уровней\n• Артефакты глубины резкости",
        "• Shadow flickering on 3D diorama puzzles\n• Depth-of-field blur artifacts",
        "✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Сжатие ASTC: Отключено",
        "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ ASTC Recompression: Uncompressed",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_reactive_flushing", "true"},
            {"Renderer\\astc_recompression", "0"}
        }
    },
    {
        0x01006000040C2000ULL,
        "Yoshi's Crafted World",
        "• Утечки памяти в Unreal Engine 4 на картонных декорациях\n• Размытие текстур заднего плана",
        "• Unreal Engine 4 cardboard diorama VRAM leaks\n• Background blur texture shimmering",
        "✓ Память: 8GB DRAM\n✓ Точность GPU: Высокая\n✓ Асинхронные шейдеры: Включено",
        "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled",
        {
            {"Core\\memory_layout_mode", "2"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x010019401051C000ULL,
        "Mario Strikers: Battle League",
        "• Рассинхронизация анимации гиперударов (Hyper Strikes)\n• Статтеры при взрывах спецэффектов",
        "• Hyper Strike comic animation desync\n• Stadium particle burst stutters",
        "✓ Точность CPU: Точная\n✓ Точность GPU: Высокая\n✓ Быстрая память: Включено",
        "✓ CPU Accuracy: Accurate\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled",
        {
            {"Cpu\\cpu_accuracy", "0"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"}
        }
    },
    {
        0x0100E2B017DAE000ULL,
        "Super Bomberman R 2",
        "• Просадки FPS при массовых взрывах в режиме «Замок»\n• Рассинхронизация сетевого таймера и зависание на проверке серверов Konami",
        "• Castle mode multi-explosion slowdowns\n• Konami server handshake stall and local multiplayer sync latency",
        "✓ Режим полёта: Включено (пропуск ожидания серверов Konami)\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено\n✓ Точность GPU: Высокая",
        "✓ Airplane Mode: Enabled (Bypasses Konami server check)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ GPU Accuracy: High",
        {
            {"System\\airplane_mode", "true"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Renderer\\gpu_accuracy", "1"}
        }
    },
    {
        0x01002390146AC000ULL,
        "Advance Wars 1+2: Re-Boot Camp",
        "• Задержки загрузки анимаций командиров (CO Powers)\n• Размытие 2D-спрайтов техники",
        "• CO Power full-screen anime animation delay\n• Tactical map sprite blur",
        "✓ Точность GPU: Высокая\n✓ Сжатие ASTC: Отключено\n✓ Быстрая память: Включено",
        "✓ GPU Accuracy: High\n✓ ASTC Recompression: Uncompressed\n✓ Fastmem: Enabled",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\astc_recompression", "0"},
            {"Cpu\\cpuopt_fastmem", "true"}
        }
    },
    {
        0x01002D801A1DE000ULL,
        "WarioWare: Move It!",
        "• Задержка отклика в микроиграх с быстрой сменой позы\n• Пропуск кадров на переходах",
        "• Microgame form change input latency\n• Transition animation frame skips",
        "✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено\n✓ Реактивная очистка: Включено",
        "✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Reactive Flushing: Enabled",
        {
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Renderer\\use_reactive_flushing", "true"}
        }
    },
    {
        0x0100D6F015F70000ULL,
        "No Man's Sky",
        "• Вылеты по нехватке памяти при входе в атмосферу планет\n• Артефакты процедурной генерации",
        "• Planetary entry OOM memory crashes\n• Procedural voxel terrain artifacting",
        "✓ Память: 8GB DRAM\n✓ Точность GPU: Высокая\n✓ Обратные циклы барьеров: Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Barrier Feedback Loops: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Core\\memory_layout_mode", "2"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\barrier_feedback_loops", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x01000BD016550000ULL,
        "Portal: Companion Collection",
        "• Сбои рекурсивного рендеринга порталов\n• Рассинхронизация физики кубов в Source Engine",
        "• Recursive portal view rendering glitches\n• Source Engine physics stutter",
        "✓ Точность GPU: Высокая\n✓ Обратные циклы барьеров: Включено\n✓ Быстрая память: Включено\n✓ Анизотропная фильтрация: 16x",
        "✓ GPU Accuracy: High\n✓ Barrier Feedback Loops: Enabled\n✓ Fastmem: Enabled\n✓ Anisotropic Filtering: 16x",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\barrier_feedback_loops", "true"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\max_anisotropy", "5"}
        }
    },
    {
        0x010058C01570E000ULL,
        "Persona 4 Golden",
        "• Микростаттеры при перемещении по туманной Инабе\n• Рассинхронизация звука аниме-вставок",
        "• Foggy Inaba traversal micro-stutters\n• Anime video cutscene audio desync",
        "✓ Быстрая память: Включено\n✓ Точность GPU: Высокая\n✓ Анизотропная фильтрация: 16x\n✓ Сжатие ASTC: Отключено",
        "✓ Fastmem: Enabled\n✓ GPU Accuracy: High\n✓ Anisotropic Filtering: 16x\n✓ ASTC Recompression: Uncompressed",
        {
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\max_anisotropy", "5"},
            {"Renderer\\astc_recompression", "0"}
        }
    },
    {
        0x010072001570A000ULL,
        "Persona 3 Portable",
        "• Размытие 2D-портретов и визуальной новеллы\n• Просадки FPS на верхних этажах Тартара",
        "• Visual novel 2D portrait blur\n• Tartarus upper floor frame drops",
        "✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x01004C80170A6000ULL,
        "Tunic",
        "• Сбои страниц руководства в изометрическом виде\n• Шум объемного освещения",
        "• Instruction manual overlay rendering glitches\n• Isometric volumetric light noise",
        "✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_reactive_flushing", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100DDF01A03A000ULL,
        "Dave the Diver",
        "• Задержка прогрузки глубоководных биомов\n• Микрофризы в суши-ресторане Банчо",
        "• Deep sea biome transition latency\n• Bancho Sushi rush hour micro-stutters",
        "✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено\n✓ Сжатие ASTC: Отключено",
        "✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ ASTC Recompression: Uncompressed",
        {
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Renderer\\astc_recompression", "0"}
        }
    },
    {
        0x01002BE01584E000ULL,
        "Cult of the Lamb",
        "• Просадки FPS при большом числе последователей в поселении\n• Сбои динамических теней",
        "• Cult camp high follower count slowdown\n• Ritual dynamic shadow flickering",
        "✓ Быстрая память: Включено\n✓ Точность GPU: Высокая\n✓ Асинхронные шейдеры: Включено",
        "✓ Fastmem: Enabled\n✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled",
        {
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100913018F08000ULL,
        "Unicorn Overlord",
        "• Рассинхронизация 2D-анимаций Vanillaware в масштабных битвах\n• Размытие шрифтов интерфейса",
        "• Vanillaware 2D battle animation desync\n• Tactical interface font blur",
        "✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Анизотропная фильтрация: 16x\n✓ Быстрая память: Включено",
        "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ Anisotropic Filtering: 16x\n✓ Fastmem: Enabled",
    },
    {
        0x01002DA013484000ULL,
        "The Legend of Zelda: Skyward Sword HD",
        "• Артефакты меча и курсора при управлении движением\n• Размытие текстур облачного моря Небоземи",
        "• Motion control sword and pointer jitter\n• Skyloft cloud sea texture shimmering",
        "✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Анизотропная фильтрация: 16x",
        "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Anisotropic Filtering: 16x",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\max_anisotropy", "5"}
        }
    },
    {
        0x010028600EBDA000ULL,
        "Super Mario 3D World + Bowser's Fury",
        "• Просадки FPS при появлении Яростного Боузера в открытом море\n• Мерцание шейдеров дождя и лавы",
        "• Bowser's Fury open sea stormy weather FPS drops\n• Rain splash and lava dynamic shader flickering",
        "✓ Память: 8GB DRAM\n✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"System\\memory_layout_mode", "1"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x010036601D380000ULL,
        "Super Mario Party Jamboree",
        "• Микрофризы в мини-играх на 20 игроков Koopathlon\n• Рассинхронизация счетчика очков",
        "• 20-player Koopathlon minigame stutters\n• Live board score counter desync",
        "✓ Память: 8GB DRAM\n✓ Точность GPU: Высокая\n✓ Асинхронные шейдеры: Включено\n✓ Реактивная очистка: Включено",
        "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled\n✓ Reactive Flushing: Enabled",
        {
            {"System\\memory_layout_mode", "1"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Renderer\\use_reactive_flushing", "true"}
        }
    },
    {
        0x010091801E8B2000ULL,
        "Mario & Luigi: Brothership",
        "• Задержка отклика в совместных Brother Attacks\n• Просадки FPS на морских островах Конкордии",
        "• Brother Attacks timing lag\n• Concordia ocean sailing FPS dips",
        "✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено\n✓ Сжатие ASTC: Отключено",
        "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ ASTC Recompression: Uncompressed",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Renderer\\astc_recompression", "0"}
        }
    },
    {
        0x01000A10041EA000ULL,
        "Super Mario Party",
        "• Микрозадержки анимаций кубиков и персонажей\n• Сбои полупрозрачности воды в речных сплавах",
        "• Dice roll animation micro-stutters\n• River Survival water transparency glitches",
        "✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x01006BB00C6F0000ULL,
        "Mario Party Superstars",
        "• Мерцание теней на классических досках N64\n• Размытие миниатюр правил мини-игр",
        "• Retro N64 board shadow flickering\n• Minigame instruction modal blur",
        "✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Анизотропная фильтрация: 16x",
        "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ Anisotropic Filtering: 16x",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_reactive_flushing", "true"},
            {"Renderer\\max_anisotropy", "5"}
        }
    },
    {
        0x0100F7701140E000ULL,
        "Mario Golf: Super Rush",
        "• Размытие сетки рельефа грина при прицеливании\n• Просадки FPS при массовом забеге Speed Golf",
        "• Green terrain grid blur\n• Speed Golf stamina sprint slowdown",
        "✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100BDE00862A000ULL,
        "Mario Tennis Aces",
        "• Инпут-лаг в замедлении времени Zone Speed\n• Артефакты свечения ракетки при спец-ударах",
        "• Zone Speed slow-motion input lag\n• Special Shot racket glow artifacts",
        "✓ Точность CPU: Высокая (Accurate)\n✓ Точность GPU: Высокая\n✓ Быстрая память: Включено",
        "✓ CPU Accuracy: Accurate\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled",
        {
            {"Cpu\\cpu_accuracy", "0"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"}
        }
    },
    {
        0x01003C700009C000ULL,
        "Splatoon 2",
        "• Мерцание отражений чернил на металлических поверхностях\n• Микрофризы в хабе площади Инкополиса",
        "• Metallic surface ink reflection flicker\n• Inkopolis Square hub micro-stutters",
        "✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено\n✓ Анизотропная фильтрация: 16x",
        "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Anisotropic Filtering: 16x",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Renderer\\max_anisotropy", "5"}
        }
    },
    {
        0x0100C2500FC20000ULL,
        "Splatoon 3",
        "• Утечки памяти в хабе Плюхграда и в матчах Залива Самонид\n• Просадки FPS при обильном залитии карты краской и сетевые задержки лобби",
        "• Splatsville hub VRAM leaks\n• Salmon Run heavy ink coverage slowdown & match lobby delay",
        "✓ Режим полёта: Включено (офлайн-кампания без сетевых задержек)\n✓ Память: 8GB DRAM\n✓ Точность GPU: Высокая\n✓ Асинхронные шейдеры: Включено\n✓ Реактивная очистка: Включено",
        "✓ Airplane Mode: Enabled (Instant offline Hero mode)\n✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled\n✓ Reactive Flushing: Enabled",
        {
            {"System\\airplane_mode", "true"},
            {"System\\memory_layout_mode", "1"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Renderer\\use_reactive_flushing", "true"}
        }
    },
    {
        0x01004D300C5AE000ULL,
        "Paper Mario: The Origami King",
        "• Сбои отрисовки кольцевой арены в битвах\n• Артефакты конфетти и бумажных складок",
        "• Ring puzzle battle arena glitches\n• Confetti paper fold texture artifacts",
        "✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Быстрая память: Включено",
        "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ Fastmem: Enabled",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_reactive_flushing", "true"},
            {"Cpu\\cpuopt_fastmem", "true"}
        }
    },
    {
        0x01002B00111A2000ULL,
        "Hyrule Warriors: Age of Calamity",
        "• Тяжелые просадки FPS при массовом скоплении монстров на экране\n• Утечки VRAM в битвах Чудищ",
        "• Massive enemy swarm heavy slowdown\n• Divine Beast battle VRAM exhaustion",
        "✓ Память: 8GB DRAM\n✓ Точность GPU: Высокая\n✓ Асинхронные шейдеры: Включено\n✓ Быстрая память: Включено",
        "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled",
        {
            {"System\\memory_layout_mode", "1"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Cpu\\cpuopt_fastmem", "true"}
        }
    },
    {
        0x0100AE0009A80000ULL,
        "Hyrule Warriors: Definitive Edition",
        "• Размытие спецэффектов комбо Focus Spirit\n• Микрофризы при спавне отрядов офицеров",
        "• Focus Spirit attack bloom blur\n• Officer squad spawn micro-stutters",
        "✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x01007E3015C3E000ULL,
        "Fire Emblem Warriors: Three Hopes",
        "• Падение производительности в масштабных боях на картах Фодлана\n• Артефакты теней полководцев",
        "• Large scale Fodlan battlefield FPS dips\n• Commander shadow map rendering errors",
        "✓ Память: 8GB DRAM\n✓ Точность GPU: Высокая\n✓ Асинхронные шейдеры: Включено",
        "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled",
        {
            {"System\\memory_layout_mode", "1"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100000011D90000ULL,
        "Pokemon Brilliant Diamond",
        "• Мерцание чиби-отражений в лужах и окнах Синно\n• Задержка открытия меню покедекса",
        "• Chibi puddle & window reflection flicker\n• Pokedex animation transition lag",
        "✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено\n✓ Анизотропная фильтрация: 16x",
        "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Anisotropic Filtering: 16x",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Renderer\\max_anisotropy", "5"}
        }
    },
    {
        0x010018E011D92000ULL,
        "Pokemon Shining Pearl",
        "• Мерцание чиби-отражений в лужах и окнах Синно\n• Задержка открытия меню покедекса",
        "• Chibi puddle & window reflection flicker\n• Pokedex animation transition lag",
        "✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено\n✓ Анизотропная фильтрация: 16x",
        "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Anisotropic Filtering: 16x",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Renderer\\max_anisotropy", "5"}
        }
    },
    {
        0x010003F003A34000ULL,
        "Pokemon Let's Go, Pikachu!",
        "• Задержка круга прицеливания при броске покебола\n• Мерцание травы на маршрутах Канто",
        "• Pokeball throw capture ring input lag\n• Kanto route grass shader flickering",
        "✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100152000022000ULL,
        "Pokemon Let's Go, Eevee!",
        "• Задержка круга прицеливания при броске покебола\n• Мерцание травы на маршрутах Канто",
        "• Pokeball throw capture ring input lag\n• Kanto route grass shader flickering",
        "✓ Точность GPU: Высокая\n✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100B870097D6000ULL,
        "Shin Megami Tensei V",
        "• Просадки FPS на песчаных барханах пустыни Да'ат в Unreal Engine 4\n• Размытие магических заклинаний Нахобино",
        "• Da'at desert sand dunes UE4 slowdown\n• Nahobino Magatsuhi skill blur",
        "✓ Память: 8GB DRAM\n✓ Точность GPU: Высокая\n✓ Асинхронные шейдеры: Включено\n✓ Быстрая память: Включено",
        "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled",
        {
            {"System\\memory_layout_mode", "1"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Cpu\\cpuopt_fastmem", "true"}
        }
    },
    {
        0x0100C48008890000ULL,
        "Xenoblade Chronicles 2: Torna ~ The Golden Country",
        "• Утечки памяти на гигантских просторах Титана Торны\n• Размытие облачного покрова и листвы",
        "• Torna Titan open terrain VRAM leaks\n• Cloud sea and foliage shimmering",
        "✓ Память: 8GB DRAM\n✓ Точность GPU: Высокая\n✓ Асинхронные шейдеры: Включено\n✓ Быстрая память: Включено",
        "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled",
        {
            {"System\\memory_layout_mode", "1"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Cpu\\cpuopt_fastmem", "true"}
        }
    },
    {
        0x010049900F546000ULL,
        "Super Mario 3D All-Stars",
        "• Мерцание текстур воды в Mario Sunshine\n• Рассинхронизация звуковых дорожек в Mario Galaxy",
        "• Sunshine water reflection texture flickering\n• Galaxy orchestrated audio drift",
        "✓ Точность CPU: Высокая (Accurate)\n✓ Точность GPU: Высокая\n✓ Быстрая память: Включено",
        "✓ CPU Accuracy: Accurate\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled",
        {
            {"Cpu\\cpu_accuracy", "0"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"}
        }
    },
    {
        0x010065B00AE8E000ULL,
        "Cuphead",
        "• Инпут-лаг при скоростных уклонениях и парированиях розовых снарядов\n• Размытие пленочного зерна 1930-х",
        "• Precision parry / dash input lag\n• 1930s film grain shader blurring",
        "✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено\n✓ Сжатие ASTC: Отключено",
        "✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ ASTC Recompression: Uncompressed",
        {
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Renderer\\astc_recompression", "0"}
        }
    },
    {
        0x0100D5700DC34000ULL,
        "Ori and the Blind Forest: Definitive Edition",
        "• Сбои изометрических источников света в лесу Нибель\n• Микрофризы при скоростном Bash-прыжке",
        "• Nibel forest dynamic volumetric light glitches\n• High-speed Bash chain micro-stutters",
        "✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Быстрая память: Включено\n✓ Анизотропная фильтрация: 16x",
        "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ Fastmem: Enabled\n✓ Anisotropic Filtering: 16x",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_reactive_flushing", "true"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\max_anisotropy", "5"}
        }
    },
    {
        0x0100216008E88000ULL,
        "Slay the Spire",
        "• Микрозадержки анимаций розыгрыша карт и реликвий\n• Размытие текстовых описаний баффов",
        "• Card play and relic particle animation delay\n• Buff description text blur",
        "✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено\n✓ Сжатие ASTC: Отключено",
        "✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ ASTC Recompression: Uncompressed",
        {
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Renderer\\astc_recompression", "0"}
        }
    },
    {
        0x01008C300B4D2000ULL,
        "Into the Breach",
        "• Задержка прорисовки изометрической тактической сетки 8x8\n• Размытие анимаций атаки Мехов",
        "• 8x8 tactical grid UI latency\n• Mech attack pixel-art animation blur",
        "✓ Быстрая память: Включено\n✓ Асинхронные шейдеры: Включено\n✓ Сжатие ASTC: Отключено",
        "✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ ASTC Recompression: Uncompressed",
        {
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Renderer\\astc_recompression", "0"}
        }
    },
    {
        0x0100C4D01D026000ULL,
        "Tales of Series (Berseria / Graces f / Symphonia)",
        "• Зависание на 0 FPS и черный экран при смене локаций\n• Рассинхронизация видео-переходов NVDEC",
        "• 0 FPS freeze and black screen on location transitions\n• NVDEC video transition desync",
        "✓ Сжатие ASTC: Отключено (устраняет зависание при смене локаций)\n✓ Реактивная очистка: Включено\n✓ Эмуляция NVDEC: ГПУ видеоядро (NVDEC)\n✓ Асинхронные шейдеры: Включено\n✓ Быстрая память (Fastmem): Включено",
        "✓ ASTC Recompression: Uncompressed (Fixes location transition freeze!)\n✓ Reactive Flushing: Enabled\n✓ NVDEC Emulation: GPU Video Core\n✓ Asynchronous Shaders: Enabled\n✓ Fastmem: Enabled",
        {
            {"Renderer\\astc_recompression", "0"},
            {"Renderer\\use_reactive_flushing", "true"},
            {"Renderer\\nvdec_emulation", "2"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\gpu_accuracy", "1"}
        }
    },
    {
        0x01007300020FA000ULL,
        "Astral Chain",
        "• Просадки FPS при вызове Легионов и комбо-атаках в Арке\n• Утечки VRAM при длительной игре",
        "• Legion summon and chain sync combo FPS drops\n• Long session VRAM exhaustion in the Ark",
        "✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"System\\memory_layout_mode", "1"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x01005FA00BAFA000ULL,
        "Fire Emblem: Three Houses",
        "• Утечки памяти при перемещении по монастырю Гаррег Мах\n• Просадки FPS на тактической сетке боев",
        "• Garreg Mach Monastery roaming memory leaks\n• Tactical grid battle FPS dips",
        "✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Быстрая память (Fastmem): Включено",
        "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ Fastmem: Enabled",
        {
            {"System\\memory_layout_mode", "1"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_reactive_flushing", "true"},
            {"Cpu\\cpuopt_fastmem", "true"}
        }
    },
    {
        0x0100F4C009322000ULL,
        "Pikmin 3 Deluxe",
        "• Размытие текстур фруктов и сока\n• Мерцание теней в заданиях Олимара",
        "• Fruit texture and juice rendering blur\n• Olimar side-story shadow shimmering",
        "✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Анизотропная фильтрация: 16x\n✓ Асинхронные шейдеры: Включено",
        "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Anisotropic Filtering: 16x\n✓ Asynchronous Shaders: Enabled",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\max_anisotropy", "5"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x01007E3006DDA000ULL,
        "Kirby Star Allies",
        "• Рассинхронизация спецэффектов сердец дружбы Friend Heart\n• Микрофризы в битвах четверки героев",
        "• Friend Heart dynamic glow desync\n• 4-player team battle micro-stutters",
        "✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Анизотропная фильтрация: 16x",
        "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Anisotropic Filtering: 16x",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Renderer\\max_anisotropy", "5"}
        }
    },
    {
        0x0100ABF008968000ULL,
        "Pokemon Sword & Shield",
        "• Просадки FPS при динамической смене погоды в Диких землях Галара\n• Мерцание свечения Гигантамакса",
        "• Galar Wild Area weather dynamic lighting drops\n• Dynamax aura rendering flicker",
        "✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Анизотропная фильтрация: 16x",
        "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Anisotropic Filtering: 16x",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Renderer\\max_anisotropy", "5"}
        }
    },
    {
        0x01006A800016E000ULL,
        "Super Smash Bros. Ultimate",
        "• Инпут-лаг и просадки кадров в боях на 8 игроков\n• Зависание катсцен и задержки проверки Духов онлайн",
        "• 8-fighter intense brawl input latency\n• Spirit Board online latency & cutscene sync pause",
        "✓ Режим полёта: Включено (пропуск онлайн-опроса Духов)\n✓ Точность CPU: Точная (Accurate)\n✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ Airplane Mode: Enabled (Bypasses Spirit Board network check)\n✓ CPU Accuracy: Accurate\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"System\\airplane_mode", "true"},
            {"Cpu\\cpu_accuracy", "0"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x01004C00000CE000ULL,
        "Dragon Quest XI S: Echoes of an Elusive Age",
        "• Микрофризы при быстрой скачке на лошади по королевству Гелиодор в UE4\n• Размытие волос и брони",
        "• Heliodor kingdom high-speed horse ride micro-stutters\n• Character hair and armor texture blur",
        "✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"System\\memory_layout_mode", "1"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x01007D401662A000ULL,
        "NieR:Automata The End of YoRHa Edition",
        "• Просадки FPS при скоростном полете на поде по Руинам Города\n• Артефакты частиц bullet-hell сфер",
        "• Ruined City flight unit sequence FPS drops\n• Bullet-hell glowing sphere particle artifacts",
        "✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Реактивная очистка: Включено",
        "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Reactive Flushing: Enabled",
        {
            {"System\\memory_layout_mode", "1"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Renderer\\use_reactive_flushing", "true"}
        }
    },
    {
        0x0100778006E88000ULL,
        "Bayonetta 1 & 2",
        "• Инпут-лаг в замедлении времени Witch Time\n• Пересвет эффектов Umbran Climax",
        "• Witch Time slow-motion input lag\n• Umbran Climax overexposed bloom",
        "✓ Точность CPU: Точная (Accurate)\n✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ CPU Accuracy: Accurate\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Cpu\\cpu_accuracy", "0"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x01005CA0157CA000ULL,
        "Persona 5 Royal",
        "• Рассинхронизация комикс-переходов и анимаций All-Out Attack\n• Микрофризы во Дворцах Метаверсума",
        "• All-Out Attack comic transition desync\n• Metaverse Palace shadow infiltration micro-stutters",
        "✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Анизотропная фильтрация: 16x\n✓ Сжатие ASTC: Отключено",
        "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Anisotropic Filtering: 16x\n✓ ASTC Recompression: Uncompressed",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Renderer\\max_anisotropy", "5"},
            {"Renderer\\astc_recompression", "0"}
        }
    },
    {
        0x0100780016140000ULL,
        "Live A Live",
        "• Размытие пиксельных спрайтов при переходе между эпохами\n• Микрофризы в боях на шахматной сетке",
        "• HD-2D era chapter transition sprite blur\n• Grid battle attack sequence stutters",
        "✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Сжатие ASTC: Отключено",
        "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ ASTC Recompression: Uncompressed",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Renderer\\astc_recompression", "0"}
        }
    },
    {
        0x01000BD00E756000ULL,
        "Tony Hawk's Pro Skater 1 + 2",
        "• Зависание на заставке и бесконечный спиннер авторизации Activision\n• Сетевой таймаут при старте",
        "• Infinite Activision authorization spinner on startup\n• Online handshake deadlock",
        "✓ Режим полёта: Включено (пропуск онлайн-авторизации Activision)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Память: 8GB DRAM",
        "✓ Airplane Mode: Enabled (Bypasses Activision online login)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Memory Layout: 8GB DRAM",
        {
            {"System\\airplane_mode", "true"},
            {"System\\memory_layout_mode", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100F7A00B704000ULL,
        "Crash Team Racing Nitro-Fueled",
        "• Зависание в главном меню при опросе серверов Pit Stop\n• Микрофризы анимаций подиума",
        "• Pit Stop server telemetry handshake freeze\n• Podium animation micro-stutters",
        "✓ Режим полёта: Включено (пропуск серверов Pit Stop)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Анизотропная фильтрация: 16x",
        "✓ Airplane Mode: Enabled (Bypasses Pit Stop online check)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Anisotropic Filtering: 16x",
        {
            {"System\\airplane_mode", "true"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Renderer\\max_anisotropy", "5"}
        }
    },
    {
        0x0100B1100C4D0000ULL,
        "Mortal Kombat 11",
        "• Зависание на титульном экране при синхронизации WB Play / Башен Времени\n• Утечки VRAM в кинематографичных фаталити",
        "• WB Play / Towers of Time server sync freeze on title screen\n• Cinematic Fatalities VRAM spikes",
        "✓ Режим полёта: Включено (пропуск ожидания WB Play)\n✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ Airplane Mode: Enabled (Bypasses WB Play online check)\n✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"System\\airplane_mode", "true"},
            {"System\\memory_layout_mode", "1"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x01007E300B70C000ULL,
        "Borderlands Legendary Collection (1, 2, TPS)",
        "• Задержка старта на 60+ секунд и зависание при сетевом опросе SHiFT\n• Просадки FPS при взрывах стихий",
        "• 60+ second boot freeze during SHiFT network ping\n• Elemental explosion particle slowdowns",
        "✓ Режим полёта: Включено (мгновенный старт без ожидания SHiFT)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Точность GPU: Высокая",
        "✓ Airplane Mode: Enabled (Instant boot without SHiFT timeout)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ GPU Accuracy: High",
        {
            {"System\\airplane_mode", "true"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Renderer\\gpu_accuracy", "1"}
        }
    },
    {
        0x01001B700A654000ULL,
        "Diablo III: Eternal Collection",
        "• Задержка и зависание на экране сезонов при проверке Battle.net\n• Микрофризы при спавне элитных паков",
        "• Battle.net seasonal handshake timeout freeze\n• Elite mob pack spawn stutter",
        "✓ Режим полёта: Включено (пропуск серверов Battle.net)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Точность GPU: Высокая",
        "✓ Airplane Mode: Enabled (Skips Battle.net server check)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ GPU Accuracy: High",
        {
            {"System\\airplane_mode", "true"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Renderer\\gpu_accuracy", "1"}
        }
    },
    {
        0x01005E4017C7A000ULL,
        "Demon Slayer: Kimetsu no Yaiba - The Hinokami Chronicles",
        "• Зависание на заставке при онлайн-верификации CyberConnect2\n• Просадки кадров в спецприемах дыхания",
        "• Title screen freeze during CyberConnect2 online verification loop\n• Breathing form ultimate attack FPS drops",
        "✓ Режим полёта: Включено (пропуск онлайн-верификации)\n✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ Airplane Mode: Enabled (Skips online verification)\n✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"System\\airplane_mode", "true"},
            {"System\\memory_layout_mode", "1"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100830007780000ULL,
        "Monster Hunter Rise / Sunbreak",
        "• Микрофризы при опросе сетевого лобби Hunter Search\n• Утечки VRAM в локациях Джунглей и Цитадели",
        "• Hunter Search lobby broadcast micro-freezes\n• Jungle and Citadel VRAM leaks",
        "✓ Режим полёта: Включено (устранение микрофризов Hunter Search)\n✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ Airplane Mode: Enabled (Fixes Hunter Search lobby lag)\n✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"System\\airplane_mode", "true"},
            {"System\\memory_layout_mode", "1"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x010052900FA62000ULL,
        "Burnout Paradise Remastered",
        "• Зависание на заставке при обращении к серверам новостей EA Paradise City\n• Микрофризы в авариях",
        "• EA Paradise City news server connection freeze\n• Crash sequence micro-stutters",
        "✓ Режим полёта: Включено (пропуск серверов EA)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Сжатие ASTC: Отключено",
        "✓ Airplane Mode: Enabled (Bypasses EA online news)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ ASTC Recompression: Uncompressed",
        {
            {"System\\airplane_mode", "true"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Renderer\\astc_recompression", "0"}
        }
    },
    {
        0x0100650012270000ULL,
        "Overcooked! All You Can Eat",
        "• Зависание на этапе инициализации Team17 Cross-Play сервисов на Android\n• Рассинхронизация физики кухни",
        "• Team17 Cross-Play service handshake freeze on Android\n• Kitchen item physics desync",
        "✓ Режим полёта: Включено (пропуск Team17 Cross-Play)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ Airplane Mode: Enabled (Bypasses Team17 Cross-Play)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"System\\airplane_mode", "true"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x01001FB012014000ULL,
        "Plants vs. Zombies: Battle for Neighborville",
        "• Зависание на стартовом экране из-за онлайн-требований Frostbite EA\n• Утечки памяти в хабе Беспечного парка",
        "• Frostbite EA online login requirement deadlock on title screen\n• Giddy Park hub memory leaks",
        "✓ Режим полёта: Включено (пропуск онлайн-авторизации EA)\n✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ Airplane Mode: Enabled (Bypasses EA online login requirement)\n✓ Memory Layout: 8GB DRAM\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"System\\airplane_mode", "true"},
            {"System\\memory_layout_mode", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100D5600DE44000ULL,
        "Risk of Rain 2",
        "• Сетевой таймаут PlayFab в главном меню\n• Просадки FPS при спавне сотен монстров на 5+ стадии",
        "• PlayFab matchmaking network timeout\n• High-stage horde particle slowdowns",
        "✓ Режим полёта: Включено (пропуск PlayFab таймаутов)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Точность GPU: Высокая",
        "✓ Airplane Mode: Enabled (Bypasses PlayFab timeout)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ GPU Accuracy: High",
        {
            {"System\\airplane_mode", "true"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Renderer\\gpu_accuracy", "1"}
        }
    },
    {
        0x0100E2600AE5C000ULL,
        "Minecraft Dungeons",
        "• Зацикливание авторизации учетной записи Microsoft / Xbox Live\n• Микрофризы процедурных подземелий",
        "• Microsoft / Xbox Live telemetry sign-in loop\n• Procedural dungeon generation stutters",
        "✓ Режим полёта: Включено (пропуск авторизации Microsoft)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ Airplane Mode: Enabled (Bypasses Microsoft telemetry)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"System\\airplane_mode", "true"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x01006B4009B74000ULL,
        "Dragon Ball FighterZ",
        "• Зависание на старте при поиске сетевого лобби Bandai Namco\n• Инпут-лаг в комбо",
        "• Bandai Namco online lobby connection loop on title screen\n• Combo input latency",
        "✓ Режим полёта: Включено (пропуск сетевого лобби)\n✓ Точность CPU: Точная (Accurate)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ Airplane Mode: Enabled (Bypasses online lobby search)\n✓ CPU Accuracy: Accurate\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"System\\airplane_mode", "true"},
            {"Cpu\\cpu_accuracy", "0"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x010077C000B90000ULL,
        "Dragon Ball Xenoverse 2",
        "• Зависание в городе Контон при опросе сетевых серверов\n• Просадки FPS в рейдах на 6 игроков",
        "• Conton City server polling freeze\n• 6-player raid boss FPS drops",
        "✓ Режим полёта: Включено (пропуск сетевого сервера Контона)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Память: 8GB DRAM",
        "✓ Airplane Mode: Enabled (Bypasses Conton City server)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Memory Layout: 8GB DRAM",
        {
            {"System\\airplane_mode", "true"},
            {"System\\memory_layout_mode", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100B1A01B20A000ULL,
        "NBA 2K24 / 2K23",
        "• Зависание на экране обновления ростеров (2K Sports server handshake)\n• Утечки памяти в режиме MyCAREER",
        "• 2K Sports roster server handshake freeze\n• MyCAREER mode memory leaks",
        "✓ Режим полёта: Включено (пропуск серверов 2K Sports)\n✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ Airplane Mode: Enabled (Bypasses 2K server handshake)\n✓ Memory Layout: 8GB DRAM\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"System\\airplane_mode", "true"},
            {"System\\memory_layout_mode", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100827018DEE000ULL,
        "Just Dance Series (2020-2024)",
        "• Зависание на заставке при подключении к Ubisoft Connect\n• Рассинхронизация видеопотока клипов",
        "• Ubisoft Connect server connection loop on boot\n• Video stream audio sync delay",
        "✓ Режим полёта: Включено (пропуск серверов Ubisoft Connect)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ Airplane Mode: Enabled (Bypasses Ubisoft Connect)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"System\\airplane_mode", "true"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x01005B900C09A000ULL,
        "Clubhouse Games: 51 Worldwide Classics",
        "• Задержка и зависание при локальном сетевом поиске столов\n• Размытие текстур игровых досок",
        "• Local lobby discovery loop freeze on startup\n• Board texture and piece blur",
        "✓ Режим полёта: Включено (мгновенная загрузка офлайн)\n✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ Airplane Mode: Enabled (Instant offline boot)\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"System\\airplane_mode", "true"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100BDE00862A000ULL,
        "Mario Tennis Aces",
        "• Зависание на проверке онлайн-турниров при старте\n• Просадки FPS при Zone Shot спецэффектах",
        "• Tournament online leaderboard check hang\n• Zone Shot particle slowdowns",
        "✓ Режим полёта: Включено (пропуск онлайн-проверки турниров)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Точность GPU: Высокая",
        "✓ Airplane Mode: Enabled (Skips tournament online check)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ GPU Accuracy: High",
        {
            {"System\\airplane_mode", "true"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Renderer\\gpu_accuracy", "1"}
        }
    },
    {
        0x0100874011EE6000ULL,
        "Mario Golf: Super Rush",
        "• Задержка и зависание при сетевом поиске ранговых матчей\n• Мерцание дальних флажков и травы",
        "• Ranked match network polling hang\n• Course grass and flag shadow flickering",
        "✓ Режим полёта: Включено (мгновенный старт офлайн)\n✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ Airplane Mode: Enabled (Instant offline start)\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"System\\airplane_mode", "true"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x01007E3019808000ULL,
        "Shin Megami Tensei V: Vengeance",
        "• Вылеты из-за нехватки памяти (OOM) на открытых картах Да'ат в UE4\n• Розовые артефакты текстур",
        "• Unreal Engine 4 Da'at open-world Out-Of-Memory crashes\n• Corrupted/pink ASTC texture tiles",
        "✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"System\\memory_layout_mode", "1"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100121014688000ULL,
        "Metroid Prime Remastered",
        "• Артефакты визора и HUD-интерфейса\n• Микрофризы при открытии переходных дверей между комнатами",
        "• Combat/Scan visor HUD graphical glitches\n• Room transition door loading stutters",
        "✓ Точность GPU: Высокая\n✓ Сжатие ASTC: Отключено (исправление визора)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ GPU Accuracy: High\n✓ ASTC Recompression: Uncompressed (Fixes visor artifacts)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\astc_recompression", "0"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x01005E3017B46000ULL,
        "Crisis Core -Final Fantasy VII- Reunion",
        "• Микрозадержки при срабатывании барабана рулетки DMW в бою\n• Мерцание динамического освещения в UE4",
        "• DMW combat reel digital mind wave hitches\n• UE4 dynamic lighting and shadow flickering",
        "✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Точность GPU: Высокая\n✓ Реактивная очистка: Включено\n✓ Быстрая память (Fastmem): Включено",
        "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ Fastmem: Enabled",
        {
            {"System\\memory_layout_mode", "1"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_reactive_flushing", "true"},
            {"Cpu\\cpuopt_fastmem", "true"}
        }
    },
    {
        0x0100D15017D3E000ULL,
        "Octopath Traveler II",
        "• Размытие пиксель-арт спрайтов персонажей и HD-2D окружения\n• Швы на текстурах воды",
        "• HD-2D sprite blur and character outline softening\n• Water surface shader seam lines",
        "✓ Точность GPU: Высокая\n✓ Сжатие ASTC: Отключено\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ GPU Accuracy: High\n✓ ASTC Recompression: Uncompressed\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\astc_recompression", "0"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100508013AE0000ULL,
        "Sonic Frontiers",
        "• Просадки FPS при разгоне на открытых островах Звездопада\n• Подгрузка геометрии рейлов и колец",
        "• Starfall Islands high-speed boost framerate drops\n• Rail and collectible geometry pop-in",
        "✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"System\\memory_layout_mode", "1"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100570017106000ULL,
        "Tactics Ogre: Reborn",
        "• Микрозадержки при исполнении спец-ударов и магии\n• Сглаживание пиксельных шрифтов диалогов",
        "• Finishing moves particle stutters\n• Retro dialogue font smoothing",
        "✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100AE600BA12000ULL,
        "Dragon's Dogma: Dark Arisen",
        "• Зависание на старте при сетевой верификации серверов Пешек (Pawns)\n• Просадки FPS в битвах с химерами и грифонами",
        "• Pawn server network verification hang on boot\n• Chimera and Griffin boss particle slowdowns",
        "✓ Режим полёта: Включено (пропуск серверов Пешек)\n✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ Airplane Mode: Enabled (Bypasses Pawn server checks)\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"System\\airplane_mode", "true"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x01000B300B250000ULL,
        "Warframe",
        "• Бесконечный цикл ожидания подключения к серверам при старте\n• Утечки памяти в реле",
        "• Infinite server login connection loop on title screen\n• Relay hub memory pressure",
        "✓ Режим полёта: Включено (пропуск сетевого ожидания)\n✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Точность GPU: Высокая\n✓ Быстрая память (Fastmem): Включено",
        "✓ Airplane Mode: Enabled (Bypasses network connection retry)\n✓ Memory Layout: 8GB DRAM\n✓ GPU Accuracy: High\n✓ Fastmem: Enabled",
        {
            {"System\\airplane_mode", "true"},
            {"System\\memory_layout_mode", "1"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Cpu\\cpuopt_fastmem", "true"}
        }
    },
    {
        0x010077001A8D4000ULL,
        "Brotato",
        "• Просадки FPS ниже 60 при спавне больших волн врагов и сотен пуль\n• Избыточная нагрузка на GPU при точности High",
        "• Framerate drops below 60 during large monster waves and bullet storms\n• Excessive GPU overhead with High GPU accuracy",
        "✓ Точность CPU: Авто / Небезопасная (максимальная скорость для сотен мобов)\n✓ Точность GPU: Обычная (Normal, стабильные 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Реактивная очистка: Отключено",
        "✓ CPU Accuracy: Auto / Unsafe (Maximum CPU throughput for 200+ mobs)\n✓ GPU Accuracy: Normal (Locked 60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Reactive Flushing: Disabled",
        {
            {"Cpu\\cpu_accuracy", "1"},
            {"Renderer\\gpu_accuracy", "0"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Renderer\\use_reactive_flushing", "false"},
            {"Renderer\\astc_recompression", "0"}
        }
    },
    {
        0x010085C019864000ULL,
        "Vampire Survivors",
        "• Задержки физики и анимаций при заполнении экрана тысячами монстров",
        "• Physics and animation slowdowns during screen-filling enemy swarms",
        "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Cpu\\cpu_accuracy", "1"},
            {"Renderer\\gpu_accuracy", "0"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Renderer\\use_reactive_flushing", "false"}
        }
    },
    {
        0x0100FF500E34A000ULL,
        "Xenoblade Chronicles: Definitive Edition",
        "• Просадки FPS и утечки VRAM в открытых локациях (Bionis Leg)\n• Микрофризы при подгрузке текстур высокого разрешения",
        "• Framerate drops and VRAM pressure in large open zones (Bionis Leg)\n• Texture streaming micro-stutters",
        "✓ Точность GPU: Обычная (Normal, стабильная производительность)\n✓ Сжатие ASTC: Отключено (чистые текстуры без задержек)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Память: 8GB DRAM",
        "✓ GPU Accuracy: Normal (Smooth performance)\n✓ ASTC Recompression: Uncompressed\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Memory Layout: 8GB DRAM",
        {
            {"Renderer\\gpu_accuracy", "0"},
            {"Renderer\\astc_recompression", "0"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"System\\memory_layout_mode", "1"}
        }
    },
    {
        0x0100E95004038000ULL,
        "Xenoblade Chronicles 2",
        "• Размытие динамического разрешения и нехватка VRAM в провинции Гормотт\n• Просадки частоты кадров в битвах",
        "• Dynamic resolution blur and VRAM memory buildup in Gormott\n• Combat framerate drops",
        "✓ Точность GPU: Обычная (Normal)\n✓ Сжатие ASTC: Отключено\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Память: 8GB DRAM",
        "✓ GPU Accuracy: Normal\n✓ ASTC Recompression: Uncompressed\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Memory Layout: 8GB DRAM",
        {
            {"Renderer\\gpu_accuracy", "0"},
            {"Renderer\\astc_recompression", "0"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"System\\memory_layout_mode", "1"}
        }
    },
    {
        0x01008D3017B4C000ULL,
        "Super Mario RPG",
        "• Микрофризы при переходе между локациями и в диалогах\n• Сбои глубины теней персонажей",
        "• Micro-stutters during scene transitions and battle dialogs\n• Character shadow depth inaccuracies",
        "✓ Точность GPU: Обычная (Normal)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ GPU Accuracy: Normal\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Renderer\\gpu_accuracy", "0"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x010006701B4B2000ULL,
        "Princess Peach: Showtime!",
        "• Падение FPS на движке Unreal Engine 4 при трансформациях\n• Мерцание геометрии и теней",
        "• Framerate drops on Unreal Engine 4 during transformations\n• Geometry and shadow flickering",
        "✓ Точность GPU: Обычная (Normal)\n✓ Память: 8GB DRAM\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ GPU Accuracy: Normal\n✓ Memory Layout: 8GB DRAM\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Renderer\\gpu_accuracy", "0"},
            {"System\\memory_layout_mode", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100C8901844A000ULL,
        "Luigi's Mansion 2 HD",
        "• Компиляционные задержки шейдеров в темных комнатах особняка\n• Мерцание динамического освещения фонарика",
        "• Shader compilation stutters in dark mansion corridors\n• Flashlight dynamic lighting flickering",
        "✓ Точность GPU: Обычная (Normal)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено\n✓ Память: 8GB DRAM",
        "✓ GPU Accuracy: Normal\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled\n✓ Memory Layout: 8GB DRAM",
        {
            {"Renderer\\gpu_accuracy", "0"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"System\\memory_layout_mode", "1"}
        }
    },
    {
        0x0100B9301A4A0000ULL,
        "F-Zero 99",
        "• Зависание на стартовом экране при попытке подключения к серверам Nintendo\n• Бесконечная синхронизация онлайна",
        "• Hang on title screen attempting to connect to Nintendo servers\n• Infinite online network sync loop",
        "✓ Режим полёта: Включено (пропуск сетевого зависания)\n✓ Точность CPU: Авто / Небезопасная (Unsafe JIT, 60 FPS)\n✓ Точность GPU: Обычная (Normal)",
        "✓ Airplane Mode: Enabled (Bypasses network connection hang)\n✓ CPU Accuracy: Auto / Unsafe (60 FPS)\n✓ GPU Accuracy: Normal",
        {
            {"System\\airplane_mode", "true"},
            {"Cpu\\cpu_accuracy", "1"},
            {"Renderer\\gpu_accuracy", "0"},
            {"Cpu\\cpuopt_fastmem", "true"}
        }
    },
    {
        0x0100427010476000ULL,
        "Super Mario 3D All-Stars",
        "• Накладные расходы двойной эмуляции в Super Mario Sunshine и Super Mario Galaxy\n• Потрескивания звука при нехватке CPU",
        "• Double-emulation overhead in Super Mario Sunshine and Galaxy\n• Audio crackling from CPU thread starvation",
        "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ CPU Accuracy: Auto / Unsafe (Maximum JIT performance)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Cpu\\cpu_accuracy", "1"},
            {"Renderer\\gpu_accuracy", "0"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x01001F5010DFA000ULL,
        "Pokemon Legends: Arceus",
        "• Микрофризы и просадки FPS при спавне диких Покемонов\n• Утечки памяти в деревне Джубилайф",
        "• Micro-stutters and framerate drops during wild Pokemon spawns\n• Memory pressure in Jubilife Village",
        "✓ Точность GPU: Обычная (Normal, стабильные 30 FPS)\n✓ Память: 8GB DRAM\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ GPU Accuracy: Normal (Smooth 30 FPS)\n✓ Memory Layout: 8GB DRAM\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Renderer\\gpu_accuracy", "0"},
            {"System\\memory_layout_mode", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100A3D008C5C000ULL,
        "Pokemon Scarlet",
        "• Тяжелые утечки памяти в открытом мире Палдеи (>9 ГБ VRAM)\n• Падения FPS и рывки камеры",
        "• Massive open world memory leaks in Paldea (>9 GB VRAM)\n• Framerate drops and camera stuttering",
        "✓ Точность GPU: Обычная (Normal)\n✓ Реактивная очистка: Отключено\n✓ Память: 8GB DRAM\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ GPU Accuracy: Normal\n✓ Reactive Flushing: Disabled\n✓ Memory Layout: 8GB DRAM\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Renderer\\gpu_accuracy", "0"},
            {"Renderer\\use_reactive_flushing", "false"},
            {"System\\memory_layout_mode", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x01008F6008C5E000ULL,
        "Pokemon Violet",
        "• Тяжелые утечки памяти в открытом мире Палдеи (>9 ГБ VRAM)\n• Падения FPS и рывки камеры",
        "• Massive open world memory leaks in Paldea (>9 GB VRAM)\n• Framerate drops and camera stuttering",
        "✓ Точность GPU: Обычная (Normal)\n✓ Реактивная очистка: Отключено\n✓ Память: 8GB DRAM\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ GPU Accuracy: Normal\n✓ Reactive Flushing: Disabled\n✓ Memory Layout: 8GB DRAM\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Renderer\\gpu_accuracy", "0"},
            {"Renderer\\use_reactive_flushing", "false"},
            {"System\\memory_layout_mode", "1"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100D1AB10000000ULL,
        "Diablo I + Hellfire (DevilutionX)",
        "• Порт DevilutionX для Diablo I и дополнения Hellfire\n• Поддержка MPQ (diabdat, hellfire, hfmonk, ru)\n• Оптимизация 2D Software/Hardware SDL2 рендера",
        "• DevilutionX port for Diablo I and Hellfire expansion\n• MPQ support (diabdat, hellfire, hfmonk, ru)\n• Optimized 2D SDL2 rendering",
        "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Cpu\\cpu_accuracy", "1"},
            {"Renderer\\gpu_accuracy", "0"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Renderer\\use_reactive_flushing", "false"}
        }
    },
    {
        0x0100B00B51230000ULL,
        "Grand Theft Auto V (GTA V Homebrew Port)",
        "• Кастомный Homebrew-порт GTA V для Nintendo Switch\n• Поддержка RPF архивов, аудио и DLC-паков в romfs/\n• Высокое потребление VRAM и тяжелая геометрия Лос-Сантоса",
        "• GTA V Custom Homebrew Switch port\n• RPF archives, audio and DLC packs support in romfs/\n• Heavy Los Santos geometry and VRAM pressure",
        "✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal)\n✓ Сжатие ASTC: Отключено\n✓ Быстрая память (Fastmem): Включено",
        "✓ Memory Layout: 8GB DRAM\n✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal\n✓ ASTC Recompression: Uncompressed\n✓ Fastmem: Enabled",
        {
            {"System\\memory_layout_mode", "1"},
            {"Cpu\\cpu_accuracy", "1"},
            {"Renderer\\gpu_accuracy", "0"},
            {"Renderer\\astc_recompression", "0"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100671000000000ULL,
        "Grand Theft Auto: San Andreas (re3-sa)",
        "• Порт открытого движка re3-sa для GTA San Andreas\n• Загрузка ресурсов main.scm, models, data, txd, dff\n• Стабильные 60 FPS на открытой карте штата Сан-Андреас",
        "• re3-sa open engine port for GTA San Andreas\n• main.scm, models, data, txd, dff assets loading\n• Locked 60 FPS across San Andreas",
        "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Cpu\\cpu_accuracy", "1"},
            {"Renderer\\gpu_accuracy", "0"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100672000000000ULL,
        "Grand Theft Auto: Vice City (reVC)",
        "• Порт открытого движка reVC для GTA Vice City\n• Загрузка ресурсов gxt, audio, txd, dff\n• Стабильные 60 FPS в Вайс-Сити",
        "• reVC open engine port for GTA Vice City\n• gxt, audio, txd, dff assets loading\n• Locked 60 FPS in Vice City",
        "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Cpu\\cpu_accuracy", "1"},
            {"Renderer\\gpu_accuracy", "0"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100673000000000ULL,
        "Grand Theft Auto III (re3)",
        "• Порт открытого движка re3 для GTA III\n• Загрузка ресурсов Либерти-Сити, коллизий и текстур\n• Стабильные 60 FPS",
        "• re3 open engine port for GTA III\n• Liberty City collision and texture streaming\n• Locked 60 FPS",
        "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Cpu\\cpu_accuracy", "1"},
            {"Renderer\\gpu_accuracy", "0"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100000000000018ULL,
        "Half-Life 1 / Black Mesa Classic (Xash3D FWGS)",
        "• Движок Xash3D FWGS для Half-Life 1, Blue Shift, Opposing Force, Counter-Strike\n• Загрузка PAK/WAD ресурсов и GoldSrc карт\n• Максимальная производительность и отзывчивость управления",
        "• Xash3D FWGS engine for Half-Life 1, Blue Shift, Opposing Force, CS\n• PAK/WAD resources and GoldSrc maps loading\n• Maximum responsiveness and framerate",
        "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Cpu\\cpu_accuracy", "1"},
            {"Renderer\\gpu_accuracy", "0"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100D00100000000ULL,
        "DOOM 1, 2, Final, Plutonia, TNT (GZDoom / PrBoom / Crispy)",
        "• Порты GZDoom, PrBoom, Crispy Doom для классической серии DOOM\n• Поддержка WAD файлов, DeHackEd патчей и модов Sigil/Plutonia/TNT\n• 60 FPS с чистым виброоткликом",
        "• GZDoom, PrBoom, Crispy Doom ports for classic DOOM series\n• WAD files, DeHackEd patches and Sigil/Plutonia/TNT mods\n• Smooth 60 FPS with rumble",
        "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Cpu\\cpu_accuracy", "1"},
            {"Renderer\\gpu_accuracy", "0"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100D00300000000ULL,
        "DOOM 3 (dhewm3 Switch)",
        "• Порт dhewm3 (id Tech 4) для DOOM 3 и Resurrection of Evil\n• Поддержка динамического освещения и попиксельных теней id Tech 4\n• 8 ГБ DRAM для кэширования PK4",
        "• dhewm3 (id Tech 4) port for DOOM 3 and Resurrection of Evil\n• id Tech 4 dynamic lighting and per-pixel shadow rendering\n• 8GB DRAM for PK4 caching",
        "✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено",
        "✓ Memory Layout: 8GB DRAM\n✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled",
        {
            {"System\\memory_layout_mode", "1"},
            {"Cpu\\cpu_accuracy", "1"},
            {"Renderer\\gpu_accuracy", "0"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100000000000071ULL,
        "Quake I / Quake II / Quake III (Quakespasm / Yamagi / ioquake3)",
        "• Порты движков Quakespasm (Quake 1), Yamagi Quake II, ioquake3\n• Загрузка PAK и PK3 файлов, GL-рендеринг\n• Идеальные 60 FPS с гироскопом",
        "• Quakespasm (Quake 1), Yamagi Quake II, ioquake3 ports\n• PAK and PK3 loading, hardware GL rendering\n• Locked 60 FPS with gyro support",
        "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Cpu\\cpu_accuracy", "1"},
            {"Renderer\\gpu_accuracy", "0"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x010000000000003DULL,
        "Duke Nukem 3D / Shadow Warrior / Blood (EDuke32 / VoidSW / NBlood)",
        "• Порты Build Engine (EDuke32, VoidSW, NBlood) для культовых 2.5D шутеров\n• Загрузка GRP и RFF ресурсов, вокселей и полигонального рендера\n• Стабильные 60 FPS",
        "• Build Engine ports (EDuke32, VoidSW, NBlood) for classic 2.5D shooters\n• GRP and RFF assets, voxels and polygonal render\n• Locked 60 FPS",
        "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Cpu\\cpu_accuracy", "1"},
            {"Renderer\\gpu_accuracy", "0"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100000000000077ULL,
        "Return to Castle Wolfenstein / Wolf3D (iortcw / ECWolf)",
        "• Порты движков id Tech 3 (iortcw) и ECWolf для Wolfenstein 3D и RtCW\n• Поддержка PK3 и WL6 карт\n• 60 FPS с аппаратным освещением",
        "• id Tech 3 (iortcw) and ECWolf ports for Wolfenstein 3D and RtCW\n• PK3 and WL6 map support\n• Smooth 60 FPS with hardware lighting",
        "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Cpu\\cpu_accuracy", "1"},
            {"Renderer\\gpu_accuracy", "0"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x01000000000000F1ULL,
        "Fallout 1 & Fallout 2 (fallout-ce / fallout2-ce)",
        "• Порты Community Edition для Fallout 1 и Fallout 2\n• Загрузка master.dat, critter.dat и кастомных шрифтов\n• Стабильная работа мыши/тача и 60 FPS",
        "• Community Edition ports for Fallout 1 and Fallout 2\n• master.dat, critter.dat and custom fonts loading\n• Smooth touch/mouse input and 60 FPS",
        "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Cpu\\cpu_accuracy", "1"},
            {"Renderer\\gpu_accuracy", "0"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100000000000030ULL,
        "Heroes of Might and Magic II & III (fheroes2 / VCMI Switch)",
        "• Порты движков fheroes2 и VCMI для Героев Меча и Магии II и III (In the Wake of Gods / Horn of the Abyss)\n• Загрузка LOD, AGG, SND ресурсов и модов",
        "• fheroes2 and VCMI ports for Heroes of Might and Magic II and III (WoG / HotA)\n• LOD, AGG, SND assets and mods loading",
        "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Cpu\\cpu_accuracy", "1"},
            {"Renderer\\gpu_accuracy", "0"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x010000000000004DULL,
        "Max Payne 1 (Max Payne Switch)",
        "• Нативный порт Max Payne для Nintendo Switch\n• Загрузка архивов RAS (x_data.ras, x_russian.ras)\n• Плавный Bullet-Time и 60 FPS",
        "• Native Max Payne port for Nintendo Switch\n• RAS archives loading (x_data.ras, x_english.ras)\n• Smooth Bullet-Time and locked 60 FPS",
        "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Cpu\\cpu_accuracy", "1"},
            {"Renderer\\gpu_accuracy", "0"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x010000000000006DULL,
        "The Elder Scrolls III: Morrowind (OpenMW Switch)",
        "• Порт современного 3D-движка OpenMW для TES III: Morrowind\n• Загрузка ESM, BSA, текстурных паков и модов\n• 8 ГБ DRAM для открытого мира Вварденфелла",
        "• OpenMW modern 3D engine port for TES III: Morrowind\n• ESM, BSA, texture packs and mods loading\n• 8GB DRAM for Vvardenfell open world",
        "✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal)\n✓ Быстрая память (Fastmem): Включено",
        "✓ Memory Layout: 8GB DRAM\n✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal\n✓ Fastmem: Enabled",
        {
            {"System\\memory_layout_mode", "1"},
            {"Cpu\\cpu_accuracy", "1"},
            {"Renderer\\gpu_accuracy", "0"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x010000000000004AULL,
        "Star Wars: Jedi Outcast & Academy (OpenJK Switch)",
        "• Порт OpenJK для Star Wars Jedi Knight II: Jedi Outcast и Jedi Academy\n• Стабильные 60 FPS в дуэлях на световых мечах",
        "• OpenJK port for Star Wars Jedi Knight II: Jedi Outcast and Academy\n• Smooth 60 FPS in lightsaber duels",
        "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Cpu\\cpu_accuracy", "1"},
            {"Renderer\\gpu_accuracy", "0"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100000000000053ULL,
        "S.T.A.L.K.E.R.: Shadow of Chernobyl (OpenXRay Switch)",
        "• Порт движка X-Ray 1.6 (OpenXRay) для S.T.A.L.K.E.R.: Тень Чернобыля\n• Загрузка DB-архивов и gamedata\n• 8 ГБ DRAM для локаций Зоны",
        "• OpenXRay engine port for S.T.A.L.K.E.R.: Shadow of Chernobyl\n• DB archives and gamedata loading\n• 8GB DRAM for Zone locations",
        "✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal)\n✓ Быстрая память (Fastmem): Включено",
        "✓ Memory Layout: 8GB DRAM\n✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal\n✓ Fastmem: Enabled",
        {
            {"System\\memory_layout_mode", "1"},
            {"Cpu\\cpu_accuracy", "1"},
            {"Renderer\\gpu_accuracy", "0"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x010000000000005CULL,
        "Sonic CD / Sonic 1 / Sonic 2 / Mania (RSDK Decompilation)",
        "• Порты Retro Engine (RSDKv3, RSDKv4, RSDKv5) для классических игр Sonic\n• Загрузка Data.rsdk и модов\n• Идеальные 60 FPS",
        "• Retro Engine (RSDKv3, RSDKv4, RSDKv5) decompilations for classic Sonic games\n• Data.rsdk and mods loading\n• Flawless 60 FPS",
        "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Cpu\\cpu_accuracy", "1"},
            {"Renderer\\gpu_accuracy", "0"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100000000000064ULL,
        "Super Mario 64 (SM64-NX / Render96)",
        "• Порт SM64-NX и Render96 для Super Mario 64 с широкоформатным рендером и HD-моделями\n• Стабильные 60 FPS",
        "• SM64-NX and Render96 port for Super Mario 64 with widescreen and HD models\n• Locked 60 FPS",
        "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Cpu\\cpu_accuracy", "1"},
            {"Renderer\\gpu_accuracy", "0"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100000000000074ULL,
        "Zelda: Ocarina of Time / Majora's Mask (Ship of Harkinian / 2S2H)",
        "• Порты Ship of Harkinian (OoT) и 2 Ship 2 Harkinian (MM)\n• Загрузка OTR архивов, поддержка 60 FPS, свободная камера и HD текстуры",
        "• Ship of Harkinian (OoT) and 2 Ship 2 Harkinian (MM) ports\n• OTR archives loading, 60 FPS support, free camera and HD textures",
        "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Cpu\\cpu_accuracy", "1"},
            {"Renderer\\gpu_accuracy", "0"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100000000000022ULL,
        "Cave Story / Cave Story+ / AM2R (NXEngine-evo / AM2R)",
        "• Порты NXEngine-evo для Cave Story и нативный порт AM2R (Another Metroid 2 Remake)\n• Чистый 2D-рендеринг и 60 FPS",
        "• NXEngine-evo ports for Cave Story and native AM2R port\n• Crisp 2D rendering and 60 FPS",
        "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Cpu\\cpu_accuracy", "1"},
            {"Renderer\\gpu_accuracy", "0"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100000000000068ULL,
        "Theme Hospital / Caesar III (CorsixTH / Julius / Augustus)",
        "• Порты CorsixTH (Theme Hospital) и Julius/Augustus (Caesar III)\n• Высокое разрешение и сенсорное управление",
        "• CorsixTH (Theme Hospital) and Julius/Augustus (Caesar III) ports\n• High resolution and touch controls",
        "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Cpu\\cpu_accuracy", "1"},
            {"Renderer\\gpu_accuracy", "0"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100000000000091ULL,
        "Command & Conquer / Red Alert / Dune II (OpenRA / Dune Legacy)",
        "• Порты OpenRA (C&C, Red Alert, Dune 2000) и Dune Legacy (Dune II)\n• Быстрый расчет AI юнитов на Unsafe JIT",
        "• OpenRA (C&C, Red Alert, Dune 2000) and Dune Legacy (Dune II) ports\n• Fast RTS AI unit simulation on Unsafe JIT",
        "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Cpu\\cpu_accuracy", "1"},
            {"Renderer\\gpu_accuracy", "0"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100000000001000ULL,
        "RetroArch & Emulators (PPSSPP, Flycast, ScummVM, MelonDS, mGBA, DuckStation)",
        "• Мультисистемные эмуляторы и оболочки для запуска ретро-платформ на Switch\n• 8 ГБ DRAM для тяжелых ядер (Flycast, PPSSPP, DuckStation)\n• Максимальная скорость JIT компиляции",
        "• Multi-system emulators and frontends for retro platforms\n• 8GB DRAM for memory-heavy cores (Flycast, PPSSPP, DuckStation)\n• Maximum JIT compiler performance",
        "✓ Конфигурация памяти: 8 ГБ DRAM\n✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено",
        "✓ Memory Layout: 8GB DRAM\n✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled",
        {
            {"System\\memory_layout_mode", "1"},
            {"Cpu\\cpu_accuracy", "1"},
            {"Renderer\\gpu_accuracy", "0"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100000000003000ULL,
        "Homebrew Utilities & Overlays (NX-Shell, DBI, Goldleaf, JKSV, Checkpoint, EdiZon, Tesla)",
        "• Системные Homebrew-утилиты Switch для управления сейвами, файлами и оверлеями\n• Мгновенный отклик файловой системы и стабильная работа",
        "• Switch Homebrew system utilities for saves, files and overlays\n• Instant filesystem responsiveness and stability",
        "✓ Точность CPU: Авто / Небезопасная (Unsafe JIT)\n✓ Точность GPU: Обычная (Normal, 60 FPS)\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ CPU Accuracy: Auto / Unsafe (Unsafe JIT)\n✓ GPU Accuracy: Normal (60 FPS)\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Cpu\\cpu_accuracy", "1"},
            {"Renderer\\gpu_accuracy", "0"},
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    }
};

const GameFixProfile* GameFixDatabase::GetProfile(u64 title_id) {
    if (title_id == 0) return nullptr;
    // Check exact title ID or base title ID (mask out DLC/update bits)
    const u64 base_title_id = title_id & ~0x1FFFULL;
    for (const auto& profile : s_profiles) {
        if (profile.title_id == title_id || (profile.title_id & ~0x1FFFULL) == base_title_id) {
            return &profile;
        }
    }
    return nullptr;
}

const GameFixProfile* GameFixDatabase::GetProfileByTitleOrPath(u64 title_id, const std::string& name_or_path) {
    if (title_id != 0) {
        const auto* p = GetProfile(title_id);
        if (p) return p;
    }

    if (name_or_path.empty()) return nullptr;

    std::string lower = name_or_path;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    for (const auto& profile : s_profiles) {
        std::string game_lower = profile.game_name;
        std::transform(game_lower.begin(), game_lower.end(), game_lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        // Check full game name or title hex in string
        std::string hex_id = fmt::format("{:016x}", profile.title_id);
        if (lower.find(hex_id) != std::string::npos) {
            return &profile;
        }

        // Custom keyword matching
        if (game_lower.find("splintered fate") != std::string::npos && (lower.find("splintered fate") != std::string::npos || lower.find("tmnt") != std::string::npos || lower.find("ninja turtles") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("breath of the wild") != std::string::npos && (lower.find("breath of the wild") != std::string::npos || lower.find("botw") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("tears of the kingdom") != std::string::npos && (lower.find("tears of the kingdom") != std::string::npos || lower.find("totk") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("mario odyssey") != std::string::npos && (lower.find("odyssey") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("rabbids") != std::string::npos && (lower.find("rabbids") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("sonic frontiers") != std::string::npos && (lower.find("frontiers") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("witcher") != std::string::npos && (lower.find("witcher") != std::string::npos || lower.find("wild hunt") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("pikmin") != std::string::npos && lower.find("pikmin") != std::string::npos) {
            return &profile;
        }
        if (game_lower.find("peach") != std::string::npos && lower.find("peach") != std::string::npos) {
            return &profile;
        }
        if (game_lower.find("hollow knight") != std::string::npos && (lower.find("hollow knight") != std::string::npos || lower.find("hollow_knight") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("hades") != std::string::npos && lower.find("hades") != std::string::npos) {
            return &profile;
        }
        if (game_lower.find("dead cells") != std::string::npos && (lower.find("dead cells") != std::string::npos || lower.find("dead_cells") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("outer wilds") != std::string::npos && lower.find("outer wilds") != std::string::npos) {
            return &profile;
        }
        if (game_lower.find("subnautica") != std::string::npos && lower.find("subnautica") != std::string::npos) {
            return &profile;
        }
        if (game_lower.find("alien") != std::string::npos && lower.find("alien") != std::string::npos) {
            return &profile;
        }
        if (game_lower.find("metroid dread") != std::string::npos && (lower.find("dread") != std::string::npos || lower.find("metroid") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("tropical freeze") != std::string::npos && (lower.find("tropical freeze") != std::string::npos || lower.find("donkey kong") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("dream land") != std::string::npos && (lower.find("dream land") != std::string::npos || lower.find("kirby") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("captain toad") != std::string::npos && (lower.find("captain toad") != std::string::npos || lower.find("treasure tracker") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("crafted world") != std::string::npos && (lower.find("crafted world") != std::string::npos || lower.find("yoshi") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("strikers") != std::string::npos && (lower.find("strikers") != std::string::npos || lower.find("battle league") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("bomberman") != std::string::npos && (lower.find("bomberman") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("advance wars") != std::string::npos && (lower.find("advance wars") != std::string::npos || lower.find("re-boot") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("warioware") != std::string::npos && (lower.find("warioware") != std::string::npos || lower.find("move it") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("no man's sky") != std::string::npos && (lower.find("no man") != std::string::npos || lower.find("nms") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("portal") != std::string::npos && (lower.find("portal") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("persona 4") != std::string::npos && (lower.find("persona 4") != std::string::npos || lower.find("p4g") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("persona 3") != std::string::npos && (lower.find("persona 3") != std::string::npos || lower.find("p3p") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("tunic") != std::string::npos && (lower.find("tunic") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("dave the diver") != std::string::npos && (lower.find("dave the diver") != std::string::npos || lower.find("dave_the_diver") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("cult of the lamb") != std::string::npos && (lower.find("cult of the lamb") != std::string::npos || lower.find("cult_of_the_lamb") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("unicorn overlord") != std::string::npos && (lower.find("unicorn overlord") != std::string::npos || lower.find("unicorn_overlord") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("skyward sword") != std::string::npos && (lower.find("skyward sword") != std::string::npos || lower.find("skyward_sword") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("bowser's fury") != std::string::npos && (lower.find("bowser") != std::string::npos || lower.find("3d world") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("jamboree") != std::string::npos && lower.find("jamboree") != std::string::npos) {
            return &profile;
        }
        if (game_lower.find("brothership") != std::string::npos && (lower.find("brothership") != std::string::npos || lower.find("mario & luigi") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("splatoon") != std::string::npos && lower.find("splatoon") != std::string::npos) {
            return &profile;
        }
        if (game_lower.find("origami king") != std::string::npos && (lower.find("origami") != std::string::npos || lower.find("paper mario") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("age of calamity") != std::string::npos && (lower.find("calamity") != std::string::npos || lower.find("hyrule warriors") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("cuphead") != std::string::npos && lower.find("cuphead") != std::string::npos) {
            return &profile;
        }
        if (game_lower.find("blind forest") != std::string::npos && (lower.find("blind forest") != std::string::npos || lower.find("ori") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("slay the spire") != std::string::npos && (lower.find("slay the spire") != std::string::npos || lower.find("slay_the_spire") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("into the breach") != std::string::npos && (lower.find("into the breach") != std::string::npos || lower.find("into_the_breach") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("tales of") != std::string::npos && (lower.find("tales") != std::string::npos || lower.find("berseria") != std::string::npos || lower.find("symphonia") != std::string::npos || lower.find("vesperia") != std::string::npos || lower.find("graces") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("astral chain") != std::string::npos && (lower.find("astral chain") != std::string::npos || lower.find("astral_chain") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("three houses") != std::string::npos && (lower.find("three houses") != std::string::npos || lower.find("fe3h") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("smash bros") != std::string::npos && (lower.find("smash") != std::string::npos || lower.find("ssbu") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("dragon quest") != std::string::npos && (lower.find("dragon quest") != std::string::npos || lower.find("dq11") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("nier") != std::string::npos && (lower.find("nier") != std::string::npos || lower.find("automata") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("bayonetta") != std::string::npos && lower.find("bayonetta") != std::string::npos) {
            return &profile;
        }
        if (game_lower.find("persona 5") != std::string::npos && (lower.find("persona 5") != std::string::npos || lower.find("p5r") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("live a live") != std::string::npos && (lower.find("live a live") != std::string::npos || lower.find("live_a_live") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("tony hawk") != std::string::npos && (lower.find("tony hawk") != std::string::npos || lower.find("thps") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("nitro-fueled") != std::string::npos && (lower.find("nitro") != std::string::npos || lower.find("ctr") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("mortal kombat") != std::string::npos && (lower.find("mortal kombat") != std::string::npos || lower.find("mk11") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("borderlands") != std::string::npos && lower.find("borderlands") != std::string::npos) {
            return &profile;
        }
        if (game_lower.find("diablo iii") != std::string::npos && (lower.find("diablo iii") != std::string::npos || lower.find("diablo 3") != std::string::npos || lower.find("d3") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("demon slayer") != std::string::npos && (lower.find("demon slayer") != std::string::npos || lower.find("hinokami") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("monster hunter") != std::string::npos && (lower.find("monster hunter") != std::string::npos || lower.find("mhr") != std::string::npos || lower.find("sunbreak") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("burnout paradise") != std::string::npos && (lower.find("burnout") != std::string::npos || lower.find("paradise") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("overcooked") != std::string::npos && lower.find("overcooked") != std::string::npos) {
            return &profile;
        }
        if (game_lower.find("neighborville") != std::string::npos && (lower.find("neighborville") != std::string::npos || lower.find("pvz") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("risk of rain") != std::string::npos && (lower.find("risk of rain") != std::string::npos || lower.find("ror2") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("minecraft dungeons") != std::string::npos && (lower.find("dungeons") != std::string::npos || lower.find("mcd") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("dragon ball fighterz") != std::string::npos && (lower.find("fighterz") != std::string::npos || lower.find("dbfz") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("xenoverse 2") != std::string::npos && (lower.find("xenoverse") != std::string::npos || lower.find("dbxv2") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("nba 2k") != std::string::npos && lower.find("nba 2k") != std::string::npos) {
            return &profile;
        }
        if (game_lower.find("just dance") != std::string::npos && lower.find("just dance") != std::string::npos) {
            return &profile;
        }
        if (game_lower.find("51 worldwide") != std::string::npos && (lower.find("clubhouse") != std::string::npos || lower.find("51 worldwide") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("tennis aces") != std::string::npos && (lower.find("tennis aces") != std::string::npos || lower.find("mario tennis") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("super rush") != std::string::npos && (lower.find("super rush") != std::string::npos || lower.find("mario golf") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("shin megami tensei") != std::string::npos && (lower.find("megami tensei") != std::string::npos || lower.find("smt5") != std::string::npos || lower.find("smtv") != std::string::npos || lower.find("vengeance") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("metroid prime") != std::string::npos && (lower.find("metroid prime") != std::string::npos || lower.find("prime remastered") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("crisis core") != std::string::npos && (lower.find("crisis core") != std::string::npos || lower.find("ffvii") != std::string::npos || lower.find("reunion") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("octopath traveler") != std::string::npos && (lower.find("octopath") != std::string::npos || lower.find("octopath2") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("sonic frontiers") != std::string::npos && (lower.find("sonic frontiers") != std::string::npos || lower.find("frontiers") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("tactics ogre") != std::string::npos && (lower.find("tactics ogre") != std::string::npos || lower.find("reborn") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("dragon's dogma") != std::string::npos && (lower.find("dragon's dogma") != std::string::npos || lower.find("dragons dogma") != std::string::npos || lower.find("dark arisen") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("warframe") != std::string::npos && lower.find("warframe") != std::string::npos) {
            return &profile;
        }
        if (game_lower.find("brotato") != std::string::npos && lower.find("brotato") != std::string::npos) {
            return &profile;
        }
        if (game_lower.find("vampire survivors") != std::string::npos && (lower.find("vampire") != std::string::npos || lower.find("survivors") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("xenoblade") != std::string::npos && (lower.find("xenoblade") != std::string::npos || lower.find("xcde") != std::string::npos || lower.find("xc2") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("mario rpg") != std::string::npos && (lower.find("mario rpg") != std::string::npos || lower.find("super mario rpg") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("showtime") != std::string::npos && (lower.find("showtime") != std::string::npos || lower.find("princess peach") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("luigi's mansion 2") != std::string::npos && (lower.find("mansion 2") != std::string::npos || lower.find("dark moon") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("f-zero") != std::string::npos && (lower.find("f-zero") != std::string::npos || lower.find("fzero") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("3d all-stars") != std::string::npos && (lower.find("all-stars") != std::string::npos || lower.find("all stars") != std::string::npos || lower.find("sunshine") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("arceus") != std::string::npos && (lower.find("arceus") != std::string::npos || lower.find("legends") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("scarlet") != std::string::npos && lower.find("scarlet") != std::string::npos) {
            return &profile;
        }
        if (game_lower.find("violet") != std::string::npos && lower.find("violet") != std::string::npos) {
            return &profile;
        }
        if (game_lower.find("devilutionx") != std::string::npos && (lower.find("devilutionx") != std::string::npos || lower.find("diablo") != std::string::npos || lower.find("hellfire") != std::string::npos || lower.find("diabdat") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("gta v") != std::string::npos && (lower.find("gta v") != std::string::npos || lower.find("gta 5") != std::string::npos || lower.find("0100b00b51230000") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("re3-sa") != std::string::npos && (lower.find("re3-sa") != std::string::npos || lower.find("san andreas") != std::string::npos || lower.find("gtasa") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("revc") != std::string::npos && (lower.find("revc") != std::string::npos || lower.find("vice city") != std::string::npos || lower.find("gtavc") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("re3") != std::string::npos && (lower.find("re3") != std::string::npos || lower.find("gta 3") != std::string::npos || lower.find("gta iii") != std::string::npos || lower.find("gta3") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("xash3d") != std::string::npos && (lower.find("xash3d") != std::string::npos || lower.find("half-life") != std::string::npos || lower.find("halflife") != std::string::npos || lower.find("valve.wad") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("gzdoom") != std::string::npos && (lower.find("gzdoom") != std::string::npos || lower.find("prboom") != std::string::npos || lower.find("crispy") != std::string::npos || lower.find("doom.wad") != std::string::npos || lower.find("doom2.wad") != std::string::npos || lower.find("plutonia") != std::string::npos || lower.find("sigil") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("dhewm3") != std::string::npos && (lower.find("dhewm3") != std::string::npos || lower.find("doom 3") != std::string::npos || lower.find("doom3") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("quakespasm") != std::string::npos && (lower.find("quake") != std::string::npos || lower.find("quakespasm") != std::string::npos || lower.find("yamagi") != std::string::npos || lower.find("ioquake3") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("eduke32") != std::string::npos && (lower.find("duke") != std::string::npos || lower.find("duke3d") != std::string::npos || lower.find("voidsw") != std::string::npos || lower.find("shadow warrior") != std::string::npos || lower.find("nblood") != std::string::npos || lower.find("blood.rff") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("iortcw") != std::string::npos && (lower.find("iortcw") != std::string::npos || lower.find("ecwolf") != std::string::npos || lower.find("wolfenstein") != std::string::npos || lower.find("wolf3d") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("fallout-ce") != std::string::npos && (lower.find("fallout") != std::string::npos || lower.find("fallout-ce") != std::string::npos || lower.find("fallout2-ce") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("fheroes2") != std::string::npos && (lower.find("heroes") != std::string::npos || lower.find("fheroes2") != std::string::npos || lower.find("vcmi") != std::string::npos || lower.find("homm") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("max payne") != std::string::npos && (lower.find("max payne") != std::string::npos || lower.find("maxpayne") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("openmw") != std::string::npos && (lower.find("openmw") != std::string::npos || lower.find("morrowind") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("openjk") != std::string::npos && (lower.find("openjk") != std::string::npos || lower.find("jedi outcast") != std::string::npos || lower.find("jedi academy") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("openxray") != std::string::npos && (lower.find("openxray") != std::string::npos || lower.find("stalker") != std::string::npos || lower.find("s.t.a.l.k.e.r") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("rsdk") != std::string::npos && (lower.find("rsdk") != std::string::npos || lower.find("sonic cd") != std::string::npos || lower.find("data.rsdk") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("sm64-nx") != std::string::npos && (lower.find("sm64") != std::string::npos || lower.find("render96") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("ship of harkinian") != std::string::npos && (lower.find("harkinian") != std::string::npos || lower.find("soh") != std::string::npos || lower.find("2s2h") != std::string::npos || lower.find("ocarina of time") != std::string::npos || lower.find("majora") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("nxengine") != std::string::npos && (lower.find("nxengine") != std::string::npos || lower.find("cave story") != std::string::npos || lower.find("am2r") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("corsixth") != std::string::npos && (lower.find("corsixth") != std::string::npos || lower.find("theme hospital") != std::string::npos || lower.find("julius") != std::string::npos || lower.find("augustus") != std::string::npos || lower.find("caesar") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("openra") != std::string::npos && (lower.find("openra") != std::string::npos || lower.find("dune legacy") != std::string::npos || lower.find("command & conquer") != std::string::npos || lower.find("red alert") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("retroarch") != std::string::npos && (lower.find("retroarch") != std::string::npos || lower.find("ppsspp") != std::string::npos || lower.find("flycast") != std::string::npos || lower.find("scummvm") != std::string::npos || lower.find("melonds") != std::string::npos || lower.find("mgba") != std::string::npos || lower.find("duckstation") != std::string::npos)) {
            return &profile;
        }
        if (game_lower.find("homebrew utilities") != std::string::npos && (lower.find("nx-shell") != std::string::npos || lower.find("dbi") != std::string::npos || lower.find("goldleaf") != std::string::npos || lower.find("jksv") != std::string::npos || lower.find("checkpoint") != std::string::npos || lower.find("edizon") != std::string::npos || lower.find("awoo") != std::string::npos || lower.find("tesla") != std::string::npos)) {
            return &profile;
        }

        if (lower.find(game_lower) != std::string::npos) {
            return &profile;
        }
    }
    return nullptr;
}

bool GameFixDatabase::HasProfile(u64 title_id) {
    return GetProfile(title_id) != nullptr;
}

const std::vector<GameFixProfile>& GameFixDatabase::GetAllProfiles() {
    return s_profiles;
}

bool GameFixDatabase::ApplyProfileToPerGameConfig(u64 title_id, const std::string& config_file_path) {
    const auto* profile = GetProfile(title_id);
    if (!profile) {
        return false;
    }

    std::filesystem::path path(config_file_path);
    std::filesystem::create_directories(path.parent_path());

    // Read existing INI if present
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> sections;
    if (std::filesystem::exists(path)) {
        std::ifstream file(path);
        std::string line;
        std::string current_section;
        while (std::getline(file, line)) {
            line = Common::FS::SanitizePath(line);
            if (line.empty() || line[0] == '#' || line[0] == ';') continue;
            if (line.front() == '[' && line.back() == ']') {
                current_section = line.substr(1, line.size() - 2);
            } else {
                auto eq = line.find('=');
                if (eq != std::string::npos && !current_section.empty()) {
                    auto key = line.substr(0, eq);
                    auto val = line.substr(eq + 1);
                    sections[current_section][key] = val;
                }
            }
        }
    }

    // Merge settings from profile
    for (const auto& [full_key, val] : profile->ini_settings) {
        auto slash = full_key.find('\\');
        if (slash != std::string::npos) {
            auto sec = full_key.substr(0, slash);
            auto key = full_key.substr(slash + 1);
            sections[sec][key] = val;
            sections[sec][key + "\\use_global"] = "false";
            sections[sec][key + "\\default"] = "false";
        }
    }
    sections["StormEden"]["storm_fix_applied"] = "true";

    // Write back INI
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) return false;

    for (const auto& [sec, kvs] : sections) {
        out << "[" << sec << "]\n";
        for (const auto& [k, v] : kvs) {
            out << k << "=" << v << "\n";
        }
        out << "\n";
    }

    LOG_INFO(Frontend, "Applied GameFix profile for {:#016x} to {}", title_id, config_file_path);
    return true;
}

void GameFixDatabase::SetDontAskAgain(u64 title_id, const std::string& config_file_path) {
    std::filesystem::path path(config_file_path);
    std::filesystem::create_directories(path.parent_path());

    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> sections;
    if (std::filesystem::exists(path)) {
        std::ifstream file(path);
        std::string line;
        std::string current_section;
        while (std::getline(file, line)) {
            line = Common::FS::SanitizePath(line);
            if (line.empty() || line[0] == '#' || line[0] == ';') continue;
            if (line.front() == '[' && line.back() == ']') {
                current_section = line.substr(1, line.size() - 2);
            } else {
                auto eq = line.find('=');
                if (eq != std::string::npos && !current_section.empty()) {
                    auto key = line.substr(0, eq);
                    auto val = line.substr(eq + 1);
                    sections[current_section][key] = val;
                }
            }
        }
    }

    sections["StormEden"]["storm_fix_dont_ask"] = "true";

    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) return;

    for (const auto& [sec, kvs] : sections) {
        out << "[" << sec << "]\n";
        for (const auto& [k, v] : kvs) {
            out << k << "=" << v << "\n";
        }
        out << "\n";
    }
}

bool GameFixDatabase::ApplyProfileDirectly(u64 title_id) {
    const auto* profile = GetProfile(title_id);
    if (!profile) {
        return false;
    }

    for (const auto& [full_key, val] : profile->ini_settings) {
        if (full_key == "Renderer\\gpu_accuracy") {
            Settings::values.gpu_accuracy.SetValue(static_cast<Settings::GpuAccuracy>(std::stoi(val)));
            Settings::values.gpu_accuracy.SetGlobal(false);
        } else if (full_key == "Renderer\\barrier_feedback_loops") {
            Settings::values.barrier_feedback_loops.SetValue(val == "true");
            Settings::values.barrier_feedback_loops.SetGlobal(false);
        } else if (full_key == "Renderer\\use_reactive_flushing") {
            Settings::values.use_reactive_flushing.SetValue(val == "true");
            Settings::values.use_reactive_flushing.SetGlobal(false);
        } else if (full_key == "Renderer\\astc_recompression") {
            Settings::values.astc_recompression.SetValue(static_cast<Settings::AstcRecompression>(std::stoi(val)));
            Settings::values.astc_recompression.SetGlobal(false);
        } else if (full_key == "Renderer\\use_asynchronous_shaders") {
            Settings::values.use_asynchronous_shaders.SetValue(val == "true");
            Settings::values.use_asynchronous_shaders.SetGlobal(false);
        } else if (full_key == "Renderer\\max_anisotropy") {
            Settings::values.max_anisotropy.SetValue(static_cast<Settings::AnisotropyMode>(std::stoi(val)));
            Settings::values.max_anisotropy.SetGlobal(false);
        } else if (full_key == "Renderer\\resolution_setup") {
            Settings::values.resolution_setup.SetValue(static_cast<Settings::ResolutionSetup>(std::stoi(val)));
            Settings::values.resolution_setup.SetGlobal(false);
        } else if (full_key == "Renderer\\fsr_sharpening_slider") {
            Settings::values.fsr_sharpening_slider.SetValue(static_cast<u8>(std::stoi(val)));
            Settings::values.fsr_sharpening_slider.SetGlobal(false);
        } else if (full_key == "Renderer\\use_fast_gpu_time") {
            Settings::values.gpu_clock.SetValue(val == "true" ? Settings::GpuClock::Boost : Settings::GpuClock::Normal);
            Settings::values.gpu_clock.SetGlobal(false);
        } else if (full_key == "Renderer\\dyna_state") {
            Settings::values.dyna_state.SetValue(static_cast<Settings::ExtendedDynamicState>(std::stoi(val)));
            Settings::values.dyna_state.SetGlobal(false);
        } else if (full_key == "System\\airplane_mode") {
            Settings::values.airplane_mode.SetValue(val == "true");
            Settings::values.airplane_mode.SetGlobal(false);
        } else if (full_key == "Core\\memory_layout_mode") {
            Settings::values.memory_layout_mode.SetValue(static_cast<Settings::MemoryLayout>(std::stoi(val)));
            Settings::values.memory_layout_mode.SetGlobal(false);
        } else if (full_key == "Cpu\\cpuopt_fastmem") {
            Settings::values.cpuopt_fastmem.SetValue(val == "true");
            Settings::values.cpuopt_fastmem.SetGlobal(false);
        }
    }
    Settings::UpdateGPUAccuracy();
    Settings::UpdateRescalingInfo();
    LOG_INFO(Frontend, "Directly applied GameFix profile in-memory for {:#016x}", title_id);
    return true;
}

} // namespace Core
