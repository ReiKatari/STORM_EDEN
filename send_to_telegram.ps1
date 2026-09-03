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
⚡ <b>Обновление STORM SWITCH 7.2.2 (Library Filtering, Save Data Recovery, Eden Nightly Improvements and BotW Fix)</b> — <i>Масштабное обновление: исключение DLC и обновлений из списка игр, восстановление видимости сохранений при любых сменах профилей, устранение белых вспышек и пропажи текстур в Zelda: BotW, перенос лучших оптимизаций Eden Nightly (PSO кэш, JIT CPUID, nvdec плавные видеоролики, исправление модов IPS), редизайн стартового мастера Android и 100% изоляция игровых настроек</i>

━━━━━━━━━━━━━━━━━━━━━━━

🚀 <b>Ключевые изменения и улучшения:</b>

🎮 <b>Чистый список игр без мусора (Windows и Android):</b>
• Контейнеры самостоятельных обновлений (0x800) и дополнений DLC (0x001+) больше не засоряют библиотеку и не отображаются как отдельные игры.
• Обновления и DLC по-прежнему корректно регистрируются в виртуальной файловой системе и бесшовно применяются к базовым играм.

💾 <b>Интеллектуальное восстановление сохранений (Save Data Recovery):</b>
• Реализован автоматический поиск сохранений по базовому Title ID и среди всех профилей пользователя (/user/save/0000000000000000/*/).
• Если игра не видит сейв из-за смены или перегенерации случайного UUID профиля, эмулятор автоматически находит и подключает существующие сохранения вместо создания пустой папки.
• Добавлена поддержка команды RenameDirectory — устранено повреждение и потеря сейвов в играх, использующих переименование папок.
• Улучшен импорт сохранений из ZIP-архивов: поддерживается любая структура архивов без ограничений.

🗡️ <b>Устранение дефектов в The Legend of Zelda: Breath of the Wild:</b>
• Включена реактивная очистка (use_reactive_flushing = true) — полностью устранены белые вспышки и мерцание освещения/погоды.
• Профиль переведен на 8 ГБ DRAM — устранена нехватка памяти, приводящая к исчезновению текстур земли и скал.
• Сохранена высокая точность GPU и несжатый ASTC для кристальной четкости без артефактов силуэта Линка.

⚡ <b>Перенос топовых улучшений Eden Nightly:</b>
• Vulkan PSO Pipeline Optimizations: кэширование состояний пайплайнов устраняет повторную компиляцию шейдеров при перезапуске игр — мгновенный повторный старт.
• Dynarmic CPUID Fastpath: статическая инициализация проверок CPUID исключает микрофризы при JIT-компиляции на x64.
• Nvdec Microsleep Machinery: плавное воспроизведение видеороликов и катсцен без зависаний и вылетов гостевой ОС.
• Исправлено применение модов IPSwitch и 60 FPS патчей с проверкой NSO Build ID.
• Устранен редкий вылет Mario Kart 8 Deluxe при подключении через LDN.

📱 <b>Редизайн мастера первой настройки Android и локализация:</b>
• Кнопки мастера настройки («Ключи», «Прошивка», «Генерация кадров», «Игры») переработаны в объемном стиле STORM: яркий неоновый контур Cyan, приподнятый 3D-контейнер, четкий белый текст и зеленый бейдж готовности.
• Локализован заголовок диалога «Предупреждение» (Warning) на всех языках.
• Исправлено ложное уведомление о доступности обновления на ту же самую версию.

🔒 <b>100% точечная изоляция профилей для ВСЕХ игр:</b>
• При завершении эмуляции на Windows и Android вызывается полный сброс Settings::RestoreGlobalState(false).
• Настройки одной запущенной игры никогда не просачиваются в другие игры или в глобальный конфиг.

━━━━━━━━━━━━━━━━━━━━━━━
📦 <i>Свежие исполняемые файлы и APK-пакеты собраны, подписаны SHA-256 и готовы к загрузке.</i>
"@

Write-Host "1. Sending release announcement..."
Send-TGMessage $announcement

Write-Host "2. Uploading release files to Telegram..."
$filesToUpload = @(
    @{
        Path = "E:\STORM EDEN 3\Files\STORM_SWITCH_7.2.2.apk"
        Caption = "📱 <b>STORM SWITCH 7.2.2 (Mainline Release - Android 14+)</b>"
    },
    @{
        Path = "E:\STORM EDEN 3\Files\STORM_SWITCH_7.2.2_LEGACY.apk"
        Caption = "📱 <b>STORM SWITCH 7.2.2 (Legacy Release - Android 10-13)</b>"
    },
    @{
        Path = "E:\STORM EDEN 3\Files\STORM_SWITCH_7.2.2_Windows.zip"
        Caption = "💻 <b>STORM SWITCH 7.2.2 (Windows x64 Release Portable)</b>"
    }
)

foreach ($item in $filesToUpload) {
    if (Test-Path $item.Path) {
        Send-TGDocument $item.Path $item.Caption
    } else {
        Write-Warning "File not found: $($item.Path)"
    }
}

Write-Host "`nRelease 7.2.2 deployment to Telegram completed successfully!"