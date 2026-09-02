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
⚡ <b>Обновление STORM SWITCH 7.2.0 (Dimensity 9400, Vulkan Stability and GameFix Overhaul)</b> — <i>Свежая сборка: глубокая оптимизация под MediaTek Dimensity 9400 (Vivo X200 Pro Mini), устранение вылетов Vulkan Scheduler и сетевых сокетов BSD, исправления для Mortal Kombat 1, Diablo II: Resurrected и homebrew GTA V, обновление Менеджера модов и 8 фирменных тем оформления</i>

━━━━━━━━━━━━━━━━━━━━━━━

🚀 <b>Ключевые изменения и улучшения:</b>

📱 <b>Оптимизация под MediaTek Dimensity 9400 (Vivo X200 Pro Mini):</b>
• Внедрено интеллектуальное распознавание архитектуры процессоров All-Big-Core и видеоядра ARM Immortalis-G925.
• Добавлены профили адаптивного энергосбережения и термоконтроля (Eco Thermal Mode, Eco Frame Pacing, Smart Shader Throttle).
• Ограничено число фоновых потоков компиляции шейдеров для защиты от перегрева корпуса смартфона.
• Отключены конфликтующие динамические состояния (Extended Dynamic State) в видеодрайвере Mali для предотвращения сбоев и артефактов геометрии.
• Интегрировано масштабирование AMD FSR с оптимизированной резкостью для максимальной плавности.

🛡️ <b>Устранение критических сбоев Vulkan Scheduler (Дампы 13840.dmp и 66592.dmp):</b>
• Устранены падения при асинхронной отправке команд рендеринга из-за разыменования нулевых буферов (null chunk dereference).
• Реализованы строгие защитные барьеры при захвате чанков (AcquireNewChunk, DispatchWork, SubmitExecution).

🌐 <b>Надёжность сетевых сокетов BSD (Homebrew и GTA V):</b>
• Устранены сбои эмулятора при запуске homebrew-проектов (порт GTA V) из-за несоответствия размеров структур сокетов sockaddr.
• Жесткие аварийные остановки (ASSERT) заменены на безопасную валидацию с корректными кодами ошибок Switch OS.

🎮 <b>Глубокая доработка базы авто-исправлений GameFix:</b>
• <b>Mortal Kombat 1 и 11</b>: исправлена конфигурация памяти до 8 ГБ DRAM, устранено зависание на аренах и фаталити.
• <b>Diablo II: Resurrected</b>: устранены графические артефакты (черные и белые квадраты/полосы) вокруг декалей и персонажей, отключено сжатие ASTC, обеспечена стабильная загрузка в режиме 8 ГБ DRAM.
• <b>GTA V Homebrew Port</b>: настроен профиль 8 ГБ DRAM для тяжелой геометрии Лос-Сантоса.
• <b>Модуль записи INI</b>: обеспечена синхронная запись параметров памяти в секции Core и System.

🧩 <b>Инструменты и менеджер модификаций:</b>
• Выровнена иконка менеджера модов в меню инструментов (слева в выделенной колонке без дублирования).
• Исправлено сохранение состояния отключения модов (PatchManager теперь точно идентифицирует оригинальные идентификаторы модификаций).
• Добавлено сопоставление GameBanana для Super Mario Bros. Wonder.

🎨 <b>Единая система 8 визуальных тем оформления:</b>
• Полная поддержка стандартизированных тем: STORM DARK, STORM NIGHT, STORM DAY, STORM MIDNIGHT, STORM MATRIX, STORM CYBERPUNK, STORM FANTASY, STORM WARHAMMER 40K.

━━━━━━━━━━━━━━━━━━━━━━━
📦 <i>Свежие исполняемые файлы и APK-пакеты собраны, подписаны SHA-256 и готовы к загрузке.</i>
"@

Write-Host "1. Sending release announcement..."
Send-TGMessage $announcement

Write-Host "2. Uploading release files to Telegram..."
$filesToUpload = @(
    @{
        Path = "E:\STORM EDEN 3\Files\STORM_SWITCH_7.2.0.apk"
        Caption = "📱 <b>STORM SWITCH 7.2.0 (Mainline Release - Android 14+)</b>"
    },
    @{
        Path = "E:\STORM EDEN 3\Files\STORM_SWITCH_7.2.0_LEGACY.apk"
        Caption = "📱 <b>STORM SWITCH 7.2.0 (Legacy Release - Android 10-13)</b>"
    },
    @{
        Path = "E:\STORM EDEN 3\Files\STORM_SWITCH_7.2.0_Windows.zip"
        Caption = "💻 <b>STORM SWITCH 7.2.0 (Windows x64 Release Portable)</b>"
    }
)

foreach ($item in $filesToUpload) {
    if (Test-Path $item.Path) {
        Send-TGDocument $item.Path $item.Caption
    } else {
        Write-Warning "File not found: $($item.Path)"
    }
}

Write-Host "`nRelease 7.2.0 deployment to Telegram completed successfully!"