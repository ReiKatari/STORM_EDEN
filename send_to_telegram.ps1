$token = "8210884351:AAEh4VOWHViz2KF_oElAqEfrMPHlI5TWCjM"
$chatId = "-5389146045"

function Send-TGMessage($text) {
    $url = "https://api.telegram.org/bot$token/sendMessage"
    $body = @{
        chat_id = $chatId
        text = $text
        parse_mode = "HTML"
        disable_web_page_preview = $true
    } | ConvertTo-Json -Compress
    
    $headers = @{ "Content-Type" = "application/json; charset=utf-8" }
    $res = Invoke-RestMethod -Uri $url -Method Post -Body ([System.Text.Encoding]::UTF8.GetBytes($body)) -Headers $headers
    Write-Host "Message Sent: $($res.ok)"
}

function Send-TGDocument($filePath, $caption) {
    Write-Host "Sending $filePath via curl..."
    $url = "https://api.telegram.org/bot$token/sendDocument"
    $fileName = [System.IO.Path]::GetFileName($filePath)
    
    & curl.exe -s -X POST $url `
        -F "chat_id=$chatId" `
        -F "parse_mode=HTML" `
        -F "caption=$caption" `
        -F "document=@$filePath"
        
    Write-Host "`nUploaded $fileName successfully!"
}

$announcement = @"
⚡ <b>Обновление STORM SWITCH 7.3.0 (Smart Mod Loader, Hardware Calibrator, Driver Isolation, LSFG Pacing, SOR4 Locked Fix, Zelda Graphics and Full Localization)</b> — <i>Масштабное обновление эмулятора Nintendo Switch: умная ручная и онлайн-установка модов GameBanana с автораспаковкой romfs/exefs/cheats, аппаратная калибровка под чипсеты (Snapdragon 8 Elite, Dimensity, Exynos, Tensor) с адаптивными профилями, полное устранение конфликтов драйверов между Zelda BotW/TotK, Diablo II и SOR4, оптимизация генератора кадров LSFG без задержек, исправление шрифтов и субтитров Alan Wake, и 100% выверенная локализация</i>

━━━━━━━━━━━━━━━━━━━━━━━

🚀 <b>Ключевые изменения и улучшения:</b>

📦 <b>Умный установщик модов GameBanana и ручная загрузка (ZIP и папки):</b>
• В окно GameBanana добавлена кнопка «Установить мод вручную» с поддержкой ZIP-архивов и директорий напрямую в папку <code>load/&lt;TitleID&gt;/</code>.
• Реализован многоуровневый интеллектуальный анализатор архивов: автоматическое снятие лишних вложенных папок, корректное развертывание <code>atmosphere/contents/&lt;TitleID&gt;</code>, маршрутизация <code>romfs</code>, <code>exefs</code>, <code>cheats</code>, упаковка свободных файлов в <code>romfs/</code> и маршрутизация читов и патчей (<code>.ips</code>, <code>.pchtxt</code>).
• Установленные моды мгновенно появляются и активируются в системном меню «Дополнения».

🎮 <b>Полная изоляция драйверов (Zelda BotW/TotK, Diablo II и SOR4):</b>
• Устранено перезаписывание конфигураций драйверов при распаковке: генерация и применение изолированных профилей <code>00-storm.conf</code> и <code>drirc</code> происходит строго после инициализации драйвера и непосредственно перед стартом рендеринга Vulkan.
• В играх The Legend of Zelda: BotW и TotK гарантированно отключен <code>tu_tile_discard</code> и применены защитные флаги глубины и LRZ, что полностью устраняет артефакты геометрии и мерцания текстур.
• В Diablo II: Resurrected и Streets of Rage 4 полностью сохранен режим <code>tu_tile_discard=true</code> с аппаратным буфером D32 для максимального FPS.

🧠 <b>Аппаратная калибровка оборудования и адаптивные профили:</b>
• Интеллектуальный анализатор StormHardwareCalibrator определяет чипсет устройства (Snapdragon 8 Elite / Adreno 830, Snapdragon 8 Gen 1-3, 888/870, Dimensity 9400/9300, Exynos, Google Tensor), объем оперативной памяти и тип устройства (смартфон или планшет).
• Пресет «По умолчанию (Сброс)» теперь выставляет оптимальные параметры именно под конкретное устройство.
• Профили «Быстрый», «Нормальный (Рекомендуется)» и «Точный» автоматически адаптируют разрешение (720p/1080p), точность CPU/GPU, режим памяти (Fastmem) и лимит кеша под возможности процессора и графического ускорителя.

⚡ <b>Оптимизация генератора кадров LSFG и сглаживание шага:</b>
• В ядре frame_gen_pacer.cpp полностью ликвидирован 60-секундный локаут генерации кадров при просадках: внедрен прогрессивный адаптивный интервал возврата (0.5–3 секунды).
• Добавлено ограничение выбросов таймингов (outlier clamping) при расчете скользящей средней (EMA) и сглажен спад накопленных кредитов (decay 0.8), устранив микрозадержки и подергивания.

🔦 <b>Alan Wake Remastered — исправление субтитров и шрифтов:</b>
• В Vulkan-рендерере vk_texture_cache.cpp скорректирован swizzle одноканальных текстур шрифтов R8_UNORM и BC4_UNORM (RGB каналы направлены в ONE, альфа в Red), что полностью вернуло чистый белый цвет субтитров и глифов.
• Добавлен специализированный профиль драйвера Turnip с флагами <code>tu_a8_unorm_swizzle_one</code> и <code>tu_r8_unorm_swizzle_alpha</code>.

👊 <b>Окончательная фиксация и запуск Streets of Rage 4 (SOR4):</b>
• В JIT-ядре arm_dynarmic_64.cpp устранен бесконечный цикл исключений при NoExecuteFault с корректным продвижением счетчика команд (pc + 4) и выходом в меню.
• За всеми 3 Title ID игры закреплены обязательные параметры GPU и памяти.

🌐 <b>100% локализация интерфейса и типографика:</b>
• Все элементы интерфейса, диалоговые окна, всплывающие уведомления и настройки переведены на чистый русский язык со строгим соблюдением Sentence case и полным запретом символа «&».

━━━━━━━━━━━━━━━━━━━━━━━
📦 <i>Свежие исполняемые файлы и APK-пакеты собраны, подписаны цифровой подписью SHA-256 и готовы к установке.</i>
"@

Write-Host "1. Sending release announcement..."
Send-TGMessage $announcement

Write-Host "2. Uploading release files to Telegram..."
$filesToUpload = @(
    @{
        Path = "E:\STORM EDEN 3\Files\STORM_SWITCH_7.3.0.apk"
        Caption = "📱 <b>STORM SWITCH 7.3.0 (Mainline Release - Android 14+)</b>"
    },
    @{
        Path = "E:\STORM EDEN 3\Files\STORM_SWITCH_7.3.0_LEGACY.apk"
        Caption = "📱 <b>STORM SWITCH 7.3.0 (Legacy Release - Android 10-13)</b>"
    },
    @{
        Path = "E:\STORM EDEN 3\Files\STORM_SWITCH_7.3.0_SDK27.apk"
        Caption = "📱 <b>STORM SWITCH 7.3.0 (SDK27 Release - Android 8.1-9)</b>"
    },
    @{
        Path = "E:\STORM EDEN 3\Files\STORM_SWITCH_7.3.0_Windows.zip"
        Caption = "💻 <b>STORM SWITCH 7.3.0 (Windows x64 Release Portable)</b>"
    }
)

foreach ($item in $filesToUpload) {
    if (Test-Path $item.Path) {
        Send-TGDocument $item.Path $item.Caption
    } else {
        Write-Warning "File not found: $($item.Path)"
    }
}

Write-Host "`nRelease 7.3.0 deployment to Telegram completed successfully!"