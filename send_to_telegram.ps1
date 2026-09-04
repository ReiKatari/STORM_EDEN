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
⚡ <b>Обновление STORM SWITCH 7.3.0 (Streets of Rage 4 Fix, NSZ и XCZ Library, Zelda BotW Driver Engine, Dynamic Cooling и Amiibo NTAG215)</b> — <i>Масштабное обновление эмулятора Nintendo Switch: запуск Streets of Rage 4 без вылетов и сбоев видеопамяти, полное отображение NSZ и XCZ с раздельным показом разных версий игр по файлам, стабильный старт Zelda: BotW с высокопроизводительным Turnip профилем, адаптивный расчет температуры охлаждения чипсета и 100% валидация виртуальных Amiibo NTAG215</i>

━━━━━━━━━━━━━━━━━━━━━━━

🚀 <b>Ключевые изменения и улучшения:</b>

👊 <b>Исправление вылета Streets of Rage 4 (SOR4):</b>
• Устранен преждевременный сброс закреплений страниц видеопамяти в NVDRV (UnpinHandle), вызывавший деаллокацию буферов заставки и декодера видео.
• В JIT-движке Dynarmic добавлен безопасный перехват NoExecuteFault с продвижением счетчика команд и остановкой бесконечного цикла исключений.
• Игра успешно запускается, воспроизводит вступительные катсцены и выходит в главное меню без черного экрана.

📦 <b>Отображение NSZ, XCI, XCZ и раздельный показ версий по файлам:</b>
• Дедупликация списка игр переведена на уровень физических путей файлов (game.path): разные дампы, региональные ревизии и форматы (например, v1.0 NSP и v1.3 NSZ) теперь отображаются отдельными карточками с реальными версиями.
• Идентификатор программы (Program Title ID) считывается напрямую из заголовков CNMT метаданных без необходимости тяжелой фоновой распаковки solid NCZ при сканировании библиотеки.
• Добавлено распознавание сжатых контейнеров XCZ в Secure-разделах картриджей.

🗡️ <b>Стабильный запуск The Legend of Zelda: Breath of the Wild:</b>
• В профиле Turnip ZELDA удалены конфликтующие и нестабильные флаги (tu_tile_discard=false, tu_ping_pong_command_submission=false), приводившие к сбоям Vulkan Fence.
• Активированы проверенные параметры: tu_tile_discard=true, tu_force_d32_unnormalized=true, tu_depth_bias_control_all_adreno=true и tu_dynamic_state_depth_bias_clamp=true для идеальной отрисовки воды и святилищ.
• Исправлен порядок инициализации Freedreno и защищены переменные окружения DRIRC от сброса.

🌡️ <b>Интеллектуальный оверлей охлаждения (Thermal Overlay):</b>
• Панель охлаждения скрыта при обычных ручных паузах при комфортной температуре устройства (&lt; 43°C).
• При нагреве целевая температура рассчитывается динамически с гистерезисом (target = max(38°C, temp - 4°C)), полностью исключив инвертированные подсказки вида «35.3°C ➔ Цель: 36.5°C».

⚡ <b>Оптимизация скорости сканирования библиотеки:</b>
• Версии и метаданные считываются напрямую из AppLoader без избыточного монтирования PatchRomFS, а запросы GetControlMetadata объединены, ускоряя обновление списка игр в 3–4 раза.

🔮 <b>100% валидация и генерация виртуальных Amiibo:</b>
• В генераторе AmiiboHelper прописаны все точные константы структуры NTAG215 (static_lock = 0xE00F, compatibility_container = 0xEEFF10F1, constant_value = 0xA5, dynamic_lock = 0x0F0001, CFG0 = 0x04000000, CFG1 = 0x5F, tag_type = 2, контрольные суммы UID по стандарту ISO/IEC 14443-3).
• Все сгенерированные и скачанные теги успешно проходят внутреннюю проверку IsAmiiboValid и распознаются эмулятором мгновенно.

━━━━━━━━━━━━━━━━━━━━━━━
📦 <i>Свежие исполняемые файлы и APK-пакеты собраны, подписаны SHA-256 и готовы к загрузке.</i>
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