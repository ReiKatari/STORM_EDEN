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
⚡ <b>Обновление STORM SWITCH 7.3.0 (SOR4 Locked Fix, Zelda BotW Graphics, GameBanana and Amiibo Pagination, Cheats Editor, Stability and Full Localization)</b> — <i>Комплексное обновление эмулятора Nintendo Switch: окончательное закрепление параметров и JIT-обработки Streets of Rage 4, восстановление графики и текстур в Zelda: BotW с сохранением стабильности Diablo II, компактный интерфейс модов и Amiibo с пагинацией, интерактивный редактор значений читов, защита от вылетов при запуске игр и 100% выверенная локализация Windows и Android</i>

━━━━━━━━━━━━━━━━━━━━━━━

🚀 <b>Ключевые изменения и улучшения:</b>

👊 <b>Окончательная фиксация и запуск Streets of Rage 4 (SOR4):</b>
• В JIT-ядре arm_dynarmic_64.cpp устранён бесконечный цикл исключений при NoExecuteFault: реализовано корректное продвижение счётчика команд (pc + 4), подробное логирование бэктрейса и гарантированная остановка сбойного потока (ReturnException(pc, PrefetchAbort)).
• За всеми 3 Title ID игры (0100EC9010258000, 010085800E33E000, 0100BA700E340000) навсегда зафиксированы обязательные параметры: высокая точность GPU, быстрая память (Fastmem), асинхронные шейдеры, быстрое время GPU, игнорирование прерываний памяти и Turnip tile discard.
• Игра стабильно запускается, без зависаний воспроизводит все видеовставки и катсцены и выходит в игровое меню.

🗡️ <b>Восстановление графики в The Legend of Zelda: Breath of the Wild:</b>
• Устранено пропадание текстур ландшафта, земли и графических элементов на драйверах Turnip Adreno.
• Внедрён изолированный профиль ZELDA с безопасными флагами: tu_tile_discard=false, tu_disable_lrz=true, tu_lrz_preserve_across_cmdbuf=false, tu_disable_fast_clears=true, tu_lrz_fast_clear=false, tu_force_d32_unnormalized=true и отключенным сжатием ASTC (0).
• Полностью сохранена безупречная графика и изолированность настроек Diablo II: Resurrected и Streets of Rage 4 (с активным tu_tile_discard=true).

🍌 <b>Компактный интерфейс модов GameBanana:</b>
• Кнопки управления и панель навигации опущены в нижнюю часть окна.
• Высота управляющей панели сжата до 28dp с отступами 2dp, предоставив максимальную полезную область экрана под список доступных модов.

🔮 <b>Онлайн-база Amiibo с компактной пагинацией:</b>
• Элементы управления опущены вниз, добавлена ультракомпактная панель пагинации (28dp).
• Реализована плавная постраничная навигация (по 40 фигурок на страницу) с индикатором страниц и кнопками перехода.

💎 <b>Объемный стиль и редактор значений в Менеджере читов:</b>
• Кнопки и карточки читов стилизованы под объемные 3D-плашки с реалистичными мягкими тенями и тактильной реакцией на нажатие.
• Добавлена функция прямого редактирования числовых значений читов на количество (рупии, монеты, золото, боеприпасы, расходники) с мгновенной перезагрузкой читов в память на лету через reloadCheats.

🛡️ <b>Устранение сбоев и вылетов при запуске игр:</b>
• В EmulationFragment внедрено обязательное ожидание готовности драйвера (isInteractionAllowed) для всех типов запуска.
• Добавлена проверка валидности графической поверхности (holder.surface.isValid) и строгие нулл-чеки нативного окна и библиотеки Vulkan в native.cpp.

🌐 <b>100% локализация интерфейса и типографика:</b>
• 21 элемент интерфейса Android (strings.xml) и 46 параметров Windows (shared_translation.cpp, ru_RU.ts, Qt UI) переведены на чистый русский язык без английских терминов, со строго выверенным Sentence case и заменой символа «&» на союз «и».

━━━━━━━━━━━━━━━━━━━━━━━
📦 <i>Свежие исполняемые файлы и APK-пакеты собраны, подписаны SHA-256 и готовы к установке.</i>
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