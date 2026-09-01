<h1 align="center">STORM SWITCH</h1>

## **О проекте**
STORM SWITCH — форк эмулятора гибридной игровой консоли Nintendo Switch для платформ Android и Windows PC. Проект ориентирован на максимальную производительность, оптимизацию JIT/NCE компилятора, стабильность видеодрайверов Vulkan, поддержку сжатых форматов (NSZ/NCZ/XCZ на лету), расширенную базу авто-исправлений GameFix Database и готический пользовательский интерфейс с глубокой кастомизацией оверлеев и телеметрии.

## **Происхождение и форки**
STORM SWITCH является высокопроизводительным форком эмулятора Nintendo Switch **Eden**, объединяющим ключевые наработки проектов **Citron** и **Yuzu** с кастомными Vulkan-пайплайнами, исправлениями глубины и освещения (BotW/TotK), потоковой декомпрессией NSZ/XCZ через Zstandard, расширенной базой авто-профилей GameFix Database и оверлеями нагрузки в реальном времени.

## **Технологический стек**
- **Языки программирования**: C++20, C, Kotlin, Java, CMake
- **Графический стек**: Vulkan 1.3, Turnip/Adreno Custom Extensions, Spirv-Cross, Shader Recompiler
- **Компиляция кода**: Dynarmic (ARM64 JIT Recompiler), Native Code Execution (NCE) для Android ARM64
- **Аудио**: Cubeb, Oboe Audio, OpenSL ES
- **Декомпрессия и форматы**: Zstandard (NSZ/NCZ/XCZ streaming decompressor), hactool, liblz4
- **Пользовательский интерфейс**: Qt6 (Windows), Jetpack Compose / Android Native UI (Android)

## **Ключевые возможности**
- **Прямая поддержка сжатых форматов (NSZ/NCZ/XCZ)**: Потоковая распаковка Zstandard на лету без необходимости предварительной распаковки гигабайтных архивов на накопитель.
- **Глубокая база авто-профилей (GameFix Database)**: Автоматическая настройка параметров памяти (8GB DRAM Layout), точности CPU (Unsafe JIT/Fastmem), ASTC-текстур и графических твиков для сотен коммерческих игр и homebrew-портов (GTA V, Half-Life, Doom 3, S.T.A.L.K.E.R. и др.).
- **Vulkan Stability & Dynamic State**: Устранение сбоев `VK_ERROR_DEVICE_LOST`, поддержка расширений `VK_EXT_extended_dynamic_state3` и аппаратного декомпрессора ASTC.
- **Встроенный менеджер дополнений и модов**: Управление DLC, обновлениями, интеграция с GameBanana Mods и встроенный браузер Amiibo со случайной генерацией UID.
- **Готический интерфейс и неоновая телеметрия**: Оверлей нагрузки с отображением реального разрешения рендера, FPS, frametime и температуры GPU/CPU.

## **Поддерживаемые платформы и эмуляторы**
- **Операционные системы**: Android 10+ (ARM64-v8a, Snapdragon / Dimensity / Exynos), Windows 10, Windows 11 (x64, DirectX 12 / Vulkan)
- **Поддерживаемая консоль**: Nintendo Switch (Horizon OS)
- **Форматы файлов**: NSP, NSZ, XCI, XCZ, NRO, NSO
- **Поддерживаемые эмуляторы и форки**: STORM SWITCH, Eden, Citron, Yuzu, Suyu, Sudachi, Torzu, Ryujinx

## **Установка и запуск**
1. Перейдите в раздел **Releases** репозитория на GitHub.
2. Для Android: скачайте и установите APK-файл `STORM_SWITCH_6.2.2.apk` или `STORM_SWITCH_6.2.2_LEGACY.apk`.
3. Для Windows: скачайте архив `STORM_SWITCH_6.2.2_Windows.zip`.
4. Установите системные ключи (`prod.keys`) и актуальную прошивку Nintendo Switch (Firmware) через меню настроек эмулятора.

## **Благодарности**
- **Команда Yuzu (Bunnei, Lioncache, Subv, BreadFish64, Blinkhawk, Morph, byte[])** — за фундаментальный проект и создание архитектуры эмуляции Nintendo Switch.
- **Команда Eden Emulator** — за развитие открытой кодовой базы и продвинутые оптимизации компилятора.
- **Команда Citron Emulator (BignBooty, EmuGamer)** — за инновации в Android-версии и управление шейдерами.
- **Команда Ryujinx (gdkchan, Ac_K, Mary, riperiperi)** — за фундаментальные исследования операционной системы Horizon OS и сервисов Switch.
- **merryhime и команда Dynarmic** — за непревзойденный ARM64 JIT-компилятор.
- **Команда Skyline и Strato (Mark, Billy, Lynx, bylaws)** — за новаторские исследования NCE (Native Code Execution) под Android ARM64.
- **Rob Clark, Danylo Piliaiev, Connor Abbott и разработчики Mesa / Turnip / Freedreno** — за мобильные Vulkan-драйверы с открытым исходным кодом под графические процессоры Adreno.
- **SciresM, TuxSH, fincs, mtheall и сообщество Atmosphere** — за разработку открытой операционной системы и homebrew-инструментов.
- **Yann Collet и команда Zstandard (Meta)** — за эталонный алгоритм потокового сжатия данных Zstd.
- **The Qt Company** — за графический фреймворк Qt6.
- **Команда Khronos Group** — за спецификации API Vulkan и инструмент трансляции шейдеров SPIRV-Cross.
- **Команда FFmpeg** — за высокопроизводительные мультимедийные декодеры аудио и видео.
