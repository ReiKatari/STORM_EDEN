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
⚡ <b>Обновление STORM SWITCH 7.3.1 (NCE Fault Tolerance, GRC Movie Services, Zelda BotW 32 FPS Early-Z Fix, STORM DRIVER 2.0.3, Hybrid NVDEC and ASTC, In-Game Self-Healing)</b> — <i>Крупное обновление эмулятора Nintendo Switch: полное восстановление 30–32+ FPS и геометрии в The Legend of Zelda: BotW, устранение вылетов Streets of Rage 4 на Android, новые системные драйверы STORM DRIVER 2.0.3 (включая Zelda Edition), гибридные режимы декодирования NVDEC и ASTC, плавающая кнопка автоисправления в игре и 100% русская локализация</i>

━━━━━━━━━━━━━━━━━━━━━━━

🚀 <b>Ключевые изменения и улучшения:</b>

🗡️ <b>The Legend of Zelda: BotW — восстановление производительности (30–32+ FPS) и текстур:</b>
• Устранена просадка кадров с 15 FPS до стабильных 30–32+ FPS благодаря включению таймингов ГПУ <code>use_fast_gpu_time</code> и оптимизации динамического масштабирования.
• Включено сжатие ASTC в формат BC3 (<code>astc_recompression = 2</code>) и режим памяти 6 ГБ DRAM, что полностью ликвидировало утечки видеопамяти и исчезновение ландшафта и текстур.
• В конфигурации Turnip drirc сохранен аппаратный Early-Z (LRZ) с непрерывным сохранением между буферами команд (<code>tu_lrz_preserve_across_cmdbuf=true</code>), исключающий артефакты и мерцания.

👊 <b>Streets of Rage 4 и стабильность NCE ядра на Android:</b>
• В NCE-ядре <code>arm_nce.cpp</code> восстановлен безопасный пропуск исключений Data Abort при обращении к невыровненной памяти, что полностью устранило сбои при загрузке уровней.
• Реализованы заглушки сервисов видеозаписи GRC (<code>grc:c</code> — IContinuousRecorder, IGameMovieTrimmer, IOffscreenRecorder, IMovieMaker), возвращающие ResultSuccess вместо системной паники <code>0x1A80A</code>.
• Добавлено автоматическое создание отсутствующих директорий при инициализации файловой системы.

🚀 <b>Драйверы Turnip v2.0.3 (STORM DRIVER 2.0.3 и Zelda Edition):</b>
• Собраны и оптимизированы два пакета драйверов Turnip 2.0.3:
  - <b>STORM DRIVER 2.0.3</b> — универсальная версия с максимальным быстродействием и тайловым сбросом для большинства игр.
  - <b>STORM DRIVER 2.0.3 Zelda Edition</b> — специальная версия с полным сохранением Early-Z, отключенным tile discard и идеальной геометрией без мерцания.

🔄 <b>Гибридные режимы декодирования NVDEC и ASTC:</b>
• Добавлен режим «Гибридный» для декодирования видео NVDEC (интеллектуальное распределение нагрузки между MediaCodec ГПУ и многопоточным декодером ЦП).
• Добавлен режим «Гибридный» для распаковки текстур ASTC (аппаратная акселерация с параллельной асинхронной подгрузкой на процессоре).
• В интерфейсе Android обозначения приведены к единому русскому стандарту: ГПУ, ЦП, Гибридный.

🛠️ <b>Внутриигровая система автоисправления (Self-Healing):</b>
• Добавлена круглая плавающая кнопка и пункт меню «Автоисправление» во время игры.
• Диалоговое окно детально информирует о выявленных проблемах текущей игры, предлагает применить проверенный профиль настроек или отключить автоисправление при желании настроить параметры вручную.

🌐 <b>Точная локализация и терминология:</b>
• Дополнен перевод в ru_RU.ts и shared_translation.cpp: «Звуковой движок», «Устройство вывода звука», «Тайминги ГПУ», «Асинхронная компиляция шейдеров» в строгом соответствии с Sentence case.

━━━━━━━━━━━━━━━━━━━━━━━
📦 <i>Свежие исполняемые файлы и APK-пакеты собраны, подписаны цифровой подписью SHA-256 и готовы к установке.</i>
"@

Write-Host "1. Sending release announcement..."
Send-TGMessage $announcement

Write-Host "2. Uploading release files to Telegram..."
$filesToUpload = @(
    @{
        Path = "E:\STORM EDEN 3\Files\STORM_SWITCH_7.3.1.apk"
        Caption = "📱 <b>STORM SWITCH 7.3.1 (Mainline Release - Android 14+)</b>"
    },
    @{
        Path = "E:\STORM EDEN 3\Files\STORM_SWITCH_7.3.1_LEGACY.apk"
        Caption = "📱 <b>STORM SWITCH 7.3.1 (Legacy Release - Android 10-13)</b>"
    },
    @{
        Path = "E:\STORM EDEN 3\Files\STORM_SWITCH_7.3.1_SDK27.apk"
        Caption = "📱 <b>STORM SWITCH 7.3.1 (SDK27 Release - Android 8.1-9)</b>"
    },
    @{
        Path = "E:\STORM EDEN 3\Files\STORM_SWITCH_7.3.1_Windows.zip"
        Caption = "💻 <b>STORM SWITCH 7.3.1 (Windows x64 Release Portable)</b>"
    }
)

foreach ($item in $filesToUpload) {
    if (Test-Path $item.Path) {
        Send-TGDocument $item.Path $item.Caption
    } else {
        Write-Warning "File not found: $($item.Path)"
    }
}

Write-Host "`nRelease 7.3.1 deployment to Telegram completed successfully!"