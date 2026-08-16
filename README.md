<!--
# SPDX-FileCopyrightText: Copyright 2026 STORM EDEN Project
# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later
-->

<h1 align="center">
  <br>
  <img src="./dist/qt_themes/default/icons/256x256/eden.png" alt="STORM EDEN" width="180">
  <br>
  <b>STORM EDEN</b>
  <br>
  <sub>Версия 3.2.2 | Высокопроизводительный эмулятор Nintendo Switch</sub>
</h1>

<p align="center">
  <a href="https://github.com/ReiKatari/STORM_EDEN/releases">
    <img src="https://img.shields.io/github/v/release/ReiKatari/STORM_EDEN?color=00e5ff&label=STORM%20EDEN%20Release" alt="Latest Release">
  </a>
  <a href="https://github.com/ReiKatari/STORM_EDEN/releases">
    <img src="https://img.shields.io/github/downloads/ReiKatari/STORM_EDEN/total?color=00e676&label=Downloads" alt="Downloads">
  </a>
  <a href="https://git.eden-emu.dev/eden-emu/eden">
    <img src="https://img.shields.io/badge/Upstream-Eden%20Emulator-5865F2" alt="Eden Upstream">
  </a>
  <a href="https://github.com/ReiKatari/STORM_EDEN/blob/main/LICENSE.txt">
    <img src="https://img.shields.io/badge/License-GPL%20v3.0-orange" alt="License">
  </a>
</p>

---

## 🌟 О проекте STORM EDEN

**STORM EDEN** — это специализированная модификация эмулятора Nintendo Switch (форк проекта **Eden**, основанного на **Yuzu**, **Sudachi** и **Citron**), созданная разработчиком **ReiKatari**. Проект ориентирован на максимальное удобство пользователей, расширенную совместимость с игровыми форматами, высокую производительность и глубокую интеграцию метаданных.

---

## 🚀 Ключевые особенности и улучшения STORM EDEN

### 📦 Полная нативная поддержка NSZ и сжатых контейнеров
* Прямой запуск игр в формате **NSZ**, **XCZ** и сжатых разделов **NCZ** без необходимости предварительной распаковки.
* Поддержка декодирования Zstandard (zstd) в реальном времени с минимальным потреблением оперативной памяти и ресурсов процессора.
* Корректная обработка встроенных обновлений и DLC из единых файлов-сборок (1G+1U+xD).

### 🗃️ Интеграция с базой данных TitleDB (Tinfoil)
* Встроенная поддержка глобальной базы данных `titledb.json` (свыше 150 МБ метаданных).
* Автоматическое извлечение и валидация ключей шифрования (TitleKeys) для всех официальных игр и обновлений.
* Распознавание уникальных названий и описаний для каждого загружаемого дополнения (DLC).

### 🎮 Продвинутый Менеджер дополнений (Addons Manager)
* Полный интерактивный список всех компонентов игры: базовая игра, накопительные патчи, DLC и пользовательские модификации (LayeredFS).
* Раздельные уникальные описания для дополнений и обновлений (без дублирования текста сюжета базовой игры).
* Удобный поиск по названиям, Title ID и статусам, а также функция быстрого экспорта списка в буфер обмена.

### 🎨 Эксклюзивные темы оформления
* Темы с индивидуальной палитрой: **Cyberpunk**, **Midnight**, **Gothic**, **Night**, **Day**.
* Адаптивный темный интерфейс с акцентами Cyan / Emerald / Gold.
* Настраиваемый и информативный подвал (Status Bar) с мгновенным переключением режимов графики, масштабирования и управления.

### ⚡ Производительность и стабильность
* Оптимизированные графические бэкенды **Vulkan** и **OpenGL**.
* Исправленные алгоритмы деинициализации и остановки гостевых ядер CPU без зависаний процесса.
* Поддержка алгоритмов масштабирования FSR, Bilinear, Bicubic, Gaussian, а также сглаживания FXAA и SMAA.
* Встроенный **Discord Rich Presence** для отображения игрового статуса.
* Автоматическая и ручная **проверка обновлений** напрямую с [GitHub Releases](https://github.com/ReiKatari/STORM_EDEN/releases).

---

## 🛠️ Сборка из исходного кода

### Системные требования для сборки (Windows)
* **Visual Studio 2022** (v143 toolset) с установленными компонентами для разработки на C++ Desktop.
* **CMake** версии 3.25 или новее.
* **Ninja** build system.
* **Git**.

### Инструкция по сборке
```bat
git clone https://github.com/ReiKatari/STORM_EDEN.git
cd STORM_EDEN
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

---

## 🤝 Благодарности и используемые компоненты

Проект **STORM EDEN** выражает искреннюю благодарность сообществу разработчиков эмуляторов и авторам открытых библиотек:

* **Eden Emulator Project & Camille LaVey** — за фундамент форка Eden и постоянное развитие эмуляции Switch.
* **Yuzu Emulator Team** — за создание легендарной архитектуры эмулятора Nintendo Switch.
* **Ryujinx Team** — за неоценимый вклад в реверс-инжиниринг форматов файловых систем и сервисов Horizon OS.
* **Sudachi & Citron Projects** — за полезные наработки и оптимизации для мобильных и настольных платформ.
* **Tinfoil & Blawar** — за спецификацию формата NSZ/NCZ и ведение глобальной базы данных TitleDB.
* **Zstandard (zstd)** — высокоскоростная библиотека сжатия данных от Yann Collet (Meta).
* **FFmpeg Team** — мультимедийный движок для аппаратного и программного декодирования видео/аудио.
* **Dynarmic (MerryMage & contributors)** — динамический рекомпилятор ARMv8.
* **Mozilla Cubeb** — кроссплатформенный низколатентный аудио-бэкенд.
* **Qt Project & The Qt Company** — интерфейсный фреймворк Qt6.
* **Vulkan SDK & LunarG** — низкоуровневый графический API.

---

## 📜 Лицензия

**STORM EDEN** распространяется под условиями свободной лицензии **GNU General Public License v3.0 (GPLv3)** или более поздней версии. Полный текст лицензии доступен в файле [LICENSE.txt](LICENSE.txt).
