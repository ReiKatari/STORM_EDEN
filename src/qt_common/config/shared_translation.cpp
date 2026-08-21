// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2024 Torzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "shared_translation.h"

#include <map>
#include <memory>
#include <utility>
#include <QCoreApplication>
#include "common/settings.h"
#include "common/settings_enums.h"
#include "common/settings_setting.h"
#include "common/time_zone.h"
#include "qt_common/config/uisettings.h"

namespace ConfigurationShared {

std::unique_ptr<TranslationMap> InitializeTranslations(QObject* parent) {
    std::unique_ptr<TranslationMap> translations = std::make_unique<TranslationMap>();
    const auto& tr = [](const char* text, const char* disambiguation = nullptr) -> QString {
        return QCoreApplication::translate("ConfigurationShared", text, disambiguation);
    };

#define INSERT(SETTINGS, ID, NAME, TOOLTIP)                                                        \
    translations->insert(std::pair{SETTINGS::values.ID.Id(), std::pair{(NAME), (TOOLTIP)}})

    // A setting can be ignored by giving it a blank name

    // Applets
    INSERT(Settings, cabinet_applet_mode, tr("Amiibo editor"), QString());
    INSERT(Settings, controller_applet_mode, tr("Controller configuration"), QString());
    INSERT(Settings, data_erase_applet_mode, tr("Data erase"), QString());
    INSERT(Settings, error_applet_mode, tr("Error"), QString());
    INSERT(Settings, net_connect_applet_mode, tr("Net connect"), QString());
    INSERT(Settings, player_select_applet_mode, tr("Player select"), QString());
    INSERT(Settings, swkbd_applet_mode, tr("Software keyboard"), QString());
    INSERT(Settings, mii_edit_applet_mode, tr("Mii Edit"), QString());
    INSERT(Settings, web_applet_mode, tr("Online web"), QString());
    INSERT(Settings, shop_applet_mode, tr("Shop"), QString());
    INSERT(Settings, photo_viewer_applet_mode, tr("Photo viewer"), QString());
    INSERT(Settings, offline_web_applet_mode, tr("Offline web"), QString());
    INSERT(Settings, login_share_applet_mode, tr("Login share"), QString());
    INSERT(Settings, wifi_web_auth_applet_mode, tr("Wifi web auth"), QString());
    INSERT(Settings, my_page_applet_mode, tr("My page"), QString());
    INSERT(Settings, enable_overlay, tr("Enable Overlay Applet"),
           tr("Enables Horizon\'s built-in overlay applet. Press and hold the home button for 1 "
              "second to show it."));

    // Audio
    INSERT(Settings, sink_id, tr("Output Engine:"), QString());
    INSERT(Settings, audio_output_device_id, tr("Output Device:"), QString());
    INSERT(Settings, audio_input_device_id, tr("Input Device:"), QString());
    INSERT(Settings, audio_muted, tr("Mute audio"), QString());
    INSERT(Settings, volume, tr("Volume:"), QString());
    INSERT(Settings, dump_audio_commands, QString(), QString());
    INSERT(UISettings, mute_when_in_background, tr("Mute audio when in background"), QString());

    // Core
    INSERT(Settings, use_multi_core, tr("Многопоточная эмуляция ЦП"),
           tr("Увеличивает количество потоков эмуляции центрального процессора с 1 до 4.\n"
              "Значительно повышает производительность на многоядерных процессорах.\n"
              "Рекомендуется всегда держать включенным."));
    INSERT(Settings, memory_layout_mode, tr("Конфигурация памяти DRAM"),
           tr("Увеличивает объем виртуальной оперативной памяти, доступной гостевой системе.\n"
              "Позволяет загружать ресурсоемкие моды высокого разрешения (HD-текстуры) и предотвращает вылеты из-за нехватки памяти."));
    INSERT(Settings, use_speed_limit, QString(), QString());
    INSERT(Settings, current_speed_mode, QString(), QString());
    INSERT(Settings, speed_limit, tr("Ограничение скорости (%)"),
           tr("Задает максимальную скорость рендеринга игры (100% = стандартная скорость).\n"
              "200% для 30 FPS игры даст 60 FPS, для 60 FPS — 120 FPS.\n"
              "Отключение опции (0%) полностью разблокирует частоту кадров."));

    INSERT(Settings, turbo_speed_limit, tr("Турбо-скорость (%)"),
           tr("Скорость эмуляции при зажатии горячей клавиши Турбо-режима."));
    INSERT(Settings, slow_speed_limit, tr("Замедленная скорость (%)"),
           tr("Скорость эмуляции при зажатии горячей клавиши Замедления."));

    INSERT(Settings, sync_core_speed, tr("Синхронизация частоты ядер ЦП"),
           tr("Синхронизирует тактовую частоту ядер ЦП со скоростью рендеринга для повышения FPS без ускорения внутриигровой физики.\n"
              "Помогает устранить рывки и микрофризы при низком фреймрейте."));

    // Cpu
    INSERT(Settings, cpu_accuracy, tr("Точность ЦП:"),
           tr("Определяет уровень точности эмуляции центрального процессора (ЦП).\n"
              "Авто (Auto) обеспечивает оптимальный баланс производительности и стабильности."));
    INSERT(Settings, cpu_clock, tr("Частота ЦП"),
           tr("Повышает тактовую частоту, о которой эмулятор сообщает гостевой системе, снимая встроенные лимиты FPS.\n"
              "На слабых процессорах может снижать общую производительность."));

    INSERT(Settings, cpu_affinity_pinning, tr("Привязка потоков ЦП (CPU Affinity Pinning)"),
           tr("Жесткая привязка критических потоков JIT и GPU к производительным ядрам ЦП, а фоновых задач — к энергоэффективным ядрам."));

    INSERT(Settings, use_custom_cpu_ticks, QString(), QString());
    INSERT(Settings, cpu_ticks, tr("Пользовательские тики ЦП"),
           tr("Устанавливает пользовательское значение тиков ЦП. Высокие значения могут повысить производительность, но способны вызвать зависания. Рекомендуется 77-21000."));
    INSERT(Settings, cpu_backend, tr("Бэкенд ЦП:"), QString());

    // Cpu Debug

    // Cpu Unsafe
    INSERT(
        Settings, cpuopt_unsafe_host_mmu, tr("Эмуляция Host MMU (fastmem)"),
        tr("Оптимизирует доступ к памяти гостевой программы путем прямого использования MMU хоста.\n"
           "Существенно увеличивает скорость работы. Отключение переводит эмуляцию на медленный программный MMU."));
    INSERT(
        Settings, cpuopt_unsafe_unfuse_fma,
        tr("Разделять FMA (повышает производительность на ЦП без FMA)"),
        tr("Повышает скорость за счет снижения точности инструкций совмещенного умножения-сложения (FMA) на старых процессорах."));
    INSERT(
        Settings, cpuopt_unsafe_reduce_fp_error, tr("Ускоренные FRSQRTE и FRECPE"),
        tr("Увеличивает скорость математических вычислений с плавающей запятой, используя аппаратные аппроксимации."));
    INSERT(Settings, cpuopt_unsafe_ignore_standard_fpcr,
           tr("Ускоренные инструкции ASIMD (только 32 бита)"),
           tr("Ускоряет 32-битные инструкции ASIMD с плавающей запятой."));
    INSERT(Settings, cpuopt_unsafe_inaccurate_nan, tr("Неточная обработка значений NaN"),
           tr("Ускоряет вычисления за счет отключения проверки нечисловых значений (NaN)."));
    INSERT(Settings, cpuopt_unsafe_fastmem_check, tr("Отключить проверку адресного пространства"),
           tr("Отключает проверку диапазона адресного пространства при операциях с fastmem."));
    INSERT(
        Settings, cpuopt_unsafe_ignore_global_monitor, tr("Игнорировать глобальный монитор памяти"),
        tr("Игнорирует глобальный монитор эксклюзивных обращений к памяти для ускорения работы потоков."));

    // Renderer
    INSERT(Settings, renderer_backend, tr("Графический API:"), QString());
    INSERT(Settings, vulkan_device, tr("Устройство:"), QString());
    INSERT(Settings, resolution_setup, tr("Разрешение:"),
           tr("Разрешение внутреннего рендеринга 3D-графики.\n"
              "Повышение разрешения улучшает четкость изображения, но требует больше ресурсов видеокарты."));
    INSERT(Settings, scaling_filter, tr("Фильтр масштабирования окон:"),
           tr("Алгоритм масштабирования низкого разрешения до разрешения экрана.\n"
              "FSR и SGSR обеспечивают наилучшую четкость."));
    INSERT(Settings, fsr_sharpening_slider, tr("Резкость FSR:"),
           tr("Степень резкости алгоритма AMD FidelityFX Super Resolution (FSR)."));
    INSERT(Settings, anti_aliasing, tr("Сглаживание (Anti-Aliasing):"),
           tr("Устраняет неровности и ступенчатость на краях 3D-объектов."));
    INSERT(Settings, fullscreen_mode, tr("Полноэкранный режим:"), QString());
    INSERT(Settings, aspect_ratio, tr("Соотношение сторон:"), QString());
    INSERT(Settings, use_disk_shader_cache, tr("Кэш шейдеров на диске"),
           tr("Сохраняет скомпилированные шейдеры на диск, устраняя задержки и фризы при повторных запусках игр."));
    INSERT(
        Settings, use_asynchronous_gpu_emulation, tr("Асинхронная эмуляция ГПУ"),
        tr("Выполняет задачи графического процессора в отдельном потоке, значительно повышая частоту кадров (FPS)."));
    INSERT(Settings, nvdec_emulation, tr("Декодирование видео NVDEC:"),
           tr("Метод воспроизведения внутриигровых видеороликов (аппаратный ГПУ или ЦП)."));
    INSERT(Settings, accelerate_astc, tr("Декодирование текстур ASTC:"),
           tr("Метод декодирования сжатых текстур ASTC.\n"
              "ГПУ — быстрое аппаратное декодирование на видеокарте.\n"
              "ЦП Асинхронно — фоновое декодирование на процессоре для устранения статтеров."));
    INSERT(Settings, astc_recompression, tr("Пересжатие ASTC:"),
           tr("Сжимает текстуры в более компактные форматы (BC1-BC5), снижая потребление видеопамяти (VRAM)."));
    INSERT(Settings, frame_pacing_mode, tr("Контроль плавности кадров (Frame Pacing)"),
           tr("Ограничивает максимальный FPS для обеспечения равномерного интервала между кадрами."));
    INSERT(Settings, vram_usage_mode, tr("Режим использования VRAM:"),
           tr("Ограничивает использование видеопамяти для видеокарт с объемом памяти менее 4 ГБ."));
    INSERT(Settings, skip_cpu_inner_invalidation, tr("Пропуск внутренней инвалидации ЦП"),
           tr("Снижает нагрузку на центральный процессор за счет пропуска некоторых сбросов кэша памяти."));
    INSERT(Settings, vsync_mode, tr("Режим VSync:"),
           tr("Синхронизирует кадры с частотой обновления монитора, предотвращая разрывы изображения."));
    INSERT(Settings, bg_red, QString(), QString());
    INSERT(Settings, bg_green, QString(), QString());
    INSERT(Settings, bg_blue, QString(), QString());

    // Renderer (Advanced Graphics)
    INSERT(Settings, use_asynchronous_gpu_emulation, QString(), QString());

    INSERT(Settings, sync_memory_operations, tr("Синхронизация операций памяти"),
           tr("Синхронизирует потоки памяти ЦП и ГПУ, устраняя мерцание текстур в играх на движке Unreal Engine."));
    INSERT(Settings, async_presentation, tr("Асинхронный вывод кадров (Present)"),
           tr("Отделяет вывод готового кадра на экран от основного потока рендеринга, снижая задержки управления."));
    INSERT(
        Settings, renderer_force_max_clock, tr("Форсировать макс. частоту ГПУ (только Adreno)"),
        tr("Принудительно удерживает графический процессор Adreno на максимальных тактовых частотах."));
    INSERT(Settings, max_anisotropy, tr("Анизотропная фильтрация:"),
           tr("Улучшает четкость текстур, расположенных под острым углом к камере."));
    INSERT(Settings, gpu_accuracy, tr("Точность ГПУ:"),
           tr("Уровень точности вычислений графического процессора.\n"
              "Обычная (Normal) — обеспечивает высокую производительность.\n"
              "Высокая (High) — необходима для устранения визуальных артефактов в некоторых играх."));
    INSERT(Settings, dma_accuracy, tr("Точность DMA"),
           tr("Управляет точностью прямого доступа к памяти (DMA) при передаче текстур и геометрии."));
    INSERT(Settings, gpu_fence_behavior, tr("Синхронизация барьеров (GPU Fence)"),
           tr("Управляет синхронизацией очередей команд ГПУ.\n"
              "Немедленно — максимальная скорость работы.\n"
              "Сбалансированно — оптимальная совместимость.\n"
              "Высокая точность и Строго — устраняют специфические графические артефакты."));
    INSERT(Settings, enable_gpu_buffer_readback, tr("Обратное чтение буфера ГПУ"),
           tr("Сохраняет модифицированные графическим процессором данные путем их считывания перед отправкой. Требуется некоторым играм для корректного рендеринга эффектов."));
    INSERT(Settings, use_asynchronous_shaders, tr("Включить асинхронную компиляцию шейдеров"),
           tr("Компилирует новые шейдеры в фоновом режиме, снижая статтеры и фризы во время игрового процесса."));
    INSERT(Settings, gpu_clock, tr("Частота ГПУ"),
           tr("Регулирует частоту, которую видит гостевая игра, позволяя удерживать максимальное разрешение без срабатывания встроенных в игру ограничителей."));
    INSERT(Settings, gpu_unswizzle_enabled, tr("Unswizzle ГПУ"),
           tr("Ускоряет декодирование 3D-текстур BCn с использованием вычислительных мощностей ГПУ.\n"
              "Отключите при возникновении графических сбоев."));
    INSERT(Settings, gpu_unswizzle_texture_size, tr("Макс. размер текстуры Unswizzle ГПУ"),
           tr("Задает максимальный размер текстур (МБ), обрабатываемых на ГПУ.\n"
              "ГПУ быстрее справляется со средними и большими текстурами, в то время как мелкие эффективнее обрабатывать на ЦП."));
    INSERT(Settings, gpu_unswizzle_stream_size, tr("Размер потока Unswizzle ГПУ"),
           tr("Максимальный объем данных текстур (в МБ), обрабатываемый за один кадр. Помогает сбалансировать скорость загрузки сцены."));
    INSERT(Settings, gpu_unswizzle_chunk_size, tr("Размер чанка Unswizzle ГПУ"),
           tr("Количество срезов глубины, обрабатываемых за один проход. Увеличение повышает пропускную способность на мощных видеокартах."));

    INSERT(Settings, use_vulkan_driver_pipeline_cache, tr("Использовать кэш пайплайнов драйвера Vulkan"),
           tr("Задействует внутренний кэш драйвера видеокарты для ускорения повторного запуска игр."));
    INSERT(Settings, vulkan_pipeline_cache, tr("Кэш конвейеров Vulkan (Vulkan Pipeline Cache)"),
           tr("Предкомпиляция и сохранение бинарного кэша конвейеров Vulkan на накопителе для полного устранения внутриигровых статтеров и микрофризов при компиляции шейдеров."));
    INSERT(Settings, vram_garbage_collection, tr("Сборщик мусора видеопамяти (VRAM Garbage Collection)"),
           tr("Периодическая фоновая очистка неиспользуемых текстурных буферов и кэша ASTC для предотвращения утечек видеопамяти и лагов."));
    INSERT(Settings, enable_compute_pipelines, tr("Включить вычислительные пайплайны (только Intel Vulkan)"),
           tr("Специальная настройка совместимости для встроенной графики Intel."));
    INSERT(
        Settings, use_reactive_flushing, tr("Включить реактивный сброс памяти"),
        tr("Использует реактивную синхронизацию памяти вместо предиктивной для более точного соответствия оригиналу."));
    INSERT(Settings, use_video_framerate, tr("Синхронизация с частотой кадров видео"),
           tr("Воспроизводит внутриигровые ролики с оригинальной скоростью даже при разблокированном фреймрейте (FPS)."));
    INSERT(Settings, barrier_feedback_loops, tr("Барьеры обратной связи (Feedback Loops)"),
           tr("Улучшает отрисовку эффектов прозрачности, зеркал и отражений в ряде игр."));
    INSERT(Settings, enable_buffer_history, tr("Включить историю буферов"),
           tr("Сохраняет предыдущие состояния буферов, повышая стабильность отрисовки пост-эффектов."));
    INSERT(Settings, fix_bloom_effects, tr("Исправить эффекты bloom"),
           tr("Устраняет избыточное размытие, пересветы и графические искажения свечения."));

    INSERT(Settings, emulate_bgr565, tr("Эмуляция формата BGR565"),
           tr("Эмулирует цветовой формат BGR565 путем программной перестановки каналов синего и красного цветов.\n"
              "Помогает исправить некорректные цвета и искажения цветопередачи."));

    INSERT(Settings, rescale_hack, tr("Включить устаревший режим масштабирования"),
           tr("Использует алгоритм масштабирования предыдущих версий эмулятора.\n"
              "Устраняет полосы на видеокартах AMD/Intel и мерцание серых текстур в Luigi's Mansion 3."));

    // Renderer (Extensions)
    INSERT(Settings, dyna_state, tr("Расширенное динамическое состояние (EDS)"),
           tr("Управляет набором расширений Extended Dynamic State в Vulkan.\n"
              "Более высокие уровни расширяют возможности оптимизации и повышают FPS на современных драйверах."));

    INSERT(Settings, vertex_input_dynamic_state, tr("Динамический ввод вершин"),
           tr("Включает динамическое состояние ввода вершин для повышения производительности геометрического конвейера."));

    INSERT(
        Settings, sample_shading, tr("Выборка затенения"),
        tr("Выполняет фрагментный шейдер для каждого сэмпла при мультисэмплинге.\n"
           "Существенно повышает качество субпиксельной детализации и текстур за счет повышенной нагрузки на ГПУ."));

    // System
    INSERT(Settings, rng_seed, tr("Сид генератора случайных чисел (RNG)"),
           tr("Фиксирует начальное значение генератора случайных чисел (RNG). Используется для спидранов."));
    INSERT(Settings, rng_seed_enabled, QString(), QString());
    INSERT(Settings, device_name, tr("Имя консоли"), tr("Отображаемое имя виртуальной консоли."));
    INSERT(Settings, eco_thermal_mode, tr("Режим «Eco / Thermal Governor»"),
           tr("Контроль пиковых частот, адаптивное управление ресурсами и предотвращение перегрева и термального троттлинга устройства."));
    INSERT(Settings, program_args, tr("Аргументы Homebrew"),
           tr("Аргументы командной строки, передаваемые homebrew-приложениям при запуске."));
    INSERT(Settings, custom_rtc, tr("Пользовательское время RTC:"),
           tr("Позволяет изменить системное время виртуальной консоли для манипуляции игровыми событиями."));
    INSERT(Settings, custom_rtc_enabled, QString(), QString());
    INSERT(Settings, custom_rtc_offset, QStringLiteral(" "),
           tr("Смещение в секундах от текущего системного времени"));
    INSERT(Settings, language_index, tr("Язык системы:"),
           tr("Язык интерфейса консоли и игр по умолчанию."));
    INSERT(Settings, region_index, tr("Регион:"), tr("Регион виртуальной консоли."));
    INSERT(Settings, time_zone_index, tr("Часовой пояс:"), tr("Часовой пояс виртуальной консоли."));
    INSERT(Settings, sound_index, tr("Режим вывода звука:"), QString());
    INSERT(Settings, use_docked_mode, tr("Режим консоли:"),
           tr("Переключает консоль между режимами \"В док-станции\" и \"Портативный\".\n"
              "В режиме док-станции игры работают в повышенном разрешении и графическом профиле."));
    INSERT(Settings, current_user, QString(), QString());

    // Ui General
    INSERT(UISettings, select_user_on_boot, tr("Запрашивать профиль при запуске игры"),
           tr("Отображает окно выбора пользователя при старте каждой игры."));
    INSERT(UISettings, pause_when_in_background, tr("Приостанавливать при потере фокуса"),
           tr("Ставит игру на паузу при переключении на другое окно."));
    INSERT(UISettings, confirm_before_stopping, tr("Подтверждать остановку эмуляции"),
           tr("Запрашивает подтверждение перед закрытием игры или эмулятора."));
    INSERT(UISettings, hide_mouse, tr("Скрывать курсор мыши при бездействии"),
           tr("Скрывает курсор мыши после 2.5 секунд неактивности."));
    INSERT(UISettings, controller_applet_disabled, tr("Отключить апплет контроллеров"),
           tr("Принудительно отключает вызов всплывающего системного апплета настройки контроллеров."));
    INSERT(UISettings, check_for_updates, tr("Проверять наличие обновлений"),
           tr("Проверять наличие свежих версий программы при запуске."));
    INSERT(UISettings, enable_floating_translate_button, tr("Плавающая кнопка переводчика (STORM TRANSLATOR)"),
           tr("Отображать плавающую кнопку авто-переводчика поверх экрана во время игры."));

    // Linux
    INSERT(UISettings, enable_gamemode, tr("Включить Gamemode"), QString());
#ifdef __unix__
    INSERT(UISettings, gui_force_x11, tr("Принудительно использовать X11"), QString());
    INSERT(UISettings, gui_hide_backend_warning, QString(), QString());
#endif

#undef INSERT

    return translations;
}

std::unique_ptr<ComboboxTranslationMap> ComboboxEnumeration(QObject* parent) {
    std::unique_ptr<ComboboxTranslationMap> translations =
        std::make_unique<ComboboxTranslationMap>();
    const auto& tr = [](const char* text, const char* disambiguation = nullptr) -> QString {
        return QCoreApplication::translate("ConfigurationShared", text, disambiguation);
    };

#define PAIR(ENUM, VALUE, TRANSLATION) {static_cast<u32>(Settings::ENUM::VALUE), (TRANSLATION)}

    // Intentionally skipping VSyncMode to let the UI fill that one out
    translations->insert({Settings::EnumMetadata<Settings::AppletMode>::Index(),
                          {
                              PAIR(AppletMode, HLE, tr("Пользовательский интерфейс")),
                              PAIR(AppletMode, LLE, tr("Системный апплет")),
                          }});

    translations->insert({Settings::EnumMetadata<Settings::SpirvOptimizeMode>::Index(),
                          {
                              PAIR(SpirvOptimizeMode, Never, tr("Никогда")),
                              PAIR(SpirvOptimizeMode, OnLoad, tr("При загрузке")),
                              PAIR(SpirvOptimizeMode, Always, tr("Всегда")),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::AstcDecodeMode>::Index(),
                          {
                              PAIR(AstcDecodeMode, Cpu, tr("ЦП")),
                              PAIR(AstcDecodeMode, Gpu, tr("ГПУ")),
                              PAIR(AstcDecodeMode, CpuAsynchronous, tr("ЦП Асинхронно")),
                          }});
    translations->insert(
        {Settings::EnumMetadata<Settings::AstcRecompression>::Index(),
         {
             PAIR(AstcRecompression, Uncompressed, tr("Без сжатия (Лучшее качество)")),
             PAIR(AstcRecompression, Bc1, tr("BC1 (Низкое качество)")),
             PAIR(AstcRecompression, Bc3, tr("BC3 (Среднее качество)")),
             PAIR(AstcRecompression, Bc5, tr("BC5 (Высокое качество)")),
         }});
    translations->insert({Settings::EnumMetadata<Settings::FramePacingMode>::Index(),
                          {
                              PAIR(FramePacingMode, Target_Auto, tr("Авто")),
                              PAIR(FramePacingMode, Target_30, tr("30 FPS")),
                              PAIR(FramePacingMode, Target_60, tr("60 FPS")),
                              PAIR(FramePacingMode, Target_90, tr("90 FPS")),
                              PAIR(FramePacingMode, Target_120, tr("120 FPS")),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::VramUsageMode>::Index(),
                          {
                              PAIR(VramUsageMode, Conservative, tr("Экономный")),
                              PAIR(VramUsageMode, Normal, tr("Нормальный")),
                              PAIR(VramUsageMode, Aggressive, tr("Агрессивный")),
                          }});
    translations->insert(
        {Settings::EnumMetadata<Settings::RendererBackend>::Index(),
         {PAIR(RendererBackend, Vulkan, tr("Vulkan")),
#ifdef HAS_OPENGL
          PAIR(RendererBackend, OpenGL_GLSL, tr("OpenGL GLSL")),
          PAIR(RendererBackend, OpenGL_GLASM, tr("OpenGL GLASM")),
          PAIR(RendererBackend, OpenGL_SPIRV, tr("OpenGL SPIR-V")),
#endif
          PAIR(RendererBackend, Null, tr("Отключено"))}});
    translations->insert({Settings::EnumMetadata<Settings::GpuAccuracy>::Index(),
                          {
                              PAIR(GpuAccuracy, Low, tr("Быстрый")),
                              PAIR(GpuAccuracy, High, tr("Высокая точность")),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::DmaAccuracy>::Index(),
                          {
                              PAIR(DmaAccuracy, Default, tr("По умолчанию")),
                              PAIR(DmaAccuracy, Normal, tr("Нормально")),
                              PAIR(DmaAccuracy, Unsafe, tr("Небезопасно")),
                              PAIR(DmaAccuracy, Safe, tr("Безопасно")),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::GpuFenceBehavior>::Index(),
                          {
                              PAIR(GpuFenceBehavior, Default, tr("По умолчанию")),
                              PAIR(GpuFenceBehavior, Immediate, tr("Немедленно")),
                              PAIR(GpuFenceBehavior, Balanced, tr("Сбалансированно")),
                              PAIR(GpuFenceBehavior, Accurate, tr("Точно")),
                              PAIR(GpuFenceBehavior, Strict, tr("Строго")),
                          }});
    translations->insert(
        {Settings::EnumMetadata<Settings::CpuAccuracy>::Index(),
         {
             PAIR(CpuAccuracy, Auto, tr("Авто")),
             PAIR(CpuAccuracy, Accurate, tr("Точный")),
             PAIR(CpuAccuracy, Unsafe, tr("Небезопасный")),
             PAIR(CpuAccuracy, Paranoid, tr("Параноидальный")),
             PAIR(CpuAccuracy, Debugging, tr("Отладка")),
         }});
    translations->insert({Settings::EnumMetadata<Settings::CpuBackend>::Index(),
                          {
                              PAIR(CpuBackend, Dynarmic, tr("Dynarmic")),
                              PAIR(CpuBackend, Nce, tr("NCE")),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::FullscreenMode>::Index(),
                          {
                              PAIR(FullscreenMode, Borderless, tr("Окно без рамки")),
                              PAIR(FullscreenMode, Exclusive, tr("Эксклюзивный полный экран")),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::NvdecEmulation>::Index(),
                          {
                              PAIR(NvdecEmulation, Off, tr("Без видео")),
                              PAIR(NvdecEmulation, Cpu, tr("Декодирование видео на ЦП")),
                              PAIR(NvdecEmulation, Gpu, tr("Декодирование видео на ГПУ")),
                          }});
    translations->insert(
        {Settings::EnumMetadata<Settings::ResolutionSetup>::Index(),
         {
             PAIR(ResolutionSetup, Res1_4X, tr("0.25X (180p/270p)")),
             PAIR(ResolutionSetup, Res1_2X, tr("0.5X (360p/540p)")),
             PAIR(ResolutionSetup, Res3_4X, tr("0.75X (540p/810p)")),
             PAIR(ResolutionSetup, Res1X, tr("1X (720p/1080p)")),
             PAIR(ResolutionSetup, Res5_4X, tr("1.25X (900p/1350p)")),
             PAIR(ResolutionSetup, Res3_2X, tr("1.5X (1080p/1620p)")),
             PAIR(ResolutionSetup, Res2X, tr("2X (1440p/2160p)")),
             PAIR(ResolutionSetup, Res3X, tr("3X (2160p/3240p)")),
             PAIR(ResolutionSetup, Res4X, tr("4X (2880p/4320p)")),
             PAIR(ResolutionSetup, Res5X, tr("5X (3600p/5400p)")),
             PAIR(ResolutionSetup, Res6X, tr("6X (4320p/6480p)")),
             PAIR(ResolutionSetup, Res7X, tr("7X (5040p/7560p)")),
             PAIR(ResolutionSetup, Res8X, tr("8X (5760p/8640p)")),
         }});
    translations->insert({Settings::EnumMetadata<Settings::ScalingFilter>::Index(),
                          {
                              PAIR(ScalingFilter, NearestNeighbor, tr("Ближайший сосед")),
                              PAIR(ScalingFilter, Bilinear, tr("Билинейный")),
                              PAIR(ScalingFilter, Bicubic, tr("Бикубический")),
                              PAIR(ScalingFilter, Gaussian, tr("Гаусс")),
                              PAIR(ScalingFilter, Lanczos, tr("Ланцош")),
                              PAIR(ScalingFilter, ScaleForce, tr("ScaleForce")),
                              PAIR(ScalingFilter, Fsr, tr("AMD FidelityFX Super Resolution")),
                              PAIR(ScalingFilter, Area, tr("Area")),
                              PAIR(ScalingFilter, Mmpx, tr("MMPX")),
                              PAIR(ScalingFilter, ZeroTangent, tr("Zero-Tangent")),
                              PAIR(ScalingFilter, BSpline, tr("B-Spline")),
                              PAIR(ScalingFilter, Mitchell, tr("Mitchell")),
                              PAIR(ScalingFilter, Spline1, tr("Spline-1")),
                              PAIR(ScalingFilter, Sgsr, tr("Snapdragon Game Super Resolution")),
                              PAIR(ScalingFilter, SgsrEdge, tr("Snapdragon Game Super Resolution EdgeDir")),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::AntiAliasing>::Index(),
                          {
                              PAIR(AntiAliasing, None, tr("Отключено")),
                              PAIR(AntiAliasing, Fxaa, tr("FXAA")),
                              PAIR(AntiAliasing, Smaa, tr("SMAA")),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::AspectRatio>::Index(),
                          {
                              PAIR(AspectRatio, R16_9, tr("16:9")),
                              PAIR(AspectRatio, R4_3, tr("4:3")),
                              PAIR(AspectRatio, R21_9, tr("21:9")),
                              PAIR(AspectRatio, R16_10, tr("16:10")),
                              PAIR(AspectRatio, Stretch, tr("Растянуть на весь экран")),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::AnisotropyMode>::Index(),
                          {
                              PAIR(AnisotropyMode, Automatic, tr("Автоматически")),
                              PAIR(AnisotropyMode, Default, tr("По умолчанию")),
                              PAIR(AnisotropyMode, X2, tr("2x")),
                              PAIR(AnisotropyMode, X4, tr("4x")),
                              PAIR(AnisotropyMode, X8, tr("8x")),
                              PAIR(AnisotropyMode, X16, tr("16x")),
                              PAIR(AnisotropyMode, X32, tr("32x")),
                              PAIR(AnisotropyMode, X64, tr("64x")),
                              PAIR(AnisotropyMode, None, tr("Отключено")),
                          }});
    translations->insert(
        {Settings::EnumMetadata<Settings::Language>::Index(),
         {
             PAIR(Language, Japanese, tr("Японский (日本語)")),
             PAIR(Language, EnglishAmerican, tr("Американский английский")),
             PAIR(Language, French, tr("Французский (français)")),
             PAIR(Language, German, tr("Немецкий (Deutsch)")),
             PAIR(Language, Italian, tr("Итальянский (italiano)")),
             PAIR(Language, Spanish, tr("Испанский (español)")),
             PAIR(Language, Chinese, tr("Китайский")),
             PAIR(Language, Korean, tr("Корейский (한국어)")),
             PAIR(Language, Dutch, tr("Нидерландский (Nederlands)")),
             PAIR(Language, Portuguese, tr("Португальский (português)")),
             PAIR(Language, Russian, tr("Русский")),
             PAIR(Language, Taiwanese, tr("Тайваньский")),
             PAIR(Language, EnglishBritish, tr("Британский английский")),
             PAIR(Language, FrenchCanadian, tr("Канадский французский")),
             PAIR(Language, SpanishLatin, tr("Латиноамериканский испанский")),
             PAIR(Language, ChineseSimplified, tr("Упрощенный китайский")),
             PAIR(Language, ChineseTraditional, tr("Традиционный китайский (正體中文)")),
             PAIR(Language, PortugueseBrazilian, tr("Бразильский португальский")),
             PAIR(Language, Polish, tr("Польский (polski)")),
             PAIR(Language, Thai, tr("Тайский (แบบไทย)")),
         }});
    translations->insert({Settings::EnumMetadata<Settings::Region>::Index(),
                          {
                              PAIR(Region, Japan, tr("Япония")),
                              PAIR(Region, Usa, tr("США")),
                              PAIR(Region, Europe, tr("Европа")),
                              PAIR(Region, Australia, tr("Австралия")),
                              PAIR(Region, China, tr("Китай")),
                              PAIR(Region, Korea, tr("Корея")),
                              PAIR(Region, Taiwan, tr("Тайвань")),
                          }});
    translations->insert(
        {Settings::EnumMetadata<Settings::TimeZone>::Index(),
         {
             {static_cast<u32>(Settings::TimeZone::Auto),
              tr("Авто (%1)", "Auto select time zone")
                  .arg(QString::fromStdString(
                      Settings::GetTimeZoneString(Settings::TimeZone::Auto)))},
             {static_cast<u32>(Settings::TimeZone::Default),
              tr("По умолчанию (%1)", "Default time zone")
                  .arg(QString::fromStdString(Common::TimeZone::GetDefaultTimeZone()))},
             PAIR(TimeZone, Cet, tr("CET")),
             PAIR(TimeZone, Cst6Cdt, tr("CST6CDT")),
             PAIR(TimeZone, Cuba, tr("Куба")),
             PAIR(TimeZone, Eet, tr("EET")),
             PAIR(TimeZone, Egypt, tr("Египет")),
             PAIR(TimeZone, Eire, tr("Ирландия")),
             PAIR(TimeZone, Est, tr("EST")),
             PAIR(TimeZone, Est5Edt, tr("EST5EDT")),
             PAIR(TimeZone, Gb, tr("Великобритания")),
             PAIR(TimeZone, GbEire, tr("Великобритания-Ирландия")),
             PAIR(TimeZone, Gmt, tr("GMT")),
             PAIR(TimeZone, GmtPlusZero, tr("GMT+0")),
             PAIR(TimeZone, GmtMinusZero, tr("GMT-0")),
             PAIR(TimeZone, GmtZero, tr("GMT0")),
             PAIR(TimeZone, Greenwich, tr("Гринвич")),
             PAIR(TimeZone, Hongkong, tr("Гонконг")),
             PAIR(TimeZone, Hst, tr("HST")),
             PAIR(TimeZone, Iceland, tr("Исландия")),
             PAIR(TimeZone, Iran, tr("Иран")),
             PAIR(TimeZone, Israel, tr("Израиль")),
             PAIR(TimeZone, Jamaica, tr("Ямайка")),
             PAIR(TimeZone, Japan, tr("Япония")),
             PAIR(TimeZone, Kwajalein, tr("Кваджалейн")),
             PAIR(TimeZone, Libya, tr("Ливия")),
             PAIR(TimeZone, Met, tr("MET")),
             PAIR(TimeZone, Mst, tr("MST")),
             PAIR(TimeZone, Mst7Mdt, tr("MST7MDT")),
             PAIR(TimeZone, Navajo, tr("Навахо")),
             PAIR(TimeZone, Nz, tr("Новая Зеландия")),
             PAIR(TimeZone, NzChat, tr("Новая Зеландия (Чатэм)")),
             PAIR(TimeZone, Poland, tr("Польша")),
             PAIR(TimeZone, Portugal, tr("Португалия")),
             PAIR(TimeZone, Prc, tr("КНР")),
             PAIR(TimeZone, Pst8Pdt, tr("PST8PDT")),
             PAIR(TimeZone, Roc, tr("Тайвань (ROC)")),
             PAIR(TimeZone, Rok, tr("Южная Корея (ROK)")),
             PAIR(TimeZone, Singapore, tr("Сингапур")),
             PAIR(TimeZone, Turkey, tr("Турция")),
             PAIR(TimeZone, Uct, tr("UCT")),
             PAIR(TimeZone, Universal, tr("Universal")),
             PAIR(TimeZone, Utc, tr("UTC")),
             PAIR(TimeZone, WSu, tr("W-SU")),
             PAIR(TimeZone, Wet, tr("WET")),
             PAIR(TimeZone, Zulu, tr("Zulu")),
         }});
    translations->insert({Settings::EnumMetadata<Settings::AudioMode>::Index(),
                          {
                              PAIR(AudioMode, Mono, tr("Моно")),
                              PAIR(AudioMode, Stereo, tr("Стерео")),
                              PAIR(AudioMode, Surround, tr("Объемный звук")),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::MemoryLayout>::Index(),
                          {
                              PAIR(MemoryLayout, Memory_4Gb, tr("4 ГБ DRAM")),
                              PAIR(MemoryLayout, Memory_6Gb, tr("6 ГБ DRAM")),
                              PAIR(MemoryLayout, Memory_8Gb, tr("8 ГБ DRAM")),
                              PAIR(MemoryLayout, Memory_10Gb, tr("10 ГБ DRAM")),
                              PAIR(MemoryLayout, Memory_12Gb, tr("12 ГБ DRAM")),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::ConsoleMode>::Index(),
                          {
                              PAIR(ConsoleMode, Docked, tr("В док-станции")),
                              PAIR(ConsoleMode, Handheld, tr("Портативный")),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::CpuClock>::Index(),
                          {
                              PAIR(CpuClock, Normal, tr("Стандартный")),
                              PAIR(CpuClock, Boost, tr("Ускоренный")),
                              PAIR(CpuClock, Overclock, tr("Разгон")),
                          }});
    translations->insert(
        {Settings::EnumMetadata<Settings::ConfirmStop>::Index(),
         {
             PAIR(ConfirmStop, Ask_Always, tr("Всегда спрашивать")),
             PAIR(ConfirmStop, Ask_Based_On_Game, tr("Только если игра требует подтверждения")),
             PAIR(ConfirmStop, Ask_Never, tr("Никогда не спрашивать")),
         }});
    translations->insert({Settings::EnumMetadata<Settings::GpuClock>::Index(),
                          {
                              PAIR(GpuClock, Normal, tr("Стандартный")),
                              PAIR(GpuClock, Boost, tr("Ускоренный")),
                              PAIR(GpuClock, Overclock, tr("Разгон")),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::GpuUnswizzleSize>::Index(),
                          {
                              PAIR(GpuUnswizzleSize, VerySmall, tr("Очень маленький (16 МБ)")),
                              PAIR(GpuUnswizzleSize, Small, tr("Маленький (32 МБ)")),
                              PAIR(GpuUnswizzleSize, Normal, tr("Стандартный (128 МБ)")),
                              PAIR(GpuUnswizzleSize, Large, tr("Большой (256 МБ)")),
                              PAIR(GpuUnswizzleSize, VeryLarge, tr("Очень большой (512 МБ)")),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::GpuUnswizzle>::Index(),
                          {
                              PAIR(GpuUnswizzle, VeryLow, tr("Очень низкий (4 МБ)")),
                              PAIR(GpuUnswizzle, Low, tr("Низкий (8 МБ)")),
                              PAIR(GpuUnswizzle, Normal, tr("Стандартный (16 МБ)")),
                              PAIR(GpuUnswizzle, Medium, tr("Средний (32 МБ)")),
                              PAIR(GpuUnswizzle, High, tr("Высокий (64 МБ)")),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::GpuUnswizzleChunk>::Index(),
                          {
                              PAIR(GpuUnswizzleChunk, VeryLow, tr("Очень низкий (32)")),
                              PAIR(GpuUnswizzleChunk, Low, tr("Низкий (64)")),
                              PAIR(GpuUnswizzleChunk, Normal, tr("Стандартный (128)")),
                              PAIR(GpuUnswizzleChunk, Medium, tr("Средний (256)")),
                              PAIR(GpuUnswizzleChunk, High, tr("Высокий (512)")),
                          }});

    translations->insert({Settings::EnumMetadata<Settings::ExtendedDynamicState>::Index(),
                          {
                              PAIR(ExtendedDynamicState, Disabled, tr("Отключено")),
                              PAIR(ExtendedDynamicState, EDS1, tr("EDS 1")),
                              PAIR(ExtendedDynamicState, EDS2, tr("EDS 2")),
                              PAIR(ExtendedDynamicState, EDS3, tr("EDS 3")),
                          }});

    translations->insert({Settings::EnumMetadata<Settings::GameListMode>::Index(),
                          {
                              PAIR(GameListMode, TreeView, tr("Дерево")),
                              PAIR(GameListMode, GridView, tr("Сетка")),
                          }});

#undef PAIR
#undef CTX_PAIR

    return translations;
}
} // namespace ConfigurationShared
