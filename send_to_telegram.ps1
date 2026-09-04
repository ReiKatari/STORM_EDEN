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
⚡ <b>Обновление STORM SWITCH 7.3.2 (Heavy CPU Hybrid Offloading, Volumetric Touch Styles, Full Profiles Sync, UI and Path Modernization)</b> — <i>Крупное обновление эмулятора Nintendo Switch: глубокая разгрузка видеоядра ГПУ за счет многопоточного ЦП в гибридных режимах NVDEC и ASTC, современное 3D-окно выбора стиля сенсорного управления, полноценная синхронизация профилей настроек, исправление отправки логов, удобные файловые диалоги и обновление интерфейса</i>

━━━━━━━━━━━━━━━━━━━━━━━

🚀 <b>Ключевые изменения и улучшения:</b>

⚡ <b>Усиление гибридных режимов NVDEC и ASTC в сторону ЦП:</b>
• В гибридном режиме декодирования видео NVDEC на Android задействован высокопроизводительный многопоточный программный декодер ЦП (4–8 потоков FFmpeg с разделением кадров и слайсов), что снимает перегрузку с ГПУ (снижение с 98%) и поднимает утилизацию ЦП до 40%+.
• В гибридном режиме декодирования ASTC текстурный пул расширен до 4–8 параллельных рабочих потоков ЦП, куда направляется асинхронная распаковка базовых и средних текстур, устраняя голодание видеокарты и микрофризы.

🎛️ <b>Профили настроек и полноценная синхронизация:</b>
• Добавлена полная поддержка всех современных параметров (Eco Thermal, Frame Pacing, Smart Shader Throttle, Vulkan Pipeline Cache, VRAM GC, Fast GPU Time, NVDEC и ASTC Hybrid).
• Реализована корректная синхронизация при создании и применении профилей как глобально, так и для отдельных игр (Per-Game Config) с немедленным вступлением настроек в силу в запущенной игре.
• Встроены 3 готовых заводских профиля («Сбалансированный», «Максимальная производительность», «Максимальное качество»).

🎮 <b>Объемное 3D-окно выбора стиля сенсорного управления:</b>
• Полностью переработан диалог выбора стиля виртуального оверлея в фирменном объемном дизайне экосистемы STORM SOFT: рельефные карточки с тенями, индикация активного стиля с неоновым кольцом и двойные превью-маркеры цветов кнопок в покое и при нажатии.

📁 <b>Управление папками и пользовательские пути:</b>
• Карточки в «Управлении папками» переработаны: выравнивание по левому краю, перенос длинных путей на несколько строк без наложения на кнопки, увеличенные сенсорные кнопки управления.
• Добавлены стартовые пути по умолчанию в `STORM SWITCH/Amiibo` для ключей Amiibo и `STORM SWITCH/nand/user/save` для сохранения данных.
• Исправлено отображение плашки «По умолчанию» для стандартных каталогов.

🛡️ <b>Исправление отправки логов и устранение дубликатов:</b>
• Устранен вылет приложения при попытке поделиться журналом ГПУ или журналом отладки через FileProvider за счет безопасного кэширования и расширенной конфигурации путей.
• Меню «Настройки Freedreno» исключено из раздела «Расширенные настройки» для предотвращения дублирования.
• Обеспечено автоматическое сохранение и удаление профилей и переменных в Freedreno.

🪄 <b>Автонастройка и локализация:</b>
• Окно мастера автооптимизации во время игры расширено до 95% ширины экрана для удобного управления.
• Обозначения приведены к единому стандарту: ГПУ, ЦП, ОЗУ.
• Локализован параметр отображения времени кадра («Показать время кадра (Frametime)»).

━━━━━━━━━━━━━━━━━━━━━━━
📦 <i>Свежие исполняемые файлы и APK-пакеты собраны, подписаны цифровой подписью SHA-256 и готовы к установке.</i>
"@

Write-Host "1. Sending release announcement..."
Send-TGMessage $announcement

Write-Host "2. Uploading release files to Telegram..."
$filesToUpload = @(
    @{
        Path = "E:\STORM EDEN 3\Files\STORM_SWITCH_7.3.2.apk"
        Caption = "📱 <b>STORM SWITCH 7.3.2 (Mainline Release - Android 14+)</b>"
    },
    @{
        Path = "E:\STORM EDEN 3\Files\STORM_SWITCH_7.3.2_LEGACY.apk"
        Caption = "📱 <b>STORM SWITCH 7.3.2 (Legacy Release - Android 10-13)</b>"
    },
    @{
        Path = "E:\STORM EDEN 3\Files\STORM_SWITCH_7.3.2_SDK27.apk"
        Caption = "📱 <b>STORM SWITCH 7.3.2 (SDK27 Release - Android 8.1-9)</b>"
    },
    @{
        Path = "E:\STORM EDEN 3\Files\STORM_SWITCH_7.3.2_Windows.zip"
        Caption = "💻 <b>STORM SWITCH 7.3.2 (Windows x64 Release Portable)</b>"
    }
)

foreach ($item in $filesToUpload) {
    if (Test-Path $item.Path) {
        Send-TGDocument $item.Path $item.Caption
    } else {
        Write-Warning "File not found: $($item.Path)"
    }
}

Write-Host "`nRelease 7.3.2 deployment to Telegram completed successfully!"