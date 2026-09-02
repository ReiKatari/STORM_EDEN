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
⚡ <b>Обновление STORM SWITCH 7.2.1 (Performance Restoration, Driver 1.2.2, 3D Keyboard and Stability Hotfix)</b> — <i>Свежая сборка: устранение вылетов Defender of the Crown и GTA V, восстановление 60 FPS в Mortal Kombat 1, устранение графических рамок в Diablo II: Resurrected, полный редизайн и масштабирование экранной клавиатуры Windows, релиз драйвера STORM DRIVER 1.2.2</i>

━━━━━━━━━━━━━━━━━━━━━━━

🚀 <b>Ключевые изменения и улучшения:</b>

🛡️ <b>Устранение вылетов Defender of the Crown и 2D-блиттинга (Дамп 7428.dmp):</b>
• Внедрены строгие защитные барьеры при аппаратном 2D-блиттинге (DrawTexture в vk_rasterizer.cpp).
• Исключены сбои разыменования нулевых указателей фреймбуфера и дескрипторов при смене меню и катсцен.

🎮 <b>Стабильность загрузки Homebrew GTA V (Дамп 34812.dmp):</b>
• Устранено зависание потоков загрузки RAGE на экране «Вход в сюжетный режим».
• Интегрированы профили Normal GPU Accuracy, Fast GPU Time и быстрого поведения фенсов.
• Защищен цикл Vulkan WorkerThread от неполных чанков при параллельном стриминге ассетов Лос-Сантоса.

⚡ <b>Восстановление 60 FPS в Mortal Kombat 1 и исправление Diablo II (STORM DRIVER 1.2.2):</b>
• Из глобального профиля драйвера удалены зажимы частот и отключение раннего Z-отсечения (tu_disable_lrz).
• Возвращено штатное раннее Z-отсечение (Early-Z / LRZ) и быстрые очистки (Fast Clears) для всех игр — Mortal Kombat 1 снова стабильно работает при 50–60 FPS.
• Устранены артефакты рамок и черных квадратов в Diablo II: Resurrected: специфичные хаки рендеринга Зельды изолированы строго под Title ID BotW и TotK.
• Восстановлено корректное масштабирование Depth Bias Z24/D32 для плоских декалей и шрифтовых атласов.

⌨️ <b>Полный редизайн экранной клавиатуры Windows (SWKBD):</b>
• Устранена ошибка двойного деления на масштаб экрана (DPI Scaling), приводившая к микроскопическому шрифту на экранах высокого разрешения.
• Увеличена высота строки ввода текста (с 28px до 52px+) и размер шрифта.
• Интегрирован современный объемный 3D-стиль экосистемы STORM: темный градиент, акцентная неоновая окантовка Cyan, тактильный отклик клавиш.

━━━━━━━━━━━━━━━━━━━━━━━
📦 <i>Свежие исполняемые файлы и APK-пакеты собраны, подписаны SHA-256 и готовы к загрузке.</i>
"@

Write-Host "1. Sending release announcement..."
Send-TGMessage $announcement

Write-Host "2. Uploading release files to Telegram..."
$filesToUpload = @(
    @{
        Path = "E:\STORM EDEN 3\Files\STORM_SWITCH_7.2.1.apk"
        Caption = "📱 <b>STORM SWITCH 7.2.1 (Mainline Release - Android 14+)</b>"
    },
    @{
        Path = "E:\STORM EDEN 3\Files\STORM_SWITCH_7.2.1_LEGACY.apk"
        Caption = "📱 <b>STORM SWITCH 7.2.1 (Legacy Release - Android 10-13)</b>"
    },
    @{
        Path = "E:\STORM EDEN 3\Files\STORM_SWITCH_7.2.1_Windows.zip"
        Caption = "💻 <b>STORM SWITCH 7.2.1 (Windows x64 Release Portable)</b>"
    }
)

foreach ($item in $filesToUpload) {
    if (Test-Path $item.Path) {
        Send-TGDocument $item.Path $item.Caption
    } else {
        Write-Warning "File not found: $($item.Path)"
    }
}

Write-Host "`nRelease 7.2.1 deployment to Telegram completed successfully!"