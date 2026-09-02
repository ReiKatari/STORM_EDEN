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
⚡ <b>Обновление STORM SWITCH 7.1.0 (Super Mario Bros. Wonder and LightLimiter Audio Fix Edition)</b> — <i>Свежая сборка: полное устранение вылета Super Mario Bros. Wonder и других игр Nintendo через PDB-анализ дампа 65740.dmp в аудио-эффекте LightLimiter</i>

━━━━━━━━━━━━━━━━━━━━━━━

🚀 <b>Свежие исправления в этом обновлении:</b>

🍄 <b>Полное устранение краша в Super Mario Bros. Wonder (Дамп 65740.dmp):</b>
• В дампе <code>STORM_SWITCH.exe.65740.dmp</code> выявлен разыменовыватель нулевого указателя (NULL dereference) в аудиоэффекте <code>LightLimiter</code>: при динамическом обновлении параметров <code>look_ahead_sample_buffers</code> оставались неинициализированными.
• Инструкция чтения упреждающего сэмпла (lookahead) обращалась по нулевому адресу <code>[rdx + r9*8]</code> при <code>rdx = 0x0</code>.
• Внедрен автоматический цикл инициализации <code>needs_init</code> для всех каналов в <code>LightLimiterVersion1Command</code> и <code>LightLimiterVersion2Command</code>.
• В <code>ApplyLightLimiterEffect</code> добавлены строгие барьеры безопасности: проверка непустоты и валидности указателей буферов, защита от деления на ноль при <code>look_ahead_samples_min &le; 0</code> и безопасный переход в режим Bypass без вылета игры.
• В эффекте <code>Delay</code> реализован полный контроль всех аудиоканалов и границ спанов.

━━━━━━━━━━━━━━━━━━━━━━━
📦 <i>Свежие исполняемые файлы и APK-пакеты собраны, подписаны SHA-256 и готовы к загрузке.</i>
"@

Write-Host "1. Sending release announcement..."
Send-TGMessage $announcement

Write-Host "2. Uploading release files to Telegram..."
$filesToUpload = @(
    @{
        Path = "E:\STORM EDEN 3\Files\STORM_SWITCH_7.1.0.apk"
        Caption = "📱 <b>STORM SWITCH 7.1.0 (Mainline Release - Android 14+)</b>"
    },
    @{
        Path = "E:\STORM EDEN 3\Files\STORM_SWITCH_7.1.0_LEGACY.apk"
        Caption = "📱 <b>STORM SWITCH 7.1.0 (Legacy Release - Android 10-13)</b>"
    },
    @{
        Path = "E:\STORM EDEN 3\Files\STORM_SWITCH_7.1.0_Windows.zip"
        Caption = "💻 <b>STORM SWITCH 7.1.0 (Windows x64 Release Portable)</b>"
    }
)

foreach ($item in $filesToUpload) {
    if (Test-Path $item.Path) {
        Send-TGDocument $item.Path $item.Caption
    } else {
        Write-Warning "File not found: $($item.Path)"
    }
}

Write-Host "`nRelease 7.0.0 deployment to Telegram completed successfully!"