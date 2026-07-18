// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dump_settings.h"

// SPDX-FileCopyrightText: Copyright 2023 Ryujinx Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// Qt on macOS doesn't define VMA shit
#include <thread>
#include <mutex>
#include <set>
#include "core/file_sys/common_funcs.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/control_metadata.h"
#include "core/file_sys/patch_manager.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/romfs.h"
#include "common/settings.h"
#include "common/settings_enums.h"
#include "frontend_common/settings_generator.h"
#include "qt_common/qt_string_lookup.h"
#include "render/performance_overlay.h"
#include "updater/update_dialog.h"
#if defined(QT_STATICPLUGIN) && !defined(__APPLE__)
#undef VMA_IMPLEMENTATION
#endif

#include "common/fs/ryujinx_compat.h"
#include <fstream>
#include "eden/main_window.h"
#include "network/network.h"
#include "qt_common/discord/discord.h"
#include "ui_main.h"

// Other Yuzu stuff //
#include "debugger/console.h"
#include "debugger/controller.h"

#include "about_dialog.h"
#include "data_dialog.h"
#include "deps_dialog.h"
#include "install_dialog.h"

#include "bootmanager.h"
#include "loading_screen.h"
#include "ryujinx_dialog.h"
#include "set_play_time_dialog.h"
#include "util/util.h"
#include "vk_device_info.h"
#include "eden/game/game_list.h"

#include "applets/qt_amiibo_settings.h"
#include "applets/qt_controller.h"
#include "applets/qt_error.h"
#include "applets/qt_profile_select.h"
#include "applets/qt_software_keyboard.h"
#include "applets/qt_web_browser.h"

#include "configuration/configure_dialog.h"
#include "configuration/configure_input.h"
#include "configuration/configure_per_game.h"
#include "configuration/configure_tas.h"

#include "util/clickable_label.h"
#include "util/controller_navigation.h"
#include "util/overlay_dialog.h"

#include "multiplayer/state.h"

// Qt Stuff //
#define QT_NO_OPENGL

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QStyleHints>
#endif

#include <QActionGroup>
#include <QAbstractButton>
#include <QCheckBox>
#include <QClipboard>
#include <QDesktopServices>
#include <QDateTime>
#include <QFile>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDialog>
#include <QVBoxLayout>
#include <QListWidget>
#include <QScrollBar>
#include <QScrollArea>
#include <QInputDialog>
#include <QMimeData>
#include <QPalette>
#include <QProgressDialog>
#include <QProgressBar>
#include <QGraphicsDropShadowEffect>
#include <QScreen>
#include <QShortcut>
#include <QStatusBar>
#include <QHBoxLayout>
#include <QtConcurrentRun>

// Qt Common //
#include "qt_common/config/shared_translation.h"
#include "qt_common/config/uisettings.h"

#include "qt_common/abstract/frontend.h"

#include "qt_common/qt_common.h"

#include "qt_common/util/content.h"
#include "qt_common/util/fs.h"
#include "qt_common/util/meta.h"
#include "qt_common/util/mod.h"
#include "qt_common/util/path.h"

// These are wrappers to avoid the calls to CreateDirectory and CreateFile because of the Windows
// defines.
static FileSys::VirtualDir VfsFilesystemCreateDirectoryWrapper(const std::string& path,
                                                               FileSys::OpenMode mode) {
    return QtCommon::vfs->CreateDirectory(path, mode);
}

static FileSys::VirtualFile VfsDirectoryCreateFileWrapper(const FileSys::VirtualDir& dir,
                                                          const std::string& path) {
    return dir->CreateFile(path);
}

// Frontend //
#include "frontend_common/play_time_manager.h"

#ifdef ENABLE_UPDATE_CHECKER
#include "frontend_common/update_checker.h"
#endif

// Common //
#include "common/fs/fs.h"
#include "common/logging.h"
#include "common/memory_detect.h"
#include "common/scm_rev.h"
#include "common/scope_exit.h"
#include "common/string_util.h"

#ifdef ARCHITECTURE_x86_64
#include "common/x64/cpu_detect.h"
#endif

// Core //
#include "core/frontend/applets/general.h"
#include "core/frontend/applets/mii_edit.h"
#include "core/frontend/applets/software_keyboard.h"

#include "core/hle/kernel/k_process.h"
#include "core/hle/service/acc/profile_manager.h"
#include "core/hle/service/am/applet_manager.h"
#include "core/hle/service/am/frontend/applet_web_browser_types.h"

#include "core/file_sys/card_image.h"
#include "core/file_sys/romfs.h"
#include "core/file_sys/savedata_factory.h"

#include "core/tools/renderdoc.h"

#include "core/perf_stats.h"

#include "core/crypto/key_manager.h"

// Input //
#include "hid_core/frontend/emulated_controller.h"
#include "hid_core/hid_core.h"
#include "input_common/drivers/virtual_amiibo.h"

// Video Core //
#include "video_core/gpu.h"
#include "video_core/renderer_base.h"
#include "video_core/shader_notify.h"

#include <SDL3/SDL.h>

#include <boost/container/flat_set.hpp>

// Platform stuff //
#include <boost/container/flat_set.hpp>

#ifdef __APPLE__
#include <unistd.h> // for chdir
#endif

#ifdef __unix__

#include <csignal>
#include <QSocketNotifier>
#include <sys/socket.h>
#include "qt_common/gui_settings.h"

#endif

#include "qt_common/gamemode.h"

#ifdef _WIN32
#include "common/windows/timer_resolution.h"
#include "core/core_timing.h"

#include <QPlatformSurfaceEvent>
#include <QSettings>
#include <dwmapi.h>
#include <windows.h>
#ifdef _MSC_VER
#pragma comment(lib, "Dwmapi.lib")
#endif

static inline void ApplyWindowsTitleBarDarkMode(HWND hwnd, bool enabled) {
    if (!hwnd)
        return;
    BOOL val = enabled ? TRUE : FALSE;
    // 20 = Win11/21H2+
    if (SUCCEEDED(DwmSetWindowAttribute(hwnd, 20, &val, sizeof(val))))
        return;
    // 19 = pre-21H2
    DwmSetWindowAttribute(hwnd, 19, &val, sizeof(val));
}

static inline void ApplyModernMicaEffect(QWidget* w) {
    if (!w) return;
    HWND hwnd = reinterpret_cast<HWND>(w->winId());
    if (!hwnd) return;

    DWORD backdrop_type = 2; // DWMSBT_MAINWINDOW (Mica)
    if (w->inherits("QMenu") || w->inherits("QDialog")) {
        backdrop_type = 3; // DWMSBT_TRANSIENTWINDOW (Acrylic)
        
        // Enable drop shadows
        MARGINS margins = {1, 1, 1, 1};
        DwmExtendFrameIntoClientArea(hwnd, &margins);
    }
    
    // Win11 22H2+ Mica/Acrylic
    DwmSetWindowAttribute(hwnd, 38, &backdrop_type, sizeof(backdrop_type));
    // Win11 21H2 Fallback
    BOOL enable = TRUE;
    DwmSetWindowAttribute(hwnd, 1029, &enable, sizeof(enable));

    // Win11 Rounded Corners (DWMWA_WINDOW_CORNER_PREFERENCE)
    DWORD corner = 2; // DWMWCP_ROUND
    DwmSetWindowAttribute(hwnd, 33, &corner, sizeof(corner));
}

static inline void ApplyDarkToTopLevel(QWidget* w, bool on) {
    if (!w || !w->isWindow())
        return;
    ApplyWindowsTitleBarDarkMode(reinterpret_cast<HWND>(w->winId()), on);
    ApplyModernMicaEffect(w);
}

namespace {
struct TitlebarFilter final : QObject {
    bool dark;
    explicit TitlebarFilter(bool is_dark) : QObject(qApp), dark(is_dark) {}

    void setDark(bool is_dark) {
        dark = is_dark;
    }

    void onFocusChanged(QWidget*, QWidget* now) {
        if (now)
            ApplyDarkToTopLevel(now->window(), dark);
    }

    bool eventFilter(QObject* obj, QEvent* ev) override {
        if (auto* w = qobject_cast<QWidget*>(obj)) {
            switch (ev->type()) {
            case QEvent::WinIdChange:
            case QEvent::Show:
            case QEvent::ShowToParent:
            case QEvent::WindowStateChange:
            case QEvent::ZOrderChange:
                ApplyDarkToTopLevel(w, dark);
                break;
            default:
                break;
            }
        }
        return QObject::eventFilter(obj, ev);
    }
};

static TitlebarFilter* g_filter = nullptr;
static QMetaObject::Connection g_focusConn;

} // namespace

static void ApplyGlobalDarkTitlebar(bool is_dark) {
    if (!g_filter) {
        g_filter = new TitlebarFilter(is_dark);
        qApp->installEventFilter(g_filter);
        g_focusConn = QObject::connect(qApp, &QApplication::focusChanged, g_filter,
                                       &TitlebarFilter::onFocusChanged);
    } else {
        g_filter->setDark(is_dark);
    }
    for (QWidget* w : QApplication::topLevelWidgets())
        ApplyDarkToTopLevel(w, is_dark);
}

static void RemoveTitlebarFilter() {
    if (!g_filter)
        return;
    qApp->removeEventFilter(g_filter);
    QObject::disconnect(g_focusConn);
    g_filter->deleteLater();
    g_filter = nullptr;
}

#endif

#ifdef YUZU_CRASH_DUMPS
#include "eden/breakpad.h"
#endif

using namespace Common::Literals;

#include "qt_common/discord/discord.h"

#ifdef USE_DISCORD_PRESENCE
#include "qt_common/discord/discord_impl.h"
#endif

#ifdef QT_STATICPLUGIN
#include <QtPlugin>

#if defined(_WIN32)
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
#elif defined(__APPLE__)
Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin)
#endif

#endif

#ifdef _WIN32
#include <windows.h>
extern "C" {
// tells Nvidia and AMD drivers to use the dedicated GPU by default on laptops with switchable
// graphics
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

constexpr int default_mouse_hide_timeout = 2500;
constexpr int default_input_update_timeout = 1;

constexpr size_t CopyBufferSize = 1_MiB;

/**
 * "Callouts" are one-time instructional messages shown to the user. In the config settings, there
 * is a bitfield "callout_flags" options, used to track if a message has already been shown to the
 * user. This is 32-bits - if we have more than 32 callouts, we should retire and recycle old ones.
 */
enum class CalloutFlag : uint32_t {
    DRDDeprecation = 0x2,
};

const int MainWindow::max_recent_files_item;

static void RemoveCachedContents() {
    const auto cache_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::CacheDir);
    const auto offline_fonts = cache_dir / "fonts";
    const auto offline_manual = cache_dir / "offline_web_applet_manual";
    const auto offline_legal_information = cache_dir / "offline_web_applet_legal_information";
    const auto offline_system_data = cache_dir / "offline_web_applet_system_data";

    Common::FS::RemoveDirRecursively(offline_fonts);
    Common::FS::RemoveDirRecursively(offline_manual);
    Common::FS::RemoveDirRecursively(offline_legal_information);
    Common::FS::RemoveDirRecursively(offline_system_data);
}

static void LogRuntimes() {
#ifdef _MSC_VER
    // It is possible that the name of the dll will change.
    // vcruntime140.dll is for 2015 and onwards
    static constexpr char runtime_dll_name[] = "vcruntime140.dll";
    UINT sz = GetFileVersionInfoSizeA(runtime_dll_name, nullptr);
    bool runtime_version_inspection_worked = false;
    if (sz > 0) {
        std::vector<u8> buf(sz);
        if (GetFileVersionInfoA(runtime_dll_name, 0, sz, buf.data())) {
            VS_FIXEDFILEINFO* pvi;
            sz = sizeof(VS_FIXEDFILEINFO);
            if (VerQueryValueA(buf.data(), "\\", reinterpret_cast<LPVOID*>(&pvi), &sz)) {
                if (pvi->dwSignature == VS_FFI_SIGNATURE) {
                    runtime_version_inspection_worked = true;
                    LOG_INFO(Frontend, "MSVC Compiler: {} Runtime: {}.{}.{}.{}", _MSC_VER,
                             pvi->dwProductVersionMS >> 16, pvi->dwProductVersionMS & 0xFFFF,
                             pvi->dwProductVersionLS >> 16, pvi->dwProductVersionLS & 0xFFFF);
                }
            }
        }
    }
    if (!runtime_version_inspection_worked) {
        LOG_INFO(Frontend, "Unable to inspect {}", runtime_dll_name);
    }
#endif
    LOG_INFO(Frontend, "Qt Compile: {} Runtime: {}", QT_VERSION_STR, qVersion());
}

static QString PrettyProductName() {
#ifdef _WIN32
    // After Windows 10 Version 2004, Microsoft decided to switch to a different notation: 20H2
    // With that notation change they changed the registry key used to denote the current version
    QSettings windows_registry(
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"),
        QSettings::NativeFormat);
    const QString release_id = windows_registry.value(QStringLiteral("ReleaseId")).toString();
    if (release_id == QStringLiteral("2009")) {
        const u32 current_build = windows_registry.value(QStringLiteral("CurrentBuild")).toUInt();
        const QString display_version =
            windows_registry.value(QStringLiteral("DisplayVersion")).toString();
        const u32 ubr = windows_registry.value(QStringLiteral("UBR")).toUInt();
        u32 version = 10;
        if (current_build >= 22000) {
            version = 11;
        }
        return QStringLiteral("Windows %1 Version %2 (Build %3.%4)")
            .arg(QString::number(version), display_version, QString::number(current_build),
                 QString::number(ubr));
    }
#endif
    return QSysInfo::prettyProductName();
}

namespace {

constexpr std::array<std::pair<u32, const char*>, 5> default_game_icon_sizes{
    std::make_pair(0, QT_TRANSLATE_NOOP("MainWindow", "None")),
    std::make_pair(32, QT_TRANSLATE_NOOP("MainWindow", "Small (32x32)")),
    std::make_pair(64, QT_TRANSLATE_NOOP("MainWindow", "Standard (64x64)")),
    std::make_pair(128, QT_TRANSLATE_NOOP("MainWindow", "Large (128x128)")),
    std::make_pair(256, QT_TRANSLATE_NOOP("MainWindow", "Full Size (256x256)")),
};

QString GetTranslatedGameIconSize(size_t index) {
    return QCoreApplication::translate("MainWindow", default_game_icon_sizes[index].second);
}

} // namespace

#ifndef _WIN32
// TODO(crueter): carboxyl does this, is it needed in qml?
inline static bool isDarkMode() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    const auto scheme = QGuiApplication::styleHints()->colorScheme();
    return scheme == Qt::ColorScheme::Dark;
#else
    const QPalette defaultPalette;
    const auto text = defaultPalette.color(QPalette::WindowText);
    const auto window = defaultPalette.color(QPalette::Window);
    return text.lightness() > window.lightness();
#endif // QT_VERSION
}
#endif // _WIN32

MainWindow::MainWindow(bool has_broken_vulkan)
    : ui{std::make_unique<Ui::MainWindow>()},
      input_subsystem{std::make_shared<InputCommon::InputSubsystem>()}, user_data_migrator{this} {
    input_subsystem->Initialize();
    QtCommon::Init(this);

    Common::FS::CreateEdenPaths();
    {
        std::ofstream df("debug_log.txt", std::ios::app);
        df << "MW: Before config instantiation\n"; df.flush();
    }
    this->config = std::make_unique<QtConfig>();
    {
        std::ofstream df("debug_log.txt", std::ios::app);
        df << "MW: After config instantiation\n"; df.flush();
    }

    if (user_data_migrator.migrated) {
        // Sort-of hack whereby we only move the old dir if it's a subfolder of the user dir

        using namespace Common::FS;

        static constexpr const std::array<const EdenPath, 4> paths = {
            EdenPath::NANDDir, EdenPath::SDMCDir, EdenPath::DumpDir, EdenPath::LoadDir};

        for (const EdenPath& path : paths) {
            std::string str_path = Common::FS::GetEdenPathString(path);
            if (str_path.starts_with(user_data_migrator.selected_emu.get_user_dir())) {
                str_path = Common::ReplaceAll(
                    str_path, user_data_migrator.selected_emu.lower_name().toStdString(), "eden");
                Common::FS::SetEdenPath(path, str_path);
            }
        }
    }

#ifdef __unix__
    SetupSigInterrupts();
#endif

    SetGamemodeEnabled(UISettings::values.enable_gamemode.GetValue());

    UISettings::RestoreWindowState(config);

    {
        std::ofstream df("debug_log.txt", std::ios::app);
        df << "MW: Before system->Initialize()\n"; df.flush();
    }
    QtCommon::system->Initialize();
    {
        std::ofstream df("debug_log.txt", std::ios::app);
        df << "MW: After system->Initialize(), before Log::Initialize()\n"; df.flush();
    }

    Common::Log::Initialize();
    {
        std::ofstream df("debug_log.txt", std::ios::app);
        df << "MW: After Log::Initialize(), before Log::Start()\n"; df.flush();
    }
    Common::Log::Start();
    {
        std::ofstream df("debug_log.txt", std::ios::app);
        df << "MW: After Log::Start()\n"; df.flush();
    }
    
    {
        std::ofstream df("debug_log.txt", std::ios::app);
        df << "MW: Before LoadTranslation()\n"; df.flush();
    }
    LoadTranslation();
    {
        std::ofstream df("debug_log.txt", std::ios::app);
        df << "MW: After LoadTranslation()\n"; df.flush();
    }
    FrontendCommon::GenerateSettings();
    {
        std::ofstream df("debug_log.txt", std::ios::app);
        df << "MW: After GenerateSettings()\n"; df.flush();
    }

    setAcceptDrops(true);
    ui->setupUi(this);
    {
        std::ofstream df("debug_log.txt", std::ios::app);
        df << "MW: After setupUi()\n"; df.flush();
    }
    statusBar()->hide();

    startup_icon_theme = QIcon::themeName();
    // fallback can only be set once, colorful theme icons are okay on both light/dark
    QIcon::setFallbackThemeName(QStringLiteral("colorful"));
    QIcon::setFallbackSearchPaths(QStringList(QStringLiteral(":/icons")));

    default_theme_paths = QIcon::themeSearchPaths();
    UpdateUITheme();

    {
        std::ofstream df("debug_log.txt", std::ios::app);
        df << "MW: Before SetDiscordEnabled()\n"; df.flush();
    }
    SetDiscordEnabled(UISettings::values.enable_discord_presence.GetValue());
    {
        std::ofstream df("debug_log.txt", std::ios::app);
        df << "MW: After SetDiscordEnabled(), before discord_rpc->Update()\n"; df.flush();
    }
    discord_rpc->Update();
    {
        std::ofstream df("debug_log.txt", std::ios::app);
        df << "MW: After discord_rpc->Update(), before play_time_manager\n"; df.flush();
    }

    play_time_manager = std::make_unique<PlayTime::PlayTimeManager>();
    {
        std::ofstream df("debug_log.txt", std::ios::app);
        df << "MW: After play_time_manager, before Network::Init()\n"; df.flush();
    }

    {
        std::ofstream df("debug_log.txt", std::ios::app);
        df << "MW: Before Network::Init()\n"; df.flush();
    }
    Network::Init();
    {
        std::ofstream df("debug_log.txt", std::ios::app);
        df << "MW: After Network::Init()\n"; df.flush();
    }

    QtCommon::Meta::RegisterMetaTypes();
    {
        std::ofstream df("debug_log.txt", std::ios::app);
        df << "MW: After RegisterMetaTypes()\n"; df.flush();
    }

    InitializeWidgets();
    {
        std::ofstream df("debug_log.txt", std::ios::app);
        df << "MW: After InitializeWidgets()\n"; df.flush();
    }
    InitializeDebugWidgets();
    {
        std::ofstream df("debug_log.txt", std::ios::app);
        df << "MW: After InitializeDebugWidgets()\n"; df.flush();
    }
    InitializeRecentFileMenuActions();
    {
        std::ofstream df("debug_log.txt", std::ios::app);
        df << "MW: After InitializeRecentFileMenuActions()\n"; df.flush();
    }
    InitializeHotkeys();
    {
        std::ofstream df("debug_log.txt", std::ios::app);
        df << "MW: After InitializeHotkeys()\n"; df.flush();
    }

    SetDefaultUIGeometry();
    {
        std::ofstream df("debug_log.txt", std::ios::app);
        df << "MW: After SetDefaultUIGeometry()\n"; df.flush();
    }
    RestoreUIState();
    {
        std::ofstream df("debug_log.txt", std::ios::app);
        df << "MW: After RestoreUIState()\n"; df.flush();
    }

    ConnectMenuEvents();
    {
        std::ofstream df("debug_log.txt", std::ios::app);
        df << "MW: After ConnectMenuEvents()\n"; df.flush();
    }
    ConnectWidgetEvents();
    {
        std::ofstream df("debug_log.txt", std::ios::app);
        df << "MW: After ConnectWidgetEvents()\n"; df.flush();
    }

    input_subsystem->ReloadInputDevices();
    QtCommon::system->HIDCore().ReloadInputDevices();
    controller_dialog->refreshConfiguration();

    const auto branch_name = std::string(Common::g_scm_branch);
    const auto description = std::string(Common::g_scm_desc);
    const auto build_id = std::string(Common::g_build_id);

    const auto yuzu_build = fmt::format("Eden Development Build | {}-{}", branch_name, description);
    const auto override_build =
        fmt::format(fmt::runtime(std::string(Common::g_title_bar_format_idle)), build_id);
    const auto yuzu_build_version = override_build.empty() ? yuzu_build : override_build;
    const auto processor_count = std::thread::hardware_concurrency();

    LOG_INFO(Frontend, "Eden Version: {}", yuzu_build_version);
    LogRuntimes();
#ifdef ARCHITECTURE_x86_64
    const auto& caps = Common::GetCPUCaps();
    std::string cpu_string = caps.cpu_string;
    if (caps.avx || caps.avx2 || caps.avx512f) {
        cpu_string += " | AVX";
        if (caps.avx512f) {
            cpu_string += "512";
        } else if (caps.avx2) {
            cpu_string += '2';
        }
        if (caps.fma || caps.fma4) {
            cpu_string += " | FMA";
        }
    }
    LOG_INFO(Frontend, "Host CPU: {}", cpu_string);
    if (std::optional<int> processor_core = Common::GetProcessorCount()) {
        LOG_INFO(Frontend, "Host CPU Cores: {}", *processor_core);
    }
#endif
    LOG_INFO(Frontend, "Host CPU Threads: {}", processor_count);
    LOG_INFO(Frontend, "Host OS: {}", PrettyProductName().toStdString());
    LOG_INFO(Frontend, "Host RAM: {:.2f} GiB",
             Common::GetMemInfo().TotalPhysicalMemory / f64{1_GiB});
    LOG_INFO(Frontend, "Host Swap: {:.2f} GiB", Common::GetMemInfo().TotalSwapMemory / f64{1_GiB});
#ifdef _WIN32
    LOG_INFO(Frontend, "Host Timer Resolution: {:.4f} ms",
             std::chrono::duration_cast<std::chrono::duration<f64, std::milli>>(
                 Common::Windows::SetCurrentTimerResolutionToMaximum())
                 .count());
    QtCommon::system->CoreTiming().SetTimerResolutionNs(
        Common::Windows::GetCurrentTimerResolution());
#endif
    UpdateWindowTitle();

    show();

#ifdef ENABLE_UPDATE_CHECKER
    if (UISettings::values.check_for_updates) {
        update_future = QtConcurrent::run(
            []() -> std::optional<Common::Net::Release> { return UpdateChecker::GetUpdate(); });
        update_watcher.connect(&update_watcher, &QFutureWatcher<QString>::finished, this,
                               &MainWindow::OnEmulatorUpdateAvailable);
        update_watcher.setFuture(update_future);
    }
#endif

    QtCommon::system->SetContentProvider(std::make_unique<FileSys::ContentProviderUnion>());
    QtCommon::system->RegisterContentProvider(FileSys::ContentProviderUnionSlot::FrontendManual,
                                              QtCommon::provider.get());
    QtCommon::system->GetFileSystemController().CreateFactories(*QtCommon::vfs);

    // Remove cached contents generated during the previous session
    RemoveCachedContents();

    // Gen keys if necessary
    OnCheckFirmwareDecryption();

#ifdef __unix__
    OnCheckGraphicsBackend();
#endif

    // Check for orphaned profiles and reset profile data if necessary
    QtCommon::Content::FixProfiles();

    if (Settings::values.use_dev_keys.GetValue()) {
        Core::Crypto::KeyManager::Instance().ReloadKeys();
    }
    game_list->LoadCompatibilityList();
    game_list->PopulateAsync(UISettings::values.game_dirs);

    // Set up game list mode checkboxes.
    SetGameListMode(UISettings::values.game_list_mode.GetValue());

    // make sure menubar has the arrow cursor instead of inheriting from this
    ui->menubar->setCursor(QCursor());
    statusBar()->setCursor(QCursor());

    mouse_hide_timer.setInterval(default_mouse_hide_timeout);
    connect(&mouse_hide_timer, &QTimer::timeout, this, &MainWindow::HideMouseCursor);
    connect(ui->menubar, &QMenuBar::hovered, this, &MainWindow::ShowMouseCursor);

    update_input_timer.setInterval(default_input_update_timeout);
    connect(&update_input_timer, &QTimer::timeout, this, &MainWindow::UpdateInputDrivers);
    update_input_timer.start();

    if (has_broken_vulkan) {
        UISettings::values.has_broken_vulkan = true;

        QMessageBox::warning(this, tr("Broken Vulkan Installation Detected"),
                             tr("Vulkan initialization failed during boot."));
#ifdef HAS_OPENGL
        Settings::values.renderer_backend = Settings::RendererBackend::OpenGL_GLSL;
#else
        Settings::values.renderer_backend = Settings::RendererBackend::Null;
#endif

        UpdateAPIText();
        renderer_status_button->setDisabled(true);
        renderer_status_button->setChecked(false);
    } else {
        VkDeviceInfo::PopulateRecords(vk_device_records, this->window()->windowHandle());
    }

#if !defined(_WIN32)
    SDL_InitSubSystem(SDL_INIT_VIDEO);

    // Set a screensaver inhibition reason string. Currently passed to DBus by SDL and visible to
    // the user through their desktop environment.
    //: TRANSLATORS: This string is shown to the user to explain why yuzu needs to prevent the
    //: computer from sleeping
    QByteArray wakelock_reason = tr("Running a game").toUtf8();
    SDL_SetHint(SDL_HINT_SCREENSAVER_INHIBIT_ACTIVITY_NAME, wakelock_reason.data());

    // SDL disables the screen saver by default, and setting the hint
    // SDL_HINT_VIDEO_ALLOW_SCREENSAVER doesn't seem to work, so we just enable the screen saver
    // for now.
    SDL_EnableScreenSaver();
#endif

    SetupPrepareForSleep();

    // Some moron added a race condition to the status bar
    // so now we have to make this completely unnecessary call
    // to prevent the UI from blowing up.
    UpdateUITheme();

    QStringList args = QApplication::arguments();

    if (args.size() < 2) {
        return;
    }

    QString game_path;
    bool should_launch_qlaunch = false;
    bool should_launch_setup = false;
    bool has_gamepath = false;
    bool is_fullscreen = false;

    // Preserves drag/drop functionality
    for (int i = 1; i < args.size(); ++i) {
        if (args[i] == QStringLiteral("-f")) {
            // Launch game in fullscreen mode
            is_fullscreen = true;
        } else if (args[i] == QStringLiteral("-u") && i < args.size() - 1) {
            // Launch game with a specific user
            int user_arg_idx = ++i;
            bool argument_ok;
            std::size_t selected_user = args[user_arg_idx].toUInt(&argument_ok);
            if (!argument_ok) {
                // try to look it up by username, only finds the first username that matches.
                std::string const user_arg_str = args[user_arg_idx].toStdString();
                auto const user_idx =
                    QtCommon::system->GetProfileManager().GetUserIndex(user_arg_str);
                if (user_idx != std::nullopt) {
                    selected_user = user_idx.value();
                } else {
                    LOG_ERROR(Frontend, "Invalid user argument '{}'", user_arg_str);
                    continue;
                }
            }
            if (QtCommon::system->GetProfileManager().UserExistsIndex(selected_user)) {
                Settings::values.current_user = s32(selected_user);
                user_flag_cmd_line = true;
            } else {
                LOG_ERROR(Frontend, "Selected user {} doesn't exist", selected_user);
            }
        } else if (args[i] == QStringLiteral("-g") && i < args.size() - 1) {
            // Launch game at path
            game_path = args[++i];
            has_gamepath = true;
        } else if (args[i] == QStringLiteral("-input-profile") && i < args.size() - 1) {
            auto& players = Settings::values.players.GetValue();
            players[0].profile_name = args[++i].toStdString();
        } else if (args[i] == QStringLiteral("-qlaunch")) {
            should_launch_qlaunch = true;
        } else if (args[i] == QStringLiteral("-setup")) {
            should_launch_setup = true;
        } else {
            game_path = args[i];
            has_gamepath = true;
        }
    }

    // Override fullscreen setting if gamepath or argument is provided
    if (has_gamepath || is_fullscreen) {
        ui->action_Fullscreen->setChecked(is_fullscreen);
    }

    if (should_launch_setup) {
        LaunchFirmwareApplet(u64(Service::AM::AppletProgramId::Starter), std::nullopt);
    } else {
        if (!game_path.isEmpty()) {
            BootGame(game_path, ApplicationAppletParameters());
        } else {
            if (should_launch_qlaunch) {
                LaunchFirmwareApplet(u64(Service::AM::AppletProgramId::QLaunch), std::nullopt);
            }
        }
    }
}

MainWindow::~MainWindow() {
    // will get automatically deleted otherwise
    if (render_window->parent() == nullptr) {
        delete render_window;
    }

#ifdef __unix__
    ::close(sig_interrupt_fds[0]);
    ::close(sig_interrupt_fds[1]);
#endif
}

void MainWindow::AmiiboSettingsShowDialog(const Core::Frontend::CabinetParameters& parameters,
                                          std::shared_ptr<Service::NFC::NfcDevice> nfp_device) {
    cabinet_applet =
        new QtAmiiboSettingsDialog(this, parameters, input_subsystem.get(), nfp_device);
    SCOPE_EXIT {
        cabinet_applet->deleteLater();
        cabinet_applet = nullptr;
    };

    cabinet_applet->setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowStaysOnTopHint |
                                   Qt::WindowTitleHint | Qt::WindowSystemMenuHint);
    cabinet_applet->setWindowModality(Qt::WindowModal);

    if (cabinet_applet->exec() == QDialog::Rejected) {
        emit AmiiboSettingsFinished(false, {});
        return;
    }

    emit AmiiboSettingsFinished(true, cabinet_applet->GetName());
}

void MainWindow::AmiiboSettingsRequestExit() {
    if (cabinet_applet) {
        cabinet_applet->reject();
    }
}

void MainWindow::ControllerSelectorReconfigureControllers(
    const Core::Frontend::ControllerParameters& parameters) {
    controller_applet =
        new QtControllerSelectorDialog(this, parameters, input_subsystem.get(), *QtCommon::system);
    SCOPE_EXIT {
        controller_applet->deleteLater();
        controller_applet = nullptr;
    };

    controller_applet->setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint |
                                      Qt::WindowStaysOnTopHint | Qt::WindowTitleHint |
                                      Qt::WindowSystemMenuHint);
    controller_applet->setWindowModality(Qt::WindowModal);
    bool is_success = controller_applet->exec() != QDialog::Rejected;

    // Don't forget to apply settings.
    QtCommon::system->HIDCore().DisableAllControllerConfiguration();
    QtCommon::system->ApplySettings();
    config->SaveAllValues();

    UpdateStatusButtons();

    emit ControllerSelectorReconfigureFinished(is_success);
}

void MainWindow::ControllerSelectorRequestExit() {
    if (controller_applet) {
        controller_applet->reject();
    }
}

void MainWindow::ProfileSelectorSelectProfile(
    const Core::Frontend::ProfileSelectParameters& parameters) {
    profile_select_applet = new QtProfileSelectionDialog(*QtCommon::system, this, parameters);
    SCOPE_EXIT {
        profile_select_applet->deleteLater();
        profile_select_applet = nullptr;
    };

    profile_select_applet->setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint |
                                          Qt::WindowStaysOnTopHint | Qt::WindowTitleHint |
                                          Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
    profile_select_applet->setWindowModality(Qt::WindowModal);
    if (profile_select_applet->exec() == QDialog::Rejected) {
        emit ProfileSelectorFinishedSelection(std::nullopt);
        return;
    }

    const auto uuid = QtCommon::system->GetProfileManager().GetUser(
        static_cast<std::size_t>(profile_select_applet->GetIndex()));
    if (!uuid.has_value()) {
        emit ProfileSelectorFinishedSelection(std::nullopt);
        return;
    }

    emit ProfileSelectorFinishedSelection(uuid);
}

void MainWindow::ProfileSelectorRequestExit() {
    if (profile_select_applet) {
        profile_select_applet->reject();
    }
}

void MainWindow::SoftwareKeyboardInitialize(
    bool is_inline, Core::Frontend::KeyboardInitializeParameters initialize_parameters) {
    if (software_keyboard) {
        LOG_ERROR(Frontend, "The software keyboard is already initialized!");
        return;
    }

    software_keyboard = new QtSoftwareKeyboardDialog(render_window, *QtCommon::system, is_inline,
                                                     std::move(initialize_parameters));

    if (is_inline) {
        connect(
            software_keyboard, &QtSoftwareKeyboardDialog::SubmitInlineText, this,
            [this](Service::AM::Frontend::SwkbdReplyType reply_type, std::u16string submitted_text,
                   s32 cursor_position) {
                emit SoftwareKeyboardSubmitInlineText(reply_type, submitted_text, cursor_position);
            },
            Qt::QueuedConnection);
    } else {
        connect(
            software_keyboard, &QtSoftwareKeyboardDialog::SubmitNormalText, this,
            [this](Service::AM::Frontend::SwkbdResult result, std::u16string submitted_text,
                   bool confirmed) {
                emit SoftwareKeyboardSubmitNormalText(result, submitted_text, confirmed);
            },
            Qt::QueuedConnection);
    }
}

void MainWindow::SoftwareKeyboardShowNormal() {
    if (!software_keyboard) {
        LOG_ERROR(Frontend, "The software keyboard is not initialized!");
        return;
    }

    const auto& layout = render_window->GetFramebufferLayout();

    const auto x = layout.screen.left;
    const auto y = layout.screen.top;
    const auto w = layout.screen.GetWidth();
    const auto h = layout.screen.GetHeight();
    const auto scale_ratio = devicePixelRatioF();

    software_keyboard->ShowNormalKeyboard(render_window->mapToGlobal(QPoint(x, y) / scale_ratio),
                                          QSize(w, h) / scale_ratio);
}

void MainWindow::SoftwareKeyboardShowTextCheck(
    Service::AM::Frontend::SwkbdTextCheckResult text_check_result,
    std::u16string text_check_message) {
    if (!software_keyboard) {
        LOG_ERROR(Frontend, "The software keyboard is not initialized!");
        return;
    }

    software_keyboard->ShowTextCheckDialog(text_check_result, text_check_message);
}

void MainWindow::SoftwareKeyboardShowInline(
    Core::Frontend::InlineAppearParameters appear_parameters) {
    if (!software_keyboard) {
        LOG_ERROR(Frontend, "The software keyboard is not initialized!");
        return;
    }

    const auto& layout = render_window->GetFramebufferLayout();

    const auto x =
        static_cast<int>(layout.screen.left + (0.5f * layout.screen.GetWidth() *
                                               ((2.0f * appear_parameters.key_top_translate_x) +
                                                (1.0f - appear_parameters.key_top_scale_x))));
    const auto y =
        static_cast<int>(layout.screen.top + (layout.screen.GetHeight() *
                                              ((2.0f * appear_parameters.key_top_translate_y) +
                                               (1.0f - appear_parameters.key_top_scale_y))));
    const auto w = static_cast<int>(layout.screen.GetWidth() * appear_parameters.key_top_scale_x);
    const auto h = static_cast<int>(layout.screen.GetHeight() * appear_parameters.key_top_scale_y);
    const auto scale_ratio = devicePixelRatioF();

    software_keyboard->ShowInlineKeyboard(std::move(appear_parameters),
                                          render_window->mapToGlobal(QPoint(x, y) / scale_ratio),
                                          QSize(w, h) / scale_ratio);
}

void MainWindow::SoftwareKeyboardHideInline() {
    if (!software_keyboard) {
        LOG_ERROR(Frontend, "The software keyboard is not initialized!");
        return;
    }

    software_keyboard->HideInlineKeyboard();
}

void MainWindow::SoftwareKeyboardInlineTextChanged(
    Core::Frontend::InlineTextParameters text_parameters) {
    if (!software_keyboard) {
        LOG_ERROR(Frontend, "The software keyboard is not initialized!");
        return;
    }

    software_keyboard->InlineTextChanged(std::move(text_parameters));
}

void MainWindow::SoftwareKeyboardExit() {
    if (!software_keyboard) {
        return;
    }

    software_keyboard->ExitKeyboard();

    software_keyboard = nullptr;
}

void MainWindow::WebBrowserOpenWebPage(const std::string& main_url,
                                       const std::string& additional_args, bool is_local) {
#ifdef YUZU_USE_QT_WEB_ENGINE

    // Raw input breaks with the web applet, Disable web applets if enabled
    if (Settings::values.disable_web_applet || Settings::values.enable_raw_input) {
        emit WebBrowserClosed(Service::AM::Frontend::WebExitReason::WindowClosed,
                              "http://localhost/");
        return;
    }

    web_applet = new QtNXWebEngineView(this, *QtCommon::system, input_subsystem.get());

    ui->action_Pause->setEnabled(false);
    ui->action_Restart->setEnabled(false);
    ui->action_Stop->setEnabled(false);

    {
        QProgressDialog loading_progress(this);
        loading_progress.setLabelText(tr("Loading Web Applet..."));
        loading_progress.setRange(0, 3);
        loading_progress.setValue(0);

        if (is_local && !Common::FS::Exists(main_url)) {
            loading_progress.show();

            auto future = QtConcurrent::run([this] { emit WebBrowserExtractOfflineRomFS(); });

            while (!future.isFinished()) {
                QCoreApplication::processEvents();

                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        loading_progress.setValue(1);

        if (is_local) {
            web_applet->LoadLocalWebPage(main_url, additional_args);
        } else {
            web_applet->LoadExternalWebPage(main_url, additional_args);
        }

        if (render_window->IsLoadingComplete()) {
            render_window->hide();
        }

        const auto& layout = render_window->GetFramebufferLayout();
        const auto scale_ratio = devicePixelRatioF();
        web_applet->resize(layout.screen.GetWidth() / scale_ratio,
                           layout.screen.GetHeight() / scale_ratio);
        web_applet->move(layout.screen.left / scale_ratio,
                         (layout.screen.top / scale_ratio) + menuBar()->height());
        web_applet->setZoomFactor(static_cast<qreal>(layout.screen.GetWidth() / scale_ratio) /
                                  static_cast<qreal>(Layout::ScreenUndocked::Width));

        web_applet->setFocus();
        web_applet->show();

        loading_progress.setValue(2);

        QCoreApplication::processEvents();

        loading_progress.setValue(3);
    }

    bool exit_check = false;

    // TODO (Morph): Remove this
    QAction* exit_action = new QAction(tr("Disable Web Applet"), this);
    connect(exit_action, &QAction::triggered, this, [this] {
        const auto result = QMessageBox::warning(
            this, tr("Disable Web Applet"),
            tr("Disabling the web applet can lead to undefined behavior and should only be used "
               "with Super Mario 3D All-Stars. Are you sure you want to disable the web "
               "applet?\n(This can be re-enabled in the Debug settings.)"),
            QMessageBox::Yes | QMessageBox::No);
        if (result == QMessageBox::Yes) {
            Settings::values.disable_web_applet = true;
            web_applet->SetFinished(true);
        }
    });
    ui->menubar->addAction(exit_action);

    while (!web_applet->IsFinished()) {
        QCoreApplication::processEvents();

        if (!exit_check) {
            web_applet->page()->runJavaScript(
                QStringLiteral("end_applet;"), [&](const QVariant& variant) {
                    exit_check = false;
                    if (variant.toBool()) {
                        web_applet->SetFinished(true);
                        web_applet->SetExitReason(
                            Service::AM::Frontend::WebExitReason::EndButtonPressed);
                    }
                });

            exit_check = true;
        }

        if (web_applet->GetCurrentURL().contains(QStringLiteral("localhost"))) {
            if (!web_applet->IsFinished()) {
                web_applet->SetFinished(true);
                web_applet->SetExitReason(Service::AM::Frontend::WebExitReason::CallbackURL);
            }

            web_applet->SetLastURL(web_applet->GetCurrentURL().toStdString());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    const auto exit_reason = web_applet->GetExitReason();
    const auto last_url = web_applet->GetLastURL();

    web_applet->hide();

    render_window->setFocus();

    if (render_window->IsLoadingComplete()) {
        render_window->show();
    }

    ui->action_Pause->setEnabled(true);
    ui->action_Restart->setEnabled(true);
    ui->action_Stop->setEnabled(true);

    ui->menubar->removeAction(exit_action);

    QCoreApplication::processEvents();

    emit WebBrowserClosed(exit_reason, last_url);

#else

    // Utilize the same fallback as the default web browser applet.
    emit WebBrowserClosed(Service::AM::Frontend::WebExitReason::WindowClosed, "http://localhost/");

#endif
}

void MainWindow::WebBrowserRequestExit() {
#ifdef YUZU_USE_QT_WEB_ENGINE
    if (web_applet) {
        web_applet->SetExitReason(Service::AM::Frontend::WebExitReason::ExitRequested);
        web_applet->SetFinished(true);
    }
#endif
}

void MainWindow::InitializeWidgets() {
DumpSettingsInfo("main_window.cpp");
DumpSettingsInfo("main_window.cpp");
    {
        std::ofstream df("debug_log.txt", std::ios::app);
        df << "MW::InitWidgets: Start\n"; df.flush();
    }
#ifdef YUZU_ENABLE_COMPATIBILITY_REPORTING
    ui->action_Report_Compatibility->setVisible(true);
#endif
    {
        std::ofstream df("debug_log.txt", std::ios::app);
        df << "MW::InitWidgets: Before GRenderWindow\n"; df.flush();
    }
    render_window = new GRenderWindow(this, emu_thread.get(), input_subsystem, *QtCommon::system);
    render_window->hide();
    {
        std::ofstream df("debug_log.txt", std::ios::app);
        df << "MW::InitWidgets: After GRenderWindow\n"; 
        df << "MW::InitWidgets: checking pointers: vfs=" << QtCommon::vfs.get()
           << ", provider=" << QtCommon::provider.get()
           << ", play_time_manager=" << play_time_manager.get()
           << ", system=" << QtCommon::system.get() << "\n";
        df.flush();
    }

    game_list = new GameList(QtCommon::vfs, QtCommon::provider.get(), *play_time_manager,
                             *QtCommon::system, this);
    ui->horizontalLayout->addWidget(game_list);
    {
        std::ofstream df("debug_log.txt", std::ios::app);
        df << "MW::InitWidgets: After GameList\n"; df.flush();
    }

    game_list_placeholder = new GameListPlaceholder(this);
    ui->horizontalLayout->addWidget(game_list_placeholder);
    game_list_placeholder->setVisible(false);
    {
        std::ofstream df("debug_log.txt", std::ios::app);
        df << "MW::InitWidgets: After GameListPlaceholder\n"; df.flush();
    }

    loading_screen = new LoadingScreen(ui->centralwidget);
    loading_screen->hide();
    connect(loading_screen, &LoadingScreen::Hidden, this, [&] {
        loading_screen->hide();
        loading_screen->Clear();
    });
    connect(render_window, &GRenderWindow::FirstFrameDisplayed, loading_screen,
            &LoadingScreen::OnLoadComplete, Qt::QueuedConnection);
    {
        std::ofstream df("debug_log.txt", std::ios::app);
        df << "MW::InitWidgets: After LoadingScreen\n"; df.flush();
    }

    multiplayer_state = new MultiplayerState(this, game_list->GetModel(), ui->action_Leave_Room,
                                             ui->action_Show_Room, *QtCommon::system);
    multiplayer_state->setVisible(false);
    {
        std::ofstream df("debug_log.txt", std::ios::app);
        df << "MW::InitWidgets: After MultiplayerState\n"; df.flush();
    }

    // Create status bar
    message_label = new QLabel();
    {
        std::ofstream df("debug_log.txt", std::ios::app);
        df << "MW::InitWidgets: After message_label\n"; df.flush();
    }
    // Configured separately for left alignment
    message_label->setFrameStyle(QFrame::NoFrame);
    message_label->setContentsMargins(4, 0, 4, 0);
    message_label->setAlignment(Qt::AlignLeft);
    statusBar()->addPermanentWidget(message_label, 1);

    shader_building_label = new QLabel();
    shader_building_label->setObjectName(QStringLiteral("ShaderBuildingLabel"));
    shader_building_label->setToolTip(tr("The amount of shaders currently being built"));
    res_scale_label = new QPushButton();
    res_scale_label->setObjectName(QStringLiteral("ResScaleButton"));
    res_scale_label->setToolTip(tr("The current selected resolution scaling multiplier."));
    res_scale_label->setFlat(true);
    emu_speed_label = new QLabel();
    emu_speed_label->setObjectName(QStringLiteral("EmuSpeedLabel"));
    emu_speed_label->setToolTip(
        tr("Current emulation speed. Values higher or lower than 100% "
           "indicate emulation is running faster or slower than a Switch."));
    game_fps_label = new QLabel();
    game_fps_label->setObjectName(QStringLiteral("GameFpsLabel"));
    game_fps_label->setToolTip(tr("How many frames per second the game is currently displaying. "
                                  "This will vary from game to game and scene to scene."));
    emu_frametime_label = new QLabel();
    emu_frametime_label->setObjectName(QStringLiteral("EmuFrametimeLabel"));
    emu_frametime_label->setToolTip(
        tr("Time taken to emulate a Switch frame, not counting framelimiting or v-sync. For "
           "full-speed emulation this should be at most 16.67 ms."));

    for (auto& label : {shader_building_label, emu_speed_label, game_fps_label,
                        emu_frametime_label}) {
        label->setVisible(false);
        label->setFrameStyle(QFrame::NoFrame);
        label->setContentsMargins(4, 0, 4, 0);
        statusBar()->addPermanentWidget(label);
    }

    res_scale_label->setVisible(false);
    res_scale_label->setContentsMargins(4, 0, 4, 0);
    res_scale_label->setFocusPolicy(Qt::NoFocus);
    statusBar()->addPermanentWidget(res_scale_label);

    firmware_label = new QLabel();
    firmware_label->setObjectName(QStringLiteral("FirmwareLabel"));
    firmware_label->setVisible(false);
    firmware_label->setContentsMargins(4, 0, 4, 0);
    firmware_label->setFocusPolicy(Qt::NoFocus);
    statusBar()->addPermanentWidget(firmware_label);

    QWidget* multiplayer_status_widget = new QWidget();
    multiplayer_status_widget->setObjectName(QStringLiteral("MultiplayerStatusBarWidget"));
    QHBoxLayout* multiplayer_layout = new QHBoxLayout(multiplayer_status_widget);
    multiplayer_layout->setContentsMargins(6, 0, 6, 0);
    multiplayer_layout->setSpacing(4);
    multiplayer_layout->addWidget(multiplayer_state->GetStatusText());
    multiplayer_layout->addWidget(multiplayer_state->GetStatusIcon());
    multiplayer_status_widget->setLayout(multiplayer_layout);
    statusBar()->addPermanentWidget(multiplayer_status_widget, 0);

    tas_label = new QLabel();
    tas_label->setObjectName(QStringLiteral("TASlabel"));
    tas_label->setFocusPolicy(Qt::NoFocus);
    statusBar()->insertPermanentWidget(0, tas_label);

    volume_popup = new QWidget(this);
    volume_popup->setWindowFlags(Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint | Qt::Popup);
    volume_popup->setLayout(new QVBoxLayout());
    volume_popup->setMinimumWidth(200);

    volume_slider = new QSlider(Qt::Horizontal);
    volume_slider->setObjectName(QStringLiteral("volume_slider"));
    volume_slider->setMaximum(200);
    volume_slider->setPageStep(5);
    volume_popup->layout()->addWidget(volume_slider);

    volume_button = new VolumeButton();
    volume_button->setObjectName(QStringLiteral("VolumeStatusBarButton"));
    volume_button->setFocusPolicy(Qt::NoFocus);
    volume_button->setCheckable(true);
    UpdateVolumeUI();
    connect(volume_slider, &QSlider::valueChanged, this, [this](int percentage) {
        Settings::values.audio_muted = false;
        const auto volume = static_cast<u8>(percentage);
        Settings::values.volume.SetValue(volume);
        UpdateVolumeUI();
    });
    connect(volume_button, &QPushButton::clicked, this, [&] {
        UpdateVolumeUI();
        volume_popup->setVisible(!volume_popup->isVisible());
        QRect rect = volume_button->geometry();
        QPoint bottomLeft = statusBar()->mapToGlobal(rect.topLeft());
        bottomLeft.setY(bottomLeft.y() - volume_popup->geometry().height());
        volume_popup->setGeometry(QRect(bottomLeft, QSize(rect.width(), rect.height())));
    });
    volume_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(volume_button, &QPushButton::customContextMenuRequested,
            [this](const QPoint& menu_location) {
                QMenu context_menu;
                context_menu.addAction(
                    Settings::values.audio_muted ? tr("Unmute") : tr("Mute"), [this] {
                        Settings::values.audio_muted = !Settings::values.audio_muted;
                        UpdateVolumeUI();
                    });

                context_menu.addAction(tr("Reset Volume"), [this] {
                    Settings::values.volume.SetValue(100);
                    UpdateVolumeUI();
                });

                context_menu.exec(volume_button->mapToGlobal(menu_location));
                volume_button->repaint();
            });
    connect(volume_button, &VolumeButton::VolumeChanged, this, &MainWindow::UpdateVolumeUI);

    // setup DLC button
    dlc_button = new QPushButton();
    dlc_button->setObjectName(QStringLiteral("DLCStatusBarButton"));
    dlc_button->setFocusPolicy(Qt::NoFocus);
    dlc_button->setMinimumWidth(240);
    dlc_button->setText(tr("ДОПОЛНЕНИЯ"));
    dlc_button->setToolTip(tr("Менеджер дополнений Tinfoil"));
    dlc_button->setStyleSheet(QStringLiteral("margin-left: 15px;"));
    connect(dlc_button, &QPushButton::clicked, this, &MainWindow::OnDLCButtonClicked);

    statusBar()->insertPermanentWidget(0, dlc_button);
    statusBar()->insertPermanentWidget(0, volume_button);

    // setup Airplane Mode button
    airplane_mode_button = new QPushButton();
    airplane_mode_button->setObjectName(QStringLiteral("AirplaneModeStatusBarButton"));
    airplane_mode_button->setFocusPolicy(Qt::NoFocus);
    airplane_mode_button->setCheckable(true);
    connect(airplane_mode_button, &QPushButton::clicked, this, &MainWindow::OnToggleAirplaneMode);
    UpdateAirplaneModeButton();

    // Load TitleDB cache and check for update in background
    LoadTitleDbFromCache();
    {
        QString titledb_path = QDir::homePath() + QStringLiteral("/.switch/titledb.json");
        QFileInfo file_info(titledb_path);
        if (!file_info.exists() || file_info.lastModified().secsTo(QDateTime::currentDateTime()) > 86400) {
            StartTitleDbDownload(false);
        }
    }

    // setup AA button
    aa_status_button = new QPushButton();
    aa_status_button->setObjectName(QStringLiteral("AAStatusBarButton"));
    aa_status_button->setFocusPolicy(Qt::NoFocus);

    statusBar()->insertPermanentWidget(0, aa_status_button);
    statusBar()->insertPermanentWidget(0, airplane_mode_button);

    connect(aa_status_button, &QPushButton::clicked, [&] {
        auto aa_mode = Settings::values.anti_aliasing.GetValue();
        aa_mode = static_cast<Settings::AntiAliasing>(static_cast<u32>(aa_mode) + 1);
        if (static_cast<u32>(aa_mode) > static_cast<u32>(Settings::EnumMetadata<Settings::AntiAliasing>::GetLast())) {
            aa_mode = Settings::AntiAliasing::None;
        }
        Settings::values.anti_aliasing.SetValue(aa_mode);
        aa_status_button->setChecked(true);
        UpdateAAText();
    });
    UpdateAAText();
    aa_status_button->setCheckable(true);
    aa_status_button->setChecked(true);
    aa_status_button->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(aa_status_button, &QPushButton::customContextMenuRequested,
            [this](const QPoint& menu_location) {
                QMenu context_menu;
                for (auto const& aa_text_pair : ConfigurationShared::anti_aliasing_texts_map) {
                    context_menu.addAction(aa_text_pair.second, [this, aa_text_pair] {
                        Settings::values.anti_aliasing.SetValue(aa_text_pair.first);
                        UpdateAAText();
                    });
                }
                context_menu.exec(aa_status_button->mapToGlobal(menu_location));
                aa_status_button->repaint();
            });
    statusBar()->insertPermanentWidget(0, aa_status_button);

    // Setup Filter button
    filter_status_button = new QPushButton();
    filter_status_button->setObjectName(QStringLiteral("FilterStatusBarButton"));
    filter_status_button->setFocusPolicy(Qt::NoFocus);
    connect(filter_status_button, &QPushButton::clicked, this, &MainWindow::OnToggleAdaptingFilter);
    UpdateFilterText();
    filter_status_button->setCheckable(true);
    filter_status_button->setChecked(true);
    filter_status_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(filter_status_button, &QPushButton::customContextMenuRequested,
            [this](const QPoint& menu_location) {
                QMenu context_menu;
                for (auto const& filter_text_pair : ConfigurationShared::scaling_filter_texts_map) {
                    context_menu.addAction(filter_text_pair.second, [this, filter_text_pair] {
                        Settings::values.scaling_filter.SetValue(filter_text_pair.first);
                        UpdateFilterText();
                    });
                }
                context_menu.exec(filter_status_button->mapToGlobal(menu_location));
                filter_status_button->repaint();
            });
    statusBar()->insertPermanentWidget(0, filter_status_button);

    // Setup Dock button
    dock_status_button = new QPushButton();
    dock_status_button->setObjectName(QStringLiteral("DockingStatusBarButton"));
    dock_status_button->setFocusPolicy(Qt::NoFocus);
    connect(dock_status_button, &QPushButton::clicked, this, &MainWindow::OnToggleDockedMode);
    dock_status_button->setCheckable(true);
    UpdateDockedButton();
    dock_status_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(dock_status_button, &QPushButton::customContextMenuRequested,
            [this](const QPoint& menu_location) {
                QMenu context_menu;

                for (auto const& pair : ConfigurationShared::use_docked_mode_texts_map) {
                    QString translated_text = QCoreApplication::translate("MainWindow", pair.second.toUtf8().constData());
                    context_menu.addAction(translated_text, [this, mode = pair.first] {
                        if (mode != Settings::values.use_docked_mode.GetValue()) {
                            OnToggleDockedMode();
                        }
                    });
                }
                context_menu.exec(dock_status_button->mapToGlobal(menu_location));
                dock_status_button->repaint();
            });
    statusBar()->insertPermanentWidget(0, dock_status_button);

    // Setup GPU Accuracy button
    gpu_accuracy_button = new QPushButton();
    gpu_accuracy_button->setObjectName(QStringLiteral("GPUStatusBarButton"));
    gpu_accuracy_button->setCheckable(true);
    gpu_accuracy_button->setFocusPolicy(Qt::NoFocus);
    connect(gpu_accuracy_button, &QPushButton::clicked, this, &MainWindow::OnToggleGpuAccuracy);
    UpdateGPUAccuracyButton();
    gpu_accuracy_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(gpu_accuracy_button, &QPushButton::customContextMenuRequested,
            [this](const QPoint& menu_location) {
                QMenu context_menu;

                for (auto const& gpu_accuracy_pair : ConfigurationShared::gpu_accuracy_texts_map) {
                    QString translated_text = QCoreApplication::translate("MainWindow", gpu_accuracy_pair.second.toUtf8().constData());
                    context_menu.addAction(translated_text, [this, gpu_accuracy_pair] {
                        Settings::values.gpu_accuracy.SetValue(gpu_accuracy_pair.first);
                        UpdateGPUAccuracyButton();
                    });
                }
                context_menu.exec(gpu_accuracy_button->mapToGlobal(menu_location));
                gpu_accuracy_button->repaint();
            });
    statusBar()->insertPermanentWidget(0, gpu_accuracy_button);

    // Setup Renderer API button
    renderer_status_button = new QPushButton();
    renderer_status_button->setObjectName(QStringLiteral("RendererStatusBarButton"));
    renderer_status_button->setCheckable(true);
    renderer_status_button->setFocusPolicy(Qt::NoFocus);
    connect(renderer_status_button, &QPushButton::clicked, this, &MainWindow::OnToggleGraphicsAPI);
    UpdateAPIText();
    renderer_status_button->setCheckable(true);
    renderer_status_button->setChecked(Settings::values.renderer_backend.GetValue() ==
                                       Settings::RendererBackend::Vulkan);
    renderer_status_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(renderer_status_button, &QPushButton::customContextMenuRequested,
            [this](const QPoint& menu_location) {
                QMenu context_menu;

                for (auto const& renderer_backend_pair :
                     ConfigurationShared::renderer_backend_texts_map) {
                    if (renderer_backend_pair.first == Settings::RendererBackend::Null) {
                        continue;
                    }
                    context_menu.addAction(
                        renderer_backend_pair.second, [this, renderer_backend_pair] {
                            Settings::values.renderer_backend.SetValue(renderer_backend_pair.first);
                            UpdateAPIText();
                        });
                }
                context_menu.exec(renderer_status_button->mapToGlobal(menu_location));
                renderer_status_button->repaint();
            });
    statusBar()->insertPermanentWidget(0, renderer_status_button);

    // Setup Refresh Button
    refresh_button = new QPushButton();
    refresh_button->setText(QStringLiteral("↻"));
    refresh_button->setObjectName(QStringLiteral("RefreshButton"));
    refresh_button->setFocusPolicy(Qt::NoFocus);
    connect(refresh_button, &QPushButton::clicked, this, &MainWindow::OnGameListRefresh);

    statusBar()->insertPermanentWidget(0, refresh_button);

    statusBar()->setVisible(true);
    setStyleSheet(QStringLiteral("QStatusBar::item{border: none;}"));
}

void MainWindow::InitializeDebugWidgets() {
    QMenu* debug_menu = ui->menu_View_Debugging;

    controller_dialog = new ControllerDialog(QtCommon::system->HIDCore(), input_subsystem, this);
    controller_dialog->hide();
    debug_menu->addAction(controller_dialog->toggleViewAction());
}

void MainWindow::InitializeRecentFileMenuActions() {
    for (int i = 0; i < max_recent_files_item; ++i) {
        actions_recent_files[i] = new QAction(this);
        actions_recent_files[i]->setVisible(false);
        connect(actions_recent_files[i], &QAction::triggered, this, &MainWindow::OnMenuRecentFile);

        ui->menu_recent_files->addAction(actions_recent_files[i]);
    }
    ui->menu_recent_files->addSeparator();
    QAction* action_clear_recent_files = new QAction(this);
    action_clear_recent_files->setText(tr("&Clear Recent Files"));
    connect(action_clear_recent_files, &QAction::triggered, this, [this] {
        UISettings::values.recent_files.clear();
        UpdateRecentFiles();
    });
    ui->menu_recent_files->addAction(action_clear_recent_files);

    UpdateRecentFiles();
}

void MainWindow::LinkActionShortcut(QAction* action, const QString& action_name,
                                    const bool tas_allowed) {
    static const auto main_window = std::string("Main Window");
    action->setShortcut(hotkey_registry.GetKeySequence(main_window, action_name.toStdString()));
    action->setShortcutContext(
        hotkey_registry.GetShortcutContext(main_window, action_name.toStdString()));
    action->setAutoRepeat(false);

    this->addAction(action);

    auto* controller =
        QtCommon::system->HIDCore().GetEmulatedController(Core::HID::NpadIdType::Player1);
    const auto* controller_hotkey =
        hotkey_registry.GetControllerHotkey(main_window, action_name.toStdString(), controller);
    connect(
        controller_hotkey, &ControllerShortcut::Activated, this,
        [action, tas_allowed, this] {
            auto [tas_status, current_tas_frame, total_tas_frames] =
                input_subsystem->GetTas()->GetStatus();
            if (tas_allowed || tas_status == InputCommon::TasInput::TasState::Stopped) {
                action->trigger();
            }
        },
        Qt::QueuedConnection);
}

void MainWindow::InitializeHotkeys() {
    hotkey_registry.LoadHotkeys();

    LinkActionShortcut(ui->action_Load_File, QStringLiteral("Load File"));
    LinkActionShortcut(ui->action_Load_Amiibo, QStringLiteral("Load/Remove Amiibo"));
    LinkActionShortcut(ui->action_Exit, QStringLiteral("Exit Eden"));
    LinkActionShortcut(ui->action_Restart, QStringLiteral("Restart Emulation"));
    LinkActionShortcut(ui->action_Pause, QStringLiteral("Continue/Pause Emulation"));
    LinkActionShortcut(ui->action_Stop, QStringLiteral("Stop Emulation"));
    LinkActionShortcut(ui->action_Show_Filter_Bar, QStringLiteral("Toggle Filter Bar"));
    LinkActionShortcut(ui->action_Show_Status_Bar, QStringLiteral("Toggle Status Bar"));
    LinkActionShortcut(ui->action_Show_Performance_Overlay,
                       QStringLiteral("Toggle Performance Overlay"));
    LinkActionShortcut(ui->action_Fullscreen, QStringLiteral("Fullscreen"));
    LinkActionShortcut(ui->action_Capture_Screenshot, QStringLiteral("Capture Screenshot"));
    LinkActionShortcut(ui->action_TAS_Start, QStringLiteral("TAS Start/Stop"), true);
    LinkActionShortcut(ui->action_TAS_Record, QStringLiteral("TAS Record"), true);
    LinkActionShortcut(ui->action_TAS_Reset, QStringLiteral("TAS Reset"), true);
    LinkActionShortcut(ui->action_View_Lobby, QStringLiteral("Browse Public Game Lobby"));
    LinkActionShortcut(ui->action_Start_Room, QStringLiteral("Create Room"));
    LinkActionShortcut(ui->action_Connect_To_Room, QStringLiteral("Direct Connect to Room"));
    LinkActionShortcut(ui->action_Show_Room, QStringLiteral("Show Current Room"));
    LinkActionShortcut(ui->action_Leave_Room, QStringLiteral("Leave Room"));
    LinkActionShortcut(ui->action_Configure, QStringLiteral("Configure"));
    LinkActionShortcut(ui->action_Configure_Current_Game, QStringLiteral("Configure Current Game"));

    static const QString main_window = QStringLiteral("Main Window");
    const auto connect_shortcut = [&]<typename Fn>(const QString& action_name, const Fn& function) {
        const auto* hotkey =
            hotkey_registry.GetHotkey(main_window.toStdString(), action_name.toStdString(), this);
        auto* controller =
            QtCommon::system->HIDCore().GetEmulatedController(Core::HID::NpadIdType::Player1);
        const auto* controller_hotkey = hotkey_registry.GetControllerHotkey(
            main_window.toStdString(), action_name.toStdString(), controller);
        connect(hotkey, &QShortcut::activated, this, function);
        connect(controller_hotkey, &ControllerShortcut::Activated, this, function,
                Qt::QueuedConnection);
    };

    connect_shortcut(QStringLiteral("Exit Fullscreen"), [&] {
        if (emulation_running && ui->action_Fullscreen->isChecked()) {
            ui->action_Fullscreen->setChecked(false);
            ToggleFullscreen();
        }
    });
    connect_shortcut(QStringLiteral("Change Adapting Filter"), &MainWindow::OnToggleAdaptingFilter);
    connect_shortcut(QStringLiteral("Change Docked Mode"), &MainWindow::OnToggleDockedMode);
    connect_shortcut(QStringLiteral("Change GPU Mode"), &MainWindow::OnToggleGpuAccuracy);
    connect_shortcut(QStringLiteral("Audio Mute/Unmute"), &MainWindow::OnMute);
    connect_shortcut(QStringLiteral("Audio Volume Down"), &MainWindow::OnDecreaseVolume);
    connect_shortcut(QStringLiteral("Audio Volume Up"), &MainWindow::OnIncreaseVolume);

    connect_shortcut(QStringLiteral("Toggle Framerate Limit"), [this] {
        Settings::ToggleStandardMode();
        SetFPSSuffix();
    });

    connect_shortcut(QStringLiteral("Toggle Turbo Speed"), [this] {
        Settings::ToggleTurboMode();
        SetFPSSuffix();
    });

    connect_shortcut(QStringLiteral("Toggle Slow Speed"), [this] {
        Settings::ToggleSlowMode();
        SetFPSSuffix();
    });

    connect_shortcut(QStringLiteral("Toggle Renderdoc Capture"), [] {
        if (Settings::values.enable_renderdoc_hotkey) {
            QtCommon::system->GetRenderdocAPI().ToggleCapture();
        }
    });
    
    // Virtual Amiibo Snapshot Hotkeys
    auto* save_state_shortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_F5), this);
    connect(save_state_shortcut, &QShortcut::activated, this, &MainWindow::OnSaveState);
    
    auto* load_state_shortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_F6), this);
    connect(load_state_shortcut, &QShortcut::activated, this, &MainWindow::OnLoadState);

    connect_shortcut(QStringLiteral("Toggle Mouse Panning"), [&] {
        Settings::values.mouse_panning = !Settings::values.mouse_panning;
        if (Settings::values.mouse_panning) {
            render_window->installEventFilter(render_window);
            render_window->setAttribute(Qt::WA_Hover, true);
        }
    });
}

void MainWindow::SetDefaultUIGeometry() {
    // geometry: 53% of the window contents are in the upper screen half, 47% in the lower half
    const QRect screenRect = QGuiApplication::primaryScreen()->geometry();

    const int w = screenRect.width() * 2 / 3;
    const int h = screenRect.height() * 2 / 3;
    const int x = (screenRect.x() + screenRect.width()) / 2 - w / 2;
    const int y = (screenRect.y() + screenRect.height()) / 2 - h * 53 / 100;

    setGeometry(x, y, w, h);
}

void MainWindow::RestoreUIState() {
    setWindowFlags(windowFlags() & ~Qt::FramelessWindowHint);
    restoreGeometry(UISettings::values.geometry);
    // Work-around because the games list isn't supposed to be full screen
    if (isFullScreen()) {
        showNormal();
    }
    restoreState(UISettings::values.state);
    render_window->setWindowFlags(render_window->windowFlags() & ~Qt::FramelessWindowHint);
    render_window->restoreGeometry(UISettings::values.renderwindow_geometry);

    game_list->LoadInterfaceLayout();

    ui->action_Single_Window_Mode->setChecked(UISettings::values.single_window_mode.GetValue());
    ToggleWindowMode();

    ui->action_Fullscreen->setChecked(UISettings::values.fullscreen.GetValue());

    ui->action_Enable_Overlay_Applet->setChecked(Settings::values.enable_overlay.GetValue());

    ui->action_Show_Filter_Bar->setChecked(UISettings::values.show_filter_bar.GetValue());
    game_list->SetFilterVisible(ui->action_Show_Filter_Bar->isChecked());

    ui->action_Show_Status_Bar->setChecked(UISettings::values.show_status_bar.GetValue());
    statusBar()->setVisible(ui->action_Show_Status_Bar->isChecked());

    ui->action_Show_Performance_Overlay->setChecked(
        UISettings::values.show_perf_overlay.GetValue());
    if (perf_overlay)
        perf_overlay->setVisible(ui->action_Show_Performance_Overlay->isChecked());
    Debugger::ToggleConsole();
}

void MainWindow::OnAppFocusStateChanged(Qt::ApplicationState state) {
    if (state != Qt::ApplicationHidden && state != Qt::ApplicationInactive &&
        state != Qt::ApplicationActive) {
        LOG_DEBUG(Frontend, "ApplicationState unusual flag: {} ", state);
    }
    if (!emulation_running) {
        return;
    }
    if (UISettings::values.pause_when_in_background) {
        if (emu_thread->IsRunning() &&
            (state & (Qt::ApplicationHidden | Qt::ApplicationInactive))) {
            auto_paused = true;
            OnPauseGame();
        } else if (!emu_thread->IsRunning() && auto_paused && (state & Qt::ApplicationActive)) {
            auto_paused = false;
            OnStartGame();
        }
    }
    if (UISettings::values.mute_when_in_background) {
        if (!Settings::values.audio_muted &&
            (state & (Qt::ApplicationHidden | Qt::ApplicationInactive))) {
            Settings::values.audio_muted = true;
            auto_muted = true;
        } else if (auto_muted && (state & Qt::ApplicationActive)) {
            Settings::values.audio_muted = false;
            auto_muted = false;
        }
        UpdateVolumeUI();
    }
}

void MainWindow::ConnectWidgetEvents() {
    connect(game_list, &GameList::BootGame, this, &MainWindow::BootGameFromList);
    connect(game_list, &GameList::GameChosen, this, &MainWindow::OnGameListLoadFile);
    connect(game_list, &GameList::OpenDirectory, this, &MainWindow::OnGameListOpenDirectory);
    connect(game_list, &GameList::OpenFolderRequested, this, &MainWindow::OnGameListOpenFolder);
    connect(game_list, &GameList::OpenTransferableShaderCacheRequested, this,
            [this](u64 program_id) { QtCommon::Path::OpenShaderCache(program_id, this); });
    connect(game_list, &GameList::RemoveInstalledEntryRequested, this,
            &MainWindow::OnGameListRemoveInstalledEntry);
    connect(game_list, &GameList::RemoveFileRequested, this, &MainWindow::OnGameListRemoveFile);
    connect(game_list, &GameList::RemovePlayTimeRequested, this,
            &MainWindow::OnGameListRemovePlayTimeData);
    connect(game_list, &GameList::SetPlayTimeRequested, this, &MainWindow::OnGameListSetPlayTime);
    connect(game_list, &GameList::DumpRomFSRequested, this, &MainWindow::OnGameListDumpRomFS);
    connect(game_list, &GameList::VerifyIntegrityRequested, this,
            &MainWindow::OnGameListVerifyIntegrity);
    connect(game_list, &GameList::CopyTIDRequested, this, &MainWindow::OnGameListCopyTID);
    connect(game_list, &GameList::NavigateToGamedbEntryRequested, this,
            &MainWindow::OnGameListNavigateToGamedbEntry);
    connect(game_list, &GameList::CreateShortcut, this, &MainWindow::OnGameListCreateShortcut);
    connect(game_list, &GameList::AddDirectory, this, &MainWindow::OnGameListAddDirectory);
    connect(game_list_placeholder, &GameListPlaceholder::AddDirectory, this,
            &MainWindow::OnGameListAddDirectory);
    connect(game_list, &GameList::ShowList, this, &MainWindow::OnGameListShowList);
    connect(game_list, &GameList::PopulatingCompleted,
            [this] { multiplayer_state->UpdateGameList(game_list->GetModel()); });
    connect(game_list, &GameList::SaveConfig, this, &MainWindow::OnSaveConfig);

    connect(game_list, &GameList::OpenPerGameGeneralRequested, this,
            &MainWindow::OnGameListOpenPerGameProperties);
    connect(game_list, &GameList::LinkToRyujinxRequested, this, &MainWindow::OnLinkToRyujinx);

    connect(this, &MainWindow::UpdateInstallProgress, this, &MainWindow::IncrementInstallProgress);

    connect(this, &MainWindow::EmulationStarting, render_window,
            &GRenderWindow::OnEmulationStarting);
    connect(this, &MainWindow::EmulationStopping, render_window,
            &GRenderWindow::OnEmulationStopping);

    // Software Keyboard Applet
    connect(this, &MainWindow::EmulationStarting, this, &MainWindow::SoftwareKeyboardExit);
    connect(this, &MainWindow::EmulationStopping, this, &MainWindow::SoftwareKeyboardExit);

    connect(&status_bar_update_timer, &QTimer::timeout, this, &MainWindow::UpdateStatusBar);
    connect(res_scale_label, &QPushButton::clicked, this, &MainWindow::OnResolutionScaleClicked);

    connect(this, &MainWindow::UpdateThemedIcons, multiplayer_state,
            &MultiplayerState::UpdateThemedIcons);
}

void MainWindow::ConnectMenuEvents() {
    const auto connect_menu = [&]<typename Fn>(QAction* action, const Fn& event_fn) {
        connect(action, &QAction::triggered, this, event_fn);
        // Add actions to this window so that hiding menus in fullscreen won't disable them
        addAction(action);
        // Add actions to the render window so that they work outside of single window mode
        render_window->addAction(action);
    };

    // File
    connect_menu(ui->action_Load_File, &MainWindow::OnMenuLoadFile);
    connect_menu(ui->action_Load_Folder, &MainWindow::OnMenuLoadFolder);
    connect_menu(ui->action_Install_File_NAND, &MainWindow::OnMenuInstallToNAND);
    connect_menu(ui->action_Exit, &QMainWindow::close);
    connect_menu(ui->action_Load_Amiibo, &MainWindow::OnLoadAmiibo);

    // Emulation
    connect_menu(ui->action_Pause, &MainWindow::OnPauseContinueGame);
    connect_menu(ui->action_Stop, &MainWindow::OnStopGame);
    connect_menu(ui->action_Report_Compatibility, &MainWindow::OnMenuReportCompatibility);
    connect_menu(ui->action_Open_Mods_Page, &MainWindow::OnOpenModsPage);
    connect_menu(ui->action_Open_Quickstart_Guide, &MainWindow::OnOpenQuickstartGuide);
    connect_menu(ui->action_Open_FAQ, &MainWindow::OnOpenFAQ);
    connect_menu(ui->action_Restart, &MainWindow::OnRestartGame);
    connect_menu(ui->action_Configure, &MainWindow::OnConfigure);
    connect_menu(ui->action_Configure_Current_Game, &MainWindow::OnConfigurePerGame);

    // View
    connect_menu(ui->action_Fullscreen, &MainWindow::ToggleFullscreen);
    connect_menu(ui->action_Single_Window_Mode, &MainWindow::ToggleWindowMode);
    connect_menu(ui->action_Show_Filter_Bar, &MainWindow::OnToggleFilterBar);
    connect_menu(ui->action_Show_Status_Bar, &MainWindow::OnToggleStatusBar);
    connect_menu(ui->action_Show_Performance_Overlay, &MainWindow::OnTogglePerfOverlay);

    connect_menu(ui->action_Reset_Window_Size_720, &MainWindow::ResetWindowSize720);
    connect_menu(ui->action_Reset_Window_Size_900, &MainWindow::ResetWindowSize900);
    connect_menu(ui->action_Reset_Window_Size_1080, &MainWindow::ResetWindowSize1080);
    ui->menu_Reset_Window_Size->addActions({ui->action_Reset_Window_Size_720,
                                            ui->action_Reset_Window_Size_900,
                                            ui->action_Reset_Window_Size_1080});

    connect_menu(ui->action_Grid_View, &MainWindow::SetGridView);
    connect_menu(ui->action_Tree_View, &MainWindow::SetTreeView);

    game_size_actions = new QActionGroup(this);
    game_size_actions->setExclusive(true);

    for (size_t i = 0; i < default_game_icon_sizes.size(); i++) {
        const auto current_size = UISettings::values.game_icon_size.GetValue();
        const auto size = default_game_icon_sizes[i].first;
        QAction* action = ui->menuGame_Icon_Size->addAction(GetTranslatedGameIconSize(i));
        action->setCheckable(true);

        if (current_size == size)
            action->setChecked(true);

        game_size_actions->addAction(action);

        connect(action, &QAction::triggered, this, [this, size](bool checked) {
            if (checked) {
                UISettings::values.game_icon_size.SetValue(size);
                CheckIconSize();
                game_list->RefreshGameDirectory();
            }
        });
    }

    CheckIconSize();

    ui->action_Show_Game_Name->setChecked(UISettings::values.show_game_name.GetValue());
    connect(ui->action_Show_Game_Name, &QAction::triggered, this, &MainWindow::ToggleShowGameName);

    // Multiplayer
    connect(ui->action_View_Lobby, &QAction::triggered, multiplayer_state,
            &MultiplayerState::OnViewLobby);
    connect(ui->action_Start_Room, &QAction::triggered, multiplayer_state,
            &MultiplayerState::OnCreateRoom);
    connect(ui->action_Leave_Room, &QAction::triggered, multiplayer_state,
            &MultiplayerState::OnCloseRoom);
    connect(ui->action_Connect_To_Room, &QAction::triggered, multiplayer_state,
            &MultiplayerState::OnDirectConnectToRoom);
    connect(ui->action_Show_Room, &QAction::triggered, multiplayer_state,
            &MultiplayerState::OnOpenNetworkRoom);
    connect(multiplayer_state, &MultiplayerState::SaveConfig, this, &MainWindow::OnSaveConfig);

    // Tools
    connect_menu(ui->action_Launch_PhotoViewer, [this] {
        LaunchFirmwareApplet(u64(Service::AM::AppletProgramId::PhotoViewer), std::nullopt);
    });
    connect_menu(ui->action_Launch_MiiEdit, [this] {
        LaunchFirmwareApplet(u64(Service::AM::AppletProgramId::MiiEdit), std::nullopt);
    });
    connect_menu(ui->action_Launch_Controller, [this] {
        LaunchFirmwareApplet(u64(Service::AM::AppletProgramId::Controller), std::nullopt);
    });
    connect_menu(ui->action_Launch_QLaunch, [this] {
        LaunchFirmwareApplet(u64(Service::AM::AppletProgramId::QLaunch), std::nullopt);
    });
    // Tools (cabinet)
    connect_menu(ui->action_Launch_Cabinet_Nickname_Owner, [this] {
        LaunchFirmwareApplet(u64(Service::AM::AppletProgramId::Cabinet),
                             {Service::NFP::CabinetMode::StartNicknameAndOwnerSettings});
    });
    connect_menu(ui->action_Launch_Cabinet_Eraser, [this] {
        LaunchFirmwareApplet(u64(Service::AM::AppletProgramId::Cabinet),
                             {Service::NFP::CabinetMode::StartGameDataEraser});
    });
    connect_menu(ui->action_Launch_Cabinet_Restorer, [this] {
        LaunchFirmwareApplet(u64(Service::AM::AppletProgramId::Cabinet),
                             {Service::NFP::CabinetMode::StartRestorer});
    });
    connect_menu(ui->action_Launch_Cabinet_Formatter, [this] {
        LaunchFirmwareApplet(u64(Service::AM::AppletProgramId::Cabinet),
                             {Service::NFP::CabinetMode::StartFormatter});
    });

    connect_menu(ui->action_Desktop, &MainWindow::OnCreateHomeMenuDesktopShortcut);
    connect_menu(ui->action_Application_Menu, &MainWindow::OnCreateHomeMenuApplicationMenuShortcut);
    connect_menu(ui->action_Capture_Screenshot, &MainWindow::OnCaptureScreenshot);

    // TAS
    connect_menu(ui->action_TAS_Start, &MainWindow::OnTasStartStop);
    connect_menu(ui->action_TAS_Record, &MainWindow::OnTasRecord);
    connect_menu(ui->action_TAS_Reset, &MainWindow::OnTasReset);
    connect_menu(ui->action_Configure_Tas, &MainWindow::OnConfigureTas);

    // Help
    connect_menu(ui->action_Root_Data_Folder, &MainWindow::OnOpenRootDataFolder);
    connect_menu(ui->action_NAND_Folder, &MainWindow::OnOpenNANDFolder);
    connect_menu(ui->action_SDMC_Folder, &MainWindow::OnOpenSDMCFolder);
    connect_menu(ui->action_Mod_Folder, &MainWindow::OnOpenModFolder);
    connect_menu(ui->action_Log_Folder, &MainWindow::OnOpenLogFolder);

    connect_menu(ui->action_Verify_installed_contents, &MainWindow::OnVerifyInstalledContents);
    connect_menu(ui->action_Firmware_From_Folder, &MainWindow::OnInstallFirmware);
    connect_menu(ui->action_Firmware_From_ZIP, &MainWindow::OnInstallFirmwareFromZIP);
    connect_menu(ui->action_Install_Keys, &MainWindow::OnInstallDecryptionKeys);
    connect_menu(ui->action_About, &MainWindow::OnAbout);
    connect_menu(ui->action_Eden_Dependencies, &MainWindow::OnEdenDependencies);
    connect_menu(ui->action_Data_Manager, &MainWindow::OnDataDialog);
}

void MainWindow::UpdateMenuState() {
    const bool is_paused = emu_thread == nullptr || !emu_thread->IsRunning();
    const bool is_firmware_available = CheckFirmwarePresence();

    const std::array running_actions{
        ui->action_Stop,
        ui->action_Restart,
        ui->action_Configure_Current_Game,
        ui->action_Report_Compatibility,
        ui->action_Load_Amiibo,
        ui->action_Pause,
    };

    const std::array applet_actions{
        ui->action_Launch_PhotoViewer,       ui->action_Launch_Cabinet_Nickname_Owner,
        ui->action_Launch_Cabinet_Eraser,    ui->action_Launch_Cabinet_Restorer,
        ui->action_Launch_Cabinet_Formatter, ui->action_Launch_MiiEdit,
        ui->action_Launch_QLaunch,           ui->action_Launch_Controller};

    for (QAction* action : running_actions) {
        action->setEnabled(emulation_running);
    }

    ui->action_Firmware_From_Folder->setEnabled(!emulation_running);
    ui->action_Firmware_From_ZIP->setEnabled(!emulation_running);
    ui->action_Install_Keys->setEnabled(!emulation_running);

    for (QAction* action : applet_actions) {
        action->setEnabled(is_firmware_available && !emulation_running);
    }

    ui->action_Capture_Screenshot->setEnabled(emulation_running && !is_paused);

    if (emulation_running && is_paused) {
        ui->action_Pause->setText(tr("&Continue"));
    } else {
        ui->action_Pause->setText(tr("&Pause"));
    }

    multiplayer_state->UpdateNotificationStatus();
}

void MainWindow::SetupPrepareForSleep() {
#ifdef __unix__
    if (auto bus = QDBusConnection::systemBus(); bus.isConnected()) {
        // See https://github.com/ConsoleKit2/ConsoleKit2/issues/150
#ifdef __linux__
        const auto dbus_logind_service = QStringLiteral("org.freedesktop.login1");
        const auto dbus_logind_path = QStringLiteral("/org/freedesktop/login1");
        const auto dbus_logind_manager_if = QStringLiteral("org.freedesktop.login1.Manager");
        // const auto dbus_logind_session_if = QStringLiteral("org.freedesktop.login1.Session");
#else
        const auto dbus_logind_service = QStringLiteral("org.freedesktop.ConsoleKit");
        const auto dbus_logind_path = QStringLiteral("/org/freedesktop/ConsoleKit/Manager");
        const auto dbus_logind_manager_if = QStringLiteral("org.freedesktop.ConsoleKit.Manager");
        // const auto dbus_logind_session_if = QStringLiteral("org.freedesktop.ConsoleKit.Session");
#endif
        const bool success = bus.connect(dbus_logind_service, dbus_logind_path,
                                         dbus_logind_manager_if, QStringLiteral("PrepareForSleep"),
                                         QStringLiteral("b"), this, SLOT(OnPrepareForSleep(bool)));
        if (!success)
            LOG_WARNING(Frontend, "Couldn't register PrepareForSleep signal");
    } else {
        LOG_WARNING(Frontend, "QDBusConnection system bus is not connected");
    }
#endif // __unix__
}

void MainWindow::OnPrepareForSleep(bool prepare_sleep) {
    if (emu_thread == nullptr) {
        return;
    }

    if (prepare_sleep) {
        if (emu_thread->IsRunning()) {
            auto_paused = true;
            OnPauseGame();
        }
    } else {
        if (!emu_thread->IsRunning() && auto_paused) {
            auto_paused = false;
            OnStartGame();
        }
    }
}

#ifdef __unix__
std::array<int, 3> MainWindow::sig_interrupt_fds{0, 0, 0};

void MainWindow::SetupSigInterrupts() {
    if (sig_interrupt_fds[2] == 1) {
        return;
    }
    socketpair(AF_UNIX, SOCK_STREAM, 0, sig_interrupt_fds.data());
    sig_interrupt_fds[2] = 1;

    struct sigaction sa;
    sa.sa_handler = &MainWindow::HandleSigInterrupt;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESETHAND;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    sig_interrupt_notifier = new QSocketNotifier(sig_interrupt_fds[1], QSocketNotifier::Read, this);
    connect(sig_interrupt_notifier, &QSocketNotifier::activated, this,
            &MainWindow::OnSigInterruptNotifierActivated);
    connect(this, &MainWindow::SigInterrupt, this, &MainWindow::close);
}

void MainWindow::HandleSigInterrupt(int sig) {
    if (sig == SIGINT) {
        _exit(1);
    }

    // Calling into Qt directly from a signal handler is not safe,
    // so wake up a QSocketNotifier with this hacky write call instead.
    char a = 1;
    int ret = write(sig_interrupt_fds[0], &a, sizeof(a));
    (void)ret;
}

void MainWindow::OnSigInterruptNotifierActivated() {
    sig_interrupt_notifier->setEnabled(false);

    char a;
    int ret = read(sig_interrupt_fds[1], &a, sizeof(a));
    (void)ret;

    sig_interrupt_notifier->setEnabled(true);

    emit SigInterrupt();
}
#endif // __unix__

void MainWindow::PreventOSSleep() {
#ifdef _WIN32
    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
#else
    SDL_DisableScreenSaver();
#endif
}

void MainWindow::AllowOSSleep() {
#ifdef _WIN32
    SetThreadExecutionState(ES_CONTINUOUS);
#else
    SDL_EnableScreenSaver();
#endif
}

bool MainWindow::LoadROM(const QString& filename, Service::AM::FrontendAppletParameters params) {
    // Shutdown previous session if the emu thread is still active...
    if (emu_thread != nullptr) {
        ShutdownGame();
    }

    if (!render_window->InitRenderTarget()) {
        return false;
    }

    QtCommon::system->SetFilesystem(QtCommon::vfs);

    if (params.launch_type == Service::AM::LaunchType::FrontendInitiated) {
        QtCommon::system->GetUserChannel().clear();
    }

    QtCommon::system->SetFrontendAppletSet({
        std::make_unique<QtAmiiboSettings>(*this), // Amiibo Settings
        (UISettings::values.controller_applet_disabled.GetValue() == true)
            ? nullptr
            : std::make_unique<QtControllerSelector>(*this), // Controller Selector
        std::make_unique<QtErrorDisplay>(*this),             // Error Display
        nullptr,                                             // Mii Editor
        nullptr,                                             // Parental Controls
        nullptr,                                             // Photo Viewer
        std::make_unique<QtProfileSelector>(*this),          // Profile Selector
        std::make_unique<QtSoftwareKeyboard>(*this),         // Software Keyboard
        std::make_unique<QtWebBrowser>(*this),               // Web Browser
        nullptr,                                             // Net Connect
    });

    /** firmware check */

    if (!QtCommon::Content::CheckGameFirmware(params.program_id))
        return false;

    /** Exec */
    Core::SystemResultStatus result = Core::SystemResultStatus::ErrorUnknown;
    try {
        result = QtCommon::system->Load(*render_window, filename.toStdString(), params);
    } catch (const std::exception& e) {
        LOG_CRITICAL(Frontend, "CRASH PREVENTED: Exception in System::Load: {}", e.what());
        QMessageBox::critical(this, tr("Fatal Error"),
                              tr("A fatal C++ exception occurred during game loading:\n%1").arg(QString::fromUtf8(e.what())));
        return false;
    } catch (...) {
        LOG_CRITICAL(Frontend, "CRASH PREVENTED: Unknown exception in System::Load!");
        QMessageBox::critical(this, tr("Fatal Error"),
                              tr("An unknown fatal C++ exception occurred during game loading."));
        return false;
    }

    const auto drd_callout = (UISettings::values.callout_flags.GetValue() &
                              static_cast<u32>(CalloutFlag::DRDDeprecation)) == 0;

    if (result == Core::SystemResultStatus::Success &&
        QtCommon::system->GetAppLoader().GetFileType() ==
            Loader::FileType::DeconstructedRomDirectory &&
        drd_callout) {
        UISettings::values.callout_flags = UISettings::values.callout_flags.GetValue() |
                                           static_cast<u32>(CalloutFlag::DRDDeprecation);
        QMessageBox::warning(
            this, tr("Warning: Outdated Game Format"),
            tr("You are using the deconstructed ROM directory format for this game, which is an "
               "outdated format that has been superseded by others such as NCA, NAX, XCI, or "
               "NSP. Deconstructed ROM directories lack icons, metadata, and update "
               "support.<br>For an explanation of the various Switch formats Eden supports, "
               "out our user handbook. This message will not be shown again."));
    }

    if (result != Core::SystemResultStatus::Success) {
        switch (result) {
        case Core::SystemResultStatus::ErrorGetLoader:
            LOG_CRITICAL(Frontend, "Failed to obtain loader for {}!", filename.toStdString());
            QMessageBox::critical(this, tr("Error while loading ROM!"),
                                  tr("The ROM format is not supported."));
            break;
        case Core::SystemResultStatus::ErrorVideoCore:
            QMessageBox::critical(
                this, tr("An error occurred initializing the video core."),
                tr("Eden has encountered an error while running the video core. "
                   "This is usually caused by outdated GPU drivers, including integrated ones. "
                   "Please see the log for more details. "
                   "For more information on accessing the log, please see the following page: "
                   "<a href='https://yuzu-mirror.github.io/help/reference/log-files/'>"
                   "How to Upload the Log File</a>. "));
            break;
        default:
            if (result > Core::SystemResultStatus::ErrorLoader) {
                const u16 loader_id = static_cast<u16>(Core::SystemResultStatus::ErrorLoader);
                const u16 error_id = static_cast<u16>(result) - loader_id;
                const std::string error_code = fmt::format("({:04X}-{:04X})", loader_id, error_id);
                LOG_CRITICAL(Frontend, "Failed to load ROM! {}", error_code);

                const auto title =
                    tr("Error while loading ROM! %1", "%1 signifies a numeric error code.")
                        .arg(QString::fromStdString(error_code));
                        
                QString translated_error_string;
                switch (static_cast<Loader::ResultStatus>(error_id)) {
                case Loader::ResultStatus::ErrorMissingBKTRBaseRomFS:
                    translated_error_string = tr("Game updates cannot be loaded directly. Load the base game instead.");
                    break;
                case Loader::ResultStatus::ErrorNoExeFS:
                    translated_error_string = tr("The Program-type NCA contains no executable. An update may be required.");
                    break;
                default:
                    translated_error_string = QString::fromStdString(
                        GetResultStatusString(static_cast<Loader::ResultStatus>(error_id)));
                    break;
                }

                const auto description =
                    tr("%1<br>Please redump your files or ask on Discord/Stoat for help.",
                       "%1 signifies an error string.")
                        .arg(translated_error_string);

                QMessageBox::critical(this, title, description);
            } else {
                QMessageBox::critical(
                    this, tr("Error while loading ROM!"),
                    tr("An unknown error occurred. Please see the log for more details."));
            }
            break;
        }
        return false;
    }
    current_game_path = filename;

    return true;
}

bool MainWindow::SelectAndSetCurrentUser(
    const Core::Frontend::ProfileSelectParameters& parameters) {
    QtProfileSelectionDialog dialog(*QtCommon::system, this, parameters);
    dialog.setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint |
                          Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
    dialog.setWindowModality(Qt::WindowModal);

    if (dialog.exec() == QDialog::Rejected) {
        return false;
    }

    Settings::values.current_user = dialog.GetIndex();
    return true;
}

void MainWindow::ConfigureFilesystemProvider(const std::string& filepath) {
    // Ensure all NCAs are registered before launching the game
    const auto file = QtCommon::vfs->OpenFile(filepath, FileSys::OpenMode::Read);
    if (!file) {
        return;
    }

    if (QtCommon::provider->AddEntriesFromContainer(file)) {
        return;
    }

    auto loader = Loader::GetLoader(*QtCommon::system, file);
    if (!loader) {
        return;
    }

    const auto file_type = loader->GetFileType();
    if (file_type == Loader::FileType::Unknown || file_type == Loader::FileType::Error) {
        return;
    }

    u64 program_id = 0;
    const auto res2 = loader->ReadProgramId(program_id);
    if (res2 == Loader::ResultStatus::Success && file_type == Loader::FileType::NCA) {
        QtCommon::provider->AddEntry(FileSys::TitleType::Application,
                                     FileSys::GetCRTypeFromNCAType(FileSys::NCA{file}.GetType()),
                                     program_id, file);
    }
}

void MainWindow::BootGame(const QString& filename, Service::AM::FrontendAppletParameters params,
                          StartGameType type) {
    Settings::is_booting = true;
    LOG_INFO(Frontend, "Eden starting...");

    if (params.program_id == 0 ||
        params.program_id > static_cast<u64>(Service::AM::AppletProgramId::MaxProgramId)) {
        StoreRecentFile(filename); // Put the filename on top of the list
    }

    // Save configurations
    UpdateUISettings();
    game_list->SaveInterfaceLayout();
    config->SaveAllValues();

    if (UISettings::values.select_user_on_boot && !user_flag_cmd_line) {
        const Core::Frontend::ProfileSelectParameters parameters{
            .mode = Service::AM::Frontend::UiMode::UserSelector,
            .invalid_uid_list = {},
            .display_options = {},
            .purpose = Service::AM::Frontend::UserSelectionPurpose::General,
        };
        if (SelectAndSetCurrentUser(parameters) == false) {
            return;
        }
    }
    user_flag_cmd_line = false;

    u64 title_id{0};

    last_filename_booted = filename;

    const auto v_file = Core::GetGameFileFromPath(QtCommon::vfs, filename.toUtf8().constData());
    const auto loader =
        Loader::GetLoader(*QtCommon::system, v_file, params.program_id, params.program_index);

    if (loader != nullptr) {
        loading_screen->Prepare(*loader);
    }

    // Show Loading Screen EARLY but AFTER loader creation to ensure we have the title and logo ready
    game_list->hide();
    game_list_placeholder->hide();
    render_window->show();
    render_window->setFocus();
    loading_screen->setGeometry(ui->centralwidget->rect());
    loading_screen->raise();
    loading_screen->show();
    QCoreApplication::processEvents();

    // Now configure the filesystem provider which may take a few seconds for large/complex games
    ConfigureFilesystemProvider(filename.toStdString());


    if (loader != nullptr && loader->ReadProgramId(title_id) == Loader::ResultStatus::Success &&
        type == StartGameType::Normal) {
        // Load per game settings
        const auto file_path =
            std::filesystem::path{Common::U16StringFromBuffer(filename.utf16(), filename.size())};
        const auto config_file_name = title_id == 0
                                          ? Common::FS::PathToUTF8String(file_path.filename())
                                          : fmt::format("{:016X}", title_id);
        QtConfig per_game_config(config_file_name, Config::ConfigType::PerGameConfig);
        QtCommon::system->HIDCore().ReloadInputDevices();
        QtCommon::system->ApplySettings();
    }

    Settings::LogSettings();

    if (!LoadROM(filename, params)) {
        Settings::is_booting = false;
        loading_screen->hide();
        render_window->hide();
        if (game_list->IsEmpty()) {
            game_list_placeholder->show();
        } else {
            game_list->show();
        }
        return;
    }

    QtCommon::system->SetShuttingDown(false);

    // Create the emulation thread
    emu_thread = std::make_unique<EmuThread>(*QtCommon::system);
    emit EmulationStarting(emu_thread.get());

    // Register an ExecuteProgram callback such that Core can execute a sub-program
    QtCommon::system->RegisterExecuteProgramCallback(
        [this](std::size_t program_index_) { render_window->ExecuteProgram(program_index_); });

    QtCommon::system->RegisterExitCallback([this] {
        emu_thread->ForceStop();
        render_window->Exit();
    });

    connect(render_window, &GRenderWindow::Closed, this, &MainWindow::OnStopGame);
    connect(render_window, &GRenderWindow::MouseActivity, this, &MainWindow::OnMouseActivity);

    // Connect shader loading progress - LoadingScreen will auto-hide on Build stage
    connect(emu_thread.get(), &EmuThread::LoadProgress, loading_screen,
            &LoadingScreen::OnLoadProgress, Qt::QueuedConnection);

    // Start the emulation thread AFTER all connections are established
    emu_thread->start();

    // Update the GUI
    UpdateStatusButtons();
    if (ui->action_Single_Window_Mode->isChecked()) {
        game_list->hide();
        game_list_placeholder->hide();
    }
    status_bar_update_timer.start(500);
    renderer_status_button->setDisabled(true);
    refresh_button->setDisabled(true);
    SetFPSSuffix();

    if (UISettings::values.hide_mouse || Settings::values.mouse_panning) {
        render_window->installEventFilter(render_window);
        render_window->setAttribute(Qt::WA_Hover, true);
    }

    if (UISettings::values.hide_mouse) {
        mouse_hide_timer.start();
    }

    render_window->InitializeCamera();

    std::string title_name;
    std::string title_version;
    const auto res = QtCommon::system->GetGameName(title_name);

    const FileSys::PatchManager pm(title_id, QtCommon::system->GetFileSystemController(),
                                   QtCommon::system->GetContentProvider());
    const auto metadata = pm.GetControlMetadata();
    if (metadata.first != nullptr) {
        title_version = metadata.first->GetVersionString();
        title_name = metadata.first->GetApplicationName();
    }

    if (title_version == "1.0.0" || title_version.empty()) {
        auto game_ver = pm.GetGameVersion();
        if (game_ver.has_value() && *game_ver != 0) {
            u32 v = *game_ver;
            std::array<u8, 4> bytes{};
            bytes[0] = static_cast<u8>(v % 0x100); v /= 0x100;
            bytes[1] = static_cast<u8>(v % 0x100); v /= 0x100;
            bytes[2] = static_cast<u8>(v % 0x100); v /= 0x100;
            bytes[3] = static_cast<u8>(v % 0x100);
            auto cnmt_ver = fmt::format("{}.{}.{}", bytes[3], bytes[2], bytes[1]);
            if (cnmt_ver != "1.0.0") {
                title_version = cnmt_ver;
            }
        }
    }
    if (res != Loader::ResultStatus::Success || title_name.empty()) {
        title_name = Common::FS::PathToUTF8String(
            std::filesystem::path{Common::U16StringFromBuffer(filename.utf16(), filename.size())}
                .filename());
    }
    const bool is_64bit = QtCommon::system->Kernel().ApplicationProcess()->Is64Bit();
    const auto instruction_set_suffix = is_64bit ? tr("(64-bit)") : tr("(32-bit)");
    title_name = tr("%1 %2", "%1 is the title name. %2 indicates if the title is 64-bit or 32-bit")
                     .arg(QString::fromStdString(title_name), instruction_set_suffix)
                     .toStdString();
    LOG_INFO(Frontend, "Booting game: {:016X} | {} | {}", title_id, title_name, title_version);
    const auto gpu_vendor = QtCommon::system->GPU().Renderer().GetDeviceVendor();
    UpdateWindowTitle(title_name, title_version, gpu_vendor);

    // loading_screen already shown before emu_thread->start()

    emulation_running = true;
    UpdateDLCButtonText();
    if (ui->action_Fullscreen->isChecked()) {
        ShowFullscreen();
    }
    OnStartGame();
}

void MainWindow::BootGameFromList(const QString& filename, StartGameType with_config) {
    BootGame(filename, ApplicationAppletParameters(), with_config);
}

bool MainWindow::OnShutdownBegin() {
    if (!emulation_running) {
        return false;
    }

    if (ui->action_Fullscreen->isChecked()) {
        HideFullscreen();
    }

    AllowOSSleep();

    // Disable unlimited frame rate and turbo/slow modes
    Settings::values.use_speed_limit.SetValue(true);
    Settings::values.current_speed_mode = Settings::SpeedMode::Standard;

    if (QtCommon::system->IsShuttingDown()) {
        return false;
    }

    if (perf_overlay) {
        perf_overlay->hide();
        perf_overlay->deleteLater();
        perf_overlay = nullptr;
    }

    QtCommon::system->SetShuttingDown(true);
    discord_rpc->Pause();

    RequestGameExit();
    emu_thread->disconnect();
    emu_thread->SetRunning(true);

    emit EmulationStopping();

    int shutdown_time = 1000;

    if (QtCommon::system->DebuggerEnabled()) {
        shutdown_time = 0;
    } else if (QtCommon::system->GetExitLocked()) {
        shutdown_time = 5000;
    }

    shutdown_timer.setSingleShot(true);
    shutdown_timer.start(shutdown_time);
    connect(&shutdown_timer, &QTimer::timeout, this, &MainWindow::OnEmulationStopTimeExpired);
    connect(emu_thread.get(), &QThread::finished, this, &MainWindow::OnEmulationStopped);

    // Disable everything to prevent anything from being triggered here
    ui->action_Pause->setEnabled(false);
    ui->action_Restart->setEnabled(false);
    ui->action_Stop->setEnabled(false);

    return true;
}

class ShutdownDialog : public QDialog {
public:
    explicit ShutdownDialog(QWidget* parent) : QDialog(parent) {
        setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::CustomizeWindowHint);
        setModal(true);
        setAttribute(Qt::WA_TranslucentBackground);
        
        setFixedSize(400, 180);
        
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(15, 15, 15, 15);
        
        auto* container = new QWidget(this);
        container->setObjectName(QStringLiteral("Container"));
        
        auto* shadow = new QGraphicsDropShadowEffect(container);
        shadow->setBlurRadius(20);
        shadow->setColor(QColor(0, 0, 0, 160));
        shadow->setOffset(0, 6);
        container->setGraphicsEffect(shadow);
        
        auto* container_layout = new QVBoxLayout(container);
        container_layout->setContentsMargins(25, 25, 25, 25);
        container_layout->setAlignment(Qt::AlignCenter);
        
        auto* title_label = new QLabel(tr("Завершение работы"), container);
        title_label->setObjectName(QStringLiteral("TitleLabel"));
        title_label->setAlignment(Qt::AlignCenter);
        
        QFont title_font = title_label->font();
        title_font.setFamily(QStringLiteral("Segoe UI"));
        title_font.setPointSize(16);
        title_font.setBold(true);
        title_label->setFont(title_font);
        
        auto* subtitle_label = new QLabel(tr("Остановка эмуляции и освобождение ресурсов..."), container);
        subtitle_label->setObjectName(QStringLiteral("SubtitleLabel"));
        subtitle_label->setAlignment(Qt::AlignCenter);
        
        QFont subtitle_font = subtitle_label->font();
        subtitle_font.setFamily(QStringLiteral("Segoe UI"));
        subtitle_font.setPointSize(10);
        subtitle_label->setFont(subtitle_font);
        
        auto* progress = new QProgressBar(container);
        progress->setObjectName(QStringLiteral("ProgressBar"));
        progress->setRange(0, 0); // Indeterminate progress
        progress->setTextVisible(false);
        progress->setFixedHeight(6);
        
        container_layout->addWidget(title_label);
        container_layout->addWidget(subtitle_label);
        container_layout->addSpacing(10);
        container_layout->addWidget(progress);
        
        layout->addWidget(container);
        
        auto* pulse_timer = new QTimer(this);
        connect(pulse_timer, &QTimer::timeout, this, [this, pulse_val = 0.3f, going_up = true]() mutable {
            if (going_up) {
                pulse_val += 0.015f;
                if (pulse_val >= 0.8f) going_up = false;
            } else {
                pulse_val -= 0.015f;
                if (pulse_val <= 0.3f) going_up = true;
            }
            this->setStyleSheet(QStringLiteral(
                "QDialog {"
                "  background: transparent;"
                "}"
                "QWidget#Container {"
                "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #1a1b20, stop:1 #111215);"
                "  border: 1px solid rgba(0, 190, 255, %1);"
                "  border-radius: 16px;"
                "}"
                "QLabel#TitleLabel {"
                "  color: #ffffff;"
                "  background: transparent;"
                "}"
                "QLabel#SubtitleLabel {"
                "  color: #8a8d9a;"
                "  background: transparent;"
                "}"
                "QProgressBar#ProgressBar {"
                "  background: rgba(255, 255, 255, 0.08);"
                "  border-radius: 3px;"
                "  border: none;"
                "}"
                "QProgressBar#ProgressBar::chunk {"
                "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0055ff, stop:1 #00bfff);"
                "  border-radius: 3px;"
                "}"
            ).arg(static_cast<double>(pulse_val)));
        });
        pulse_timer->start(20);
        
        if (parent) {
            auto parent_geometry = parent->geometry();
            move(parent_geometry.center() - rect().center());
        }
    }
};

void MainWindow::OnShutdownBeginDialog() {
    shutdown_dialog = new ShutdownDialog(this);
    shutdown_dialog->open();
}

void MainWindow::OnEmulationStopTimeExpired() {
    if (emu_thread) {
        emu_thread->ForceStop();
    }
}

void MainWindow::OnEmulationStopped() {
    Settings::is_booting = false;
    shutdown_timer.stop();
    if (emu_thread) {
        emu_thread->disconnect();
        emu_thread->wait();
        emu_thread.reset();
    }

    if (shutdown_dialog) {
        shutdown_dialog->deleteLater();
        shutdown_dialog = nullptr;
    }

    emulation_running = false;
    UpdateDLCButtonText();

    discord_rpc->Update();
    Common::FeralGamemode::Stop();

    // The emulation is stopped, so closing the window or not does not matter anymore
    disconnect(render_window, &GRenderWindow::Closed, this, &MainWindow::OnStopGame);

    // Update the GUI
    UpdateMenuState();

    render_window->hide();
    loading_screen->hide();
    loading_screen->Clear();
    if (game_list->IsEmpty()) {
        game_list_placeholder->show();
    } else {
        game_list->show();
    }
    game_list->SetFilterFocus();
    tas_label->clear();
    input_subsystem->GetTas()->Stop();
    OnTasStateChanged();
    render_window->FinalizeCamera();

    QtCommon::system->GetFrontendAppletHolder().SetCurrentAppletId(Service::AM::AppletId::None);

    // Enable all controllers
    QtCommon::system->HIDCore().SetSupportedStyleTag({Core::HID::NpadStyleSet::All});

    render_window->removeEventFilter(render_window);
    render_window->setAttribute(Qt::WA_Hover, false);

    UpdateWindowTitle();

    // Disable status bar updates
    status_bar_update_timer.stop();
    shader_building_label->setVisible(false);
    res_scale_label->setVisible(false);
    emu_speed_label->setVisible(false);
    game_fps_label->setVisible(false);
    emu_frametime_label->setVisible(false);
    renderer_status_button->setEnabled(!UISettings::values.has_broken_vulkan);
    refresh_button->setEnabled(true);

    if (!firmware_label->text().isEmpty()) {
        firmware_label->setVisible(true);
    }

    current_game_path.clear();

    // When closing the game, destroy the GLWindow to clear the context after the game is closed
    render_window->ReleaseRenderTarget();

    // Enable game list
    game_list->setEnabled(true);

    Settings::RestoreGlobalState(QtCommon::system->IsPoweredOn());
    QtCommon::system->HIDCore().ReloadInputDevices();
    UpdateStatusButtons();
}

void MainWindow::ShutdownGame() {
    if (!emulation_running) {
        return;
    }

    // TODO(crueter): make this common as well (frontend_common?)
    play_time_manager->Stop();
    OnShutdownBegin();
    OnEmulationStopTimeExpired();
    OnEmulationStopped();
}

void MainWindow::StoreRecentFile(const QString& filename) {
    UISettings::values.recent_files.prepend(filename);
    UISettings::values.recent_files.removeDuplicates();
    while (UISettings::values.recent_files.size() > max_recent_files_item) {
        UISettings::values.recent_files.removeLast();
    }

    UpdateRecentFiles();
}

void MainWindow::UpdateRecentFiles() {
    const int num_recent_files =
        (std::min)(static_cast<int>(UISettings::values.recent_files.size()), max_recent_files_item);

    for (int i = 0; i < num_recent_files; i++) {
        const QString text = QStringLiteral("&%1. %2").arg(i + 1).arg(
            QFileInfo(UISettings::values.recent_files[i]).fileName());
        actions_recent_files[i]->setText(text);
        actions_recent_files[i]->setData(UISettings::values.recent_files[i]);
        actions_recent_files[i]->setToolTip(UISettings::values.recent_files[i]);
        actions_recent_files[i]->setVisible(true);
    }

    for (int j = num_recent_files; j < max_recent_files_item; ++j) {
        actions_recent_files[j]->setVisible(false);
    }

    // Enable the recent files menu if the list isn't empty
    ui->menu_recent_files->setEnabled(num_recent_files != 0);
}

void MainWindow::OnGameListLoadFile(QString game_path, u64 program_id) {
    auto params = ApplicationAppletParameters();
    params.program_id = program_id;

    BootGame(game_path, params);
}

// TODO(crueter): Common profile selector
void MainWindow::OnGameListOpenFolder(u64 program_id, GameListOpenTarget target,
                                      const std::string& game_path) {
    std::filesystem::path path;
    QString open_target;

    const auto [user_save_size, device_save_size] = [&game_path, &program_id] {
        const FileSys::PatchManager pm{program_id, QtCommon::system->GetFileSystemController(),
                                       QtCommon::system->GetContentProvider()};
        const auto control = pm.GetControlMetadata().first;
        if (control != nullptr) {
            return std::make_pair(control->GetDefaultNormalSaveSize(),
                                  control->GetDeviceSaveDataSize());
        } else {
            const auto file = Core::GetGameFileFromPath(QtCommon::vfs, game_path);
            const auto loader = Loader::GetLoader(*QtCommon::system, file);

            FileSys::NACP nacp{};
            loader->ReadControlData(nacp);
            return std::make_pair(nacp.GetDefaultNormalSaveSize(), nacp.GetDeviceSaveDataSize());
        }
    }();

    const bool has_user_save{user_save_size > 0};
    const bool has_device_save{device_save_size > 0};

    ASSERT_MSG(has_user_save != has_device_save, "Game uses both user and device savedata?");

    switch (target) {
    case GameListOpenTarget::SaveData: {
        open_target = tr("Save Data");
        const auto save_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::SaveDir);
        auto vfs_save_dir = QtCommon::vfs->OpenDirectory(Common::FS::PathToUTF8String(save_dir),
                                                         FileSys::OpenMode::Read);

        if (has_user_save) {
            // User save data
            const auto user_id = GetProfileID();
            assert(user_id);

            const auto user_save_data_path = FileSys::SaveDataFactory::GetFullPath(
                {}, vfs_save_dir, FileSys::SaveDataSpaceId::User, FileSys::SaveDataType::Account,
                program_id, user_id->AsU128(), 0);

            path = Common::FS::ConcatPathSafe(save_dir, user_save_data_path);
        } else {
            // Device save data
            const auto device_save_data_path = FileSys::SaveDataFactory::GetFullPath(
                {}, vfs_save_dir, FileSys::SaveDataSpaceId::User, FileSys::SaveDataType::Account,
                program_id, {}, 0);

            path = Common::FS::ConcatPathSafe(save_dir, device_save_data_path);
        }

        if (!Common::FS::CreateDirs(path)) {
            LOG_ERROR(Frontend, "Unable to create the directories for save data");
        }

        break;
    }
    case GameListOpenTarget::ModData: {
        open_target = tr("Mod Data");
        path = Common::FS::GetEdenPath(Common::FS::EdenPath::LoadDir) /
               fmt::format("{:016X}", program_id);
        break;
    }
    default:
        UNIMPLEMENTED();
        break;
    }

    const QString qpath = QString::fromStdString(Common::FS::PathToUTF8String(path));
    const QDir dir(qpath);
    if (!dir.exists()) {
        QMessageBox::warning(this, tr("Error Opening %1 Folder").arg(open_target),
                             tr("Folder does not exist!"));
        return;
    }
    LOG_INFO(Frontend, "Opening {} path for program_id={:016x}", open_target.toStdString(),
             program_id);
    QDesktopServices::openUrl(QUrl::fromLocalFile(qpath));
}

static bool RomFSRawCopy(size_t total_size, size_t& read_size, QProgressDialog& dialog,
                         const FileSys::VirtualDir& src, const FileSys::VirtualDir& dest,
                         bool full) {
    if (src == nullptr || dest == nullptr || !src->IsReadable() || !dest->IsWritable())
        return false;
    if (dialog.wasCanceled())
        return false;

    std::vector<u8> buffer(CopyBufferSize);
    auto last_timestamp = std::chrono::steady_clock::now();

    const auto QtRawCopy = [&](const FileSys::VirtualFile& src_file,
                               const FileSys::VirtualFile& dest_file) {
        if (src_file == nullptr || dest_file == nullptr) {
            return false;
        }
        if (!dest_file->Resize(src_file->GetSize())) {
            return false;
        }

        for (std::size_t i = 0; i < src_file->GetSize(); i += buffer.size()) {
            if (dialog.wasCanceled()) {
                dest_file->Resize(0);
                return false;
            }

            using namespace std::literals::chrono_literals;
            const auto new_timestamp = std::chrono::steady_clock::now();

            if ((new_timestamp - last_timestamp) > 33ms) {
                last_timestamp = new_timestamp;
                dialog.setValue(
                    static_cast<int>((std::min)(read_size, total_size) * 100 / total_size));
                QCoreApplication::processEvents();
            }

            const auto read = src_file->Read(buffer.data(), buffer.size(), i);
            dest_file->Write(buffer.data(), read, i);

            read_size += read;
        }

        return true;
    };

    if (full) {
        for (const auto& file : src->GetFiles()) {
            const auto out = VfsDirectoryCreateFileWrapper(dest, file->GetName());
            if (!QtRawCopy(file, out))
                return false;
        }
    }

    for (const auto& dir : src->GetSubdirectories()) {
        const auto out = dest->CreateSubdirectory(dir->GetName());
        if (!RomFSRawCopy(total_size, read_size, dialog, dir, out, full))
            return false;
    }

    return true;
}

// TODO(crueter): All this can be transfered to qt_common
// Aldoe I need to decide re: message boxes for QML
// translations_common? strings_common? qt_strings? who knows
void MainWindow::OnGameListRemoveInstalledEntry(u64 program_id,
                                                QtCommon::Game::InstalledEntryType type) {
    const QString entry_question = [type] {
        switch (type) {
        case QtCommon::Game::InstalledEntryType::Game:
            return tr("Remove Installed Game Contents?");
        case QtCommon::Game::InstalledEntryType::Update:
            return tr("Remove Installed Game Update?");
        case QtCommon::Game::InstalledEntryType::AddOnContent:
            return tr("Remove Installed Game DLC?");
        default:
            return QStringLiteral("Remove Installed Game <Invalid Type>?");
        }
    }();

    if (!question(this, tr("Remove Entry"), entry_question, QMessageBox::Yes | QMessageBox::No,
                  QMessageBox::No)) {
        return;
    }

    // TODO(crueter): move this to QtCommon (populate async?)
    switch (type) {
    case QtCommon::Game::InstalledEntryType::Game:
        QtCommon::Game::RemoveBaseContent(program_id, type);
        [[fallthrough]];
    case QtCommon::Game::InstalledEntryType::Update:
        QtCommon::Game::RemoveUpdateContent(program_id, type);
        if (type != QtCommon::Game::InstalledEntryType::Game) {
            break;
        }
        [[fallthrough]];
    case QtCommon::Game::InstalledEntryType::AddOnContent:
        QtCommon::Game::RemoveAddOnContent(program_id, type);
        break;
    }
    Common::FS::RemoveDirRecursively(Common::FS::GetEdenPath(Common::FS::EdenPath::CacheDir) /
                                     "game_list");
    game_list->PopulateAsync(UISettings::values.game_dirs);
}

void MainWindow::OnGameListRemoveFile(u64 program_id, QtCommon::Game::GameListRemoveTarget target,
                                      const std::string& game_path) {
    const QString question = [target] {
        switch (target) {
        case QtCommon::Game::GameListRemoveTarget::GlShaderCache:
            return tr("Delete OpenGL Transferable Shader Cache?");
        case QtCommon::Game::GameListRemoveTarget::VkShaderCache:
            return tr("Delete Vulkan Transferable Shader Cache?");
        case QtCommon::Game::GameListRemoveTarget::AllShaderCache:
            return tr("Delete All Transferable Shader Caches?");
        case QtCommon::Game::GameListRemoveTarget::CustomConfiguration:
            return tr("Remove Custom Game Configuration?");
        case QtCommon::Game::GameListRemoveTarget::CacheStorage:
            return tr("Remove Cache Storage?");
        default:
            return QString{};
        }
    }();

    if (!MainWindow::question(this, tr("Remove File"), question, QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No)) {
        return;
    }

    switch (target) {
    case QtCommon::Game::GameListRemoveTarget::VkShaderCache:
        QtCommon::Game::RemoveVulkanDriverPipelineCache(program_id);
        [[fallthrough]];
    case QtCommon::Game::GameListRemoveTarget::GlShaderCache:
        QtCommon::Game::RemoveTransferableShaderCache(program_id, target);
        break;
    case QtCommon::Game::GameListRemoveTarget::AllShaderCache:
        QtCommon::Game::RemoveAllTransferableShaderCaches(program_id);
        break;
    case QtCommon::Game::GameListRemoveTarget::CustomConfiguration:
        QtCommon::Game::RemoveCustomConfiguration(program_id, game_path);
        break;
    case QtCommon::Game::GameListRemoveTarget::CacheStorage:
        QtCommon::Game::RemoveCacheStorage(program_id);
        break;
    }
}

void MainWindow::OnGameListSetPlayTime(u64 program_id) {
    const u64 current_play_time = play_time_manager->GetPlayTime(program_id);

    SetPlayTimeDialog dialog(this, current_play_time);

    if (dialog.exec() == QDialog::Accepted) {
        const u64 total_seconds = dialog.GetTotalSeconds();
        play_time_manager->SetPlayTime(program_id, total_seconds);
        game_list->PopulateAsync(UISettings::values.game_dirs);
    }
}

void MainWindow::OnGameListRemovePlayTimeData(u64 program_id) {
    if (QMessageBox::question(this, tr("Remove Play Time Data"), tr("Reset play time?"),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    play_time_manager->ResetProgramPlayTime(program_id);
    game_list->PopulateAsync(UISettings::values.game_dirs);
}

void MainWindow::OnGameListDumpRomFS(u64 program_id, const std::string& game_path,
                                     DumpRomFSTarget target) {
    const auto failed = [this] {
        QMessageBox::warning(this, tr("RomFS Extraction Failed!"),
                             tr("There was an error copying the RomFS files or the user "
                                "cancelled the operation."));
    };

    const auto loader = Loader::GetLoader(
        *QtCommon::system, QtCommon::vfs->OpenFile(game_path, FileSys::OpenMode::Read));
    if (loader == nullptr) {
        failed();
        return;
    }

    FileSys::VirtualFile packed_update_raw{};
    loader->ReadUpdateRaw(packed_update_raw);

    const auto& installed = QtCommon::system->GetContentProvider();

    u64 title_id{};
    u8 raw_type{};
    if (!SelectRomFSDumpTarget(installed, program_id, &title_id, &raw_type)) {
        failed();
        return;
    }

    const auto type = static_cast<FileSys::ContentRecordType>(raw_type);
    const auto base_nca = installed.GetEntry(title_id, type);
    if (!base_nca) {
        failed();
        return;
    }

    const FileSys::NCA update_nca{packed_update_raw, nullptr};
    if (type != FileSys::ContentRecordType::Program ||
        update_nca.GetStatus() != Loader::ResultStatus::ErrorMissingBKTRBaseRomFS ||
        update_nca.GetTitleId() != FileSys::GetUpdateTitleID(title_id)) {
        packed_update_raw = {};
    }

    const auto base_romfs = base_nca->GetRomFS();
    const auto dump_dir =
        target == DumpRomFSTarget::Normal
            ? Common::FS::GetEdenPath(Common::FS::EdenPath::DumpDir)
            : Common::FS::GetEdenPath(Common::FS::EdenPath::SDMCDir) / "atmosphere" / "contents";
    const auto romfs_dir = fmt::format("{:016X}/romfs", title_id);

    const auto path = Common::FS::PathToUTF8String(dump_dir / romfs_dir);

    const FileSys::PatchManager pm{title_id, QtCommon::system->GetFileSystemController(),
                                   installed};
    auto romfs = pm.PatchRomFS(base_nca.get(), base_romfs, type, packed_update_raw, false);

    const auto out = VfsFilesystemCreateDirectoryWrapper(path, FileSys::OpenMode::ReadWrite);

    if (out == nullptr) {
        failed();
        QtCommon::vfs->DeleteDirectory(path);
        return;
    }

    bool ok = false;
    const QStringList selections{tr("Full"), tr("Skeleton")};
    const auto res = QInputDialog::getItem(
        this, tr("Select RomFS Dump Mode"),
        tr("Please select the how you would like the RomFS dumped.<br>Full will copy all of the "
           "files into the new directory while <br>skeleton will only create the directory "
           "structure."),
        selections, 0, false, &ok);
    if (!ok) {
        failed();
        QtCommon::vfs->DeleteDirectory(path);
        return;
    }

    const auto extracted = FileSys::ExtractRomFS(romfs);
    if (extracted == nullptr) {
        failed();
        return;
    }

    const auto full = res == selections.constFirst();

    // The expected required space is the size of the RomFS + 1 GiB
    const auto minimum_free_space = romfs->GetSize() + 0x40000000;

    if (full && Common::FS::GetFreeSpaceSize(path) < minimum_free_space) {
        QMessageBox::warning(this, tr("RomFS Extraction Failed!"),
                             tr("There is not enough free space at %1 to extract the RomFS. Please "
                                "free up space or select a different dump directory at "
                                "Emulation > Configure > System > Filesystem > Dump Root")
                                 .arg(QString::fromStdString(path)));
        return;
    }

    QProgressDialog progress(tr("Extracting RomFS..."), tr("Cancel"), 0, 100, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(100);
    progress.setAutoClose(false);
    progress.setAutoReset(false);

    size_t read_size = 0;

    if (RomFSRawCopy(romfs->GetSize(), read_size, progress, extracted, out, full)) {
        progress.close();
        QMessageBox::information(this, tr("RomFS Extraction Succeeded!"),
                                 tr("The operation completed successfully."));
        QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(path)));
    } else {
        progress.close();
        failed();
        QtCommon::vfs->DeleteDirectory(path);
    }
}

// END
void MainWindow::OnGameListVerifyIntegrity(const std::string& game_path) {
    QtCommon::Content::VerifyGameContents(game_path);
}

void MainWindow::OnGameListCopyTID(u64 program_id) {
    QClipboard* clipboard = QGuiApplication::clipboard();
    clipboard->setText(QString::fromStdString(fmt::format("{:016X}", program_id)));
}

void MainWindow::OnGameListNavigateToGamedbEntry(u64 program_id,
                                                 const CompatibilityList& compatibility_list) {
    const auto it = FindMatchingCompatibilityEntry(compatibility_list, program_id);

    QString directory;
    if (it != compatibility_list.end()) {
        directory = it->second.second;
    }

    QDesktopServices::openUrl(QUrl(
        QStringLiteral(
            "https://www.emuready.com/listings?emulatorIds=43bfc023-ec22-422d-8324-048a8ec9f28f") +
        directory));
}

void MainWindow::OnGameListCreateShortcut(u64 program_id, const std::string& game_path,
                                          const QtCommon::Game::ShortcutTarget target) {
    // Create shortcu
    std::string arguments = fmt::format("-g \"{:s}\"", game_path);

    QtCommon::Game::CreateShortcut(game_path, program_id, "", target, arguments, true);
}

void MainWindow::OnGameListOpenDirectory(const QString& directory) {
    // TODO(crueter): QtCommon
    std::filesystem::path fs_path;
    if (directory == QStringLiteral("SDMC")) {
        fs_path =
            Common::FS::GetEdenPath(Common::FS::EdenPath::SDMCDir) / "Nintendo/Contents/registered";
    } else if (directory == QStringLiteral("UserNAND")) {
        fs_path =
            Common::FS::GetEdenPath(Common::FS::EdenPath::NANDDir) / "user/Contents/registered";
    } else if (directory == QStringLiteral("SysNAND")) {
        fs_path =
            Common::FS::GetEdenPath(Common::FS::EdenPath::NANDDir) / "system/Contents/registered";
    } else {
        fs_path = directory.toStdString();
    }

    const auto qt_path = QString::fromStdString(Common::FS::PathToUTF8String(fs_path));

    if (!Common::FS::IsDir(fs_path)) {
        QMessageBox::critical(this, tr("Error Opening %1").arg(qt_path),
                              tr("Folder does not exist!"));
        return;
    }

    QDesktopServices::openUrl(QUrl::fromLocalFile(qt_path));
}

void MainWindow::OnGameListAddDirectory() {
    const QString dir_path = QFileDialog::getExistingDirectory(this, tr("Select Directory"));
    if (dir_path.isEmpty()) {
        return;
    }

    UISettings::GameDir game_dir{dir_path.toStdString(), false, true};
    if (!UISettings::values.game_dirs.contains(game_dir)) {
        UISettings::values.game_dirs.append(game_dir);
        game_list->PopulateAsync(UISettings::values.game_dirs);
    } else {
        LOG_WARNING(Frontend, "Selected directory is already in the game list");
    }

    OnSaveConfig();
}

void MainWindow::OnGameListShowList(bool show) {
    if (emulation_running && ui->action_Single_Window_Mode->isChecked())
        return;
    game_list->setVisible(show);
    game_list_placeholder->setVisible(!show);
};

void MainWindow::OnGameListOpenPerGameProperties(u64 title_id, const std::string& file) {
    static std::mutex mutex;
    static std::set<u64> loading_games;

    {
        std::lock_guard lock(mutex);
        if (loading_games.contains(title_id)) {
            return; // Deduplicate rapid clicks
        }
        loading_games.insert(title_id);
    }

    std::thread([this, title_id, file]() {
        // AppLoader parsing is NO LONGER needed to get title_id! We already have it!
        // So we can instantly open the properties dialog.
        
        QMetaObject::invokeMethod(this, [this, title_id, file]() {
            {
                std::lock_guard lock(mutex);
                loading_games.erase(title_id);
            }
            OpenPerGameConfiguration(title_id, file);
        }, Qt::QueuedConnection);
    }).detach();
}

void MainWindow::OnLinkToRyujinx(const u64& program_id) {
    namespace fs = std::filesystem;

    fs::path ryu_dir;

    // find an existing Ryujinx linked path in config.ini; if it exists, use it as a "hint"
    // If it's not defined in config.ini, use default
    const fs::path existing_path =
        UISettings::values.ryujinx_link_paths
            .value(program_id, QDir(Common::FS::GetLegacyPath(Common::FS::RyujinxDir)))
            .filesystemAbsolutePath();

    // this function also prompts the user to manually specify a portable location
    ryu_dir = QtCommon::FS::GetRyujinxSavePath(existing_path, program_id);

    if (ryu_dir.empty())
        return;

    const std::string user_id = GetProfileIDString();
    if (user_id.empty())
        return;

    const std::string hex_program = fmt::format("{:016X}", program_id);

    const fs::path eden_dir = FrontendCommon::DataManager::GetDataDir(
                                  FrontendCommon::DataManager::DataDir::Saves, user_id) /
                              hex_program;

    // CheckUnlink basically just checks to see if one or both are linked, and prompts the user to
    // unlink if this is the case.
    // If it returns false, neither dir is linked so it's fine to continue
    if (!QtCommon::FS::CheckUnlink(eden_dir, ryu_dir)) {
        RyujinxDialog dialog(eden_dir, ryu_dir, this);
        if (dialog.exec() == QDialog::Accepted) {
            UISettings::values.ryujinx_link_paths.insert(
                program_id,
                QString::fromStdString(Common::FS::GetRyuPathFromSavePath(ryu_dir).string()));
        }
    } else {
        UISettings::values.ryujinx_link_paths.remove(program_id);
    }

    config->SaveAllValues();
}

void MainWindow::OnMenuLoadFile() {
    if (is_load_file_select_active) {
        return;
    }

    is_load_file_select_active = true;
    const QString extensions =
        QStringLiteral("*.")
            .append(GameList::supported_file_extensions.join(QStringLiteral(" *.")))
            .append(QStringLiteral(" main"));
    const QString file_filter = tr("Switch Executable (%1);;All Files (*.*)",
                                   "%1 is an identifier for the Switch executable file extensions.")
                                    .arg(extensions);
    const QString filename = QFileDialog::getOpenFileName(
        this, tr("Load File"), QString::fromStdString(UISettings::values.roms_path), file_filter);
    is_load_file_select_active = false;

    if (filename.isEmpty()) {
        return;
    }

    UISettings::values.roms_path = QFileInfo(filename).path().toStdString();
    BootGame(filename, ApplicationAppletParameters());
}

void MainWindow::OnMenuLoadFolder() {
    const QString dir_path =
        QFileDialog::getExistingDirectory(this, tr("Open Extracted ROM Directory"));

    if (dir_path.isNull()) {
        return;
    }

    const QDir dir{dir_path};
    const QStringList matching_main = dir.entryList({QStringLiteral("main")}, QDir::Files);
    if (matching_main.size() == 1) {
        BootGame(dir.path() + QDir::separator() + matching_main[0], ApplicationAppletParameters());
    } else {
        QMessageBox::warning(this, tr("Invalid Directory Selected"),
                             tr("The directory you have selected does not contain a 'main' file."));
    }
}

void MainWindow::IncrementInstallProgress() {
    install_progress->setValue(install_progress->value() + 1);
}

void MainWindow::OnMenuInstallToNAND() {
    const QString file_filter =
        tr("Installable Switch File (*.nca *.nsp *.xci *.ncz *.nsz *.xcz);;Nintendo Content Archive "
           "(*.nca);;Nintendo Submission Package (*.nsp);;NX Cartridge "
           "Image (*.xci);;Nintendo Content Archive Zstd (*.ncz);;Nintendo Submission Package Zstd (*.nsz);;NX Cartridge Zstd (*.xcz)");

    QStringList filenames = QFileDialog::getOpenFileNames(
        this, tr("Install Files"), QString::fromStdString(UISettings::values.roms_path),
        file_filter);

    if (filenames.isEmpty()) {
        return;
    }

    InstallDialog installDialog(this, filenames);
    if (installDialog.exec() == QDialog::Rejected) {
        return;
    }

    const QStringList files = installDialog.GetFiles();

    if (files.isEmpty()) {
        return;
    }

    // Save folder location of the first selected file
    UISettings::values.roms_path = QFileInfo(filenames[0]).path().toStdString();

    int remaining = filenames.size();

    // This would only overflow above 2^51 bytes (2.252 PB)
    int total_size = 0;
    for (const QString& file : files) {
        total_size += static_cast<int>(QFile(file).size() / CopyBufferSize);
    }
    if (total_size < 0) {
        LOG_CRITICAL(Frontend, "Attempting to install too many files, aborting.");
        return;
    }

    QStringList new_files{};         // Newly installed files that do not yet exist in the NAND
    QStringList overwritten_files{}; // Files that overwrote those existing in the NAND
    QStringList failed_files{};      // Files that failed to install due to errors
    bool detected_base_install{};    // Whether a base game was attempted to be installed

    ui->action_Install_File_NAND->setEnabled(false);

    install_progress = new QProgressDialog(QString{}, tr("Cancel"), 0, total_size, this);
    install_progress->setWindowFlags(windowFlags() & ~Qt::WindowMaximizeButtonHint);
    install_progress->setAttribute(Qt::WA_DeleteOnClose, true);
    install_progress->setFixedWidth(installDialog.GetMinimumWidth() + 40);
    install_progress->show();

    for (const QString& file : files) {
        install_progress->setWindowTitle(tr("%n file(s) remaining", "", remaining));
        install_progress->setLabelText(
            tr("Installing file \"%1\"...").arg(QFileInfo(file).fileName()));

        QFuture<ContentManager::InstallResult> future;
        ContentManager::InstallResult result;

        if (file.endsWith(QStringLiteral("nsp"), Qt::CaseInsensitive)) {
            const auto progress_callback = [this](size_t size, size_t progress) {
                emit UpdateInstallProgress();
                if (install_progress->wasCanceled()) {
                    return true;
                }
                return false;
            };
            future = QtConcurrent::run([&file, progress_callback] {
                return ContentManager::InstallNSP(*QtCommon::system, *QtCommon::vfs,
                                                  file.toStdString(), progress_callback);
            });

            while (!future.isFinished()) {
                QCoreApplication::processEvents();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            result = future.result();

        } else {
            result = InstallNCA(file);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        switch (result) {
        case ContentManager::InstallResult::Success:
            new_files.append(QFileInfo(file).fileName());
            break;
        case ContentManager::InstallResult::Overwrite:
            overwritten_files.append(QFileInfo(file).fileName());
            break;
        case ContentManager::InstallResult::Failure:
            failed_files.append(QFileInfo(file).fileName());
            break;
        case ContentManager::InstallResult::BaseInstallAttempted:
            failed_files.append(QFileInfo(file).fileName());
            detected_base_install = true;
            break;
        }

        --remaining;
    }

    install_progress->close();

    if (detected_base_install) {
        QMessageBox::warning(
            this, tr("Install Results"),
            tr("To avoid possible conflicts, we discourage users from installing base games to the "
               "NAND.\nPlease, only use this feature to install updates and DLC."));
    }

    const QString install_results =
        (new_files.isEmpty() ? QString{}
                             : tr("%n file(s) were newly installed\n", "", new_files.size())) +
        (overwritten_files.isEmpty()
             ? QString{}
             : tr("%n file(s) were overwritten\n", "", overwritten_files.size())) +
        (failed_files.isEmpty() ? QString{}
                                : tr("%n file(s) failed to install\n", "", failed_files.size()));

    QMessageBox::information(this, tr("Install Results"), install_results);
    Common::FS::RemoveDirRecursively(Common::FS::GetEdenPath(Common::FS::EdenPath::CacheDir) /
                                     "game_list");
    game_list->PopulateAsync(UISettings::values.game_dirs);
    ui->action_Install_File_NAND->setEnabled(true);
}

ContentManager::InstallResult MainWindow::InstallNCA(const QString& filename) {
    const QStringList tt_options{tr("System Application"),
                                 tr("System Archive"),
                                 tr("System Application Update"),
                                 tr("Firmware Package (Type A)"),
                                 tr("Firmware Package (Type B)"),
                                 tr("Game"),
                                 tr("Game Update"),
                                 tr("Game DLC"),
                                 tr("Delta Title")};
    bool ok;
    const auto item = QInputDialog::getItem(
        this, tr("Select NCA Install Type..."),
        tr("Please select the type of title you would like to install this NCA as:\n(In "
           "most instances, the default 'Game' is fine.)"),
        tt_options, 5, false, &ok);

    auto index = tt_options.indexOf(item);
    if (!ok || index == -1) {
        QMessageBox::warning(this, tr("Failed to Install"),
                             tr("The title type you selected for the NCA is invalid."));
        return ContentManager::InstallResult::Failure;
    }

    // If index is equal to or past Game, add the jump in TitleType.
    if (index >= 5) {
        index += static_cast<size_t>(FileSys::TitleType::Application) -
                 static_cast<size_t>(FileSys::TitleType::FirmwarePackageB);
    }

    const bool is_application = index >= static_cast<s32>(FileSys::TitleType::Application);
    const auto& fs_controller = QtCommon::system->GetFileSystemController();
    auto* registered_cache = is_application ? fs_controller.GetUserNANDContents()
                                            : fs_controller.GetSystemNANDContents();

    const auto progress_callback = [this](size_t size, size_t progress) {
        emit UpdateInstallProgress();
        if (install_progress->wasCanceled()) {
            return true;
        }
        return false;
    };
    return ContentManager::InstallNCA(*QtCommon::vfs, filename.toStdString(), *registered_cache,
                                      static_cast<FileSys::TitleType>(index), progress_callback);
}

void MainWindow::OnMenuRecentFile() {
    QAction* action = qobject_cast<QAction*>(sender());
    assert(action);

    const QString filename = action->data().toString();
    if (QFileInfo::exists(filename)) {
        BootGame(filename, ApplicationAppletParameters());
    } else {
        // Display an error message and remove the file from the list.
        QMessageBox::information(this, tr("File not found"),
                                 tr("File \"%1\" not found").arg(filename));

        UISettings::values.recent_files.removeOne(filename);
        UpdateRecentFiles();
    }
}

void MainWindow::OnStartGame() {
    PreventOSSleep();

    emu_thread->SetRunning(true);

    UpdateMenuState();
    OnTasStateChanged();

    play_time_manager->SetProgramId(QtCommon::system->GetApplicationProcessProgramID());
    play_time_manager->Start();

    discord_rpc->Update();
    Common::FeralGamemode::Start();
}

void MainWindow::OnRestartGame() {
    if (!QtCommon::system->IsPoweredOn()) {
        return;
    }

    if (ConfirmShutdownGame()) {
        // Make a copy since ShutdownGame edits game_path
        const auto current_game = QString(current_game_path);
        ShutdownGame();
        BootGame(current_game, ApplicationAppletParameters());
    }
}

void MainWindow::OnPauseGame() {
    emu_thread->SetRunning(false);
    play_time_manager->Stop();
    UpdateMenuState();
    AllowOSSleep();
    Common::FeralGamemode::Stop();
}

void MainWindow::OnPauseContinueGame() {
    if (emulation_running) {
        if (emu_thread->IsRunning()) {
            OnPauseGame();
        } else {
            OnStartGame();
        }
    }
}

void MainWindow::OnSaveState() {
    if (emu_thread == nullptr || !emu_thread->IsRunning()) {
        return;
    }
    const QString extensions{QStringLiteral("*.stt")};
    const QString file_filter = tr("Save State File (%1);; All Files (*.*)").arg(extensions);
    const QString filename = QFileDialog::getSaveFileName(this, tr("Save State"), {}, file_filter);
    if (!filename.isEmpty()) {
        if (QtCommon::system->SaveState(filename.toStdString())) {
            QMessageBox::information(this, tr("Save State"), tr("State successfully saved."));
        } else {
            QMessageBox::warning(this, tr("Error"), tr("Failed to save state."));
        }
    }
}

void MainWindow::OnLoadState() {
    if (emu_thread == nullptr || !emu_thread->IsRunning()) {
        return;
    }
    const QString extensions{QStringLiteral("*.stt")};
    const QString file_filter = tr("Save State File (%1);; All Files (*.*)").arg(extensions);
    const QString filename = QFileDialog::getOpenFileName(this, tr("Load State"), {}, file_filter);
    if (!filename.isEmpty()) {
        if (QtCommon::system->LoadState(filename.toStdString())) {
            QMessageBox::information(this, tr("Load State"), tr("State successfully loaded."));
        } else {
            QMessageBox::warning(this, tr("Error"), tr("Failed to load state."));
        }
    }
}

void MainWindow::OnStopGame() {
    if (ConfirmShutdownGame()) {
        play_time_manager->Stop();
        // Update game list to show new play time
        game_list->PopulateAsync(UISettings::values.game_dirs);
        if (OnShutdownBegin()) {
            OnShutdownBeginDialog();
        } else {
            OnEmulationStopped();
        }
    }
}

bool MainWindow::ConfirmShutdownGame() {
    if (UISettings::values.confirm_before_stopping.GetValue() == ConfirmStop::Ask_Always) {
        if (QtCommon::system->GetExitLocked()) {
            if (!ConfirmForceLockedExit()) {
                return false;
            }
        } else {
            if (!ConfirmChangeGame()) {
                return false;
            }
        }
    } else {
        if (UISettings::values.confirm_before_stopping.GetValue() ==
                ConfirmStop::Ask_Based_On_Game &&
            QtCommon::system->GetExitLocked()) {
            if (!ConfirmForceLockedExit()) {
                return false;
            }
        }
    }
    return true;
}

void MainWindow::OnLoadComplete() {
    loading_screen->OnLoadComplete();

    perf_overlay = new PerformanceOverlay(this);
    perf_overlay->setVisible(ui->action_Show_Performance_Overlay->isChecked());

    connect(perf_overlay, &PerformanceOverlay::closed, perf_overlay,
            [this]() { ui->action_Show_Performance_Overlay->setChecked(false); });
}

void MainWindow::OnExecuteProgram(std::size_t program_index) {
    ShutdownGame();

    auto params = ApplicationAppletParameters();
    params.program_index = static_cast<s32>(program_index);
    params.launch_type = Service::AM::LaunchType::ApplicationInitiated;
    BootGame(last_filename_booted, params);
}

void MainWindow::OnExit() {
    ShutdownGame();
}

void MainWindow::OnSaveConfig() {
    QtCommon::system->ApplySettings();
    config->SaveAllValues();
}

void MainWindow::ErrorDisplayDisplayError(QString error_code, QString error_text) {
    error_applet = new OverlayDialog(render_window, *QtCommon::system, error_code, error_text,
                                     QString{}, tr("OK"), Qt::AlignLeft | Qt::AlignVCenter);
    SCOPE_EXIT {
        error_applet->deleteLater();
        error_applet = nullptr;
    };
    error_applet->exec();

    emit ErrorDisplayFinished();
}

void MainWindow::ErrorDisplayRequestExit() {
    if (error_applet) {
        error_applet->reject();
    }
}

void MainWindow::OnMenuReportCompatibility() {
    QtCommon::Frontend::Critical(
        tr("Function Disabled"),
        tr("Compatibility list reporting is currently disabled. Check back later!"));

    // #if defined(ARCHITECTURE_x86_64) && !defined(__APPLE__)
    //     const auto& caps = Common::GetCPUCaps();
    //     const bool has_fma = caps.fma || caps.fma4;
    //     const auto processor_count = std::thread::hardware_concurrency();
    //     const bool has_4threads = processor_count == 0 || processor_count >= 4;
    //     const bool has_8gb_ram = Common::GetMemInfo().TotalPhysicalMemory >= 8_GiB;
    //     const bool has_broken_vulkan = UISettings::values.has_broken_vulkan;

    //     if (!has_fma || !has_4threads || !has_8gb_ram || has_broken_vulkan) {
    //         QMessageBox::critical(this, tr("Hardware requirements not met"),
    //                               tr("Your system does not meet the recommended hardware
    //                               requirements. "
    //                                  "Compatibility reporting has been disabled."));
    //         return;
    //     }

    //     if (!Settings::values.eden_token.GetValue().empty() &&
    //         !Settings::values.eden_username.GetValue().empty()) {
    //     } else {
    //         QMessageBox::critical(
    //             this, tr("Missing yuzu Account"),
    //             tr("In order to submit a game compatibility test case, you must set up your web
    //             token "
    //                "and "
    //                "username.<br><br/>To link your eden account, go to Emulation &gt;
    //                Configuration "
    //                "&gt; "
    //                "Web."));
    //     }
    // #else
    //     QMessageBox::critical(this, tr("Hardware requirements not met"),
    //                           tr("Your system does not meet the recommended hardware
    //                           requirements. "
    //                              "Compatibility reporting has been disabled."));
    // #endif
}

void MainWindow::OpenURL(const QUrl& url) {
    const bool open = QDesktopServices::openUrl(url);
    if (!open) {
        QMessageBox::warning(this, tr("Error opening URL"),
                             tr("Unable to open the URL \"%1\".").arg(url.toString()));
    }
}

void MainWindow::OnOpenModsPage() {
    OpenURL(QUrl(QStringLiteral("https://github.com/eden-emulator/eden-mod-archive")));
}

void MainWindow::OnOpenQuickstartGuide() {
    OpenURL(QUrl(QStringLiteral("https://github.com/eden-emulator/eden/wiki")));
}

void MainWindow::OnOpenFAQ() {
    OpenURL(QUrl(QStringLiteral("https://github.com/eden-emulator/eden/issues")));
}

void MainWindow::ToggleFullscreen() {
    if (!emulation_running) {
        return;
    }
    if (ui->action_Fullscreen->isChecked()) {
        ShowFullscreen();
    } else {
        HideFullscreen();
    }
}

// We're going to return the screen that the given window has the most pixels on
static QScreen* GuessCurrentScreen(QWidget* window) {
    const QList<QScreen*> screens = QGuiApplication::screens();
    return *std::max_element(
        screens.cbegin(), screens.cend(), [window](const QScreen* left, const QScreen* right) {
            const QSize left_size = left->geometry().intersected(window->geometry()).size();
            const QSize right_size = right->geometry().intersected(window->geometry()).size();
            return (left_size.height() * left_size.width()) <
                   (right_size.height() * right_size.width());
        });
}

bool MainWindow::UsingExclusiveFullscreen() {
    return Settings::values.fullscreen_mode.GetValue() == Settings::FullscreenMode::Exclusive ||
           QGuiApplication::platformName() == QStringLiteral("wayland") ||
           QGuiApplication::platformName() == QStringLiteral("wayland-egl");
}

void MainWindow::ShowFullscreen() {
    const auto show_fullscreen = [this](QWidget* window) {
        if (UsingExclusiveFullscreen()) {
            window->showFullScreen();
            return;
        }
        window->hide();
        window->setWindowFlags(window->windowFlags() | Qt::FramelessWindowHint);
        const auto screen_geometry = GuessCurrentScreen(window)->geometry();
        window->setGeometry(screen_geometry.x(), screen_geometry.y(), screen_geometry.width(),
                            screen_geometry.height() + 1);
        window->raise();
        window->showNormal();
    };

    if (ui->action_Single_Window_Mode->isChecked()) {
        UISettings::values.geometry = saveGeometry();

        ui->menubar->hide();
        statusBar()->hide();

        show_fullscreen(this);
    } else {
        UISettings::values.renderwindow_geometry = render_window->saveGeometry();
        show_fullscreen(render_window);
    }
}

void MainWindow::HideFullscreen() {
    if (ui->action_Single_Window_Mode->isChecked()) {
        if (UsingExclusiveFullscreen()) {
            showNormal();
            restoreGeometry(UISettings::values.geometry);
        } else {
            hide();
            setWindowFlags(windowFlags() & ~Qt::FramelessWindowHint);
            restoreGeometry(UISettings::values.geometry);
            raise();
            show();
        }

        statusBar()->setVisible(ui->action_Show_Status_Bar->isChecked());
        ui->menubar->show();
    } else {
        if (UsingExclusiveFullscreen()) {
            render_window->showNormal();
            render_window->restoreGeometry(UISettings::values.renderwindow_geometry);
        } else {
            render_window->hide();
            render_window->setWindowFlags(windowFlags() & ~Qt::FramelessWindowHint);
            render_window->restoreGeometry(UISettings::values.renderwindow_geometry);
            render_window->raise();
            render_window->show();
        }
    }
}

void MainWindow::ToggleWindowMode() {
    if (ui->action_Single_Window_Mode->isChecked()) {
        // Render in the main window...
        render_window->BackupGeometry();
        ui->horizontalLayout->addWidget(render_window);
        render_window->setFocusPolicy(Qt::StrongFocus);
        if (emulation_running) {
            render_window->setVisible(true);
            render_window->setFocus();
            game_list->hide();
        }

    } else {
        // Render in a separate window...
        ui->horizontalLayout->removeWidget(render_window);
        render_window->setParent(nullptr);
        render_window->setFocusPolicy(Qt::NoFocus);
        if (emulation_running) {
            render_window->setVisible(true);
            render_window->RestoreGeometry();
            game_list->show();
        }
    }
}

void MainWindow::ResetWindowSize(u32 width, u32 height) {
    const auto aspect_ratio = Layout::EmulationAspectRatio(Settings::values.aspect_ratio.GetValue(),
                                                           float(height) / width);
    if (!ui->action_Single_Window_Mode->isChecked()) {
        render_window->resize(height / aspect_ratio, height);
    } else {
        const bool show_status_bar = ui->action_Show_Status_Bar->isChecked();
        const auto status_bar_height = show_status_bar ? statusBar()->height() : 0;
        resize(height / aspect_ratio, height + menuBar()->height() + status_bar_height);
    }
}

void MainWindow::ResetWindowSize720() {
    ResetWindowSize(Layout::ScreenUndocked::Width, Layout::ScreenUndocked::Height);
}

void MainWindow::ResetWindowSize900() {
    ResetWindowSize(1600U, 900U);
}

void MainWindow::ResetWindowSize1080() {
    ResetWindowSize(Layout::ScreenDocked::Width, Layout::ScreenDocked::Height);
}

void MainWindow::SetGameListMode(Settings::GameListMode mode) {
    ui->action_Grid_View->setChecked(mode == Settings::GameListMode::GridView);
    ui->action_Tree_View->setChecked(mode == Settings::GameListMode::TreeView);

    UISettings::values.game_list_mode = mode;
    ui->action_Show_Game_Name->setEnabled(mode == Settings::GameListMode::GridView);

    CheckIconSize();
    game_list->ResetViewMode();
}

void MainWindow::SetGridView() {
    SetGameListMode(Settings::GameListMode::GridView);
}

void MainWindow::SetTreeView() {
    SetGameListMode(Settings::GameListMode::TreeView);
}

void MainWindow::CheckIconSize() {
    // When in grid view mode, with text off
    // there is no point in having icons turned off..
    auto actions = game_size_actions->actions();
    if (UISettings::values.game_list_mode.GetValue() == Settings::GameListMode::GridView &&
        !UISettings::values.show_game_name.GetValue()) {
        u32 newSize = UISettings::values.game_icon_size.GetValue();
        if (newSize == 0) {
            newSize = 64;
            UISettings::values.game_icon_size.SetValue(newSize);
        }

        // Then disable the "none" action and update that menu.
        for (size_t i = 0; i < default_game_icon_sizes.size(); i++) {
            const auto current_size = newSize;
            const auto size = default_game_icon_sizes[i].first;
            if (current_size == size)
                actions.at(i)->setChecked(true);
        }

        // Update this if you add anything before None.
        actions.at(0)->setEnabled(false);
    } else {
        actions.at(0)->setEnabled(true);
    }
}

void MainWindow::ToggleShowGameName() {
    auto& setting = UISettings::values.show_game_name;
    const bool newValue = !setting.GetValue();
    ui->action_Show_Game_Name->setChecked(newValue);
    setting.SetValue(newValue);

    CheckIconSize();

    game_list->RefreshGameDirectory();
}

void MainWindow::OnConfigure() {
    const auto old_theme = UISettings::values.theme;
    const bool old_discord_presence = UISettings::values.enable_discord_presence.GetValue();
    const auto old_language_index = Settings::values.language_index.GetValue();
    const bool old_gamemode = UISettings::values.enable_gamemode.GetValue();
#ifdef __unix__
    const bool old_force_x11 = UISettings::values.gui_force_x11.GetValue();
#endif

    Settings::SetConfiguringGlobal(true);
    ConfigureDialog configure_dialog(this, hotkey_registry, input_subsystem.get(),
                                     vk_device_records, *QtCommon::system,
                                     !multiplayer_state->IsHostingPublicRoom());
    connect(&configure_dialog, &ConfigureDialog::LanguageChanged, this,
            &MainWindow::OnLanguageChanged);
    connect(&configure_dialog, &ConfigureDialog::ExternalContentDirsChanged, this,
            &MainWindow::OnGameListRefresh);

    const auto result = configure_dialog.exec();
    if (result != QDialog::Accepted && !UISettings::values.configuration_applied &&
        !UISettings::values.reset_to_defaults) {
        // Runs if the user hit Cancel or closed the window, and did not ever press the Apply button
        // or `Reset to Defaults` button
        return;
    } else if (result == QDialog::Accepted) {
        // Only apply new changes if user hit Okay
        // This is here to avoid applying changes if the user hit Apply, made some changes, then hit
        // Cancel
        configure_dialog.ApplyConfiguration();
    } else if (UISettings::values.reset_to_defaults) {
        LOG_INFO(Frontend, "Resetting all settings to defaults");
        if (!Common::FS::RemoveFile(config->GetConfigFilePath())) {
            LOG_WARNING(Frontend, "Failed to remove configuration file");
        }
        if (!Common::FS::RemoveDirContentsRecursively(
                Common::FS::GetEdenPath(Common::FS::EdenPath::ConfigDir) / "custom")) {
            LOG_WARNING(Frontend, "Failed to remove custom configuration files");
        }
        if (!Common::FS::RemoveDirRecursively(
                Common::FS::GetEdenPath(Common::FS::EdenPath::CacheDir) / "game_list")) {
            LOG_WARNING(Frontend, "Failed to remove game metadata cache files");
        }

        // Explicitly save the game directories, since reinitializing config does not explicitly do
        // so.
        QVector<UISettings::GameDir> old_game_dirs = std::move(UISettings::values.game_dirs);
        QVector<u64> old_favorited_ids = std::move(UISettings::values.favorited_ids);

        Settings::values.disabled_addons.clear();

        config = std::make_unique<QtConfig>();
        UISettings::values.reset_to_defaults = false;

        UISettings::values.game_dirs = std::move(old_game_dirs);
        UISettings::values.favorited_ids = std::move(old_favorited_ids);

        InitializeRecentFileMenuActions();

        SetDefaultUIGeometry();
        RestoreUIState();
    }
    InitializeHotkeys();

    if (UISettings::values.theme != old_theme) {
        UpdateUITheme();
    }
    if (UISettings::values.enable_discord_presence.GetValue() != old_discord_presence) {
        SetDiscordEnabled(UISettings::values.enable_discord_presence.GetValue());
    }
    if (UISettings::values.enable_gamemode.GetValue() != old_gamemode) {
        SetGamemodeEnabled(UISettings::values.enable_gamemode.GetValue());
    }
#ifdef __unix__
    if (UISettings::values.gui_force_x11.GetValue() != old_force_x11) {
        GraphicsBackend::SetForceX11(UISettings::values.gui_force_x11.GetValue());
    }
#endif

    if (!multiplayer_state->IsHostingPublicRoom()) {
        multiplayer_state->UpdateCredentials();
    }

    emit UpdateThemedIcons();

    const auto reload = UISettings::values.is_game_list_reload_pending.exchange(false);
    if (reload || Settings::values.language_index.GetValue() != old_language_index) {
        game_list->PopulateAsync(UISettings::values.game_dirs);
    }

    UISettings::values.configuration_applied = false;

    config->SaveAllValues();

    if ((UISettings::values.hide_mouse || Settings::values.mouse_panning) && emulation_running) {
        render_window->installEventFilter(render_window);
        render_window->setAttribute(Qt::WA_Hover, true);
    } else {
        render_window->removeEventFilter(render_window);
        render_window->setAttribute(Qt::WA_Hover, false);
    }

    if (UISettings::values.hide_mouse) {
        mouse_hide_timer.start();
    }

    // Restart camera config
    if (emulation_running) {
        render_window->FinalizeCamera();
        render_window->InitializeCamera();
    }

    if (!UISettings::values.has_broken_vulkan) {
        renderer_status_button->setEnabled(!emulation_running);
    }

    UpdateStatusButtons();
    controller_dialog->refreshConfiguration();
    QtCommon::system->ApplySettings();
}

void MainWindow::OnConfigureTas() {
    ConfigureTasDialog dialog(this);
    const auto result = dialog.exec();

    if (result != QDialog::Accepted && !UISettings::values.configuration_applied) {
        Settings::RestoreGlobalState(QtCommon::system->IsPoweredOn());
        return;
    } else if (result == QDialog::Accepted) {
        dialog.ApplyConfiguration();
        OnSaveConfig();
    }
}

void MainWindow::OnTasStartStop() {
    if (!emulation_running) {
        return;
    }

    // Disable system buttons to prevent TAS from executing a hotkey
    auto* controller =
        QtCommon::system->HIDCore().GetEmulatedController(Core::HID::NpadIdType::Player1);
    controller->ResetSystemButtons();

    input_subsystem->GetTas()->StartStop();
    OnTasStateChanged();
}

void MainWindow::OnTasRecord() {
    if (!emulation_running) {
        return;
    }
    if (is_tas_recording_dialog_active) {
        return;
    }

    // Disable system buttons to prevent TAS from recording a hotkey
    auto* controller =
        QtCommon::system->HIDCore().GetEmulatedController(Core::HID::NpadIdType::Player1);
    controller->ResetSystemButtons();

    const bool is_recording = input_subsystem->GetTas()->Record();
    if (!is_recording) {
        if (Settings::values.tas_show_recording_dialog.GetValue()) {
            is_tas_recording_dialog_active = true;

            bool answer = question(this, tr("TAS Recording"), tr("Overwrite file of player 1?"),
                                   QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

            input_subsystem->GetTas()->SaveRecording(answer);
            is_tas_recording_dialog_active = false;
        } else {
            input_subsystem->GetTas()->SaveRecording(true);
        }
    }
    OnTasStateChanged();
}

void MainWindow::OnTasReset() {
    input_subsystem->GetTas()->Reset();
}

void MainWindow::OnToggleDockedMode() {
    const bool is_docked = Settings::IsDockedMode();
    auto* player_1 =
        QtCommon::system->HIDCore().GetEmulatedController(Core::HID::NpadIdType::Player1);
    auto* handheld =
        QtCommon::system->HIDCore().GetEmulatedController(Core::HID::NpadIdType::Handheld);

    if (!is_docked && handheld->IsConnected()) {
        QMessageBox::warning(this, tr("Invalid config detected"),
                             tr("Handheld controller can't be used on docked mode. Pro "
                                "controller will be selected."));
        handheld->Disconnect();
        player_1->SetNpadStyleIndex(Core::HID::NpadStyleIndex::Fullkey);
        player_1->Connect();
        controller_dialog->refreshConfiguration();
    }

    Settings::values.use_docked_mode.SetValue(is_docked ? Settings::ConsoleMode::Handheld
                                                        : Settings::ConsoleMode::Docked);
    UpdateDockedButton();
    OnDockedModeChanged(is_docked, !is_docked, *QtCommon::system);
}

void MainWindow::OnToggleGpuAccuracy() {
    switch (Settings::values.gpu_accuracy.GetValue()) {
    case Settings::GpuAccuracy::Low:
        Settings::values.gpu_accuracy.SetValue(Settings::GpuAccuracy::Medium);
        break;
    case Settings::GpuAccuracy::Medium:
        Settings::values.gpu_accuracy.SetValue(Settings::GpuAccuracy::High);
        break;
    case Settings::GpuAccuracy::High:
        Settings::values.gpu_accuracy.SetValue(Settings::GpuAccuracy::Low);
        break;
    }

    QtCommon::system->ApplySettings();
    UpdateGPUAccuracyButton();
}

void MainWindow::OnMute() {
    Settings::values.audio_muted = !Settings::values.audio_muted;
    UpdateVolumeUI();
}

void MainWindow::OnDecreaseVolume() {
    Settings::values.audio_muted = false;
    const auto current_volume = static_cast<s32>(Settings::values.volume.GetValue());
    int step = 5;
    if (current_volume <= 30) {
        step = 2;
    }
    if (current_volume <= 6) {
        step = 1;
    }
    Settings::values.volume.SetValue((std::max)(current_volume - step, 0));
    UpdateVolumeUI();
}

void MainWindow::OnIncreaseVolume() {
    Settings::values.audio_muted = false;
    const auto current_volume = static_cast<s32>(Settings::values.volume.GetValue());
    int step = 5;
    if (current_volume < 30) {
        step = 2;
    }
    if (current_volume < 6) {
        step = 1;
    }
    Settings::values.volume.SetValue(current_volume + step);
    UpdateVolumeUI();
}

void MainWindow::OnToggleAdaptingFilter() {
    auto filter = Settings::values.scaling_filter.GetValue();
    filter = static_cast<Settings::ScalingFilter>(static_cast<u32>(filter) + 1);
    if (static_cast<u32>(filter) > static_cast<u32>(Settings::EnumMetadata<Settings::ScalingFilter>::GetLast())) {
        filter = Settings::ScalingFilter::NearestNeighbor;
    }
    Settings::values.scaling_filter.SetValue(filter);
    filter_status_button->setChecked(true);
    UpdateFilterText();
}

void MainWindow::OnToggleGraphicsAPI() {
    auto api = Settings::values.renderer_backend.GetValue();
    switch (api) {
#ifdef HAS_OPENGL
    case Settings::RendererBackend::Vulkan:
        api = Settings::RendererBackend::OpenGL_GLSL;
        break;
    case Settings::RendererBackend::OpenGL_GLSL:
        api = Settings::RendererBackend::OpenGL_GLSL;
        break;
    case Settings::RendererBackend::OpenGL_SPIRV:
        api = Settings::RendererBackend::OpenGL_GLASM;
        break;
    case Settings::RendererBackend::OpenGL_GLASM:
        api = Settings::RendererBackend::Null;
        break;
#else
    case Settings::RendererBackend::Vulkan:
        api = Settings::RendererBackend::Null;
        break;
#endif
    case Settings::RendererBackend::Null:
        api = Settings::RendererBackend::Vulkan;
        break;
    default:
        break;
    }
    Settings::values.renderer_backend.SetValue(api);
    renderer_status_button->setChecked(api == Settings::RendererBackend::Vulkan);
    UpdateAPIText();
}

void MainWindow::OnConfigurePerGame() {
    const u64 title_id = QtCommon::system->GetApplicationProcessProgramID();
    OpenPerGameConfiguration(title_id, current_game_path.toStdString());
}

void MainWindow::OpenPerGameConfiguration(u64 title_id, const std::string& file_name) {
    const auto v_file = Core::GetGameFileFromPath(QtCommon::vfs, file_name);

    Settings::SetConfiguringGlobal(false);
    ConfigurePerGame dialog(this, title_id, file_name, vk_device_records, *QtCommon::system);
    dialog.LoadFromFile(v_file);

    const auto result = dialog.exec();

    if (result != QDialog::Accepted && !UISettings::values.configuration_applied) {
        Settings::RestoreGlobalState(QtCommon::system->IsPoweredOn());
        return;
    } else if (result == QDialog::Accepted) {
        dialog.ApplyConfiguration();
    }

    const auto reload = UISettings::values.is_game_list_reload_pending.exchange(false);
    if (reload) {
        OnGameListRefresh();
    }

    // Do not cause the global config to write local settings into the config file
    const bool is_powered_on = QtCommon::system->IsPoweredOn();
    Settings::RestoreGlobalState(is_powered_on);
    QtCommon::system->HIDCore().ReloadInputDevices();

    UISettings::values.configuration_applied = false;

    if (!is_powered_on) {
        config->SaveAllValues();
    }
}

void MainWindow::OnLoadAmiibo() {
    if (emu_thread == nullptr || !emu_thread->IsRunning()) {
        return;
    }
    if (is_amiibo_file_select_active) {
        return;
    }

    auto* virtual_amiibo = input_subsystem->GetVirtualAmiibo();

    // Remove amiibo if one is connected
    if (virtual_amiibo->GetCurrentState() == InputCommon::VirtualAmiibo::State::TagNearby) {
        virtual_amiibo->CloseAmiibo();
        QMessageBox::warning(this, tr("Amiibo"), tr("The current amiibo has been removed"));
        return;
    }

    if (virtual_amiibo->GetCurrentState() != InputCommon::VirtualAmiibo::State::WaitingForAmiibo) {
        QMessageBox::warning(this, tr("Error"), tr("The current game is not looking for amiibos"));
        return;
    }

    is_amiibo_file_select_active = true;
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, tr("Virtual Amiibo"), tr("Do you want to load Amiibo from an online URL?"), QMessageBox::Yes | QMessageBox::No);
    QString filename;
    if (reply == QMessageBox::Yes) {
        bool ok;
        QString text = QInputDialog::getText(this, tr("Virtual Amiibo API"), tr("Enter Amiibo direct URL (.bin):"), QLineEdit::Normal, QStringLiteral("https://"), &ok);
        if (ok && !text.isEmpty()) {
            filename = text;
        }
    } else {
        const QString extensions{QStringLiteral("*.bin")};
        const QString file_filter = tr("Amiibo File (%1);; All Files (*.*)").arg(extensions);
        filename = QFileDialog::getOpenFileName(this, tr("Load Amiibo"), {}, file_filter);
    }
    is_amiibo_file_select_active = false;

    if (filename.isEmpty()) {
        return;
    }

    LoadAmiibo(filename);
}

// TODO(crueter): does this need to be ported to QML?
bool MainWindow::question(QWidget* parent, const QString& title, const QString& text,
                          QMessageBox::StandardButtons buttons,
                          QMessageBox::StandardButton defaultButton) {
    QMessageBox* box_dialog = new QMessageBox(parent);
    box_dialog->setWindowTitle(title);
    box_dialog->setText(text);
    box_dialog->setStandardButtons(buttons);
    box_dialog->setDefaultButton(defaultButton);

    const bool is_russian = (QLocale().language() == QLocale::Russian);
    if (QAbstractButton* yesButton = box_dialog->button(QMessageBox::Yes)) {
        yesButton->setText(is_russian ? QStringLiteral("Да") : tr("Yes"));
    }
    if (QAbstractButton* noButton = box_dialog->button(QMessageBox::No)) {
        noButton->setText(is_russian ? QStringLiteral("Нет") : tr("No"));
    }
    if (QAbstractButton* okButton = box_dialog->button(QMessageBox::Ok)) {
        okButton->setText(is_russian ? QStringLiteral("ОК") : tr("OK"));
    }
    if (QAbstractButton* cancelButton = box_dialog->button(QMessageBox::Cancel)) {
        cancelButton->setText(is_russian ? QStringLiteral("Отмена") : tr("Cancel"));
    }

    ControllerNavigation* controller_navigation =
        new ControllerNavigation(QtCommon::system->HIDCore(), box_dialog);
    connect(controller_navigation, &ControllerNavigation::TriggerKeyboardEvent,
            [box_dialog](Qt::Key key) {
                QKeyEvent* event = new QKeyEvent(QEvent::KeyPress, key, Qt::NoModifier);
                QCoreApplication::postEvent(box_dialog, event);
            });
    int res = box_dialog->exec();

    controller_navigation->UnloadController();
    return res == QMessageBox::Yes;
}

void MainWindow::LoadAmiibo(const QString& filename) {
    auto* virtual_amiibo = input_subsystem->GetVirtualAmiibo();
    const QString title = tr("Error loading Amiibo data");
    // Remove amiibo if one is connected
    if (virtual_amiibo->GetCurrentState() == InputCommon::VirtualAmiibo::State::TagNearby) {
        virtual_amiibo->CloseAmiibo();
        QMessageBox::warning(this, tr("Amiibo"), tr("The current amiibo has been removed"));
        return;
    }

    switch (virtual_amiibo->LoadAmiibo(filename.toStdString())) {
    case InputCommon::VirtualAmiibo::Info::NotAnAmiibo:
        QMessageBox::warning(this, title, tr("The selected file is not a valid amiibo"));
        break;
    case InputCommon::VirtualAmiibo::Info::UnableToLoad:
        QMessageBox::warning(this, title, tr("The selected file is already on use"));
        break;
    case InputCommon::VirtualAmiibo::Info::WrongDeviceState:
        QMessageBox::warning(this, title, tr("The current game is not looking for amiibos"));
        break;
    case InputCommon::VirtualAmiibo::Info::Unknown:
        QMessageBox::warning(this, title, tr("An unknown error occurred"));
        break;
    default:
        break;
    }
}

void MainWindow::OnOpenRootDataFolder() {
    QtCommon::Game::OpenRootDataFolder();
}

void MainWindow::OnOpenNANDFolder() {
    QtCommon::Game::OpenNANDFolder();
}

void MainWindow::OnOpenSDMCFolder() {
    QtCommon::Game::OpenSDMCFolder();
}

void MainWindow::OnOpenModFolder() {
    QtCommon::Game::OpenModFolder();
}

void MainWindow::OnOpenLogFolder() {
    QtCommon::Game::OpenLogFolder();
}

void MainWindow::OnVerifyInstalledContents() {
    QtCommon::Content::VerifyInstalledContents();
}

void MainWindow::InstallFirmware(const QString& location, bool recursive) {
    QtCommon::Content::InstallFirmware(location, recursive);
    OnCheckFirmwareDecryption();
}

void MainWindow::OnInstallFirmware() {
    // Don't do this while emulation is running, that'd probably be a bad idea.
    if (emu_thread != nullptr && emu_thread->IsRunning()) {
        return;
    }

    // Check for installed keys, error out, suggest restart?
    if (!ContentManager::AreKeysPresent()) {
        QMessageBox::information(
            this, tr("Keys not installed"),
            tr("Install decryption keys and restart Eden before attempting to install firmware."));
        return;
    }

    const QString firmware_source_location = QFileDialog::getExistingDirectory(
        this, tr("Select Dumped Firmware Source Location"), {}, QFileDialog::ShowDirsOnly);
    if (firmware_source_location.isEmpty()) {
        return;
    }

    InstallFirmware(firmware_source_location);
}

void MainWindow::OnInstallFirmwareFromZIP() {
    // Don't do this while emulation is running, that'd probably be a bad idea.
    if (emu_thread != nullptr && emu_thread->IsRunning()) {
        return;
    }

    // Check for installed keys, error out, suggest restart?
    if (!ContentManager::AreKeysPresent()) {
        QMessageBox::information(
            this, tr("Keys not installed"),
            tr("Install decryption keys and restart Eden before attempting to install firmware."));
        return;
    }

    const QString firmware_zip_location = QFileDialog::getOpenFileName(
        this, tr("Select Dumped Firmware ZIP"), {}, tr("Zipped Archives (*.zip)"));
    if (firmware_zip_location.isEmpty()) {
        return;
    }

    const QString qCacheDir = QtCommon::Content::UnzipFirmwareToTmp(firmware_zip_location);

    // In this case, it has to be done recursively, since sometimes people
    // will pack it into a subdirectory after dumping
    if (!qCacheDir.isEmpty()) {
        InstallFirmware(qCacheDir, true);
        std::error_code ec;
        std::filesystem::remove_all(std::filesystem::temp_directory_path() / "eden" / "firmware",
                                    ec);

        if (ec) {
            QMessageBox::warning(this, tr("Firmware cleanup failed"),
                                 tr("Failed to clean up extracted firmware cache.\n"
                                    "Check write permissions in the system temp directory and try "
                                    "again.\nOS reported error: %1")
                                     .arg(QString::fromStdString(ec.message())));
        }
    }
}

void MainWindow::OnInstallDecryptionKeys() {
    // Don't do this while emulation is running.
    if (emu_thread != nullptr && emu_thread->IsRunning()) {
        return;
    }

    QtCommon::Content::InstallKeys();

    game_list->PopulateAsync(UISettings::values.game_dirs);
    OnCheckFirmwareDecryption();
}

void MainWindow::OnAbout() {
    AboutDialog aboutDialog(this);
    aboutDialog.exec();
}

void MainWindow::OnEdenDependencies() {
    DepsDialog depsDialog(this);
    depsDialog.exec();
}

void MainWindow::OnDataDialog() {
    DataDialog dataDialog(this);
    dataDialog.exec();

    // refresh stuff in case it was cleared
    OnGameListRefresh();
}

void MainWindow::OnToggleFilterBar() {
    game_list->SetFilterVisible(ui->action_Show_Filter_Bar->isChecked());
    if (ui->action_Show_Filter_Bar->isChecked()) {
        game_list->SetFilterFocus();
    } else {
        game_list->ClearFilter();
    }
}

void MainWindow::OnToggleStatusBar() {
    statusBar()->setVisible(ui->action_Show_Status_Bar->isChecked());
}

void MainWindow::OnTogglePerfOverlay() {
    if (perf_overlay) {
        perf_overlay->setVisible(ui->action_Show_Performance_Overlay->isChecked());
    }
}

void MainWindow::OnGameListRefresh() {
    // Resets metadata cache and reloads
    QtCommon::Game::ResetMetadata(false);
    game_list->RefreshGameDirectory();
    SetFirmwareVersion();
}

void MainWindow::LaunchFirmwareApplet(u64 raw_program_id,
                                      std::optional<Service::NFP::CabinetMode> cabinet_mode) {
    auto const program_id = Service::AM::AppletProgramId(raw_program_id);
    auto result = FirmwareManager::VerifyFirmware(*QtCommon::system.get());
    using namespace QtCommon::StringLookup;
    switch (result) {
    case FirmwareManager::ErrorFirmwareMissing:
        QMessageBox::warning(this, tr("No firmware available"),
                             Lookup(FwCheckErrorFirmwareMissing));
        return;
    case FirmwareManager::ErrorFirmwareCorrupted:
        QMessageBox::warning(this, tr("Firmware Corrupted"), Lookup(FwCheckErrorFirmwareCorrupted));
        return;
    default:
        break;
    }
    auto bis_system = QtCommon::system->GetFileSystemController().GetSystemNANDContents();
    if (auto applet_nca =
            bis_system->GetEntry(u64(program_id), FileSys::ContentRecordType::Program);
        applet_nca) {
        if (auto const applet_id =
                [program_id] {
                    using namespace Service::AM;
                    switch (program_id) {
                    case AppletProgramId::OverlayDisplay:
                        return AppletId::OverlayDisplay;
                    case AppletProgramId::QLaunch:
                        return AppletId::QLaunch;
                    case AppletProgramId::Starter:
                        return AppletId::Starter;
                    case AppletProgramId::Auth:
                        return AppletId::Auth;
                    case AppletProgramId::Cabinet:
                        return AppletId::Cabinet;
                    case AppletProgramId::Controller:
                        return AppletId::Controller;
                    case AppletProgramId::DataErase:
                        return AppletId::DataErase;
                    case AppletProgramId::Error:
                        return AppletId::Error;
                    case AppletProgramId::NetConnect:
                        return AppletId::NetConnect;
                    case AppletProgramId::ProfileSelect:
                        return AppletId::ProfileSelect;
                    case AppletProgramId::SoftwareKeyboard:
                        return AppletId::SoftwareKeyboard;
                    case AppletProgramId::MiiEdit:
                        return AppletId::MiiEdit;
                    case AppletProgramId::Web:
                        return AppletId::Web;
                    case AppletProgramId::Shop:
                        return AppletId::Shop;
                    case AppletProgramId::PhotoViewer:
                        return AppletId::PhotoViewer;
                    case AppletProgramId::Settings:
                        return AppletId::Settings;
                    case AppletProgramId::OfflineWeb:
                        return AppletId::OfflineWeb;
                    case AppletProgramId::LoginShare:
                        return AppletId::LoginShare;
                    case AppletProgramId::WebAuth:
                        return AppletId::WebAuth;
                    case AppletProgramId::MyPage:
                        return AppletId::MyPage;
                    default:
                        return AppletId::None;
                    }
                }();
            applet_id != Service::AM::AppletId::None) {
            QtCommon::system->GetFrontendAppletHolder().SetCurrentAppletId(applet_id);
            if (cabinet_mode)
                QtCommon::system->GetFrontendAppletHolder().SetCabinetMode(*cabinet_mode);
            // ?
            auto const filename = QString::fromStdString((applet_nca->GetFullPath()));
            UISettings::values.roms_path = QFileInfo(filename).path().toStdString();
            BootGame(filename, LibraryAppletParameters(u64(program_id), applet_id));
        } else {
            QMessageBox::warning(this, tr("Unknown applet"),
                                 tr("Applet doesn't map to a known value."));
        }
    } else {
        QMessageBox::warning(this, tr("Record not found"),
                             tr("Applet not found. Please reinstall firmware."));
    }
}

void MainWindow::OnCreateHomeMenuDesktopShortcut() {
    QtCommon::Game::CreateHomeMenuShortcut(QtCommon::Game::ShortcutTarget::Desktop);
}

void MainWindow::OnCreateHomeMenuApplicationMenuShortcut() {
    QtCommon::Game::CreateHomeMenuShortcut(QtCommon::Game::ShortcutTarget::Applications);
}

void MainWindow::OnCaptureScreenshot() {
    if (emu_thread == nullptr || !emu_thread->IsRunning()) {
        return;
    }

    const u64 title_id = QtCommon::system->GetApplicationProcessProgramID();
    const auto screenshot_path =
        QString::fromStdString(Common::FS::GetEdenPathString(Common::FS::EdenPath::ScreenshotsDir));
    const auto date =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_hh-mm-ss-zzz"));
    QString filename = QStringLiteral("%1/%2_%3.png")
                           .arg(screenshot_path)
                           .arg(title_id, 16, 16, QLatin1Char{'0'})
                           .arg(date);

    if (!Common::FS::CreateDir(screenshot_path.toStdString())) {
        return;
    }

#ifdef _WIN32
    if (UISettings::values.enable_screenshot_save_as) {
        OnPauseGame();
        filename = QFileDialog::getSaveFileName(this, tr("Capture Screenshot"), filename,
                                                tr("PNG Image (*.png)"));
        OnStartGame();
        if (filename.isEmpty()) {
            return;
        }
    }
#endif
    render_window->CaptureScreenshot(filename);
}

#ifdef ENABLE_UPDATE_CHECKER
void MainWindow::OnEmulatorUpdateAvailable() {
    std::optional<Common::Net::Release> version = update_future.result();
    if (!version)
        return;

    UpdateDialog dialog(version.value(), this);
    dialog.exec();
}
#endif

void MainWindow::UpdateWindowTitle(std::string_view title_name, std::string_view title_version,
                                   std::string_view gpu_vendor) {
    static const std::string build_id = std::string{Common::g_build_id};
    static const std::string yuzu_title =
        fmt::format("{} | {} | {}", std::string{Common::g_build_name},
                    std::string{Common::g_build_version}, std::string{Common::g_compiler_id});

    const auto override_title =
        fmt::format(fmt::runtime(std::string(Common::g_title_bar_format_idle)), build_id);
    const auto window_title = override_title.empty() ? yuzu_title : override_title;

    if (title_name.empty()) {
        setWindowTitle(QString::fromStdString(window_title));
    } else {
        const auto run_title = [window_title, title_name, title_version, gpu_vendor]() {
            if (title_version.empty()) {
                return fmt::format("{} | {} | {}", window_title, title_name, gpu_vendor);
            }
            return fmt::format("{} | {} | {} | {}", window_title, title_name, title_version,
                               gpu_vendor);
        }();
        setWindowTitle(QString::fromStdString(run_title));
    }
}

std::string MainWindow::CreateTASFramesString(
    std::array<size_t, InputCommon::TasInput::PLAYER_NUMBER> frames) const {
    std::string string = "";
    size_t maxPlayerIndex = 0;
    for (size_t i = 0; i < frames.size(); i++) {
        if (frames[i] != 0) {
            if (maxPlayerIndex != 0)
                string += ", ";
            while (maxPlayerIndex++ != i)
                string += "0, ";
            string += std::to_string(frames[i]);
        }
    }
    return string;
}

QString MainWindow::GetTasStateDescription() const {
    auto [tas_status, current_tas_frame, total_tas_frames] = input_subsystem->GetTas()->GetStatus();
    std::string tas_frames_string = CreateTASFramesString(total_tas_frames);
    switch (tas_status) {
    case InputCommon::TasInput::TasState::Running:
        return tr("TAS state: Running %1/%2")
            .arg(current_tas_frame)
            .arg(QString::fromStdString(tas_frames_string));
    case InputCommon::TasInput::TasState::Recording:
        return tr("TAS state: Recording %1").arg(total_tas_frames[0]);
    case InputCommon::TasInput::TasState::Stopped:
        return tr("TAS state: Idle %1/%2")
            .arg(current_tas_frame)
            .arg(QString::fromStdString(tas_frames_string));
    default:
        return tr("TAS State: Invalid");
    }
}

void MainWindow::OnTasStateChanged() {
    bool is_running = false;
    bool is_recording = false;
    if (emulation_running) {
        const InputCommon::TasInput::TasState tas_status =
            std::get<0>(input_subsystem->GetTas()->GetStatus());
        is_running = tas_status == InputCommon::TasInput::TasState::Running;
        is_recording = tas_status == InputCommon::TasInput::TasState::Recording;
    }

    ui->action_TAS_Start->setText(is_running ? tr("&Stop Running") : tr("&Start"));
    ui->action_TAS_Record->setText(is_recording ? tr("Stop R&ecording") : tr("R&ecord"));

    ui->action_TAS_Start->setEnabled(emulation_running);
    ui->action_TAS_Record->setEnabled(emulation_running);
    ui->action_TAS_Reset->setEnabled(emulation_running);
}

void MainWindow::UpdateStatusBar() {
    if (emu_thread == nullptr || !QtCommon::system->IsPoweredOn()) {
        status_bar_update_timer.stop();
        return;
    }

    if (Settings::values.tas_enable) {
        tas_label->setText(GetTasStateDescription());
    } else {
        tas_label->clear();
    }

    auto results = QtCommon::system->GetAndResetPerfStats();
    auto& shader_notify = QtCommon::system->GPU().ShaderNotify();
    const int shaders_building = shader_notify.ShadersBuilding();

    emit statsUpdated(results, shader_notify);

    if (shaders_building > 0) {
        shader_building_label->setText(tr("Building: %n shader(s)", "", shaders_building));
        shader_building_label->setVisible(true);
    } else {
        shader_building_label->setVisible(false);
    }

    const auto res_info = Settings::values.resolution_info;
    const auto res_scale = res_info.up_factor;
    res_scale_label->setText(
        tr("Scale: %1x", "%1 is the resolution scaling factor").arg(res_scale));

    if (Settings::values.use_speed_limit.GetValue()) {
        emu_speed_label->setText(tr("Speed: %1% / %2%")
                                     .arg(results.emulation_speed * 100.0, 0, 'f', 0)
                                     .arg(Settings::SpeedLimit()));
    } else {
        emu_speed_label->setText(tr("Speed: %1%").arg(results.emulation_speed * 100.0, 0, 'f', 0));
    }

    QString fpsText = tr("Game: %1 FPS").arg(std::round(results.average_game_fps), 0, 'f', 0);
    if (!m_fpsSuffix.isEmpty())
        fpsText = fpsText % QStringLiteral(" (%1)").arg(m_fpsSuffix);

    game_fps_label->setText(fpsText);

    emu_frametime_label->setText(tr("Frame: %1 ms").arg(results.frametime * 1000.0, 0, 'f', 2));

    res_scale_label->setVisible(true);
    emu_speed_label->setVisible(!Settings::values.use_multi_core.GetValue());
    game_fps_label->setVisible(true);
    emu_frametime_label->setVisible(true);
    firmware_label->setVisible(false);
}

void MainWindow::OnResolutionScaleClicked() {
    QMenu menu(this);
    
    struct ResolutionOption {
        QString label;
        Settings::ResolutionSetup setup;
    };
    
    const std::vector<ResolutionOption> options = {
        {tr("0.25x (180p/270p)"), Settings::ResolutionSetup::Res1_4X},
        {tr("0.5x (360p/540p)"), Settings::ResolutionSetup::Res1_2X},
        {tr("0.75x (540p/810p)"), Settings::ResolutionSetup::Res3_4X},
        {tr("1x (720p/1080p) [Normal]"), Settings::ResolutionSetup::Res1X},
        {tr("1.25x (900p/1350p)"), Settings::ResolutionSetup::Res5_4X},
        {tr("1.5x (1080p/1620p)"), Settings::ResolutionSetup::Res3_2X},
        {tr("2x (1440p/2160p) [2K/4K]"), Settings::ResolutionSetup::Res2X},
        {tr("3x (2160p/3240p) [4K]"), Settings::ResolutionSetup::Res3X},
        {tr("4x (2880p/4320p) [8K]"), Settings::ResolutionSetup::Res4X},
        {tr("5x"), Settings::ResolutionSetup::Res5X},
        {tr("6x"), Settings::ResolutionSetup::Res6X},
        {tr("7x"), Settings::ResolutionSetup::Res7X},
        {tr("8x"), Settings::ResolutionSetup::Res8X},
    };
    
    const auto current_setup = Settings::values.resolution_setup.GetValue();
    
    for (const auto& opt : options) {
        QAction* action = menu.addAction(opt.label);
        action->setCheckable(true);
        action->setChecked(current_setup == opt.setup);
        
        connect(action, &QAction::triggered, this, [this, setup = opt.setup]() {
            Settings::values.resolution_setup.SetValue(setup);
            Settings::UpdateRescalingInfo();
            UpdateStatusBar();
        });
    }
    
    menu.exec(res_scale_label->mapToGlobal(QPoint(0, -menu.sizeHint().height())));
}

void MainWindow::UpdateGPUAccuracyButton() {
    const auto gpu_accuracy = Settings::values.gpu_accuracy.GetValue();
    const auto gpu_accuracy_text =
        ConfigurationShared::gpu_accuracy_texts_map.find(gpu_accuracy)->second;
    gpu_accuracy_button->setText(
        QCoreApplication::translate("MainWindow", gpu_accuracy_text.toUtf8().constData()).toUpper());
    gpu_accuracy_button->setChecked(gpu_accuracy != Settings::GpuAccuracy::Low);
}

void MainWindow::UpdateDockedButton() {
    const auto console_mode = Settings::values.use_docked_mode.GetValue();
    dock_status_button->setChecked(Settings::IsDockedMode());
    const auto console_mode_text =
        ConfigurationShared::use_docked_mode_texts_map.find(console_mode)->second;
    dock_status_button->setText(
        QCoreApplication::translate("MainWindow", console_mode_text.toUtf8().constData()).toUpper());
}

void MainWindow::UpdateAirplaneModeButton() {
    const bool is_enabled = Settings::values.airplane_mode.GetValue();
    airplane_mode_button->setChecked(is_enabled);

    std::string lang = UISettings::values.language.GetValue();
    if (lang.empty()) {
        lang = QLocale().name().left(2).toStdString();
    }
    std::transform(lang.begin(), lang.end(), lang.begin(), ::tolower);

    QString text;
    if (lang.rfind("ru", 0) == 0) {
        text = is_enabled ? QStringLiteral("РЕЖИМ ПОЛЕТА: ВКЛ.") : QStringLiteral("РЕЖИМ ПОЛЕТА: ВЫКЛ.");
    } else if (lang.rfind("uk", 0) == 0) {
        text = is_enabled ? QStringLiteral("РЕЖИМ ПОЛЬОТУ: УВІМК.") : QStringLiteral("РЕЖИМ ПОЛЬОТУ: ВИМК.");
    } else if (lang.rfind("de", 0) == 0) {
        text = is_enabled ? QStringLiteral("FLUGMODUS: AN") : QStringLiteral("FLUGMODUS: AUS");
    } else if (lang.rfind("fr", 0) == 0) {
        text = is_enabled ? QStringLiteral("MODE AVION : ACTIVÉ") : QStringLiteral("MODE AVION : DÉSACTIVÉ");
    } else if (lang.rfind("es", 0) == 0) {
        text = is_enabled ? QStringLiteral("MODO AVIÓN: ACTIVADO") : QStringLiteral("MODO AVIÓN: DESACTIVADO");
    } else if (lang.rfind("it", 0) == 0) {
        text = is_enabled ? QStringLiteral("MODALITÀ AEREO: ON") : QStringLiteral("MODALITÀ AEREO: OFF");
    } else if (lang.rfind("pt", 0) == 0) {
        text = is_enabled ? QStringLiteral("MODO AVIÃO: ATIVADO") : QStringLiteral("MODO AVIÃO: DESATIVADO");
    } else if (lang.rfind("tr", 0) == 0) {
        text = is_enabled ? QStringLiteral("UÇAK MODU: AÇIK") : QStringLiteral("UÇAK MODU: KAPALI");
    } else if (lang.rfind("zh_tw", 0) == 0 || lang == "zh-tw") {
        text = is_enabled ? QStringLiteral("飛行模式：開啟") : QStringLiteral("飛行模式：關閉");
    } else if (lang.rfind("zh", 0) == 0) {
        text = is_enabled ? QStringLiteral("飞行模式：开启") : QStringLiteral("飞行模式：关闭");
    } else if (lang.rfind("ja", 0) == 0) {
        text = is_enabled ? QStringLiteral("機内モード：オン") : QStringLiteral("機内モード：オフ");
    } else if (lang.rfind("ko", 0) == 0) {
        text = is_enabled ? QStringLiteral("비행기 모드: 켜짐") : QStringLiteral("비행기 모드: 꺼짐");
    } else {
        text = is_enabled ? tr("AIRPLANE: ON") : tr("AIRPLANE: OFF");
    }
    airplane_mode_button->setText(text);
}

void MainWindow::OnToggleAirplaneMode() {
    Settings::values.airplane_mode.SetValue(!Settings::values.airplane_mode.GetValue());
    QtCommon::system->ApplySettings();
    UpdateAirplaneModeButton();
}

void MainWindow::UpdateAPIText() {
    const auto api = Settings::values.renderer_backend.GetValue();
    const auto renderer_status_text =
        ConfigurationShared::renderer_backend_texts_map.find(api)->second;
    renderer_status_button->setText(renderer_status_text.toUpper());
}

void MainWindow::UpdateFilterText() {
    const auto filter = Settings::values.scaling_filter.GetValue();
    const auto filter_text = ConfigurationShared::scaling_filter_texts_map.find(filter)->second;
    filter_status_button->setText(filter == Settings::ScalingFilter::Fsr ? tr("FSR")
                                                                         : filter_text.toUpper());
}

void MainWindow::UpdateAAText() {
    const auto aa_mode = Settings::values.anti_aliasing.GetValue();
    const auto aa_text = ConfigurationShared::anti_aliasing_texts_map.find(aa_mode)->second;
    aa_status_button->setText(aa_mode == Settings::AntiAliasing::None
                                  ? QStringLiteral(QT_TRANSLATE_NOOP("MainWindow", "NO AA"))
                                  : aa_text.toUpper());
}

void MainWindow::UpdateVolumeUI() {
    const auto volume_value = static_cast<int>(Settings::values.volume.GetValue());
    volume_slider->setValue(volume_value);
    if (Settings::values.audio_muted) {
        volume_button->setChecked(false);
        volume_button->setText(tr("VOLUME: MUTE"));
    } else {
        volume_button->setChecked(true);
        volume_button->setText(tr("VOLUME: %1%", "Volume percentage (e.g. 50%)").arg(volume_value));
    }
}

void MainWindow::UpdateStatusButtons() {
    renderer_status_button->setChecked(Settings::values.renderer_backend.GetValue() ==
                                       Settings::RendererBackend::Vulkan);
    UpdateAPIText();
    UpdateGPUAccuracyButton();
    UpdateDockedButton();
    UpdateAirplaneModeButton();
    UpdateFilterText();
    UpdateAAText();
    UpdateVolumeUI();
}

// TODO(crueter): Use this for game list stuff
void MainWindow::UpdateUISettings() {
    if (!ui->action_Fullscreen->isChecked()) {
        UISettings::values.geometry = saveGeometry();
        UISettings::values.renderwindow_geometry = render_window->saveGeometry();
    }
    UISettings::values.state = saveState();
    UISettings::values.single_window_mode = ui->action_Single_Window_Mode->isChecked();
    UISettings::values.fullscreen = ui->action_Fullscreen->isChecked();
    UISettings::values.show_filter_bar = ui->action_Show_Filter_Bar->isChecked();
    UISettings::values.show_status_bar = ui->action_Show_Status_Bar->isChecked();
    UISettings::values.show_perf_overlay = ui->action_Show_Performance_Overlay->isChecked();

    UISettings::values.first_start = false;

    Settings::values.enable_overlay = ui->action_Enable_Overlay_Applet->isChecked();
}

void MainWindow::UpdateInputDrivers() {
    if (!input_subsystem) {
        return;
    }
    input_subsystem->PumpEvents();
}

void MainWindow::HideMouseCursor() {
    if (emu_thread == nullptr && UISettings::values.hide_mouse) {
        mouse_hide_timer.stop();
        ShowMouseCursor();
        return;
    }
    render_window->setCursor(QCursor(Qt::BlankCursor));
}

void MainWindow::ShowMouseCursor() {
    render_window->unsetCursor();
    if (emu_thread != nullptr && UISettings::values.hide_mouse) {
        mouse_hide_timer.start();
    }
}

void MainWindow::OnMouseActivity() {
    if (!Settings::values.mouse_panning) {
        ShowMouseCursor();
    }
}

void MainWindow::OnCheckFirmwareDecryption() {
    if (!ContentManager::AreKeysPresent()) {
        const auto res = QtCommon::Frontend::Warning(
            tr("Derivation Components Missing"),
            tr("Decryption keys are missing. Install them now?"),
            QtCommon::Frontend::StandardButton::Yes | QtCommon::Frontend::StandardButton::No);

        if (res == QtCommon::Frontend::StandardButton::Yes)
            OnInstallDecryptionKeys();
    }

    SetFirmwareVersion();
    UpdateMenuState();
}

#ifdef __unix__
void MainWindow::OnCheckGraphicsBackend() {
    const QString platformName = QGuiApplication::platformName();
    const QByteArray qtPlatform = qgetenv("QT_QPA_PLATFORM");

    if (platformName == QStringLiteral("xcb") || qtPlatform == "xcb")
        return;

    const bool isWayland =
        platformName.startsWith(QStringLiteral("wayland"), Qt::CaseInsensitive) ||
        qtPlatform.startsWith("wayland");
    if (!isWayland)
        return;

    const bool currently_hidden = UISettings::values.gui_hide_backend_warning.GetValue();
    if (currently_hidden)
        return;

    QMessageBox msgbox(this);
    msgbox.setWindowTitle(tr("Wayland Detected!"));
    msgbox.setText(
        tr("Wayland is known to have significant performance issues and mysterious bugs.\n"
           "It's recommended to use X11 instead.\n\n"
           "Would you like to force it for future launches?"));
    msgbox.setIcon(QMessageBox::Warning);

    QPushButton* okButton = msgbox.addButton(tr("Use X11"), QMessageBox::AcceptRole);
    msgbox.addButton(tr("Continue with Wayland"), QMessageBox::RejectRole);
    msgbox.setDefaultButton(okButton);

    QCheckBox* cb = new QCheckBox(tr("Don't show again"), &msgbox);
    cb->setChecked(currently_hidden);
    msgbox.setCheckBox(cb);

    msgbox.exec();

    const bool hide = cb->isChecked();
    if (hide != currently_hidden) {
        UISettings::values.gui_hide_backend_warning.SetValue(hide);
    }

    if (msgbox.clickedButton() == okButton) {
        UISettings::values.gui_force_x11.SetValue(true);
        GraphicsBackend::SetForceX11(true);
        QMessageBox::information(this, tr("Restart Required"),
                                 tr("Restart Eden to apply the X11 backend."));
    }
}
#endif

bool MainWindow::CheckFirmwarePresence() {
    return FirmwareManager::CheckFirmwarePresence(*QtCommon::system.get());
}

void MainWindow::SetFirmwareVersion() {
    const auto pair = FirmwareManager::GetFirmwareVersion(*QtCommon::system.get());
    const auto firmware_data = pair.first;
    const auto result = pair.second;

    if (result.IsError() || !CheckFirmwarePresence()) {
        LOG_INFO(Frontend, "Installed firmware: No firmware available");
        ui->menu_Applets->setEnabled(false);
        ui->menu_Create_Shortcuts->setEnabled(false);
        ui->menu_cabinet_applet->setEnabled(false);
        firmware_label->setVisible(false);
        return;
    }

    firmware_label->setVisible(true);
    ui->menu_Applets->setEnabled(true);
    ui->menu_Create_Shortcuts->setEnabled(true);
    ui->menu_cabinet_applet->setEnabled(true);

    const std::string display_version(firmware_data.display_version.data());
    const std::string display_title(firmware_data.display_title.data());

    LOG_INFO(Frontend, "Installed firmware: {}", display_version);

    firmware_label->setText(QString::fromStdString(display_version));
    firmware_label->setToolTip(QString::fromStdString(display_title));
}

void MainWindow::SetFPSSuffix() {
    switch (Settings::values.current_speed_mode.GetValue()) {
    case Settings::SpeedMode::Slow:
        m_fpsSuffix = tr("Slow");
        break;
    case Settings::SpeedMode::Turbo:
        m_fpsSuffix = tr("Turbo");
        break;
    case Settings::SpeedMode::Standard:
        const bool limited = Settings::values.use_speed_limit.GetValue();
        m_fpsSuffix = limited ? QString{} : tr("Unlocked");
        break;
    }
}

bool MainWindow::SelectRomFSDumpTarget(const FileSys::ContentProvider& installed, u64 program_id,
                                       u64* selected_title_id, u8* selected_content_record_type) {
    using ContentInfo = std::tuple<u64, FileSys::TitleType, FileSys::ContentRecordType>;
    boost::container::flat_set<ContentInfo> available_title_ids;

    const auto RetrieveEntries = [&](FileSys::TitleType title_type,
                                     FileSys::ContentRecordType record_type) {
        const auto entries = installed.ListEntriesFilter(title_type, record_type);
        for (const auto& entry : entries) {
            if (FileSys::GetBaseTitleID(entry.title_id) == program_id &&
                installed.GetEntry(entry)->GetStatus() == Loader::ResultStatus::Success) {
                available_title_ids.insert({entry.title_id, title_type, record_type});
            }
        }
    };

    RetrieveEntries(FileSys::TitleType::Application, FileSys::ContentRecordType::Program);
    RetrieveEntries(FileSys::TitleType::Application, FileSys::ContentRecordType::HtmlDocument);
    RetrieveEntries(FileSys::TitleType::Application, FileSys::ContentRecordType::LegalInformation);
    RetrieveEntries(FileSys::TitleType::AOC, FileSys::ContentRecordType::Data);

    if (available_title_ids.empty()) {
        return false;
    }

    size_t title_index = 0;

    if (available_title_ids.size() > 1) {
        QStringList list;
        for (auto& [title_id, title_type, record_type] : available_title_ids) {
            const auto hex_title_id = QString::fromStdString(fmt::format("{:X}", title_id));
            if (record_type == FileSys::ContentRecordType::Program) {
                list.push_back(QStringLiteral("Program [%1]").arg(hex_title_id));
            } else if (record_type == FileSys::ContentRecordType::HtmlDocument) {
                list.push_back(QStringLiteral("HTML document [%1]").arg(hex_title_id));
            } else if (record_type == FileSys::ContentRecordType::LegalInformation) {
                list.push_back(QStringLiteral("Legal information [%1]").arg(hex_title_id));
            } else {
                list.push_back(
                    QStringLiteral("DLC %1 [%2]").arg(title_id & 0x7FF).arg(hex_title_id));
            }
        }

        bool ok;
        const auto res = QInputDialog::getItem(
            this, tr("Select RomFS Dump Target"),
            tr("Please select which RomFS you would like to dump."), list, 0, false, &ok);
        if (!ok) {
            return false;
        }

        title_index = list.indexOf(res);
    }

    const auto& [title_id, title_type, record_type] = *available_title_ids.nth(title_index);
    *selected_title_id = title_id;
    *selected_content_record_type = static_cast<u8>(record_type);
    return true;
}

bool MainWindow::ConfirmClose() {
    if (emu_thread == nullptr ||
        UISettings::values.confirm_before_stopping.GetValue() == ConfirmStop::Ask_Never) {
        return true;
    }
    if (!QtCommon::system->GetExitLocked() &&
        UISettings::values.confirm_before_stopping.GetValue() == ConfirmStop::Ask_Based_On_Game) {
        return true;
    }
    const auto text = tr("Are you sure you want to close Eden?");
    return question(this, tr("Eden"), text);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (!ConfirmClose()) {
        event->ignore();
        return;
    }

    UpdateUISettings();
    game_list->SaveInterfaceLayout();
    UISettings::SaveWindowState();
    hotkey_registry.SaveHotkeys();

    // Unload controllers early
    controller_dialog->UnloadController();
    game_list->UnloadController();

    // Shutdown session if the emu thread is active...
    if (emu_thread != nullptr) {
        ShutdownGame();
    }

    render_window->close();
    multiplayer_state->Close();
    QtCommon::system->HIDCore().UnloadInputDevices();
    Network::Shutdown();

    QWidget::closeEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    if (loading_screen && loading_screen->isVisible()) {
        loading_screen->setGeometry(ui->centralwidget->rect());
    }
    emit sizeChanged(event->size());
}

void MainWindow::moveEvent(QMoveEvent* event) {
    auto window_frame_height = frameGeometry().height() - geometry().height();
    emit positionChanged(event->pos() - QPoint{0, window_frame_height});
}

static bool IsSingleFileDropEvent(const QMimeData* mime) {
    return mime->hasUrls() && mime->urls().length() == 1;
}

void MainWindow::AcceptDropEvent(QDropEvent* event) {
    if (IsSingleFileDropEvent(event->mimeData())) {
        event->setDropAction(Qt::DropAction::LinkAction);
        event->accept();
    }
}

bool MainWindow::DropAction(QDropEvent* event) {
    if (!IsSingleFileDropEvent(event->mimeData())) {
        return false;
    }

    const QMimeData* mime_data = event->mimeData();
    const QString& filename = mime_data->urls().at(0).toLocalFile();

    if (emulation_running && QFileInfo(filename).suffix() == QStringLiteral("bin")) {
        // Amiibo
        LoadAmiibo(filename);
    } else {
        // Game
        if (ConfirmChangeGame()) {
            BootGame(filename, ApplicationAppletParameters());
        }
    }
    return true;
}

void MainWindow::dropEvent(QDropEvent* event) {
    DropAction(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    AcceptDropEvent(event);
}

void MainWindow::dragMoveEvent(QDragMoveEvent* event) {
    AcceptDropEvent(event);
}

bool MainWindow::ConfirmChangeGame() {
    if (emu_thread == nullptr)
        return true;

    // Use custom question to link controller navigation
    return question(
        this, tr("Eden"),
        tr("Are you sure you want to stop the emulation? Any unsaved progress will be lost."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
}

bool MainWindow::ConfirmForceLockedExit() {
    if (emu_thread == nullptr) {
        return true;
    }
    const auto text = tr("The currently running application has requested Eden to not exit.\n\n"
                         "Would you like to bypass this and exit anyway?");

    return question(this, tr("Eden"), text);
}

void MainWindow::RequestGameExit() {
    if (!QtCommon::system->IsPoweredOn()) {
        return;
    }

    QtCommon::system->SetExitRequested(true);
    QtCommon::system->GetAppletManager().RequestExit();
}

void MainWindow::filterBarSetChecked(bool state) {
    ui->action_Show_Filter_Bar->setChecked(state);
    emit(OnToggleFilterBar());
}

static void AdjustLinkColor() {
    QPalette new_pal(qApp->palette());
    if (UISettings::IsDarkTheme()) {
        new_pal.setColor(QPalette::Link, QColor(0, 190, 255, 255));
    } else {
        new_pal.setColor(QPalette::Link, QColor(0, 140, 200, 255));
    }
    if (qApp->palette().color(QPalette::Link) != new_pal.color(QPalette::Link)) {
        qApp->setPalette(new_pal);
    }
}

void MainWindow::UpdateUITheme() {
    const QString default_theme = QString::fromUtf8(
        UISettings::themes[static_cast<size_t>(UISettings::default_theme)].second);
    QString current_theme = QString::fromStdString(UISettings::values.theme);

    if (current_theme.isEmpty()) {
        current_theme = default_theme;
    }

#ifdef _WIN32
    QIcon::setThemeName(current_theme);
    AdjustLinkColor();
#else
    if (current_theme == QStringLiteral("default") || current_theme == QStringLiteral("colorful")) {
        QIcon::setThemeName(current_theme == QStringLiteral("colorful") ? current_theme
                                                                        : startup_icon_theme);
        QIcon::setThemeSearchPaths(QStringList(default_theme_paths));
        if (isDarkMode()) {
            current_theme = QStringLiteral("default_dark");
        }
    } else {
        QIcon::setThemeName(current_theme);
        QIcon::setThemeSearchPaths(QStringList(QStringLiteral(":/icons")));
        AdjustLinkColor();
    }
#endif

    if (current_theme != default_theme) {
        QString theme_uri{QStringLiteral(":%1/style.qss").arg(current_theme)};
        QFile f(theme_uri);
        if (!f.open(QFile::ReadOnly | QFile::Text)) {
            LOG_ERROR(Frontend, "Unable to open style \"{}\", fallback to the default theme",
                      UISettings::values.theme);
            current_theme = default_theme;
        }
    }

    QString theme_uri{QStringLiteral(":%1/style.qss").arg(current_theme)};
    QFile f(theme_uri);
    if (f.open(QFile::ReadOnly | QFile::Text)) {
        QTextStream ts(&f);
        QString css = ts.readAll();
        
        // Inject modern UI/UX requested improvements
        css += QStringLiteral(
            "\n/* Modern UX/UI Overrides */\n"
            "QPushButton, QComboBox, QLineEdit {\n"
            "    border-radius: 6px;\n"
            "}\n"
            "QMenu, QDialog {\n"
            "    border-radius: 6px;\n"
            "}\n"
            "QMenu::item:selected {\n"
            "    border-radius: 4px;\n"
            "}\n"
            "QListWidget::item, QTabBar::tab {\n"
            "    font-weight: bold;\n"
            "}\n"
        );
        
        const bool is_dark = current_theme.contains(QStringLiteral("dark"), Qt::CaseInsensitive) ||
                             current_theme.contains(QStringLiteral("night"), Qt::CaseInsensitive) ||
                             current_theme.contains(QStringLiteral("twilight"), Qt::CaseInsensitive) ||
                             current_theme.contains(QStringLiteral("midnight"), Qt::CaseInsensitive);
        if (is_dark) {
            css += QStringLiteral(
                "\n/* Status Bar Plashki (Dark Theme) */\n"
                "QLabel#ShaderBuildingLabel, "
                "QLabel#EmuSpeedLabel, "
                "QLabel#GameFpsLabel, "
                "QLabel#EmuFrametimeLabel, "
                "QPushButton#RendererStatusBarButton, "
                "QPushButton#GPUStatusBarButton, "
                "QPushButton#DockingStatusBarButton, "
                "QPushButton#AAStatusBarButton, "
                "QPushButton#FilterStatusBarButton, "
                "QPushButton#VolumeStatusBarButton, "
                "QPushButton#AirplaneModeStatusBarButton, "
                "QPushButton#RefreshButton, "
                "QPushButton#ResScaleButton {\n"
                "    background-color: rgba(255, 255, 255, 12);\n"
                "    border: 1px solid rgba(255, 255, 255, 20);\n"
                "    border-radius: 6px;\n"
                "    padding: 0px 8px;\n"
                "    margin: 1px 2px;\n"
                "    color: #E2E8F0;\n"
                "    font-weight: 500;\n"
                "    height: 20px;\n"
                "    min-height: 20px;\n"
                "    max-height: 20px;\n"
                "}\n"
                "QPushButton#RendererStatusBarButton:hover, "
                "QPushButton#GPUStatusBarButton:hover, "
                "QPushButton#DockingStatusBarButton:hover, "
                "QPushButton#AAStatusBarButton:hover, "
                "QPushButton#FilterStatusBarButton:hover, "
                "QPushButton#VolumeStatusBarButton:hover, "
                "QPushButton#AirplaneModeStatusBarButton:hover, "
                "QPushButton#RefreshButton:hover, "
                "QPushButton#ResScaleButton:hover {\n"
                "    background-color: rgba(255, 255, 255, 24);\n"
                "    border-color: rgba(255, 255, 255, 40);\n"
                "}\n"
                "QPushButton#RendererStatusBarButton:pressed, "
                "QPushButton#GPUStatusBarButton:pressed, "
                "QPushButton#DockingStatusBarButton:pressed, "
                "QPushButton#AAStatusBarButton:pressed, "
                "QPushButton#FilterStatusBarButton:pressed, "
                "QPushButton#VolumeStatusBarButton:pressed, "
                "QPushButton#AirplaneModeStatusBarButton:pressed, "
                "QPushButton#RefreshButton:pressed, "
                "QPushButton#ResScaleButton:pressed {\n"
                "    background-color: rgba(255, 255, 255, 36);\n"
                "}\n"
                "QPushButton#RendererStatusBarButton {\n"
                "    color: #38bdf8;\n"
                "}\n"
                "QPushButton#GPUStatusBarButton {\n"
                "    color: #fb923c;\n"
                "}\n"
                "QPushButton#DockingStatusBarButton {\n"
                "    color: #38bdf8;\n"
                "}\n"
                "QPushButton#AAStatusBarButton {\n"
                "    color: #a78bfa;\n"
                "}\n"
                "QPushButton#FilterStatusBarButton {\n"
                "    color: #a78bfa;\n"
                "}\n"
                "QPushButton#VolumeStatusBarButton {\n"
                "    color: #ffffff;\n"
                "}\n"
                "QPushButton#ResScaleButton {\n"
                "    color: #a78bfa;\n"
                "}\n"
                "QPushButton#AirplaneModeStatusBarButton {\n"
                "    color: #94a3b8;\n"
                "}\n"
                "QPushButton#AirplaneModeStatusBarButton:checked {\n"
                "    color: #f43f5e;\n"
                "    background-color: rgba(244, 63, 94, 25);\n"
                "    border-color: rgba(244, 63, 94, 50);\n"
                "}\n"
                "QLabel#GameFpsLabel {\n"
                "    color: #38bdf8;\n"
                "}\n"
                "QLabel#EmuFrametimeLabel {\n"
                "    color: #94a3b8;\n"
                "}\n"
                "QPushButton#RefreshButton {\n"
                "    color: #38bdf8;\n"
                "    font-size: 13px;\n"
                "    font-weight: bold;\n"
                "}\n"
                "QWidget#MultiplayerStatusBarWidget {\n"
                "    background-color: rgba(255, 255, 255, 12);\n"
                "    border: 1px solid rgba(255, 255, 255, 20);\n"
                "    border-radius: 6px;\n"
                "    margin: 1px 2px;\n"
                "    height: 20px;\n"
                "    min-height: 20px;\n"
                "    max-height: 20px;\n"
                "}\n"
                "QWidget#MultiplayerStatusBarWidget:hover {\n"
                "    background-color: rgba(255, 255, 255, 24);\n"
                "    border-color: rgba(255, 255, 255, 40);\n"
                "}\n"
                "QWidget#MultiplayerStatusBarWidget QLabel {\n"
                "    color: #38bdf8;\n"
                "    font-weight: bold;\n"
                "    font-size: 11px;\n"
                "    background: transparent;\n"
                "    border: none;\n"
                "    padding: 0px;\n"
                "    margin: 0px;\n"
                "}\n"
            );
        } else {
            css += QStringLiteral(
                "\n/* Status Bar Plashki (Light Theme) */\n"
                "QLabel#ShaderBuildingLabel, "
                "QLabel#EmuSpeedLabel, "
                "QLabel#GameFpsLabel, "
                "QLabel#EmuFrametimeLabel, "
                "QPushButton#RendererStatusBarButton, "
                "QPushButton#GPUStatusBarButton, "
                "QPushButton#DockingStatusBarButton, "
                "QPushButton#AAStatusBarButton, "
                "QPushButton#FilterStatusBarButton, "
                "QPushButton#VolumeStatusBarButton, "
                "QPushButton#AirplaneModeStatusBarButton, "
                "QPushButton#RefreshButton, "
                "QPushButton#ResScaleButton {\n"
                "    background-color: rgba(0, 0, 0, 10);\n"
                "    border: 1px solid rgba(0, 0, 0, 18);\n"
                "    border-radius: 6px;\n"
                "    padding: 0px 8px;\n"
                "    margin: 1px 2px;\n"
                "    color: #1E293B;\n"
                "    font-weight: 500;\n"
                "    height: 20px;\n"
                "    min-height: 20px;\n"
                "    max-height: 20px;\n"
                "}\n"
                "QPushButton#RendererStatusBarButton:hover, "
                "QPushButton#GPUStatusBarButton:hover, "
                "QPushButton#DockingStatusBarButton:hover, "
                "QPushButton#AAStatusBarButton:hover, "
                "QPushButton#FilterStatusBarButton:hover, "
                "QPushButton#VolumeStatusBarButton:hover, "
                "QPushButton#AirplaneModeStatusBarButton:hover, "
                "QPushButton#RefreshButton:hover, "
                "QPushButton#ResScaleButton:hover {\n"
                "    background-color: rgba(0, 0, 0, 20);\n"
                "    border-color: rgba(0, 0, 0, 30);\n"
                "}\n"
                "QPushButton#RendererStatusBarButton:pressed, "
                "QPushButton#GPUStatusBarButton:pressed, "
                "QPushButton#DockingStatusBarButton:pressed, "
                "QPushButton#AAStatusBarButton:pressed, "
                "QPushButton#FilterStatusBarButton:pressed, "
                "QPushButton#VolumeStatusBarButton:pressed, "
                "QPushButton#AirplaneModeStatusBarButton:pressed, "
                "QPushButton#RefreshButton:pressed, "
                "QPushButton#ResScaleButton:pressed {\n"
                "    background-color: rgba(0, 0, 0, 30);\n"
                "}\n"
                "QPushButton#RendererStatusBarButton {\n"
                "    color: #0284c7;\n"
                "}\n"
                "QPushButton#GPUStatusBarButton {\n"
                "    color: #ea580c;\n"
                "}\n"
                "QPushButton#DockingStatusBarButton {\n"
                "    color: #0284c7;\n"
                "}\n"
                "QPushButton#AAStatusBarButton {\n"
                "    color: #7c3aed;\n"
                "}\n"
                "QPushButton#FilterStatusBarButton {\n"
                "    color: #7c3aed;\n"
                "}\n"
                "QPushButton#VolumeStatusBarButton {\n"
                "    color: #0f172a;\n"
                "}\n"
                "QPushButton#ResScaleButton {\n"
                "    color: #7c3aed;\n"
                "}\n"
                "QPushButton#AirplaneModeStatusBarButton {\n"
                "    color: #64748b;\n"
                "}\n"
                "QPushButton#AirplaneModeStatusBarButton:checked {\n"
                "    color: #e11d48;\n"
                "    background-color: rgba(225, 29, 72, 20);\n"
                "    border-color: rgba(225, 29, 72, 40);\n"
                "}\n"
                "QLabel#GameFpsLabel {\n"
                "    color: #0284c7;\n"
                "}\n"
                "QLabel#EmuFrametimeLabel {\n"
                "    color: #475569;\n"
                "}\n"
                "QPushButton#RefreshButton {\n"
                "    color: #0284c7;\n"
                "    font-size: 13px;\n"
                "    font-weight: bold;\n"
                "}\n"
                "QWidget#MultiplayerStatusBarWidget {\n"
                "    background-color: rgba(0, 0, 0, 10);\n"
                "    border: 1px solid rgba(0, 0, 0, 18);\n"
                "    border-radius: 6px;\n"
                "    margin: 1px 2px;\n"
                "    height: 20px;\n"
                "    min-height: 20px;\n"
                "    max-height: 20px;\n"
                "}\n"
                "QWidget#MultiplayerStatusBarWidget:hover {\n"
                "    background-color: rgba(0, 0, 0, 20);\n"
                "    border-color: rgba(0, 0, 0, 30);\n"
                "}\n"
                "QWidget#MultiplayerStatusBarWidget QLabel {\n"
                "    color: #0284c7;\n"
                "    font-weight: bold;\n"
                "    font-size: 11px;\n"
                "    background: transparent;\n"
                "    border: none;\n"
                "    padding: 0px;\n"
                "    margin: 0px;\n"
                "}\n"
            );
        }
        
        qApp->setStyleSheet(css);
        setStyleSheet(css);
    } else {
        LOG_ERROR(Frontend, "Unable to set style \"{}\", stylesheet file not found",
                  UISettings::values.theme);
        qApp->setStyleSheet({});
        setStyleSheet({});
    }

#ifdef _WIN32
    RemoveTitlebarFilter();
    ApplyGlobalDarkTitlebar(UISettings::IsDarkTheme());
#endif
}

void MainWindow::LoadTranslation() {
    bool loaded;

    if (UISettings::values.language.GetValue().empty()) {
        // If the selected language is empty, use system locale
        loaded = translator.load(QLocale(), {}, {}, QStringLiteral(":/languages/"));
    } else {
        // Otherwise load from the specified file
        loaded = translator.load(QString::fromStdString(UISettings::values.language.GetValue()),
                                 QStringLiteral(":/languages/"));
    }

    if (loaded) {
        qApp->installTranslator(&translator);
    } else {
        UISettings::values.language = std::string("en");
    }
}

void MainWindow::OnLanguageChanged(const QString& locale) {
    if (UISettings::values.language.GetValue() != std::string("en")) {
        qApp->removeTranslator(&translator);
    }

    QList<QAction*> actions = game_size_actions->actions();
    for (size_t i = 0; i < default_game_icon_sizes.size(); i++) {
        actions.at(i)->setText(GetTranslatedGameIconSize(i));
    }

    UISettings::values.language = locale.toStdString();
    LoadTranslation();
    ui->retranslateUi(this);
    multiplayer_state->retranslateUi();
    UpdateWindowTitle();

    // Dynamically update programmatically translated status bar pills
    UpdateDockedButton();
    UpdateGPUAccuracyButton();
    UpdateAPIText();
    UpdateFilterText();
    UpdateAAText();
    UpdateVolumeUI();
    UpdateAirplaneModeButton();
}

void MainWindow::SetDiscordEnabled([[maybe_unused]] bool state) {
    {
        std::ofstream df("debug_log.txt", std::ios::app);
        df << "MW: Inside SetDiscordEnabled(), state=" << state << "\n"; df.flush();
    }
#ifdef USE_DISCORD_PRESENCE
    if (state) {
        discord_rpc = std::make_unique<DiscordRPC::DiscordImpl>(*QtCommon::system);
    } else {
        discord_rpc = std::make_unique<DiscordRPC::NullImpl>();
    }
#else
    discord_rpc = std::make_unique<DiscordRPC::NullImpl>();
#endif
    {
        std::ofstream df("debug_log.txt", std::ios::app);
        df << "MW: Inside SetDiscordEnabled(), after make_unique\n"; df.flush();
    }
    discord_rpc->Update();
    {
        std::ofstream df("debug_log.txt", std::ios::app);
        df << "MW: Inside SetDiscordEnabled(), after Update\n"; df.flush();
    }
}

void MainWindow::SetGamemodeEnabled(bool state) {
    if (emulation_running) {
        if (state)
            Common::FeralGamemode::Start();
        else
            Common::FeralGamemode::Stop();
    }
}

void MainWindow::changeEvent(QEvent* event) {
#ifdef __unix__
    // PaletteChange event appears to only reach so far into the GUI, explicitly asking to
    // UpdateUITheme is a decent work around
    if (event->type() == QEvent::PaletteChange) {
        const QPalette test_palette(qApp->palette());
        const QString current_theme = QString::fromStdString(UISettings::values.theme);
        // Keeping eye on QPalette::Window to avoid looping. QPalette::Text might be useful too
        static QColor last_window_color;
        const QColor window_color = test_palette.color(QPalette::Active, QPalette::Window);
        if (last_window_color != window_color && (current_theme == QStringLiteral("default") ||
                                                  current_theme == QStringLiteral("colorful"))) {
            UpdateUITheme();
        }
        last_window_color = window_color;
    }
#endif // __unix__
    QWidget::changeEvent(event);
}

Service::AM::FrontendAppletParameters MainWindow::ApplicationAppletParameters() {
    return Service::AM::FrontendAppletParameters{
        .applet_id = Service::AM::AppletId::Application,
        .applet_type = Service::AM::AppletType::Application,
    };
}

Service::AM::FrontendAppletParameters MainWindow::LibraryAppletParameters(
    u64 program_id, Service::AM::AppletId applet_id) {
    return Service::AM::FrontendAppletParameters{
        .program_id = program_id,
        .applet_id = applet_id,
        .applet_type = Service::AM::AppletType::LibraryApplet,
    };
}

void VolumeButton::wheelEvent(QWheelEvent* event) {

    int num_degrees = event->angleDelta().y() / 8;
    int num_steps = (num_degrees / 15) * scroll_multiplier;
    // Stated in QT docs: Most mouse types work in steps of 15 degrees, in which case the delta
    // value is a multiple of 120; i.e., 120 units * 1/8 = 15 degrees.

    if (num_steps > 0) {
        Settings::values.volume.SetValue(
            (std::min)(200, Settings::values.volume.GetValue() + num_steps));
    } else {
        Settings::values.volume.SetValue(
            (std::max)(0, Settings::values.volume.GetValue() + num_steps));
    }

    scroll_multiplier = (std::min)(MaxMultiplier, scroll_multiplier * 2);
    scroll_timer.start(100); // reset the multiplier if no scroll event occurs within 100 ms

    emit VolumeChanged();
    event->accept();
}

void VolumeButton::ResetMultiplier() {
    scroll_multiplier = 1;
}

void MainWindow::OnDLCButtonClicked() {
    if (!titledb_loaded) {
        StartTitleDbDownload(true);
        return;
    }

    const auto current_title_id = QtCommon::system->GetApplicationProcessProgramID();
    const auto base_title_id = FileSys::GetBaseTitleID(current_title_id);
    
    if (base_title_id == 0) {
        QMessageBox::information(this, tr("Дополнения"), tr("Игра не запущена."));
        return;
    }

    std::vector<u64> installed_dlc;
    const auto& rcu = QtCommon::system->GetContentProvider();
    const auto list = rcu.ListEntriesFilter(FileSys::TitleType::AOC, FileSys::ContentRecordType::Data);
    for (const auto& entry : list) {
        if (FileSys::GetBaseTitleID(entry.title_id) == base_title_id) {
            auto entry_ptr = rcu.GetEntry(entry.title_id, FileSys::ContentRecordType::Data);
            if (entry_ptr && entry_ptr->GetStatus() == Loader::ResultStatus::Success) {
                installed_dlc.push_back(entry.title_id);
            }
        }
    }

    QDialog* dialog = new QDialog(this, Qt::FramelessWindowHint | Qt::Dialog);
    dialog->setModal(true);
    dialog->setMinimumSize(900, 500);
    dialog->resize(900, 550);
    dialog->setStyleSheet(QStringLiteral("QDialog { background-color: #121212; border: 1px solid #444; border-radius: 12px; }"));

    QVBoxLayout* layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(8);

    QLabel* title = new QLabel(tr("ДОПОЛНЕНИЯ"));
    title->setStyleSheet(QStringLiteral("color: #FFFFFF; font-size: 18px; font-weight: bold; margin-bottom: 12px; letter-spacing: 1px; background-color: transparent;"));
    layout->addWidget(title);

    QListWidget* listWidget = new QListWidget();
    listWidget->setSpacing(8);
    listWidget->setStyleSheet(QStringLiteral(
        "QListWidget { background-color: transparent; border: none; outline: none; }"
        "QListWidget::item { padding: 4px; border-radius: 6px; }"
        "QListWidget::item:hover { background-color: rgba(255, 255, 255, 0.05); }"
    ));
    layout->addWidget(listWidget);

    int dlc_total = 0;
    int dlc_active = 0;
    int dlc_counter = 0;

    for (int i = 0; i <= 2000; i++) {
        u64 tid = FileSys::GetAOCBaseTitleID(base_title_id) + i;
        QString idStrUpper = QStringLiteral("%1").arg(tid, 16, 16, QLatin1Char('0')).toUpper();
        QString idStrLower = QStringLiteral("%1").arg(tid, 16, 16, QLatin1Char('0')).toLower();
        
        auto it = titledb_object.find(idStrUpper);
        if (it == titledb_object.end()) {
            it = titledb_object.find(idStrLower);
        }

        bool installed = std::find(installed_dlc.begin(), installed_dlc.end(), tid) != installed_dlc.end();
        bool in_db = (it != titledb_object.end());

        if (in_db || installed) {
            dlc_counter++;
            QString dlc_name;
            if (in_db) {
                QJsonObject item = it.value().toObject();
                dlc_name = item.value(QStringLiteral("name")).toString();
                dlc_total++;
            } else {
                dlc_name = QStringLiteral("Неизвестное дополнение [%1]").arg(idStrUpper);
            }

            if (installed) dlc_active++;

            QWidget* row = new QWidget();
            row->setStyleSheet(QStringLiteral("QWidget { background-color: #212124; border: 1px solid #333; border-radius: 8px; }"));
            QHBoxLayout* rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(16, 14, 16, 14);

            QLabel* numLabel = new QLabel(QString::number(dlc_counter) + QStringLiteral("."));
            numLabel->setStyleSheet(QStringLiteral("color: #757575; font-size: 14px; font-weight: bold; border: none; background: transparent;"));
            numLabel->setFixedWidth(30);
            
            QLabel* nameLabel = new QLabel(dlc_name);
            nameLabel->setStyleSheet(QStringLiteral("color: #E0E0E0; font-size: 15px; font-weight: 500; border: none; background: transparent;"));
            nameLabel->setWordWrap(true);
            nameLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
            nameLabel->setContextMenuPolicy(Qt::CustomContextMenu);
            connect(nameLabel, &QLabel::customContextMenuRequested, [nameLabel](const QPoint& pos) {
                QMenu menu;
                menu.setStyleSheet(QStringLiteral(
                    "QMenu { background-color: #2D2D30; border: 1px solid #454545; border-radius: 6px; padding: 4px; color: #E0E0E0; font-size: 13px; }"
                    "QMenu::item { padding: 6px 24px 6px 12px; border-radius: 4px; }"
                    "QMenu::item:selected { background-color: #094771; }"
                ));
                QAction* copyAction = menu.addAction(QObject::tr("Копировать"));
                QAction* copyAllAction = menu.addAction(QObject::tr("Копировать все"));
                
                QAction* selectedItem = menu.exec(nameLabel->mapToGlobal(pos));
                if (selectedItem == copyAction) {
                    QString text = nameLabel->selectedText();
                    if (text.isEmpty()) text = nameLabel->text();
                    QGuiApplication::clipboard()->setText(text);
                } else if (selectedItem == copyAllAction) {
                    QGuiApplication::clipboard()->setText(nameLabel->text());
                }
            });

            QLabel* statusLabel = new QLabel(installed ? tr("АКТИВНО") : tr("НЕ АКТИВНО"));
            statusLabel->setFixedSize(140, 36);
            statusLabel->setAlignment(Qt::AlignCenter);
            statusLabel->setStyleSheet(installed ? QStringLiteral("color: #4CAF50; font-weight: bold; font-size: 12px; border: 1px solid rgba(76, 175, 80, 0.4); border-radius: 6px; background-color: rgba(76, 175, 80, 0.15);") : QStringLiteral("color: #F44336; font-weight: bold; font-size: 12px; border: 1px solid rgba(244, 67, 54, 0.4); border-radius: 6px; background-color: rgba(244, 67, 54, 0.15);"));
            
            rowLayout->addWidget(numLabel);
            rowLayout->addWidget(nameLabel, 1);
            rowLayout->addWidget(statusLabel);

            QListWidgetItem* listItem = new QListWidgetItem(listWidget);
            listItem->setSizeHint(row->sizeHint());
            listWidget->setItemWidget(listItem, row);
        }
    }

    if (dlc_total == 0) {
        QLabel* emptyLabel = new QLabel(tr("Нет известных дополнений Tinfoil для этой игры."));
        emptyLabel->setStyleSheet(QStringLiteral("color: #888; font-style: italic; background: transparent; border: none;"));
        layout->addWidget(emptyLabel);
    }

    dlc_button->setText(QStringLiteral("ДОПОЛНЕНИЯ (%1/%2)").arg(dlc_active).arg(dlc_total));

    QPushButton* closeBtn = new QPushButton(tr("Закрыть"));
    closeBtn->setStyleSheet(QStringLiteral("QPushButton { background-color: #2d2d30; color: white; padding: 10px; border-radius: 8px; font-weight: bold; border: 1px solid #444; font-size: 14px; } QPushButton:hover { background-color: #3e3e42; border: 1px solid #666; }"));
    connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::accept);
    layout->addWidget(closeBtn);

    dialog->exec();
}

void MainWindow::StartTitleDbDownload(bool priority) {
    if (titledb_downloading) {
        if (priority && !titledb_dialog_pending) {
            titledb_dialog_pending = true;
            dlc_button->setEnabled(false);
            dlc_button->setText(tr("ЗАГРУЗКА..."));
        }
        return;
    }

    titledb_downloading = true;
    if (priority) {
        titledb_dialog_pending = true;
        dlc_button->setEnabled(false);
        dlc_button->setText(tr("ЗАГРУЗКА..."));
    }

    if (!titledb_manager) {
        titledb_manager = new QNetworkAccessManager(this);
    }

    QNetworkRequest request(QUrl(QStringLiteral("https://tinfoil.media/repo/db/titles.json")));
    if (priority) {
        request.setPriority(QNetworkRequest::HighPriority);
    } else {
        request.setPriority(QNetworkRequest::LowPriority);
    }

    titledb_reply = titledb_manager->get(request);

    connect(titledb_reply, &QNetworkReply::finished, this, [this]() {
        QNetworkReply* reply = titledb_reply;
        titledb_reply = nullptr;
        titledb_downloading = false;

        if (reply->error() != QNetworkReply::NoError) {
            titledb_dialog_pending = false;
            dlc_button->setEnabled(true);
            UpdateDLCButtonText();
            reply->deleteLater();
            return;
        }

        QByteArray db_data = reply->readAll();
        reply->deleteLater();

        auto future = QtConcurrent::run([this, db_data]() {
            QString titledb_path = QDir::homePath() + QStringLiteral("/.switch/titledb.json");
            QDir().mkpath(QFileInfo(titledb_path).absolutePath());
            QFile file(titledb_path);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(db_data);
                file.close();
            }

            QJsonDocument doc = QJsonDocument::fromJson(db_data);
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                QMetaObject::invokeMethod(this, [this, obj]() {
                    titledb_object = obj;
                    titledb_loaded = true;
                    dlc_button->setEnabled(true);
                    UpdateDLCButtonText();

                    if (titledb_dialog_pending) {
                        titledb_dialog_pending = false;
                        OnDLCButtonClicked();
                    }
                });
            } else {
                QMetaObject::invokeMethod(this, [this]() {
                    dlc_button->setEnabled(true);
                    UpdateDLCButtonText();
                    titledb_dialog_pending = false;
                });
            }
        });
        (void)future;
    });
}

void MainWindow::LoadTitleDbFromCache() {
    QString titledb_path = QDir::homePath() + QStringLiteral("/.switch/titledb.json");
    QFile file(titledb_path);
    if (!file.exists()) {
        return;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    
    QByteArray db_data = file.readAll();
    file.close();

    auto future = QtConcurrent::run([this, db_data]() {
        QJsonDocument doc = QJsonDocument::fromJson(db_data);
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            QMetaObject::invokeMethod(this, [this, obj]() {
                titledb_object = obj;
                titledb_loaded = true;
                UpdateDLCButtonText();
            });
        }
    });
    (void)future;
}

void MainWindow::UpdateDLCButtonText() {
    if (!emulation_running) {
        dlc_button->setText(tr("ДОПОЛНЕНИЯ"));
        return;
    }

    const auto current_title_id = QtCommon::system->GetApplicationProcessProgramID();
    const auto base_title_id = FileSys::GetBaseTitleID(current_title_id);
    if (base_title_id == 0) {
        dlc_button->setText(tr("ДОПОЛНЕНИЯ"));
        return;
    }

    if (!titledb_loaded) {
        dlc_button->setText(tr("ДОПОЛНЕНИЯ"));
        return;
    }

    int dlc_total = 0;
    int dlc_active = 0;

    const auto& rcu = QtCommon::system->GetContentProvider();
    const auto list = rcu.ListEntriesFilter(FileSys::TitleType::AOC, FileSys::ContentRecordType::Data);
    std::vector<u64> installed_dlc;
    for (const auto& entry : list) {
        if (FileSys::GetBaseTitleID(entry.title_id) == base_title_id) {
            auto entry_ptr = rcu.GetEntry(entry.title_id, FileSys::ContentRecordType::Data);
            if (entry_ptr && entry_ptr->GetStatus() == Loader::ResultStatus::Success) {
                installed_dlc.push_back(entry.title_id);
            }
        }
    }

    for (int i = 0; i <= 2000; i++) {
        u64 tid = FileSys::GetAOCBaseTitleID(base_title_id) + i;
        QString idStrUpper = QStringLiteral("%1").arg(tid, 16, 16, QLatin1Char('0')).toUpper();
        QString idStrLower = QStringLiteral("%1").arg(tid, 16, 16, QLatin1Char('0')).toLower();
        
        auto it = titledb_object.find(idStrUpper);
        if (it == titledb_object.end()) {
            it = titledb_object.find(idStrLower);
        }

        bool installed = std::find(installed_dlc.begin(), installed_dlc.end(), tid) != installed_dlc.end();
        bool in_db = (it != titledb_object.end());

        if (in_db) dlc_total++;
        if (installed) dlc_active++;
    }

    dlc_button->setText(QStringLiteral("ДОПОЛНЕНИЯ (%1/%2)").arg(dlc_active).arg(dlc_total));
}

#ifdef main
#undef main
#endif

#if !defined(QT_STATICPLUGIN) || defined(__APPLE__)
#define VMA_IMPLEMENTATION
#include "video_core/vulkan_common/vma.h"
#endif



