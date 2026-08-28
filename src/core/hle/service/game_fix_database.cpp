// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/hle/service/game_fix_database.h"
#include <fstream>
#include <sstream>
#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "common/logging.h"

namespace Core {

static const std::vector<GameFixProfile> s_profiles = {
    {
        0x01007EF00011E000ULL,
        "The Legend of Zelda: Breath of the Wild",
        "• Белая непрозрачная вода из-за рассинхронизации буфера глубины и рефракций\n• Желтые/ослепляющие вспышки частиц и тумана в Святилищах (Shrines)\n• Мерцание текстур травы и теней",
        "• White opaque water caused by depth buffer and refraction desync\n• Yellow/blinding particle flashes and fog in Shrines\n• Grass and shadow texture flickering",
        "✓ Точность GPU: Высокая (High — идеальная прозрачная вода)\n✓ Реактивный сброс (Reactive Flushing): Включено\n✓ Сжатие ASTC: Отключено (Uncompressed — решает проблемы воды и Святилищ)\n✓ Асинхронные шейдеры: Включено\n✓ Память: 8GB DRAM",
        "✓ GPU Accuracy: High (Crystal clear water transparency)\n✓ Reactive Flushing: Enabled\n✓ ASTC Recompression: Uncompressed (Fixes water & Shrines)\n✓ Asynchronous Shaders: Enabled\n✓ Memory Layout: 8GB DRAM",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_reactive_flushing", "true"},
            {"Renderer\\astc_recompression", "0"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Core\\memory_layout_mode", "2"}
        }
    },
    {
        0x0100F2C0115B6000ULL,
        "The Legend of Zelda: Tears of the Kingdom",
        "• Черный экран при смене оружия и способностей (Fuse/Ultrahand)\n• Утечки VRAM в кавернах и святилищах\n• Мерцание текстур скверны (Gloom) и теней облаков",
        "• Black screen during weapon/ability switching (Fuse menu)\n• VRAM leaks in Depths and Shrines\n• Gloom texture flickering and cloud shadow artifacts",
        "✓ Точность GPU: Высокая (High)\n✓ Реактивный сброс (Reactive Flushing): Включено\n✓ Сжатие ASTC: Отключено (Uncompressed)\n✓ Анизотропная фильтрация: 16x\n✓ Асинхронные шейдеры: Включено\n✓ Память: 8GB DRAM",
        "✓ GPU Accuracy: High\n✓ Reactive Flushing: Enabled\n✓ ASTC Recompression: Uncompressed\n✓ Anisotropic Filtering: 16x\n✓ Asynchronous Shaders: Enabled\n✓ Memory Layout: 8GB DRAM",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_reactive_flushing", "true"},
            {"Renderer\\astc_recompression", "0"},
            {"Renderer\\max_anisotropy", "5"},
            {"Renderer\\use_asynchronous_shaders", "true"},
            {"Core\\memory_layout_mode", "2"}
        }
    },
    {
        0x01004D701742A000ULL,
        "Paper Mario: The Thousand-Year Door",
        "• Черный экран на катсценах в прологе\n• Сбои 2D-шрифтов диалогов и мерцание текстур",
        "• Black screen during prologue cutscenes\n• Corrupted battle text boxes and flickering textures",
        "✓ Точность GPU: Высокая (High)\n✓ Сжатие ASTC: Отключено (для идеального видео)\n✓ Реактивный сброс (Reactive Flushing): Включено",
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
        "• Вылет из-за нехватки памяти (OOM) при загрузке замка Хогвартс\n• Высокое потребление ОЗУ (>8.5 ГБ) на мобильных чипах",
        "• Out of memory (OOM) crash when loading Hogwarts Castle\n• High RAM consumption (>8.5 GB) on mobile SoCs",
        "✓ Разрешение: Handheld 0.75X + FSR 80%\n✓ Сжатие текстур ASTC: BC1 (снижает RAM до 4.8 ГБ)\n✓ Режим памяти: 8GB DRAM",
        "✓ Resolution: Handheld 0.75X + FSR 80%\n✓ ASTC Recompression: BC1 (lowers RAM to 4.8 GB)\n✓ Memory Layout: 8GB DRAM",
        {
            {"Renderer\\astc_recompression", "1"},
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
        "✓ Разрешение: Handheld 0.75X-1.0X + FSR 80%\n✓ Сброс реактивной памяти (Reactive Flushing): Отключено\n✓ Защита от троттлинга GPU (Floor Clamp)",
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
        "✓ Поддержка RenameDirectory в STORM EDEN 4.6.0+\n✓ Быстрая память (Fastmem): Включено\n✓ Асинхронные шейдеры: Включено",
        "✓ RenameDirectory support in STORM EDEN 4.6.0+\n✓ Fastmem: Enabled\n✓ Asynchronous Shaders: Enabled",
        {
            {"Cpu\\cpuopt_fastmem", "true"},
            {"Renderer\\use_asynchronous_shaders", "true"}
        }
    },
    {
        0x0100923008C54000ULL,
        "LEGO Star Wars: The Skywalker Saga",
        "• Бесконечная загрузка (зацикливание индикатора загрузки TT Games)\n• Зависание асинхронного таймера GPU при обращении к ресурсам",
        "• Infinite loading screen (TT Games loading indicator loop)\n• GPU async timer deadlock during asset initialization",
        "✓ Быстрое время GPU (Fast GPU Time): Отключено (устраняет вечную загрузку!)\n✓ Динамическое состояние: Базовое (EDS1)\n✓ Точность GPU: Высокая (High)\n✓ Точность CPU: Точная (Accurate)\n✓ Реактивный сброс: Отключено\n✓ Память: 8GB DRAM",
        "✓ Fast GPU Time: Disabled (Fixes infinite loading!)\n✓ Dynamic State: Basic (EDS1)\n✓ GPU Accuracy: High\n✓ CPU Accuracy: Accurate\n✓ Reactive Flushing: Disabled\n✓ Memory Layout: 8GB DRAM",
        {
            {"Renderer\\use_fast_gpu_time", "false"},
            {"Renderer\\dyna_state", "0"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_reactive_flushing", "false"},
            {"Renderer\\astc_recompression", "0"},
            {"Renderer\\use_asynchronous_shaders", "false"},
            {"Cpu\\cpu_accuracy", "0"},
            {"Core\\memory_layout_mode", "2"}
        }
    },
    {
        0x0100E26017E5E000ULL,
        "Red Dead Redemption",
        "• Хрипы и рассинхронизация звука в катсценах\n• Микрофризы физических потоков RAGE Engine",
        "• Audio crackling and desync in cutscenes\n• Micro-stutters in RAGE Engine physics threads",
        "✓ Точность CPU: Точная (Accurate)\n✓ Аудио-буфер Cubeb: 80 ms\n✓ Синхронизация памяти (Sync Memory): Отключено",
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
        "✓ Extended Dynamic State: Включено\n✓ Точность GPU: Высокая (High)\n✓ Точность CPU: Accurate",
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
        "✓ Depth Clip Control: Включено (STORM DRIVER)\n✓ Точность GPU: Высокая (High)",
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
        "✓ Эмуляция цвета BGR565: Включено\n✓ Коррекция эффектов свечения (Fix Bloom): Включено",
        "✓ Emulate BGR565: Enabled\n✓ Fix Bloom Effects: Enabled",
        {
            {"Renderer\\emulate_bgr565", "true"},
            {"Renderer\\fix_bloom_effects", "true"}
        }
    },
    {
        0x01006A800016E000ULL,
        "Super Smash Bros. Ultimate",
        "• Вылет на экране победы или в меню новостей (Web Applet)",
        "• Crash on victory screen or news board (Web Applet)",
        "✓ Отключение Web-апплета: Включено\n✓ Mii Applet: LLE",
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
        "✓ Требуется Firmware 18.0.0+ и системные файлы Mii\n✓ Сжатие ASTC: Uncompressed",
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
        "✓ Точность GPU: Высокая (High)\n✓ Анизотропная фильтрация: 16x\n✓ Декодирование ASTC на GPU: Включено",
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
        "✓ Разрешение: Handheld 0.75X + FSR 75%\n✓ Сжатие ASTC: BC3\n✓ Ограничение VRAM: Conservative",
        "✓ Resolution: Handheld 0.75X + FSR 75%\n✓ ASTC Recompression: BC3\n✓ VRAM Usage: Conservative",
        {
            {"Renderer\\astc_recompression", "2"},
            {"Renderer\\resolution_setup", "1"},
            {"Renderer\\vram_usage_mode", "1"}
        }
    },
    {
        0x0100A3D0086EE000ULL,
        "Pokemon Violet",
        "• Утечки памяти в открытом мире Палдеи\n• Мерцание ландшафта и текстур",
        "• Open-world memory leaks in Paldea\n• Terrain and texture flickering",
        "✓ Разрешение: Handheld 0.75X + FSR 75%\n✓ Сжатие ASTC: BC3\n✓ Ограничение VRAM: Conservative",
        "✓ Resolution: Handheld 0.75X + FSR 75%\n✓ ASTC Recompression: BC3\n✓ VRAM Usage: Conservative",
        {
            {"Renderer\\astc_recompression", "2"},
            {"Renderer\\resolution_setup", "1"},
            {"Renderer\\vram_usage_mode", "1"}
        }
    },
    {
        0x0100B9F010DC4000ULL,
        "Doom Eternal",
        "• Вылет драйвера Vulkan при первом выстреле / спавне BFG",
        "• Vulkan device loss crash on weapon fire / BFG",
        "✓ Проверка границ глубины: STORM DRIVER (pan_depth_bounds)\n✓ Точность DMA: Safe",
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
        "• Вылет по нехватке памяти (OOM) при погонях на Бэтмобиле",
        "• OOM crash during Batmobile chase sequences",
        "✓ Разрешение: Handheld 0.75X + FSR 80%\n✓ Сжатие ASTC: BC1\n✓ Память: 8GB DRAM",
        "✓ Resolution: Handheld 0.75X + FSR 80%\n✓ ASTC Recompression: BC1\n✓ Memory Layout: 8GB DRAM",
        {
            {"Renderer\\astc_recompression", "1"},
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
        "✓ Точность DMA: Safe\n✓ Точность GPU: Высокая (High)\n✓ Barrier Feedback Loops: Включено",
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
        "✓ Разрешение: Handheld 0.75X + FSR 80%\n✓ Сжатие ASTC: BC1\n✓ Память: 8GB DRAM",
        "✓ Resolution: Handheld 0.75X + FSR 80%\n✓ ASTC Recompression: BC1\n✓ Memory Layout: 8GB DRAM",
        {
            {"Renderer\\astc_recompression", "1"},
            {"Renderer\\resolution_setup", "1"},
            {"Core\\memory_layout_mode", "2"}
        }
    },
    {
        0x01004AB00A266000ULL,
        "Dark Souls: Remastered",
        "• Просадки кадровой частоты у костров и зацикливание звука баффов",
        "• Bonfire particle slowdown and weapon buff sound loop",
        "✓ Точность CPU: Accurate\n✓ Точность GPU: High\n✓ Аудио-движок: Cubeb",
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
        "• Утечка памяти и вылеты при смене локаций (OOM)\n• Размытие и мерцание текста диалогов TextMeshPro\n• Цветовые артефакты акварельных портретов и фонов",
        "• Out of memory (OOM) crash on zone transitions\n• TextMeshPro dialogue font blur and jitter\n• Color compression artifacts on painted portraits and backdrops",
        "✓ Память: 6GB DRAM (ликвидация OOM вылетов Unity)\n✓ Сжатие ASTC: Отключено (Uncompressed — идеальное качество артов)\n✓ Динамическое состояние: EDS1\n✓ Точность GPU: Высокая (High)\n✓ Быстрая память (Fastmem): Включено\n✓ Реактивный сброс: Включено",
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
        "✓ Точность CPU: Точная (Accurate)\n✓ Память: 6GB DRAM\n✓ Динамическое состояние: EDS1",
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
        "✓ Точность GPU: Высокая (High)\n✓ Динамическое состояние: EDS2\n✓ Сжатие ASTC: BC3\n✓ Память: 6GB DRAM",
        "✓ GPU Accuracy: High\n✓ Dynamic State: EDS2\n✓ ASTC Recompression: BC3\n✓ Memory Layout: 6GB DRAM",
        {
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\dyna_state", "2"},
            {"Renderer\\astc_recompression", "2"},
            {"Core\\memory_layout_mode", "1"},
            {"Renderer\\use_fast_gpu_time", "true"}
        }
    },
    {
        0x0100E95004038000ULL,
        "Xenoblade Chronicles 2",
        "• Просадки кадровой частоты в густонаселенных локациях (Гула, Мор Ардайн)",
        "• Heavy frame drops in dense titan areas (Gormott, Mor Ardain)",
        "✓ Динамическое состояние: EDS2\n✓ Сжатие ASTC: BC3\n✓ Память: 6GB DRAM\n✓ Быстрое время GPU: Включено",
        "✓ Dynamic State: EDS2\n✓ ASTC Recompression: BC3\n✓ Memory Layout: 6GB DRAM\n✓ Fast GPU Time: Enabled",
        {
            {"Renderer\\dyna_state", "2"},
            {"Renderer\\astc_recompression", "2"},
            {"Core\\memory_layout_mode", "1"},
            {"Renderer\\use_fast_gpu_time", "true"}
        }
    },
    {
        0x010074F013262000ULL,
        "Xenoblade Chronicles 3",
        "• Утечки VRAM и микростаттеры в битвах с 7 персонажами",
        "• VRAM leaks and micro-stutters during full 7-character battle parties",
        "✓ Сжатие ASTC: BC3\n✓ Память: 6GB DRAM\n✓ Динамическое состояние: EDS2\n✓ Точность GPU: Высокая (High)",
        "✓ ASTC Recompression: BC3\n✓ Memory Layout: 6GB DRAM\n✓ Dynamic State: EDS2\n✓ GPU Accuracy: High",
        {
            {"Renderer\\astc_recompression", "2"},
            {"Core\\memory_layout_mode", "1"},
            {"Renderer\\dyna_state", "2"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\use_fast_gpu_time", "true"}
        }
    },
    {
        0x0100E67012924000ULL,
        "The Witcher 3: Wild Hunt - Complete Edition",
        "• Вылеты по памяти (OOM) и заикания физики в Новиграде и Туссенте",
        "• OOM crashes and physics stutter in Novigrad and Toussaint",
        "✓ Память: 6GB DRAM\n✓ Точность GPU: Высокая (High)\n✓ Сжатие ASTC: BC3\n✓ Быстрое время GPU: Включено\n✓ Реактивный сброс: Включено",
        "✓ Memory Layout: 6GB DRAM\n✓ GPU Accuracy: High\n✓ ASTC Recompression: BC3\n✓ Fast GPU Time: Enabled\n✓ Reactive Flushing: Enabled",
        {
            {"Core\\memory_layout_mode", "1"},
            {"Renderer\\gpu_accuracy", "1"},
            {"Renderer\\astc_recompression", "2"},
            {"Renderer\\use_fast_gpu_time", "true"},
            {"Renderer\\use_reactive_flushing", "true"}
        }
    }
};

const GameFixProfile* GameFixDatabase::GetProfile(u64 title_id) {
    // Check exact title ID or base title ID (mask out DLC/update bits)
    const u64 base_title_id = title_id & ~0x1FFFULL;
    for (const auto& profile : s_profiles) {
        if (profile.title_id == title_id || (profile.title_id & ~0x1FFFULL) == base_title_id) {
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

} // namespace Core
