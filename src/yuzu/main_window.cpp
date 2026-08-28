// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// Static Qt on macOS doesn't use Vulkan
// Other platforms do, and thus have conflicting VulkanMemoryAllocator symbols
#if defined(QT_STATICPLUGIN) && !defined(__APPLE__)
#undef VMA_IMPLEMENTATION
#endif

#include <fstream>
#include <QRegularExpression>
#include <boost/algorithm/string/split.hpp>
#include "common/cityhash.h"
#include "common/fs/path_util.h"
#include "common/settings.h"
#include "common/settings_enums.h"
#include "frontend_common/settings_generator.h"
#include "render/performance_overlay.h"
#include "updater/update_dialog.h"

#include <QDesktopServices>
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPainterPath>
#include <QTextEdit>
#include "common/fs/ryujinx_compat.h"
#include "main_window.h"
#include "network/network.h"
#include "qt_common/discord/discord.h"
#include "qt_common/titledb.h"
#include "ui_main.h"

// Other Yuzu stuff //
#include "debugger/console.h"
#include "debugger/controller.h"

#include "about_dialog.h"
#include "amiibo_browser_dialog.h"
#include "cheats_dialog.h"
#include "mod_manager_dialog.h"
#include "data_dialog.h"
#include "deps_dialog.h"
#include "install_dialog.h"
#include "translator/floating_translate_button.h"
#include "translator/game_translator.h"
#include "in_game_notification.h"

#include "bootmanager.h"
#include "loading_screen.h"
#include "qt_common/util/vk.h"
#include "ryujinx_dialog.h"
#include "set_play_time_dialog.h"
#include "util/util.h"
#include "yuzu/game/game_list.h"

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
#include "core/hle/service/game_fix_database.h"

// Qt Stuff //
#define QT_NO_OPENGL

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QStyleHints>
#endif

#include <QActionGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QInputDialog>
#include <QMimeData>
#include <QPalette>
#include <QProgressDialog>
#include <QScreen>
#include <QShortcut>
#include <QStatusBar>
#include <QTableWidget>
#include <QHeaderView>
#include <QtConcurrentRun>

// Qt Common //
#include "qt_common/config/shared_translation.h"
#include "qt_common/config/uisettings.h"

#include "qt_common/abstract/frontend.h"

#include "qt_common/qt_common.h"
#include "qt_common/qt_string_lookup.h"

#include "qt_common/util/content.h"
#include "qt_common/util/fs.h"
#include "qt_common/util/meta.h"
#include "qt_common/util/mod.h"
#include "qt_common/util/path.h"

#include "qt_common/render/emu_thread.h"

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
#include "common/cpu_features.h"
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

static inline void ApplyDarkToTopLevel(QWidget* w, bool on) {
    if (!w || !w->isWindow())
        return;
    ApplyWindowsTitleBarDarkMode(reinterpret_cast<HWND>(w->winId()), on);
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
#include "yuzu/breakpad.h"
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
    QtCommon::Init(this);

    Common::FS::CreateEdenPaths();
    this->config = std::make_unique<QtConfig>();

    if (user_data_migrator.migrated) {
        // Sort-of hack whereby we only move the old dir if it's a subfolder of the user dir

        using namespace Common::FS;

        static constexpr const std::array<const EdenPath, 4> paths = {
            EdenPath::NANDDir, EdenPath::SDMCDir, EdenPath::DumpDir, EdenPath::LoadDir};

        for (const EdenPath& path : paths) {
            std::string str_path = Common::FS::GetEdenPathString(path);
            if (str_path.starts_with(user_data_migrator.selected_emu.get_user_dir())) {
                boost::replace_all(
                    str_path, user_data_migrator.selected_emu.lower_name().toStdString(), "storm_eden");
                Common::FS::SetEdenPath(path, str_path);
            }
        }
    }

#ifdef __unix__
    SetupSigInterrupts();
#endif

    SetGamemodeEnabled(UISettings::values.enable_gamemode.GetValue());

    UISettings::RestoreWindowState(config);

    LoadTranslation();
    FrontendCommon::GenerateSettings();

    setAcceptDrops(true);
    ui->setupUi(this);
    statusBar()->hide();

    startup_icon_theme = QIcon::themeName();
    // fallback can only be set once, colorful theme icons are okay on both light/dark
    QIcon::setFallbackThemeName(QStringLiteral("colorful"));
    QIcon::setFallbackSearchPaths(QStringList(QStringLiteral(":/icons")));

    default_theme_paths = QIcon::themeSearchPaths();
    UpdateUITheme();

    SetDiscordEnabled(UISettings::values.enable_discord_presence.GetValue());
    discord_rpc->Update();

    play_time_manager = std::make_unique<PlayTime::PlayTimeManager>();

    InitializeWidgets();
    InitializeDebugWidgets();
    InitializeRecentFileMenuActions();
    SetupMenuIcons();
    InitializeHotkeys();

    SetDefaultUIGeometry();
    RestoreUIState();

    ConnectMenuEvents();
    ConnectWidgetEvents();

    QtCommon::SetupHID();
    controller_dialog->refreshConfiguration();

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

    // Setup content providers.
    QtCommon::SetupContentProviders();

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
    game_list->PopulateAsync(UISettings::values.game_dirs);
    QTimer::singleShot(2500, this, &MainWindow::StartSilentCheatsSync);

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

    // Pre-load TitleDB database in background
    TitleDB::TitleDatabase::Instance().EnsureLoaded();

    // Some moron added a race condition to the status bar
    // so now we have to make this completely unnecessary call
    // to prevent the UI from blowing up.
    UpdateUITheme();

    QTimer::singleShot(2500, this, [this] {
        OnCheckUpdates(false);
    });

    QStringList args = QApplication::arguments();

    if (args.size() < 2) {
        return;
    }

    QString game_path;
    bool should_launch_qlaunch = false;
    bool should_launch_hlaunch = false;
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
        } else if (args[i] == QStringLiteral("-hlaunch")) {
            should_launch_hlaunch = true;
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
        } else if (should_launch_qlaunch) {
            LaunchFirmwareApplet(u64(Service::AM::AppletProgramId::QLaunch), std::nullopt);
        } else if (should_launch_hlaunch) {
            std::filesystem::path const sd_dir =
                Common::FS::GetEdenPathString(Common::FS::EdenPath::SDMCDir);
            auto const hbl_path = (sd_dir / "atmosphere" / "hbl.nsp").string();
            BootGame(
                QString::fromStdString(hbl_path),
                LibraryAppletParameters(0x010000000000100Dull, Service::AM::AppletId::QLaunch));
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
    render_window = new GRenderWindow(this, input_subsystem);
    render_window->hide();

    game_list = new GameList(QtCommon::vfs, QtCommon::provider.get(), *play_time_manager,
                             *QtCommon::system, this);
    ui->horizontalLayout->addWidget(game_list);

    game_list_placeholder = new GameListPlaceholder(this);
    ui->horizontalLayout->addWidget(game_list_placeholder);
    game_list_placeholder->setVisible(false);

    loading_screen = new LoadingScreen(ui->centralwidget);
    loading_screen->hide();
    connect(loading_screen, &LoadingScreen::Hidden, this, [&] {
        loading_screen->Clear();
    });

    multiplayer_state = new MultiplayerState(this, game_list->GetModel(), ui->action_Leave_Room,
                                             ui->action_Show_Room, *QtCommon::system);
    multiplayer_state->setVisible(false);

    // Create status bar
    message_label = new QLabel();
    // Configured separately for left alignment
    message_label->setFrameStyle(QFrame::NoFrame);
    message_label->setContentsMargins(4, 0, 4, 0);
    message_label->setAlignment(Qt::AlignLeft);
    statusBar()->addPermanentWidget(message_label, 1);

    shader_building_label = new QLabel();
    shader_building_label->setToolTip(tr("The amount of shaders currently being built"));
    res_scale_label = new QLabel();
    res_scale_label->setToolTip(tr("The current selected resolution scaling multiplier."));
    emu_speed_label = new QLabel();
    emu_speed_label->setToolTip(
        tr("Current emulation speed. Values higher or lower than 100% "
           "indicate emulation is running faster or slower than a Switch."));
    game_fps_label = new QLabel();
    game_fps_label->setToolTip(tr("How many frames per second the game is currently displaying. "
                                  "This will vary from game to game and scene to scene."));
    emu_frametime_label = new QLabel();
    emu_frametime_label->setToolTip(
        tr("Time taken to emulate a Switch frame, not counting framelimiting or v-sync. For "
           "full-speed emulation this should be at most 16.67 ms."));

    for (auto& label : {shader_building_label, res_scale_label, emu_speed_label, game_fps_label,
                        emu_frametime_label}) {
        label->setVisible(false);
        label->setFrameStyle(QFrame::NoFrame);
        label->setContentsMargins(4, 0, 4, 0);
        statusBar()->addPermanentWidget(label);
    }

    firmware_label = new QLabel();
    firmware_label->setObjectName(QStringLiteral("FirmwareLabel"));
    firmware_label->setVisible(false);
    firmware_label->setContentsMargins(4, 0, 4, 0);
    firmware_label->setFocusPolicy(Qt::NoFocus);
    statusBar()->addPermanentWidget(firmware_label);

    statusBar()->addPermanentWidget(multiplayer_state->GetStatusText(), 0);
    statusBar()->addPermanentWidget(multiplayer_state->GetStatusIcon(), 0);

    tas_label = new QLabel();
    tas_label->setObjectName(QStringLiteral("TASlabel"));
    tas_label->setFocusPolicy(Qt::NoFocus);
    statusBar()->insertPermanentWidget(0, tas_label);

    volume_popup = new QWidget(this);
    volume_popup->setWindowFlags(Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint | Qt::Popup);
    auto* pop_layout = new QVBoxLayout(volume_popup);
    pop_layout->setContentsMargins(10, 8, 10, 8);
    pop_layout->setSpacing(6);
    volume_popup->setStyleSheet(QStringLiteral(
        "QWidget { background-color: #090a10; color: #e0e6ed; font-family: 'Segoe UI', sans-serif; border: 2px solid #00f0ff; border-radius: 6px; }"
        "QSlider::groove:horizontal { height: 6px; background: #121624; border-radius: 3px; border: 1px solid #00f0ff; }"
        "QSlider::sub-page:horizontal { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #ff0055, stop:1 #ffee00); border-radius: 3px; }"
        "QSlider::handle:horizontal { background: #ffee00; border: 1px solid #000000; width: 14px; margin-top: -5px; margin-bottom: -5px; border-radius: 7px; }"
        "QSlider::handle:horizontal:hover { background: #00f0ff; }"
        "QPushButton { background-color: #121624; color: #00f0ff; border: 1px solid #00f0ff; border-radius: 3px; padding: 2px 6px; font-size: 8pt; font-weight: bold; }"
        "QPushButton:hover { background-color: #ffee00; color: #000000; }"
    ));

    volume_val_label = new QLabel(tr("Громкость: 100%"), volume_popup);
    volume_val_label->setStyleSheet(QStringLiteral("font-weight: bold; color: #ffee00; font-size: 9.5pt; border: none; background: transparent;"));
    volume_val_label->setAlignment(Qt::AlignCenter);
    pop_layout->addWidget(volume_val_label);

    volume_slider = new QSlider(Qt::Horizontal, volume_popup);
    volume_slider->setObjectName(QStringLiteral("volume_slider"));
    volume_slider->setMaximum(200);
    volume_slider->setPageStep(5);
    volume_slider->setTickPosition(QSlider::TicksBelow);
    volume_slider->setTickInterval(25);
    pop_layout->addWidget(volume_slider);

    auto* scale_layout = new QHBoxLayout();
    scale_layout->setContentsMargins(0, 0, 0, 0);
    const QStringList ticks = {QStringLiteral("0%"), QStringLiteral("50%"), QStringLiteral("100%"), QStringLiteral("150%"), QStringLiteral("200%")};
    for (const QString& tick : ticks) {
        auto* lbl = new QLabel(tick, volume_popup);
        lbl->setStyleSheet(QStringLiteral("color: #7b8fa3; font-size: 7.5pt; border: none; background: transparent;"));
        lbl->setAlignment(Qt::AlignCenter);
        scale_layout->addWidget(lbl);
    }
    pop_layout->addLayout(scale_layout);

    auto* preset_layout = new QHBoxLayout();
    preset_layout->setContentsMargins(0, 2, 0, 0);
    preset_layout->setSpacing(4);
    const std::vector<int> presets = {0, 50, 100, 150, 200};
    for (int p_val : presets) {
        auto* btn = new QPushButton(QStringLiteral("%1%").arg(p_val), volume_popup);
        btn->setFocusPolicy(Qt::NoFocus);
        connect(btn, &QPushButton::clicked, this, [this, p_val] {
            Settings::values.audio_muted.SetValue(false);
            Settings::values.volume.SetValue(static_cast<u8>(p_val));
            UpdateVolumeUI();
            UpdateMuteButton();
        });
        preset_layout->addWidget(btn);
    }
    pop_layout->addLayout(preset_layout);

    volume_button = new VolumeButton();
    volume_button->setObjectName(QStringLiteral("TogglableStatusBarButton"));
    volume_button->setFocusPolicy(Qt::NoFocus);
    volume_button->setCheckable(true);
    UpdateVolumeUI();
    connect(volume_slider, &QSlider::valueChanged, this, [this](int percentage) {
        Settings::values.audio_muted.SetValue(false);
        const auto volume = static_cast<u8>(percentage);
        Settings::values.volume.SetValue(volume);
        UpdateVolumeUI();
        UpdateMuteButton();
    });
    connect(volume_button, &QPushButton::clicked, this, [this] {
        UpdateVolumeUI();
        const bool will_show = !volume_popup->isVisible();
        volume_popup->setVisible(will_show);
        if (will_show) {
            const QPoint btn_top_left = volume_button->mapToGlobal(QPoint(0, 0));
            const int popup_width = 240;
            const int popup_height = 115;
            const int popup_x = btn_top_left.x() + (volume_button->width() - popup_width) / 2;
            const int popup_y = btn_top_left.y() - popup_height - 6;
            volume_popup->setGeometry(popup_x, popup_y, popup_width, popup_height);
            volume_popup->raise();
        }
    });
    volume_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(volume_button, &QPushButton::customContextMenuRequested,
            [this](const QPoint& menu_location) {
                QMenu context_menu;
                context_menu.addAction(
                    Settings::values.audio_muted.GetValue() ? tr("Включить звук") : tr("Выключить звук"), [this] {
                        OnMute();
                    });

                context_menu.addAction(tr("Сбросить громкость (100%)"), [this] {
                    Settings::values.volume.SetValue(100);
                    UpdateVolumeUI();
                });

                ShowMenuAtWidget(context_menu, volume_button);
                volume_button->repaint();
            });
    connect(volume_button, &VolumeButton::VolumeChanged, this, &MainWindow::UpdateVolumeUI);

    // setup AA button
    aa_status_button = new QPushButton();
    aa_status_button->setObjectName(QStringLiteral("TogglableStatusBarButton"));
    aa_status_button->setFocusPolicy(Qt::NoFocus);
    aa_status_button->setCheckable(true);
    aa_status_button->setChecked(true);
    UpdateAAText();
    auto show_aa_menu = [this]() {
        QMenu context_menu(this);
        const auto cur_aa = Settings::values.anti_aliasing.GetValue();
        for (auto const& aa_text_pair : ConfigurationShared::anti_aliasing_texts_map) {
            auto* act = context_menu.addAction(aa_text_pair.second, [this, aa_text_pair] {
                Settings::values.anti_aliasing.SetValue(aa_text_pair.first);
                UpdateAAText();
            });
            act->setCheckable(true);
            act->setChecked(aa_text_pair.first == cur_aa);
        }
        ShowMenuAtWidget(context_menu, aa_status_button);
        aa_status_button->repaint();
    };
    connect(aa_status_button, &QPushButton::clicked, show_aa_menu);
    aa_status_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(aa_status_button, &QPushButton::customContextMenuRequested, show_aa_menu);

    // Setup Filter button
    filter_status_button = new QPushButton();
    filter_status_button->setObjectName(QStringLiteral("TogglableStatusBarButton"));
    filter_status_button->setFocusPolicy(Qt::NoFocus);
    filter_status_button->setCheckable(true);
    filter_status_button->setChecked(true);
    UpdateFilterText();
    auto show_filter_menu = [this]() {
        QMenu context_menu(this);
        const auto cur_filter = Settings::values.scaling_filter.GetValue();
        for (auto const& filter_text_pair : ConfigurationShared::scaling_filter_texts_map) {
            auto* act = context_menu.addAction(filter_text_pair.second, [this, filter_text_pair] {
                Settings::values.scaling_filter.SetValue(filter_text_pair.first);
                UpdateFilterText();
            });
            act->setCheckable(true);
            act->setChecked(filter_text_pair.first == cur_filter);
        }
        ShowMenuAtWidget(context_menu, filter_status_button);
        filter_status_button->repaint();
    };
    connect(filter_status_button, &QPushButton::clicked, show_filter_menu);
    filter_status_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(filter_status_button, &QPushButton::customContextMenuRequested, show_filter_menu);

    // Setup Dock button
    dock_status_button = new QPushButton();
    dock_status_button->setObjectName(QStringLiteral("DockingStatusBarButton"));
    dock_status_button->setFocusPolicy(Qt::NoFocus);
    dock_status_button->setCheckable(true);
    UpdateDockedButton();
    auto show_dock_menu = [this]() {
        QMenu context_menu(this);
        const auto cur_dock = Settings::values.use_docked_mode.GetValue();
        for (auto const& pair : ConfigurationShared::use_docked_mode_texts_map) {
            auto* act = context_menu.addAction(pair.second, [this, pair] {
                if (pair.first != Settings::values.use_docked_mode.GetValue()) {
                    OnToggleDockedMode();
                }
            });
            act->setCheckable(true);
            act->setChecked(pair.first == cur_dock);
        }
        ShowMenuAtWidget(context_menu, dock_status_button);
        dock_status_button->repaint();
    };
    connect(dock_status_button, &QPushButton::clicked, show_dock_menu);
    dock_status_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(dock_status_button, &QPushButton::customContextMenuRequested, show_dock_menu);

    // Setup GPU Accuracy button
    gpu_accuracy_button = new QPushButton();
    gpu_accuracy_button->setObjectName(QStringLiteral("GPUStatusBarButton"));
    gpu_accuracy_button->setCheckable(true);
    gpu_accuracy_button->setFocusPolicy(Qt::NoFocus);
    UpdateGPUAccuracyButton();
    auto show_gpu_menu = [this]() {
        QMenu context_menu(this);
        const auto cur_gpu = Settings::values.gpu_accuracy.GetValue();
        for (auto const& gpu_accuracy_pair : ConfigurationShared::gpu_accuracy_texts_map) {
            auto* act = context_menu.addAction(gpu_accuracy_pair.second, [this, gpu_accuracy_pair] {
                Settings::values.gpu_accuracy.SetValue(gpu_accuracy_pair.first);
                UpdateGPUAccuracyButton();
            });
            act->setCheckable(true);
            act->setChecked(gpu_accuracy_pair.first == cur_gpu);
        }
        ShowMenuAtWidget(context_menu, gpu_accuracy_button);
        gpu_accuracy_button->repaint();
    };
    connect(gpu_accuracy_button, &QPushButton::clicked, show_gpu_menu);
    gpu_accuracy_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(gpu_accuracy_button, &QPushButton::customContextMenuRequested, show_gpu_menu);

    // Setup Renderer API button
    renderer_status_button = new QPushButton();
    renderer_status_button->setObjectName(QStringLiteral("RendererStatusBarButton"));
    renderer_status_button->setCheckable(true);
    renderer_status_button->setFocusPolicy(Qt::NoFocus);
    UpdateAPIText();
    renderer_status_button->setChecked(Settings::values.renderer_backend.GetValue() ==
                                       Settings::RendererBackend::Vulkan);
    auto show_renderer_menu = [this]() {
        QMenu context_menu(this);
        const auto cur_api = Settings::values.renderer_backend.GetValue();
        for (auto const& renderer_backend_pair : ConfigurationShared::renderer_backend_texts_map) {
            if (renderer_backend_pair.first == Settings::RendererBackend::Null) {
                continue;
            }
            auto* act = context_menu.addAction(renderer_backend_pair.second, [this, renderer_backend_pair] {
                Settings::values.renderer_backend.SetValue(renderer_backend_pair.first);
                UpdateAPIText();
            });
            act->setCheckable(true);
            act->setChecked(renderer_backend_pair.first == cur_api);
        }
        ShowMenuAtWidget(context_menu, renderer_status_button);
        renderer_status_button->repaint();
    };
    connect(renderer_status_button, &QPushButton::clicked, show_renderer_menu);
    renderer_status_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(renderer_status_button, &QPushButton::customContextMenuRequested, show_renderer_menu);

    // Setup Aspect Ratio button
    aspect_ratio_button = new QPushButton();
    aspect_ratio_button->setObjectName(QStringLiteral("TogglableStatusBarButton"));
    aspect_ratio_button->setFocusPolicy(Qt::NoFocus);
    UpdateAspectText();
    auto show_aspect_menu = [this]() {
        QMenu context_menu(this);
        const auto cur_aspect = static_cast<u32>(Settings::values.aspect_ratio.GetValue());
        const auto combo_map = ConfigurationShared::ComboboxEnumeration(this);
        const auto it = combo_map->find(Settings::EnumMetadata<Settings::AspectRatio>::Index());
        if (it != combo_map->end()) {
            for (const auto& item : it->second) {
                const u32 val = item.first;
                const QString name = item.second;
                auto* act = context_menu.addAction(name, [this, val] {
                    Settings::values.aspect_ratio.SetValue(static_cast<Settings::AspectRatio>(val));
                    UpdateAspectText();
                });
                act->setCheckable(true);
                act->setChecked(val == cur_aspect);
            }
        }
        ShowMenuAtWidget(context_menu, aspect_ratio_button);
    };
    connect(aspect_ratio_button, &QPushButton::clicked, show_aspect_menu);
    aspect_ratio_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(aspect_ratio_button, &QPushButton::customContextMenuRequested, show_aspect_menu);

    // Setup DMA Accuracy button
    dma_accuracy_button = new QPushButton();
    dma_accuracy_button->setObjectName(QStringLiteral("TogglableStatusBarButton"));
    dma_accuracy_button->setFocusPolicy(Qt::NoFocus);
    UpdateDmaText();
    auto show_dma_menu = [this]() {
        QMenu context_menu(this);
        const auto cur_dma = Settings::values.dma_accuracy.GetValue();
        const std::vector<std::pair<Settings::DmaAccuracy, QString>> items = {
            {Settings::DmaAccuracy::Default, tr("По умолчанию")},
            {Settings::DmaAccuracy::Normal, tr("Нормально")},
            {Settings::DmaAccuracy::Unsafe, tr("Небезопасно")},
            {Settings::DmaAccuracy::Safe, tr("Безопасно")},
        };
        for (const auto& item : items) {
            auto* act = context_menu.addAction(item.second, [this, item] {
                Settings::values.dma_accuracy.SetValue(item.first);
                UpdateDmaText();
            });
            act->setCheckable(true);
            act->setChecked(item.first == cur_dma);
        }
        ShowMenuAtWidget(context_menu, dma_accuracy_button);
    };
    connect(dma_accuracy_button, &QPushButton::clicked, show_dma_menu);
    dma_accuracy_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(dma_accuracy_button, &QPushButton::customContextMenuRequested, show_dma_menu);

    // Setup GPU Fence button
    gpu_fence_button = new QPushButton();
    gpu_fence_button->setObjectName(QStringLiteral("TogglableStatusBarButton"));
    gpu_fence_button->setFocusPolicy(Qt::NoFocus);
    UpdateGpuFenceText();
    auto show_gpu_fence_menu = [this]() {
        QMenu context_menu(this);
        const auto cur_fence = Settings::values.gpu_fence_behavior.GetValue();
        const std::vector<std::pair<Settings::GpuFenceBehavior, QString>> items = {
            {Settings::GpuFenceBehavior::Default, tr("По умолчанию")},
            {Settings::GpuFenceBehavior::Immediate, tr("Немедленно")},
            {Settings::GpuFenceBehavior::Balanced, tr("Сбалансированно")},
            {Settings::GpuFenceBehavior::Accurate, tr("Точно")},
            {Settings::GpuFenceBehavior::Strict, tr("Строго")},
        };
        for (const auto& item : items) {
            auto* act = context_menu.addAction(item.second, [this, item] {
                Settings::values.gpu_fence_behavior.SetValue(item.first);
                UpdateGpuFenceText();
            });
            act->setCheckable(true);
            act->setChecked(item.first == cur_fence);
        }
        ShowMenuAtWidget(context_menu, gpu_fence_button);
    };
    connect(gpu_fence_button, &QPushButton::clicked, show_gpu_fence_menu);
    gpu_fence_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(gpu_fence_button, &QPushButton::customContextMenuRequested, show_gpu_fence_menu);

    // Setup VRAM Mode button
    vram_mode_button = new QPushButton();
    vram_mode_button->setObjectName(QStringLiteral("TogglableStatusBarButton"));
    vram_mode_button->setFocusPolicy(Qt::NoFocus);
    UpdateVramText();
    auto show_vram_menu = [this]() {
        QMenu context_menu(this);
        const auto cur_vram = Settings::values.vram_usage_mode.GetValue();
        const std::vector<std::pair<Settings::VramUsageMode, QString>> options = {
            {Settings::VramUsageMode::Conservative, tr("Экономный")},
            {Settings::VramUsageMode::Normal, tr("Нормальный")},
            {Settings::VramUsageMode::Aggressive, tr("Агрессивный")},
        };
        for (const auto& opt : options) {
            auto* act = context_menu.addAction(opt.second, [this, opt] {
                Settings::values.vram_usage_mode.SetValue(opt.first);
                UpdateVramText();
            });
            act->setCheckable(true);
            act->setChecked(opt.first == cur_vram);
        }
        ShowMenuAtWidget(context_menu, vram_mode_button);
    };
    connect(vram_mode_button, &QPushButton::clicked, show_vram_menu);
    vram_mode_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(vram_mode_button, &QPushButton::customContextMenuRequested, show_vram_menu);

    // Setup Anisotropy button
    anisotropy_button = new QPushButton();
    anisotropy_button->setObjectName(QStringLiteral("TogglableStatusBarButton"));
    anisotropy_button->setFocusPolicy(Qt::NoFocus);
    UpdateAnisotropyText();
    auto show_anisotropy_menu = [this]() {
        QMenu context_menu(this);
        const auto cur_aniso = Settings::values.max_anisotropy.GetValue();
        const std::vector<std::pair<Settings::AnisotropyMode, QString>> options = {
            {Settings::AnisotropyMode::Automatic, tr("Автоматически")},
            {Settings::AnisotropyMode::Default, tr("По умолчанию")},
            {Settings::AnisotropyMode::X2, QStringLiteral("2x")},
            {Settings::AnisotropyMode::X4, QStringLiteral("4x")},
            {Settings::AnisotropyMode::X8, QStringLiteral("8x")},
            {Settings::AnisotropyMode::X16, QStringLiteral("16x")},
            {Settings::AnisotropyMode::X32, QStringLiteral("32x")},
            {Settings::AnisotropyMode::X64, QStringLiteral("64x")},
            {Settings::AnisotropyMode::None, tr("Отключено")},
        };
        for (const auto& opt : options) {
            auto* act = context_menu.addAction(opt.second, [this, opt] {
                Settings::values.max_anisotropy.SetValue(opt.first);
                UpdateAnisotropyText();
            });
            act->setCheckable(true);
            act->setChecked(opt.first == cur_aniso);
        }
        ShowMenuAtWidget(context_menu, anisotropy_button);
    };
    connect(anisotropy_button, &QPushButton::clicked, show_anisotropy_menu);
    anisotropy_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(anisotropy_button, &QPushButton::customContextMenuRequested, show_anisotropy_menu);

    // Setup ASTC Decode button
    astc_decode_button = new QPushButton();
    astc_decode_button->setObjectName(QStringLiteral("TogglableStatusBarButton"));
    astc_decode_button->setFocusPolicy(Qt::NoFocus);
    UpdateAstcDecodeText();
    auto show_astc_decode_menu = [this]() {
        QMenu context_menu(this);
        const auto cur_dec = Settings::values.accelerate_astc.GetValue();
        const std::vector<std::pair<Settings::AstcDecodeMode, QString>> options = {
            {Settings::AstcDecodeMode::CpuAsynchronous, tr("ЦП Асинхронно")},
            {Settings::AstcDecodeMode::Cpu, tr("ЦП")},
            {Settings::AstcDecodeMode::Gpu, tr("ГПУ")},
        };
        for (const auto& opt : options) {
            auto* act = context_menu.addAction(opt.second, [this, opt] {
                Settings::values.accelerate_astc.SetValue(opt.first);
                UpdateAstcDecodeText();
            });
            act->setCheckable(true);
            act->setChecked(opt.first == cur_dec);
        }
        ShowMenuAtWidget(context_menu, astc_decode_button);
    };
    connect(astc_decode_button, &QPushButton::clicked, show_astc_decode_menu);
    astc_decode_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(astc_decode_button, &QPushButton::customContextMenuRequested, show_astc_decode_menu);

    // Setup ASTC Recompress button
    astc_recompress_button = new QPushButton();
    astc_recompress_button->setObjectName(QStringLiteral("TogglableStatusBarButton"));
    astc_recompress_button->setFocusPolicy(Qt::NoFocus);
    UpdateAstcRecompressText();
    auto show_astc_recompress_menu = [this]() {
        QMenu context_menu(this);
        const auto cur_rec = Settings::values.astc_recompression.GetValue();
        const std::vector<std::pair<Settings::AstcRecompression, QString>> options = {
            {Settings::AstcRecompression::Uncompressed, tr("Без сжатия (Лучшее качество)")},
            {Settings::AstcRecompression::Bc1, tr("BC1 (Низкое качество)")},
            {Settings::AstcRecompression::Bc3, tr("BC3 (Среднее качество)")},
            {Settings::AstcRecompression::Bc5, tr("BC5 (Высокое качество)")},
        };
        for (const auto& opt : options) {
            auto* act = context_menu.addAction(opt.second, [this, opt] {
                Settings::values.astc_recompression.SetValue(opt.first);
                UpdateAstcRecompressText();
            });
            act->setCheckable(true);
            act->setChecked(opt.first == cur_rec);
        }
        ShowMenuAtWidget(context_menu, astc_recompress_button);
    };
    connect(astc_recompress_button, &QPushButton::clicked, show_astc_recompress_menu);
    astc_recompress_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(astc_recompress_button, &QPushButton::customContextMenuRequested, show_astc_recompress_menu);

    // Setup Addons button
    addons_status_button = new QPushButton();
    addons_status_button->setObjectName(QStringLiteral("TogglableStatusBarButton"));
    addons_status_button->setFocusPolicy(Qt::NoFocus);
    UpdateAddonsStatusButton();
    connect(addons_status_button, &QPushButton::clicked, this, [this] {
        if (m_current_addons_title_id == 0 && game_list) {
            const auto [tid, path] = game_list->GetSelectedGameInfo();
            if (tid != 0) {
                m_current_addons_title_id = tid;
                m_current_addons_game_path = path.toStdString();
                m_current_addons_game_name = QFileInfo(path).completeBaseName();
                UpdateAddonsStatusButton(m_current_addons_title_id, m_current_addons_game_name);
            }
        }
        ShowDLCDialog(m_current_addons_title_id, m_current_addons_game_name);
    });
    auto show_addons_menu = [this]() {
        if (m_current_addons_title_id == 0 && game_list) {
            const auto [tid, path] = game_list->GetSelectedGameInfo();
            if (tid != 0) {
                m_current_addons_title_id = tid;
                m_current_addons_game_path = path.toStdString();
                m_current_addons_game_name = QFileInfo(path).completeBaseName();
                UpdateAddonsStatusButton(m_current_addons_title_id, m_current_addons_game_name);
            }
        }
        QMenu context_menu(this);
        if (m_current_addons_title_id == 0) {
            auto* act = context_menu.addAction(tr("⚠️ Нет выделенной или запущенной игры"));
            act->setEnabled(false);
        } else {
            const FileSys::PatchManager patch_manager(m_current_addons_title_id, QtCommon::system->GetFileSystemController(), QtCommon::system->GetContentProvider());
            auto patches = patch_manager.GetPatches();

            auto* title_act = context_menu.addAction(tr("🎮 Дополнения и патчи (ID: 0x%1)")
                .arg(QStringLiteral("%1").arg(m_current_addons_title_id, 16, 16, QLatin1Char('0')).toUpper()));
            title_act->setEnabled(false);
            context_menu.addSeparator();

            context_menu.addAction(tr("📋 Открыть менеджер дополнений..."), [this] {
                ShowDLCDialog(m_current_addons_title_id, m_current_addons_game_name);
            });

            context_menu.addAction(tr("📑 Копировать список дополнений"), [this] {
                const FileSys::PatchManager pm(m_current_addons_title_id, QtCommon::system->GetFileSystemController(), QtCommon::system->GetContentProvider());
                const auto pts = pm.GetPatches();
                QStringList lines;
                lines << QStringLiteral("STORM EDEN — Список дополнений");
                lines << QStringLiteral("Игра: %1 (ID: 0x%2)").arg(m_current_addons_game_name, QStringLiteral("%1").arg(m_current_addons_title_id, 16, 16, QLatin1Char('0')).toUpper());
                lines << QStringLiteral("------------------------------------------------------------");
                int idx = 1;
                for (const auto& p : pts) {
                    if (p.type == FileSys::PatchType::DLC || p.type == FileSys::PatchType::Mod || p.type == FileSys::PatchType::Update) {
                        QString ptype = (p.type == FileSys::PatchType::Update) ? tr("Обновление") :
                                        (p.type == FileSys::PatchType::DLC) ? tr("Дополнение") : tr("Мод");
                        lines << QStringLiteral("%1. 0x%2 — [%3] %4 (%5)")
                            .arg(QString::number(idx++), QStringLiteral("%1").arg(p.title_id, 16, 16, QLatin1Char('0')).toUpper(),
                                 ptype, QString::fromStdString(p.name), p.enabled ? tr("Включено") : tr("Отключено"));
                    }
                }
                QGuiApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
                statusBar()->showMessage(tr("Список дополнений скопирован в буфер обмена."), 3000);
            });

            context_menu.addSeparator();

            int addon_count = 0;
            for (const auto& patch : patches) {
                QString name = QString::fromStdString(patch.name);
                QString ver = QString::fromStdString(patch.version);
                QString icon = QStringLiteral("📦");
                if (patch.type == FileSys::PatchType::Update) {
                    icon = QStringLiteral("🆙");
                    name = tr("Обновление");
                } else if (patch.type == FileSys::PatchType::DLC) {
                    icon = QStringLiteral("🧩");
                    name = tr("Дополнение");
                } else if (patch.type == FileSys::PatchType::Mod) {
                    icon = QStringLiteral("⚡");
                }

                QString text = ver.isEmpty() ? QStringLiteral("%1  %2").arg(icon, name)
                                             : QStringLiteral("%1  %2: %3").arg(icon, name, ver);
                QAction* act = context_menu.addAction(text);
                act->setCheckable(true);
                act->setChecked(patch.enabled);
                act->setEnabled(false);
                addon_count++;
            }

            if (addon_count == 0) {
                auto* empty_act = context_menu.addAction(tr("Нет установленных дополнений"));
                empty_act->setEnabled(false);
            }

            context_menu.addSeparator();
            context_menu.addAction(tr("⚙️ Свойства игры (Управление дополнениями)..."), [this] {
                OpenPerGameConfiguration(m_current_addons_title_id, m_current_addons_game_path);
            });
        }
        ShowMenuAtWidget(context_menu, addons_status_button);
    };
    addons_status_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(addons_status_button, &QPushButton::customContextMenuRequested, show_addons_menu);

    // Setup Resolution Scale button
    res_scale_button = new QPushButton();
    res_scale_button->setObjectName(QStringLiteral("TogglableStatusBarButton"));
    res_scale_button->setFocusPolicy(Qt::NoFocus);
    UpdateResScaleText();
    auto show_res_menu = [this]() {
        QMenu context_menu(this);
        const auto cur_res = Settings::values.resolution_setup.GetValue();
        const std::vector<std::pair<Settings::ResolutionSetup, QString>> res_options = {
            {Settings::ResolutionSetup::Res1_2X, tr("0.5X")},
            {Settings::ResolutionSetup::Res3_4X, tr("0.75X")},
            {Settings::ResolutionSetup::Res1X, tr("1X")},
            {Settings::ResolutionSetup::Res3_2X, tr("1.5X")},
            {Settings::ResolutionSetup::Res2X, tr("2X")},
            {Settings::ResolutionSetup::Res3X, tr("3X")},
            {Settings::ResolutionSetup::Res4X, tr("4X")},
        };
        for (const auto& opt : res_options) {
            auto* act = context_menu.addAction(opt.second, [this, opt] {
                Settings::values.resolution_setup.SetValue(opt.first);
                UpdateResScaleText();
            });
            act->setCheckable(true);
            act->setChecked(opt.first == cur_res);
        }
        ShowMenuAtWidget(context_menu, res_scale_button);
    };
    connect(res_scale_button, &QPushButton::clicked, show_res_menu);
    res_scale_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(res_scale_button, &QPushButton::customContextMenuRequested, show_res_menu);

    // Setup Refresh Button
    refresh_button = new QPushButton();
    refresh_button->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
    refresh_button->setText(tr("СПИСОК:\nОбновить"));
    refresh_button->setObjectName(QStringLiteral("RefreshButton"));
    refresh_button->setToolTip(tr("Обновить список игр"));
    refresh_button->setFocusPolicy(Qt::NoFocus);
    connect(refresh_button, &QPushButton::clicked, this, &MainWindow::OnGameListRefresh);

    // Setup Airplane Mode button
    airplane_mode_button = new QPushButton();
    airplane_mode_button->setObjectName(QStringLiteral("TogglableStatusBarButton"));
    airplane_mode_button->setFocusPolicy(Qt::NoFocus);
    UpdateAirplaneModeButton();
    connect(airplane_mode_button, &QPushButton::clicked, this, [this] {
        Settings::values.airplane_mode.SetValue(!Settings::values.airplane_mode.GetValue());
        UpdateAirplaneModeButton();
    });
    airplane_mode_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(airplane_mode_button, &QPushButton::customContextMenuRequested, [this] {
        Settings::values.airplane_mode.SetValue(!Settings::values.airplane_mode.GetValue());
        UpdateAirplaneModeButton();
    });

    // Setup VSync button
    vsync_mode_button = new QPushButton();
    vsync_mode_button->setObjectName(QStringLiteral("TogglableStatusBarButton"));
    vsync_mode_button->setFocusPolicy(Qt::NoFocus);
    UpdateVSyncText();
    auto show_vsync_menu = [this]() {
        QMenu context_menu(this);
        const auto cur_vsync = Settings::values.vsync_mode.GetValue();
        const std::vector<std::pair<Settings::VSyncMode, QString>> vsync_options = {
            {Settings::VSyncMode::Fifo, tr("FIFO")},
            {Settings::VSyncMode::FifoRelaxed, tr("FIFO Relaxed")},
            {Settings::VSyncMode::Mailbox, tr("Mailbox")},
            {Settings::VSyncMode::Immediate, tr("Immediate")},
        };
        for (const auto& opt : vsync_options) {
            auto* act = context_menu.addAction(opt.second, [this, opt] {
                Settings::values.vsync_mode.SetValue(opt.first);
                UpdateVSyncText();
            });
            act->setCheckable(true);
            act->setChecked(opt.first == cur_vsync);
        }
        ShowMenuAtWidget(context_menu, vsync_mode_button);
    };
    connect(vsync_mode_button, &QPushButton::clicked, show_vsync_menu);
    vsync_mode_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(vsync_mode_button, &QPushButton::customContextMenuRequested, show_vsync_menu);

    // Setup Speed Limit button
    speed_limit_button = new QPushButton();
    speed_limit_button->setObjectName(QStringLiteral("TogglableStatusBarButton"));
    speed_limit_button->setFocusPolicy(Qt::NoFocus);
    UpdateSpeedLimitText();
    auto show_speed_menu = [this]() {
        QMenu context_menu(this);
        const bool use_limit = Settings::values.use_speed_limit.GetValue();
        const u16 speed_val = Settings::values.speed_limit.GetValue();

        auto* act100 = context_menu.addAction(tr("100%"), [this] {
            Settings::values.use_speed_limit.SetValue(true);
            Settings::values.speed_limit.SetValue(100);
            UpdateSpeedLimitText();
        });
        act100->setCheckable(true);
        act100->setChecked(use_limit && speed_val == 100);

        auto* act150 = context_menu.addAction(tr("150%"), [this] {
            Settings::values.use_speed_limit.SetValue(true);
            Settings::values.speed_limit.SetValue(150);
            UpdateSpeedLimitText();
        });
        act150->setCheckable(true);
        act150->setChecked(use_limit && speed_val == 150);

        auto* act200 = context_menu.addAction(tr("200%"), [this] {
            Settings::values.use_speed_limit.SetValue(true);
            Settings::values.speed_limit.SetValue(200);
            UpdateSpeedLimitText();
        });
        act200->setCheckable(true);
        act200->setChecked(use_limit && speed_val == 200);

        auto* act300 = context_menu.addAction(tr("300%"), [this] {
            Settings::values.use_speed_limit.SetValue(true);
            Settings::values.speed_limit.SetValue(300);
            UpdateSpeedLimitText();
        });
        act300->setCheckable(true);
        act300->setChecked(use_limit && speed_val == 300);

        auto* act_unlimit = context_menu.addAction(tr("Без лимита скорости"), [this] {
            Settings::values.use_speed_limit.SetValue(false);
            UpdateSpeedLimitText();
        });
        act_unlimit->setCheckable(true);
        act_unlimit->setChecked(!use_limit);

        ShowMenuAtWidget(context_menu, speed_limit_button);
    };
    connect(speed_limit_button, &QPushButton::clicked, show_speed_menu);
    speed_limit_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(speed_limit_button, &QPushButton::customContextMenuRequested, show_speed_menu);

    // Setup NVDEC button
    nvdec_status_button = new QPushButton();
    nvdec_status_button->setObjectName(QStringLiteral("TogglableStatusBarButton"));
    nvdec_status_button->setFocusPolicy(Qt::NoFocus);
    UpdateNvdecText();
    auto show_nvdec_menu = [this]() {
        QMenu context_menu(this);
        const auto cur_nvdec = Settings::values.nvdec_emulation.GetValue();
        const std::vector<std::pair<Settings::NvdecEmulation, QString>> nvdec_options = {
            {Settings::NvdecEmulation::Gpu, tr("ГПУ")},
            {Settings::NvdecEmulation::Cpu, tr("ЦП")},
            {Settings::NvdecEmulation::Off, tr("Выключено")},
        };
        for (const auto& opt : nvdec_options) {
            auto* act = context_menu.addAction(opt.second, [this, opt] {
                Settings::values.nvdec_emulation.SetValue(opt.first);
                UpdateNvdecText();
            });
            act->setCheckable(true);
            act->setChecked(opt.first == cur_nvdec);
        }
        ShowMenuAtWidget(context_menu, nvdec_status_button);
    };
    connect(nvdec_status_button, &QPushButton::clicked, show_nvdec_menu);
    nvdec_status_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(nvdec_status_button, &QPushButton::customContextMenuRequested, show_nvdec_menu);

    // Setup CPU Accuracy button
    cpu_accuracy_button = new QPushButton();
    cpu_accuracy_button->setObjectName(QStringLiteral("TogglableStatusBarButton"));
    cpu_accuracy_button->setFocusPolicy(Qt::NoFocus);
    UpdateCpuAccuracyText();
    auto show_cpu_menu = [this]() {
        QMenu context_menu(this);
        const auto cur_cpu = Settings::values.cpu_accuracy.GetValue();
        const std::vector<std::pair<Settings::CpuAccuracy, QString>> cpu_options = {
            {Settings::CpuAccuracy::Auto, tr("Авто")},
            {Settings::CpuAccuracy::Accurate, tr("Точно")},
            {Settings::CpuAccuracy::Unsafe, tr("Небезопасно")},
        };
        for (const auto& opt : cpu_options) {
            auto* act = context_menu.addAction(opt.second, [this, opt] {
                Settings::values.cpu_accuracy.SetValue(opt.first);
                UpdateCpuAccuracyText();
            });
            act->setCheckable(true);
            act->setChecked(opt.first == cur_cpu);
        }
        ShowMenuAtWidget(context_menu, cpu_accuracy_button);
    };
    connect(cpu_accuracy_button, &QPushButton::clicked, show_cpu_menu);
    cpu_accuracy_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(cpu_accuracy_button, &QPushButton::customContextMenuRequested, show_cpu_menu);

    // Setup Disk Shader Cache button
    disk_cache_button = new QPushButton();
    disk_cache_button->setObjectName(QStringLiteral("TogglableStatusBarButton"));
    disk_cache_button->setFocusPolicy(Qt::NoFocus);
    UpdateDiskCacheText();
    connect(disk_cache_button, &QPushButton::clicked, this, [this] {
        Settings::values.use_disk_shader_cache.SetValue(!Settings::values.use_disk_shader_cache.GetValue());
        UpdateDiskCacheText();
    });
    disk_cache_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(disk_cache_button, &QPushButton::customContextMenuRequested, [this] {
        Settings::values.use_disk_shader_cache.SetValue(!Settings::values.use_disk_shader_cache.GetValue());
        UpdateDiskCacheText();
    });

    // Setup Fullscreen button
    fullscreen_button = new QPushButton();
    fullscreen_button->setObjectName(QStringLiteral("TogglableStatusBarButton"));
    fullscreen_button->setFocusPolicy(Qt::NoFocus);
    UpdateFullscreenButton();
    connect(fullscreen_button, &QPushButton::clicked, this, [this] {
        ui->action_Fullscreen->setChecked(!ui->action_Fullscreen->isChecked());
        ToggleFullscreen();
        UpdateFullscreenButton();
    });

    // Setup Mute button
    mute_button = new QPushButton();
    mute_button->setObjectName(QStringLiteral("TogglableStatusBarButton"));
    mute_button->setFocusPolicy(Qt::NoFocus);
    UpdateMuteButton();
    connect(mute_button, &QPushButton::clicked, this, &MainWindow::OnMute);

    // Setup Footer Customize button
    footer_customize_button = new QPushButton(QStringLiteral("⚙"));
    footer_customize_button->setObjectName(QStringLiteral("FooterCustomizeButton"));
    footer_customize_button->setToolTip(tr("Настройка отображения разделов и кнопок подвала"));
    footer_customize_button->setFocusPolicy(Qt::NoFocus);
    footer_customize_button->setFixedWidth(28);
    footer_customize_button->setMinimumHeight(32);
    footer_customize_button->setStyleSheet(QStringLiteral(
        "QPushButton#FooterCustomizeButton {"
        "  background-color: rgba(255, 255, 255, 0.08);"
        "  color: #00f2fe;"
        "  border: 1px solid rgba(255, 255, 255, 0.15);"
        "  border-radius: 4px;"
        "  font-size: 11pt;"
        "  font-weight: bold;"
        "  padding: 0px;"
        "}"
        "QPushButton#FooterCustomizeButton:hover {"
        "  background-color: #00f2fe;"
        "  color: #000000;"
        "  border-color: #00f2fe;"
        "}"
    ));
    connect(footer_customize_button, &QPushButton::clicked, this, &MainWindow::ShowFooterCustomizeMenu);

    // ============================================================
    // GROUPED STATUS BAR LAYOUT
    // ============================================================

    m_status_groups.clear();

    auto createGroup = [this](const QString& title, const QString& color, const QList<QWidget*>& widgets) -> QWidget* {
        auto* container = new QWidget();
        container->setObjectName(QStringLiteral("StatusBarGroup"));
        container->setStyleSheet(QStringLiteral(
            "QWidget#StatusBarGroup {"
            "  background-color: #0e1320;"
            "  border: 1px solid rgba(255, 255, 255, 0.10);"
            "  border-top: 2px solid %1;"
            "  border-radius: 5px;"
            "  margin: 1px 1px;"
            "  padding: 1px 2px;"
            "}"
            "QWidget#StatusBarGroup:hover {"
            "  background-color: #141b2e;"
            "  border: 1px solid %1;"
            "  border-top: 2px solid %1;"
            "}"
            "QPushButton#StatusBarGroupLabel {"
            "  background-color: rgba(255, 255, 255, 0.07);"
            "  color: %1;"
            "  font-size: 6.8pt;"
            "  font-weight: 800;"
            "  letter-spacing: 0.8px;"
            "  padding: 1px 6px;"
            "  margin: 0px 1px 2px 1px;"
            "  border: 1px solid rgba(255, 255, 255, 0.08);"
            "  border-radius: 3px;"
            "  min-height: 12px;"
            "}"
            "QPushButton#StatusBarGroupLabel:hover {"
            "  background-color: %1;"
            "  color: #000000;"
            "  border: 1px solid %1;"
            "}"
            "QPushButton#TogglableStatusBarButton, QPushButton {"
            "  background-color: #121826;"
            "  color: #ffffff;"
            "  border: 1px solid rgba(255, 255, 255, 0.13);"
            "  border-radius: 4px;"
            "  padding: 2px 5px;"
            "  font-size: 6.8pt;"
            "  font-weight: 700;"
            "  min-height: 26px;"
            "  min-width: 44px;"
            "}"
            "QPushButton#TogglableStatusBarButton:hover, QPushButton:hover {"
            "  background-color: rgba(255, 255, 255, 0.16);"
            "  color: %1;"
            "  border: 1px solid %1;"
            "}"
            "QPushButton#TogglableStatusBarButton:pressed, QPushButton:pressed {"
            "  background-color: %1;"
            "  color: #000000;"
            "}"
            "QLabel {"
            "  color: #ffffff;"
            "  font-size: 6.8pt;"
            "  font-weight: 700;"
            "}"
            "QLabel:hover {"
            "  color: %1;"
            "  background-color: rgba(255, 255, 255, 0.10);"
            "  border-radius: 2px;"
            "}"
        ).arg(color));

        auto* layout = new QVBoxLayout(container);
        layout->setContentsMargins(2, 1, 2, 1);
        layout->setSpacing(1);
        layout->setAlignment(Qt::AlignCenter);

        auto* headerBtn = new QPushButton(title);
        headerBtn->setObjectName(QStringLiteral("StatusBarGroupLabel"));
        headerBtn->setCursor(Qt::PointingHandCursor);
        headerBtn->setFocusPolicy(Qt::NoFocus);
        headerBtn->setFlat(true);
        headerBtn->setToolTip(tr("Нажмите для быстрого меню раздела «%1»").arg(title));
        auto show_grp = [this, title, container]() {
            ShowGroupMenu(title, container);
        };
        connect(headerBtn, &QPushButton::clicked, show_grp);
        headerBtn->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(headerBtn, &QPushButton::customContextMenuRequested, show_grp);
        layout->addWidget(headerBtn);

        auto* btnRow = new QWidget();
        btnRow->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
        auto* btnLayout = new QHBoxLayout(btnRow);
        btnLayout->setContentsMargins(0, 0, 0, 0);
        btnLayout->setSpacing(2);
        btnLayout->setAlignment(Qt::AlignCenter);
        for (auto* w : widgets) {
            w->setMinimumHeight(26);
            btnLayout->addWidget(w);
        }
        layout->addWidget(btnRow);

        container->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(container, &QWidget::customContextMenuRequested, show_grp);

        m_status_groups.push_back(container);
        return container;
    };

    // --- Group 1: УПРАВЛЕНИЕ (Green accent) ---
    statusBar()->insertPermanentWidget(0, createGroup(
        tr("УПРАВЛЕНИЕ"), QStringLiteral("#00e676"),
        {refresh_button, fullscreen_button}));

    // --- Group 2: ДОПОЛНЕНИЯ (Purple/Magenta accent) ---
    statusBar()->insertPermanentWidget(1, createGroup(
        tr("ДОПОЛНЕНИЯ"), QStringLiteral("#e040fb"),
        {addons_status_button}));

    // --- Group 3: РЕНДЕР (Cyan accent) ---
    statusBar()->insertPermanentWidget(2, createGroup(
        tr("РЕНДЕР"), QStringLiteral("#00e5ff"),
        {renderer_status_button, gpu_accuracy_button, cpu_accuracy_button, vsync_mode_button, dma_accuracy_button, gpu_fence_button, nvdec_status_button}));

    // --- Group 4: ГРАФИКА (Yellow accent) ---
    statusBar()->insertPermanentWidget(3, createGroup(
        tr("ГРАФИКА"), QStringLiteral("#ffca28"),
        {aa_status_button, filter_status_button, aspect_ratio_button, res_scale_button, vram_mode_button, anisotropy_button, disk_cache_button}));

    // --- Group 5: ASTC (Pink accent) ---
    statusBar()->insertPermanentWidget(4, createGroup(
        tr("ASTC"), QStringLiteral("#ff4081"),
        {astc_decode_button, astc_recompress_button}));

    // --- Group 6: РЕЖИМ (Purple accent) ---
    statusBar()->insertPermanentWidget(5, createGroup(
        tr("РЕЖИМ"), QStringLiteral("#b388ff"),
        {dock_status_button, airplane_mode_button, speed_limit_button, volume_button, mute_button}));

    // --- Group 7: СИСТЕМА (Orange accent) ---
    auto* sys_group = createGroup(
        tr("СИСТЕМА"), QStringLiteral("#ff9100"),
        {firmware_label});
    sys_group->setMinimumWidth(90);
    firmware_label->setMinimumWidth(80);
    statusBar()->insertPermanentWidget(6, sys_group);

    // --- Group 8: СЕТЬ (Blue/Cyan accent) ---
    auto* net_group = createGroup(
        tr("СЕТЬ"), QStringLiteral("#00b0ff"),
        {multiplayer_state->GetStatusIcon(), multiplayer_state->GetStatusText()});
    net_group->setMinimumWidth(90);
    statusBar()->insertPermanentWidget(7, net_group);

    statusBar()->insertPermanentWidget(8, footer_customize_button);
    statusBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(statusBar(), &QStatusBar::customContextMenuRequested, this, &MainWindow::ShowFooterCustomizeMenu);

    LoadFooterSettings();

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

static QIcon CreateVectorMenuIcon(const QString& icon_type, const QColor& accent_color) {
    QPixmap pixmap(18, 18);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    QPen pen(accent_color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    if (icon_type == QStringLiteral("nand") || icon_type == QStringLiteral("save")) {
        QPainterPath path;
        path.moveTo(3, 2);
        path.lineTo(12, 2);
        path.lineTo(15, 5);
        path.lineTo(15, 16);
        path.lineTo(3, 16);
        path.closeSubpath();
        painter.drawPath(path);
        painter.drawRect(5, 2, 6, 4);
        painter.fillRect(5, 9, 8, 5, accent_color);
    } else if (icon_type == QStringLiteral("folder") || icon_type == QStringLiteral("folder_open")) {
        QPainterPath path;
        path.moveTo(2, 4);
        path.lineTo(6, 4);
        path.lineTo(8, 6);
        path.lineTo(16, 6);
        path.lineTo(16, 15);
        path.lineTo(2, 15);
        path.closeSubpath();
        painter.setBrush(accent_color.lighter(110));
        painter.drawPath(path);
        painter.setBrush(Qt::NoBrush);
        if (icon_type == QStringLiteral("folder_open")) {
            painter.setPen(QPen(Qt::white, 1.4));
            painter.drawLine(5, 11, 13, 11);
            painter.drawLine(9, 8, 9, 14);
        }
    } else if (icon_type == QStringLiteral("clock")) {
        painter.drawEllipse(2, 2, 14, 14);
        painter.drawLine(9, 9, 9, 5);
        painter.drawLine(9, 9, 13, 9);
    } else if (icon_type == QStringLiteral("tag") || icon_type == QStringLiteral("amiibo")) {
        QPainterPath path;
        path.moveTo(3, 8);
        path.lineTo(8, 3);
        path.lineTo(15, 3);
        path.lineTo(15, 15);
        path.lineTo(3, 15);
        path.closeSubpath();
        painter.drawPath(path);
        painter.drawEllipse(10, 5, 2, 2);
    } else if (icon_type == QStringLiteral("globe") || icon_type == QStringLiteral("web")) {
        painter.drawEllipse(2, 2, 14, 14);
        painter.drawLine(2, 9, 16, 9);
        painter.drawEllipse(5, 2, 8, 14);
    } else if (icon_type == QStringLiteral("door") || icon_type == QStringLiteral("exit")) {
        painter.drawRect(2, 2, 8, 14);
        painter.drawLine(10, 9, 16, 9);
        painter.drawLine(13, 6, 16, 9);
        painter.drawLine(13, 12, 16, 9);
    } else if (icon_type == QStringLiteral("pause")) {
        painter.fillRect(4, 3, 3, 12, accent_color);
        painter.fillRect(11, 3, 3, 12, accent_color);
    } else if (icon_type == QStringLiteral("stop")) {
        painter.fillRect(3, 3, 12, 12, accent_color);
    } else if (icon_type == QStringLiteral("play")) {
        QPolygon polygon;
        polygon << QPoint(5, 3) << QPoint(14, 9) << QPoint(5, 15);
        painter.setBrush(accent_color);
        painter.drawPolygon(polygon);
    } else if (icon_type == QStringLiteral("refresh") || icon_type == QStringLiteral("restart")) {
        painter.drawArc(2, 2, 14, 14, 45 * 16, 250 * 16);
        painter.drawLine(14, 5, 16, 9);
        painter.drawLine(11, 8, 16, 9);
    } else if (icon_type == QStringLiteral("gear") || icon_type == QStringLiteral("config")) {
        painter.drawEllipse(6, 6, 6, 6);
        for (int i = 0; i < 8; ++i) {
            painter.save();
            painter.translate(9, 9);
            painter.rotate(i * 45);
            painter.fillRect(-1, -7, 2, 3, accent_color);
            painter.restore();
        }
    } else if (icon_type == QStringLiteral("gamepad")) {
        painter.drawRoundedRect(2, 4, 14, 10, 3, 3);
        painter.drawLine(5, 9, 8, 9);
        painter.drawLine(6.5, 7.5, 6.5, 10.5);
        painter.drawPoint(12, 7.5);
        painter.drawPoint(13.5, 9);
        painter.drawPoint(12, 10.5);
        painter.drawPoint(10.5, 9);
    } else if (icon_type == QStringLiteral("fullscreen")) {
        painter.drawLine(2, 6, 2, 2);
        painter.drawLine(2, 2, 6, 2);
        painter.drawLine(16, 6, 16, 2);
        painter.drawLine(16, 2, 12, 2);
        painter.drawLine(2, 12, 2, 16);
        painter.drawLine(2, 16, 6, 16);
        painter.drawLine(16, 12, 16, 16);
        painter.drawLine(16, 16, 12, 16);
    } else if (icon_type == QStringLiteral("window")) {
        painter.drawRect(2, 3, 14, 12);
        painter.drawLine(2, 6, 16, 6);
    } else if (icon_type == QStringLiteral("display") || icon_type == QStringLiteral("screen")) {
        painter.drawRect(2, 3, 14, 10);
        painter.drawLine(9, 13, 9, 16);
        painter.drawLine(6, 16, 12, 16);
    } else if (icon_type == QStringLiteral("grid")) {
        painter.drawRect(2, 2, 5, 5);
        painter.drawRect(11, 2, 5, 5);
        painter.drawRect(2, 11, 5, 5);
        painter.drawRect(11, 11, 5, 5);
    } else if (icon_type == QStringLiteral("list") || icon_type == QStringLiteral("tree")) {
        painter.drawLine(6, 4, 16, 4);
        painter.drawLine(6, 9, 16, 9);
        painter.drawLine(6, 14, 16, 14);
        painter.drawEllipse(2, 3, 2, 2);
        painter.drawEllipse(2, 8, 2, 2);
        painter.drawEllipse(2, 13, 2, 2);
    } else if (icon_type == QStringLiteral("carousel")) {
        painter.drawRect(5, 2, 8, 14);
        painter.drawLine(2, 4, 2, 14);
        painter.drawLine(16, 4, 16, 14);
    } else if (icon_type == QStringLiteral("search")) {
        painter.drawEllipse(3, 3, 8, 8);
        painter.drawLine(10, 10, 15, 15);
    } else if (icon_type == QStringLiteral("chart") || icon_type == QStringLiteral("stats")) {
        painter.fillRect(3, 10, 3, 5, accent_color);
        painter.fillRect(7, 6, 3, 9, accent_color);
        painter.fillRect(11, 3, 3, 12, accent_color);
    } else if (icon_type == QStringLiteral("key")) {
        painter.drawEllipse(3, 6, 5, 5);
        painter.drawLine(8, 8.5, 15, 8.5);
        painter.drawLine(12, 8.5, 12, 11.5);
        painter.drawLine(15, 8.5, 15, 11);
    } else if (icon_type == QStringLiteral("check") || icon_type == QStringLiteral("verify")) {
        QPainterPath path;
        path.moveTo(9, 2);
        path.lineTo(15, 4);
        path.lineTo(15, 10);
        path.quadTo(15, 15, 9, 16.5);
        path.quadTo(3, 15, 3, 10);
        path.lineTo(3, 4);
        path.closeSubpath();
        painter.drawPath(path);
        painter.drawLine(6, 9, 8, 12);
        painter.drawLine(8, 12, 13, 6);
    } else if (icon_type == QStringLiteral("database") || icon_type == QStringLiteral("data")) {
        painter.drawEllipse(3, 3, 12, 4);
        painter.drawArc(3, 6, 12, 4, 180 * 16, 180 * 16);
        painter.drawArc(3, 10, 12, 4, 180 * 16, 180 * 16);
        painter.drawLine(3, 5, 3, 12);
        painter.drawLine(15, 5, 15, 12);
    } else if (icon_type == QStringLiteral("edit") || icon_type == QStringLiteral("pencil")) {
        painter.drawLine(3, 15, 5, 15);
        painter.drawLine(3, 15, 3, 13);
        painter.drawLine(4, 12, 12, 4);
        painter.drawLine(6, 14, 14, 6);
        painter.drawLine(12, 4, 14, 6);
    } else if (icon_type == QStringLiteral("trash")) {
        painter.drawLine(2, 4, 16, 4);
        painter.drawLine(6, 2, 12, 2);
        painter.drawRect(4, 5, 10, 11);
        painter.drawLine(7, 8, 7, 13);
        painter.drawLine(11, 8, 11, 13);
    } else if (icon_type == QStringLiteral("applet")) {
        painter.drawRoundedRect(2, 2, 5, 5, 1, 1);
        painter.drawRoundedRect(11, 2, 5, 5, 1, 1);
        painter.drawRoundedRect(2, 11, 5, 5, 1, 1);
        painter.drawRoundedRect(11, 11, 5, 5, 1, 1);
    } else if (icon_type == QStringLiteral("photo")) {
        painter.drawRect(2, 2, 14, 14);
        painter.drawEllipse(5, 5, 2, 2);
        painter.drawLine(3, 13, 7, 9);
        painter.drawLine(7, 9, 11, 12);
        painter.drawLine(11, 12, 15, 8);
    } else if (icon_type == QStringLiteral("user") || icon_type == QStringLiteral("mii")) {
        painter.drawEllipse(6, 3, 6, 6);
        painter.drawArc(3, 10, 12, 8, 0, 180 * 16);
    } else if (icon_type == QStringLiteral("home")) {
        QPainterPath path;
        path.moveTo(9, 3);
        path.lineTo(15, 8);
        path.lineTo(13, 8);
        path.lineTo(13, 15);
        path.lineTo(5, 15);
        path.lineTo(5, 8);
        path.lineTo(3, 8);
        path.closeSubpath();
        painter.drawPath(path);
        painter.drawRect(7, 10, 4, 5);
    } else if (icon_type == QStringLiteral("link") || icon_type == QStringLiteral("shortcut")) {
        painter.drawRoundedRect(3, 6, 7, 6, 3, 3);
        painter.drawRoundedRect(8, 6, 7, 6, 3, 3);
    } else if (icon_type == QStringLiteral("camera") || icon_type == QStringLiteral("screenshot")) {
        painter.drawRoundedRect(2, 5, 14, 11, 2, 2);
        painter.drawRect(6, 3, 6, 2);
        painter.drawEllipse(6, 7, 6, 6);
    } else if (icon_type == QStringLiteral("translate")) {
        painter.drawRoundedRect(2, 2, 9, 9, 2, 2);
        painter.drawRoundedRect(7, 7, 9, 9, 2, 2);
        QFont f = painter.font();
        f.setPixelSize(7);
        f.setBold(true);
        painter.setFont(f);
        painter.drawText(QRect(2, 2, 9, 9), Qt::AlignCenter, QStringLiteral("A"));
        painter.drawText(QRect(7, 7, 9, 9), Qt::AlignCenter, QStringLiteral("文"));
    } else if (icon_type == QStringLiteral("lightning") || icon_type == QStringLiteral("tas")) {
        QPolygon poly;
        poly << QPoint(10, 2) << QPoint(4, 9) << QPoint(9, 9) << QPoint(8, 16) << QPoint(14, 8) << QPoint(9, 8);
        painter.setBrush(accent_color);
        painter.drawPolygon(poly);
    } else if (icon_type == QStringLiteral("record")) {
        painter.drawEllipse(2, 2, 14, 14);
        painter.setBrush(QColor(239, 68, 68));
        painter.drawEllipse(5, 5, 8, 8);
    } else if (icon_type == QStringLiteral("plus") || icon_type == QStringLiteral("add")) {
        painter.drawLine(9, 3, 9, 15);
        painter.drawLine(3, 9, 15, 9);
    } else if (icon_type == QStringLiteral("users") || icon_type == QStringLiteral("lobby")) {
        painter.drawEllipse(6, 3, 5, 5);
        painter.drawArc(3, 9, 10, 7, 0, 180 * 16);
        painter.drawEllipse(12, 5, 4, 4);
        painter.drawArc(10, 10, 7, 6, 0, 180 * 16);
    } else if (icon_type == QStringLiteral("book") || icon_type == QStringLiteral("guide")) {
        painter.drawLine(9, 3, 9, 15);
        painter.drawArc(3, 3, 12, 4, 180 * 16, 180 * 16);
        painter.drawLine(3, 5, 3, 15);
        painter.drawLine(15, 5, 15, 15);
        painter.drawArc(3, 13, 12, 4, 180 * 16, 180 * 16);
    } else if (icon_type == QStringLiteral("faq") || icon_type == QStringLiteral("question")) {
        painter.drawEllipse(2, 2, 14, 14);
        QFont f = painter.font();
        f.setPixelSize(11);
        f.setBold(true);
        painter.setFont(f);
        painter.drawText(QRect(0, 0, 18, 18), Qt::AlignCenter, QStringLiteral("?"));
    } else if (icon_type == QStringLiteral("info") || icon_type == QStringLiteral("about")) {
        painter.drawEllipse(2, 2, 14, 14);
        QFont f = painter.font();
        f.setPixelSize(11);
        f.setBold(true);
        painter.setFont(f);
        painter.drawText(QRect(0, 0, 18, 18), Qt::AlignCenter, QStringLiteral("i"));
    } else {
        painter.drawRect(5, 5, 8, 8);
    }

    return QIcon(pixmap);
}

void MainWindow::SetupMenuIcons() {
    QFont menu_font = ui->menubar->font();
    menu_font.setBold(true);
    ui->menubar->setFont(menu_font);

    auto clean_action_text = [](QAction* act) {
        if (!act) return;
        QString text = act->text();
        static const QRegularExpression emoji_re(QStringLiteral(R"(^[\p{Emoji}\p{Symbol}\s]+)"), QRegularExpression::UseUnicodePropertiesOption);
        text.remove(emoji_re);
        act->setText(text.trimmed());
    };

    auto apply_action = [&](QAction* act, const QString& icon_type, const QColor& color) {
        if (!act) return;
        clean_action_text(act);
        act->setIcon(CreateVectorMenuIcon(icon_type, color));
    };

    auto apply_menu = [&](QMenu* menu, const QString& icon_type, const QColor& color) {
        if (!menu) return;
        clean_action_text(menu->menuAction());
        menu->menuAction()->setIcon(CreateVectorMenuIcon(icon_type, color));
    };

    // Color definitions (vibrant standalone palette matching Screenshot 1)
    const QColor col_yellow(255, 193, 7);
    const QColor col_amber(255, 167, 38);
    const QColor col_blue(66, 165, 245);
    const QColor col_cyan(0, 229, 255);
    const QColor col_teal(38, 198, 218);
    const QColor col_green(102, 187, 106);
    const QColor col_lime(156, 204, 101);
    const QColor col_red(239, 83, 80);
    const QColor col_pink(236, 64, 122);
    const QColor col_purple(171, 71, 188);
    const QColor col_indigo(92, 107, 192);
    const QColor col_grey(176, 190, 197);

    // File Menu
    apply_action(ui->action_Install_File_NAND, QStringLiteral("nand"), col_blue);
    apply_action(ui->action_Load_File, QStringLiteral("folder_open"), col_yellow);
    apply_action(ui->action_Load_Folder, QStringLiteral("folder"), col_yellow);
    apply_menu(ui->menu_recent_files, QStringLiteral("clock"), col_teal);
    apply_action(ui->action_Load_Amiibo, QStringLiteral("amiibo"), col_amber);
    apply_action(ui->action_Amiibo_Online_Database, QStringLiteral("globe"), col_cyan);
    apply_menu(ui->menuOpen_Eden_Folders, QStringLiteral("folder"), col_yellow);
    apply_action(ui->action_Root_Data_Folder, QStringLiteral("folder"), col_yellow);
    apply_action(ui->action_NAND_Folder, QStringLiteral("nand"), col_blue);
    apply_action(ui->action_SDMC_Folder, QStringLiteral("database"), col_amber);
    apply_action(ui->action_Mod_Folder, QStringLiteral("gear"), col_purple);
    apply_action(ui->action_Log_Folder, QStringLiteral("list"), col_grey);
    apply_action(ui->action_Exit, QStringLiteral("exit"), col_red);

    // Emulation Menu
    apply_action(ui->action_Pause, QStringLiteral("pause"), col_amber);
    apply_action(ui->action_Stop, QStringLiteral("stop"), col_red);
    apply_action(ui->action_Restart, QStringLiteral("restart"), col_cyan);
    apply_action(ui->action_Configure, QStringLiteral("gear"), col_amber);
    apply_action(ui->action_Configure_Current_Game, QStringLiteral("gamepad"), col_green);

    // View Menu
    apply_action(ui->action_Fullscreen, QStringLiteral("fullscreen"), col_indigo);
    apply_action(ui->action_Single_Window_Mode, QStringLiteral("window"), col_blue);
    apply_menu(ui->menu_Reset_Window_Size, QStringLiteral("display"), col_blue);
    apply_action(ui->action_Reset_Window_Size_720, QStringLiteral("display"), col_blue);
    apply_action(ui->action_Reset_Window_Size_900, QStringLiteral("display"), col_indigo);
    apply_action(ui->action_Reset_Window_Size_1080, QStringLiteral("display"), col_purple);
    apply_menu(ui->menu_View_Debugging, QStringLiteral("gear"), col_amber);
    apply_menu(ui->menu_Game_List_Mode, QStringLiteral("grid"), col_indigo);
    apply_action(ui->action_Tree_View, QStringLiteral("tree"), col_blue);
    apply_action(ui->action_Grid_View, QStringLiteral("grid"), col_indigo);
    apply_action(ui->action_Carousel_View, QStringLiteral("carousel"), col_pink);
    apply_menu(ui->menuGame_Icon_Size, QStringLiteral("stats"), col_cyan);
    apply_action(ui->action_Show_Filter_Bar, QStringLiteral("search"), col_cyan);
    apply_action(ui->action_Show_Status_Bar, QStringLiteral("stats"), col_lime);
    apply_action(ui->action_Show_Performance_Overlay, QStringLiteral("chart"), col_amber);

    // Tools Menu
    apply_action(ui->action_Install_Keys, QStringLiteral("key"), col_amber);
    apply_menu(ui->menuInstall_Firmware, QStringLiteral("nand"), col_purple);
    apply_action(ui->action_Firmware_From_Folder, QStringLiteral("folder"), col_yellow);
    apply_action(ui->action_Firmware_From_ZIP, QStringLiteral("save"), col_blue);
    apply_action(ui->action_Verify_installed_contents, QStringLiteral("verify"), col_green);
    apply_action(ui->action_Data_Manager, QStringLiteral("data"), col_teal);
    apply_menu(ui->menu_cabinet_applet, QStringLiteral("edit"), col_cyan);
    apply_action(ui->action_Launch_Cabinet_Nickname_Owner, QStringLiteral("edit"), col_cyan);
    apply_action(ui->action_Launch_Cabinet_Eraser, QStringLiteral("trash"), col_red);
    apply_action(ui->action_Launch_Cabinet_Restorer, QStringLiteral("restart"), col_green);
    apply_action(ui->action_Launch_Cabinet_Formatter, QStringLiteral("lightning"), col_amber);
    apply_menu(ui->menu_Applets, QStringLiteral("applet"), col_purple);
    apply_action(ui->action_Launch_PhotoViewer, QStringLiteral("photo"), col_purple);
    apply_action(ui->action_Launch_MiiEdit, QStringLiteral("mii"), col_indigo);
    apply_action(ui->action_Launch_Controller, QStringLiteral("gamepad"), col_green);
    apply_action(ui->action_Launch_QLaunch, QStringLiteral("home"), col_teal);
    apply_action(ui->action_Enable_Overlay_Applet, QStringLiteral("window"), col_cyan);
    apply_menu(ui->menu_Create_Shortcuts, QStringLiteral("shortcut"), col_blue);
    apply_action(ui->action_Desktop, QStringLiteral("display"), col_blue);
    apply_action(ui->action_Application_Menu, QStringLiteral("list"), col_indigo);
    apply_action(ui->action_Capture_Screenshot, QStringLiteral("screenshot"), col_pink);
    apply_action(ui->action_Translate_Screen, QStringLiteral("translate"), col_cyan);
    apply_action(ui->action_Cheats, QStringLiteral("lightning"), col_cyan);
    apply_menu(ui->menuTAS, QStringLiteral("tas"), col_lime);
    apply_action(ui->action_TAS_Start, QStringLiteral("play"), col_green);
    apply_action(ui->action_TAS_Record, QStringLiteral("record"), col_red);
    apply_action(ui->action_TAS_Reset, QStringLiteral("restart"), col_amber);
    apply_action(ui->action_Configure_Tas, QStringLiteral("gear"), col_grey);

    // Multiplayer Menu
    apply_action(ui->action_View_Lobby, QStringLiteral("globe"), col_cyan);
    apply_action(ui->action_Start_Room, QStringLiteral("plus"), col_green);
    apply_action(ui->action_Connect_To_Room, QStringLiteral("link"), col_blue);
    apply_action(ui->action_Show_Room, QStringLiteral("users"), col_indigo);
    apply_action(ui->action_Leave_Room, QStringLiteral("exit"), col_red);

    // Help Menu
    apply_action(ui->action_Check_Updates, QStringLiteral("refresh"), col_green);
    apply_action(ui->action_Open_Quickstart_Guide, QStringLiteral("guide"), col_blue);
    apply_action(ui->action_Open_FAQ, QStringLiteral("faq"), col_amber);
    apply_action(ui->action_Open_Mods_Page, QStringLiteral("gear"), col_purple);
    apply_action(ui->action_Eden_Dependencies, QStringLiteral("book"), col_teal);
    apply_action(ui->action_About, QStringLiteral("about"), col_cyan);
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
    LinkActionShortcut(ui->action_Translate_Screen, QStringLiteral("Translate Screen"));
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
        if (QtCommon::emu_thread->IsRunning() &&
            (state & (Qt::ApplicationHidden | Qt::ApplicationInactive))) {
            auto_paused = true;
            OnPauseGame();
        } else if (!QtCommon::emu_thread->IsRunning() && auto_paused &&
                   (state & Qt::ApplicationActive)) {
            auto_paused = false;
            OnStartGame();
        }
    }
    if (UISettings::values.mute_when_in_background) {
        if (!Settings::values.audio_muted.GetValue() &&
            (state & (Qt::ApplicationHidden | Qt::ApplicationInactive))) {
            Settings::values.audio_muted.SetValue(true);
            auto_muted = true;
        } else if (auto_muted && (state & Qt::ApplicationActive)) {
            Settings::values.audio_muted.SetValue(false);
            auto_muted = false;
        }
        UpdateVolumeUI();
        UpdateMuteButton();
    }
}

void MainWindow::ConnectWidgetEvents() {
    connect(game_list, &GameList::BootGame, this, &MainWindow::BootGameFromList);
    connect(game_list, &GameList::GameSelected, this, [this](u64 program_id, const QString& game_path) {
        if (program_id != 0) {
            m_current_addons_title_id = program_id;
            m_current_addons_game_path = game_path.toStdString();
            m_current_addons_game_name = QFileInfo(game_path).completeBaseName();
            UpdateAddonsStatusButton(m_current_addons_title_id, m_current_addons_game_name);
        }
    });
    connect(game_list, &GameList::GameChosen, this, [this](const QString& game_path, u64 program_id) {
        m_current_addons_game_path = game_path.toStdString();
        UpdateAddonsStatusButton(program_id, QFileInfo(game_path).completeBaseName());
        OnGameListLoadFile(game_path, program_id);
    });
    connect(game_list, &GameList::OpenDirectory, this, &MainWindow::OnGameListOpenDirectory);
    connect(game_list, &GameList::OpenFolderRequested, this, &MainWindow::OnGameListOpenFolder);
    connect(game_list, &GameList::OpenModManagerRequested, this,
            [this](u64 program_id, const QString& game_path) {
                const QString game_name = QFileInfo(game_path).completeBaseName();
                ModManagerDialog dialog(this, *QtCommon::system, program_id, game_path, game_name);
                dialog.exec();
            });
    connect(game_list, &GameList::OpenCheatsRequested, this,
            [this](u64 program_id, const QString& game_path) {
                CheatsDialog dialog(this, *QtCommon::system, program_id, game_path);
                dialog.exec();
            });
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

    // Software Keyboard Applet
    connect(this, &MainWindow::EmulationStarting, this, &MainWindow::SoftwareKeyboardExit);
    connect(this, &MainWindow::EmulationStopping, this, &MainWindow::SoftwareKeyboardExit);

    connect(&status_bar_update_timer, &QTimer::timeout, this, &MainWindow::UpdateStatusBar);

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
    connect_menu(ui->action_Open_Mods_Page, &MainWindow::OnOpenModsPage);
    connect_menu(ui->action_Open_Quickstart_Guide, &MainWindow::OnOpenQuickstartGuide);
    connect_menu(ui->action_Open_FAQ, &MainWindow::OnOpenFAQ);
    connect_menu(ui->action_Restart, &MainWindow::OnRestartGame);
    connect_menu(ui->action_Configure, &MainWindow::OnConfigure);
    connect_menu(ui->action_Configure_Current_Game, &MainWindow::OnConfigurePerGame);
    connect_menu(ui->action_Mod_Manager, &MainWindow::OnModManagerDialog);
    connect_menu(ui->action_Cheats, &MainWindow::OnCheatsDialog);

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
    connect_menu(ui->action_Carousel_View, &MainWindow::SetCarouselView);

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

                game_list->UpdateIconSizes();
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
    connect_menu(ui->action_Amiibo_Online_Database, &MainWindow::OnAmiiboOnlineDatabase);

    connect_menu(ui->action_Desktop, &MainWindow::OnCreateHomeMenuDesktopShortcut);
    connect_menu(ui->action_Application_Menu, &MainWindow::OnCreateHomeMenuApplicationMenuShortcut);
    connect_menu(ui->action_Capture_Screenshot, &MainWindow::OnCaptureScreenshot);
    connect_menu(ui->action_Translate_Screen, &MainWindow::OnTranslateScreen);

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
    connect_menu(ui->action_Check_Updates, [this] { OnCheckUpdates(true); });
    connect_menu(ui->action_About, &MainWindow::OnAbout);
    connect_menu(ui->action_Eden_Dependencies, &MainWindow::OnEdenDependencies);
    connect_menu(ui->action_Data_Manager, &MainWindow::OnDataDialog);
}

void MainWindow::UpdateMenuState() {
    const bool is_paused = QtCommon::emu_thread == nullptr || !QtCommon::emu_thread->IsRunning();
    const bool is_firmware_available = CheckFirmwarePresence();

    const std::array running_actions{
        ui->action_Stop,
        ui->action_Restart,
        ui->action_Configure_Current_Game,
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
    if (QtCommon::emu_thread == nullptr)
        return;

    if (prepare_sleep) {
        if (QtCommon::emu_thread->IsRunning()) {
            auto_paused = true;
            OnPauseGame();
        }
    } else {
        if (!QtCommon::emu_thread->IsRunning() && auto_paused) {
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
    if (QtCommon::emu_thread != nullptr)
        ShutdownGame();

    if (!render_window->InitRenderTarget())
        return false;

    QtCommon::system->SetFilesystem(QtCommon::vfs);

    if (params.launch_type == Service::AM::LaunchType::FrontendInitiated)
        QtCommon::system->GetUserChannel().clear();

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
    const Core::SystemResultStatus result{
        QtCommon::system->Load(*render_window, filename.toStdString(), params)};

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
               "support.<br>For an explanation of the various Switch formats STORM EDEN supports, "
               "out our user handbook. This message will not be shown again."));
    }

    if (result != Core::SystemResultStatus::Success) {
        switch (result) {
        case Core::SystemResultStatus::ErrorGetLoader:
            LOG_CRITICAL(Frontend, "Failed to obtain loader for {}!", filename.toStdString());
            QMessageBox::critical(this, tr("Ошибка при загрузке игры!"),
                                  tr("Формат файла игры не поддерживается."));
            break;
        case Core::SystemResultStatus::ErrorVideoCore:
            QMessageBox::critical(
                this, tr("Ошибка инициализации видеоядра."),
                tr("STORM EDEN столкнулся с ошибкой при запуске видеоядра GPU. "
                   "Обычно это вызвано устаревшими драйверами видеокарты. "
                   "Пожалуйста, обновите графические драйверы."));
            break;
        default:
            if (result > Core::SystemResultStatus::ErrorLoader) {
                const u16 loader_id = static_cast<u16>(Core::SystemResultStatus::ErrorLoader);
                const u16 error_id = static_cast<u16>(result) - loader_id;
                const std::string error_code = fmt::format("({:04X}-{:04X})", loader_id, error_id);
                LOG_CRITICAL(Frontend, "Failed to load ROM! {}", error_code);

                const auto title =
                    tr("Ошибка при загрузке игры! %1", "%1 signifies a numeric error code.")
                        .arg(QString::fromStdString(error_code));
                const auto description =
                    tr("%1<br><br>Пожалуйста, проверьте наличие свежих ключей prod.keys или пересоздайте дамп файла игры.",
                       "%1 signifies an error string.")
                        .arg(QString::fromStdString(
                            GetResultStatusString(static_cast<Loader::ResultStatus>(error_id))));

                QMessageBox::critical(this, title, description);
            } else {
                QMessageBox::critical(
                    this, tr("Ошибка при загрузке игры!"),
                    tr("Произошла неизвестная ошибка при запуске. Подробности смотрите в логе."));
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

void MainWindow::BootGame(const QString& filename, Service::AM::FrontendAppletParameters params,
                          StartGameType type) {
    LOG_INFO(Frontend, "STORM EDEN starting...");

    if (params.program_id == 0 ||
        params.program_id > static_cast<u64>(Service::AM::AppletProgramId::MaxProgramId)) {
        StoreRecentFile(filename); // Put the filename on top of the list
    }

    // Save configurations
    UpdateUISettings();
    game_list->SaveInterfaceLayout();
    config->SaveAllValues();

    u64 title_id{0};

    last_filename_booted = filename;

    const auto utf8_str = filename.toUtf8();
    QtCommon::Content::configureFilesystemProvider(filename.toStdString());
    const auto v_file = Core::GetGameFileFromPath(QtCommon::vfs, utf8_str.constData());
    const auto loader =
        Loader::GetLoader(*QtCommon::system, v_file, params.program_id, params.program_index);

    if (loader != nullptr) {
        loader->ReadProgramId(title_id);
    }
    if (title_id == 0) {
        static const QRegularExpression tid_regex(QStringLiteral(R"(\[([0-9a-fA-F]{16})\])"));
        const auto match = tid_regex.match(filename);
        if (match.hasMatch()) {
            bool ok = false;
            const u64 parsed = match.captured(1).toULongLong(&ok, 16);
            if (ok && parsed != 0) {
                title_id = parsed;
            }
        }
    }

    if (title_id != 0 && type == StartGameType::Normal) {
        const auto file_path_hash = Common::CityHash64(utf8_str.constData(), static_cast<std::size_t>(utf8_str.size()));
        const auto specific_config = fmt::format("{:016X}_{:016X}", title_id, file_path_hash);
        const auto legacy_config = fmt::format("{:016X}", title_id);

        std::filesystem::path custom_path = Common::FS::GetEdenPath(Common::FS::EdenPath::ConfigDir) / "custom";
        std::string target_ini = (custom_path / (specific_config + ".ini")).string();

        const auto* profile = Core::GameFixDatabase::GetProfile(title_id);
        if (profile != nullptr) {
            bool already_applied = false;
            std::string check_ini = target_ini;
            if (!std::filesystem::exists(check_ini) && std::filesystem::exists(custom_path / (legacy_config + ".ini"))) {
                check_ini = (custom_path / (legacy_config + ".ini")).string();
            }
            if (std::filesystem::exists(check_ini)) {
                std::ifstream f(check_ini);
                std::string l;
                while (std::getline(f, l)) {
                    if (l.find("storm_fix_applied=true") != std::string::npos || l.find("storm_fix_applied = true") != std::string::npos) {
                        already_applied = true;
                        break;
                    }
                }
            }

            if (!already_applied) {
                QMessageBox msgBox(this);
                msgBox.setWindowTitle(tr("🔧 Оптимизация STORM EDEN: %1").arg(QString::fromStdString(profile->game_name)));
                
                QString issues_formatted = QString::fromStdString(profile->issues_ru);
                issues_formatted.replace(QStringLiteral("\n"), QStringLiteral("<br>"));
                QString fixes_formatted = QString::fromStdString(profile->fixes_ru);
                fixes_formatted.replace(QStringLiteral("\n"), QStringLiteral("<br>"));

                QString htmlText = QString::fromUtf8(
                    "<h3>🎮 %1</h3>"
                    "<p style='color:#ef4444;'><b>⚠️ Обнаружены известные проблемы в игре:</b><br>%2</p>"
                    "<p style='color:#10b981;'><b>⚡ Рекомендуемые настройки STORM EDEN:</b><br>%3</p>"
                    "<p>Применить оптимизированные настройки для этой игры и сохранить их?")
                    .arg(QString::fromStdString(profile->game_name))
                    .arg(issues_formatted)
                    .arg(fixes_formatted);
                
                msgBox.setText(htmlText);
                msgBox.setIcon(QMessageBox::Information);
                QPushButton* applyBtn = msgBox.addButton(tr("⚡ Применить и запустить"), QMessageBox::AcceptRole);
                QPushButton* skipBtn = msgBox.addButton(tr("Запустить без изменений"), QMessageBox::RejectRole);
                msgBox.setDefaultButton(applyBtn);

                msgBox.exec();

                if (msgBox.clickedButton() == applyBtn) {
                    Core::GameFixDatabase::ApplyProfileToPerGameConfig(title_id, target_ini);
                    Core::GameFixDatabase::ApplyProfileToPerGameConfig(title_id, (custom_path / (legacy_config + ".ini")).string());
                }
            }
        }

        // Load per game settings
        std::string config_to_load = specific_config;
        if (!std::filesystem::exists(custom_path / (specific_config + ".ini")) &&
            std::filesystem::exists(custom_path / (legacy_config + ".ini"))) {
            config_to_load = legacy_config;
        }

        QtConfig per_game_config(config_to_load, Config::ConfigType::PerGameConfig);
        QtCommon::system->HIDCore().ReloadInputDevices();
        QtCommon::system->ApplySettings();
    }

    Settings::LogSettings();

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

    // If the user specifies -u (successfully) on the cmd line, don't prompt for a user on first
    // game startup only. If the user stops emulation and starts a new one, go back to the expected
    // behavior of asking.
    user_flag_cmd_line = false;

    if (!LoadROM(filename, params)) {
        return;
    }

    QtCommon::system->SetShuttingDown(false);
    game_list->setDisabled(true);

    // Create and start the emulation thread
    QtCommon::emu_thread = std::make_unique<EmuThread>();
    emit EmulationStarting();
    QtCommon::emu_thread->start();

    // Register an ExecuteProgram callback such that Core can execute a sub-program
    QtCommon::system->RegisterExecuteProgramCallback(
        [this](std::size_t program_index_) { render_window->ExecuteProgram(program_index_); });

    QtCommon::system->RegisterExitCallback([this] {
        QtCommon::emu_thread->ForceStop();
        render_window->Exit();
    });

    connect(render_window, &GRenderWindow::Closed, this, &MainWindow::OnStopGame);
    connect(render_window, &GRenderWindow::MouseActivity, this, &MainWindow::OnMouseActivity);

    connect(QtCommon::emu_thread.get(), &EmuThread::LoadProgress, loading_screen,
            &LoadingScreen::OnLoadProgress, Qt::QueuedConnection);

    // Update the GUI
    UpdateStatusButtons();
    if (ui->action_Single_Window_Mode->isChecked()) {
        game_list->hide();
        game_list_placeholder->hide();
        render_window->show();
        render_window->setFocus();
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
    std::string title_developer;
    QPixmap game_icon_pix;
    if (metadata.first != nullptr) {
        title_version = metadata.first->GetVersionString();
        title_name = metadata.first->GetApplicationName();
        title_developer = metadata.first->GetDeveloperName();
    }
    if (metadata.second != nullptr) {
        const auto bytes = metadata.second->ReadAllBytes();
        game_icon_pix.loadFromData(bytes.data(), static_cast<u32>(bytes.size()));
    } else {
        std::vector<u8> bytes;
        if (loader != nullptr && loader->ReadIcon(bytes) == Loader::ResultStatus::Success) {
            game_icon_pix.loadFromData(bytes.data(), static_cast<u32>(bytes.size()));
        }
    }
    if (res != Loader::ResultStatus::Success || title_name.empty()) {
        title_name = Common::FS::PathToUTF8String(
            std::filesystem::path{Common::U16StringFromBuffer(filename.utf16(), filename.size())}
                .filename());
    }
    const auto full_file_info_name = QFileInfo(filename).fileName();
    QString resolved_display_version = QString::fromStdString(title_version);

    u32 raw_internal_version = pm.GetGameVersion().value_or(0);
    static const QRegularExpression fn_pair_ver_regex{QStringLiteral(R"(\(([0-9]+\.[0-9]+(?:\.[0-9]+)*)\s*-\s*([0-9]+))")};
    const auto fm = fn_pair_ver_regex.match(full_file_info_name);
    if (fm.hasMatch() && !fm.captured(1).isEmpty()) {
        resolved_display_version = fm.captured(1);
        if (raw_internal_version == 0 && !fm.captured(2).isEmpty()) {
            raw_internal_version = fm.captured(2).toUInt();
        }
    } else {
        static const QRegularExpression fn_ver_regex{QStringLiteral(R"((?:[\(\[\s]v?|\b)([0-9]+\.[0-9]+(?:\.[0-9]+)*)(?!\s*(?:GB|MB|KB|TB|ГБ|МБ|КБ|Б|B)\b))")};
        const auto m = fn_ver_regex.match(full_file_info_name);
        if (m.hasMatch() && m.hasCaptured(1)) {
            if (resolved_display_version.isEmpty() || resolved_display_version == QStringLiteral("1.0.0") || resolved_display_version == QStringLiteral("0")) {
                resolved_display_version = m.captured(1);
            }
        }
    }

    while (resolved_display_version.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
        resolved_display_version.remove(0, 1);
    }
    resolved_display_version = resolved_display_version.trimmed();
    if (resolved_display_version.isEmpty() || resolved_display_version == QStringLiteral("0") || resolved_display_version == QStringLiteral("PACKED")) {
        resolved_display_version = QStringLiteral("1.0.0");
    }

    if (raw_internal_version == 0) {
        static const QRegularExpression ver_regex(QStringLiteral(R"([\[\(_]v(\d+)[\]\)]|[-_\s](\d{5,8})[-_\s\)])"), QRegularExpression::CaseInsensitiveOption);
        const auto match = ver_regex.match(full_file_info_name);
        if (match.hasMatch()) {
            for (int i = 1; i <= match.lastCapturedIndex(); ++i) {
                const auto cap = match.captured(i);
                if (!cap.isEmpty()) {
                    raw_internal_version = cap.toUInt();
                    break;
                }
            }
        }
    }

    const std::string display_version_str = resolved_display_version.toStdString();
    if (raw_internal_version == 0 && !display_version_str.empty() && display_version_str != "1.0.0" && display_version_str != "1.0") {
        int major = 1, minor = 0, patch = 0;
        if (std::sscanf(display_version_str.c_str(), "%d.%d.%d", &major, &minor, &patch) >= 2) {
            if (major >= 1) {
                raw_internal_version = static_cast<u32>((major - 1) * 655360 + minor * 65536 + (patch * 65536) / 10);
            }
        }
    }
    title_version = display_version_str;
    const std::string internal_version_str = std::to_string(raw_internal_version);
    std::string raw_gpu_vendor = "GPU";
    try {
        if (QtCommon::system != nullptr) {
            raw_gpu_vendor = QtCommon::system->GPU().Renderer().GetDeviceVendor();
        }
    } catch (...) {
        raw_gpu_vendor = "VULKAN GPU";
    }
    const auto gpu_vendor = QString::fromStdString(raw_gpu_vendor).toUpper().toStdString();

    const auto full_file_name = full_file_info_name.toStdString();
    LOG_INFO(Frontend, "Booting game: {:016X} | {} | {} | {} | {}", title_id, full_file_name, display_version_str, internal_version_str, gpu_vendor);
    UpdateWindowTitle(full_file_name, display_version_str, internal_version_str, gpu_vendor);
    m_current_addons_game_path = filename.toStdString();
    UpdateAddonsStatusButton(title_id, QString::fromStdString(title_name));

    const QString file_ext = QFileInfo(filename).suffix().toUpper();
    loading_screen->SetGameInfo(
        QString::fromStdString(title_name),
        QString::fromStdString(title_version),
        QString::fromStdString(title_developer),
        title_id,
        game_icon_pix,
        file_ext
    );
    loading_screen->Prepare(QtCommon::system->GetAppLoader());
    if (ui->action_Single_Window_Mode->isChecked()) {
        loading_screen->setGeometry(ui->centralwidget->rect());
    }
    loading_screen->show();
    loading_screen->raise();

    emulation_running = true;
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
    QtCommon::emu_thread->disconnect();
    QtCommon::emu_thread->SetRunning(true);

    connect(QtCommon::emu_thread.get(), &QThread::finished, this, &MainWindow::OnEmulationStopped);
    emit EmulationStopping();

    int shutdown_time = 1000;

    if (QtCommon::system->DebuggerEnabled()) {
        shutdown_time = 0;
    } else if (QtCommon::system->GetExitLocked()) {
        shutdown_time = 5000;
    }

    shutdown_timer.stop();
    shutdown_timer.disconnect();
    shutdown_timer.setSingleShot(true);
    connect(&shutdown_timer, &QTimer::timeout, this, &MainWindow::OnEmulationStopTimeExpired);
    shutdown_timer.start(shutdown_time);

    // Disable everything to prevent anything from being triggered here
    ui->action_Pause->setEnabled(false);
    ui->action_Restart->setEnabled(false);
    ui->action_Stop->setEnabled(false);

    if (ui->action_Single_Window_Mode->isChecked()) {
        loading_screen->setGeometry(ui->centralwidget->rect());
        loading_screen->ShowShutdownState();
    }

    return true;
}

void MainWindow::OnShutdownBeginDialog() {
    if (ui->action_Single_Window_Mode->isChecked()) {
        loading_screen->ShowShutdownState();
    } else {
        shutdown_dialog =
            new OverlayDialog(this, *QtCommon::system, QString{}, tr("Closing software..."), QString{},
                              QString{}, Qt::AlignHCenter | Qt::AlignVCenter);
        shutdown_dialog->open();
    }
}

void MainWindow::OnEmulationStopTimeExpired() {
    if (QtCommon::emu_thread) {
        QtCommon::emu_thread->ForceStop();
    }
}

void MainWindow::OnEmulationStopped() {
    shutdown_timer.stop();
    shutdown_timer.disconnect();
    if (QtCommon::emu_thread) {
        QtCommon::emu_thread->disconnect();
        if (QtCommon::emu_thread->isRunning()) {
            QtCommon::emu_thread->ForceStop();
            QtCommon::emu_thread->wait();
        }
        QtCommon::emu_thread.reset();
    }

    if (shutdown_dialog) {
        shutdown_dialog->deleteLater();
        shutdown_dialog = nullptr;
    }

    emulation_running = false;

    if (floating_translate_button) {
        floating_translate_button->SetVisibleState(false);
    }

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
            return tr("Удалить установленный контент игры?");
        case QtCommon::Game::InstalledEntryType::Update:
            return tr("Удалить установленное обновление игры?");
        case QtCommon::Game::InstalledEntryType::AddOnContent:
            return tr("Удалить установленные DLC игры?");
        default:
            return tr("Удалить установленный элемент?");
        }
    }();

    if (!question(this, tr("Удаление"), entry_question, QMessageBox::Yes | QMessageBox::No,
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
            return tr("Удалить переносимый кэш шейдеров OpenGL?");
        case QtCommon::Game::GameListRemoveTarget::VkShaderCache:
            return tr("Удалить переносимый кэш шейдеров Vulkan?");
        case QtCommon::Game::GameListRemoveTarget::AllShaderCache:
            return tr("Удалить все кэши шейдеров?");
        case QtCommon::Game::GameListRemoveTarget::CustomConfiguration:
            return tr("Удалить пользовательскую конфигурацию игры?");
        case QtCommon::Game::GameListRemoveTarget::CacheStorage:
            return tr("Удалить кэш хранилища?");
        default:
            return QString{};
        }
    }();

    if (!MainWindow::question(this, tr("Удаление файла"), question, QMessageBox::Yes | QMessageBox::No,
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

    UISettings::GameDir game_dir{dir_path.toStdString(), true, true};
    const auto it = std::find(UISettings::values.game_dirs.begin(),
                              UISettings::values.game_dirs.end(), game_dir);
    if (it == UISettings::values.game_dirs.end()) {
        UISettings::values.game_dirs.append(game_dir);
    } else {
        it->expanded = true;
        it->deep_scan = true;
    }
    game_list->PopulateAsync(UISettings::values.game_dirs);
    OnSaveConfig();
}

void MainWindow::OnGameListShowList(bool show) {
    if (emulation_running && ui->action_Single_Window_Mode->isChecked())
        return;
    game_list->setVisible(show);
    game_list_placeholder->setVisible(!show);
};

void MainWindow::OnGameListOpenPerGameProperties(const std::string& file) {
    u64 title_id{};
    const auto v_file = Core::GetGameFileFromPath(QtCommon::vfs, file);
    const auto loader = Loader::GetLoader(*QtCommon::system, v_file);

    if (loader == nullptr || loader->ReadProgramId(title_id) != Loader::ResultStatus::Success) {
        QMessageBox::information(this, tr("Properties"),
                                 tr("The game properties could not be loaded."));
        return;
    }

    OpenPerGameConfiguration(title_id, file);
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
            .append(QtCommon::supported_file_extensions.join(QStringLiteral(" *.")))
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
        tr("Installable Switch File (*.nca *.nsp *.nsz *.xci *.xcz);;Nintendo Content Archive "
           "(*.nca);;Nintendo Submission Package (*.nsp *.nsz);;NX Cartridge "
           "Image (*.xci *.xcz)");

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

    QtCommon::emu_thread->SetRunning(true);

    UpdateMenuState();
    OnTasStateChanged();

    play_time_manager->SetProgramId(QtCommon::system->GetApplicationProcessProgramID());
    play_time_manager->Start();

    discord_rpc->Update();
    Common::FeralGamemode::Start();

    const bool enable_floating = UISettings::values.enable_floating_translate_button.GetValue();
    if (enable_floating) {
        if (!floating_translate_button) {
            floating_translate_button = new FloatingTranslateButton(this);
            connect(floating_translate_button, &FloatingTranslateButton::TranslateRequested, this, &MainWindow::OnTranslateScreen);
            connect(floating_translate_button, &FloatingTranslateButton::OpenSettingsRequested, this, &MainWindow::OnOpenTranslatorSettings);
        }
        if (render_window) {
            QPoint p = render_window->mapToGlobal(QPoint(std::max(10, render_window->width() - 80), std::max(10, render_window->height() - 120)));
            floating_translate_button->move(p);
        }
        floating_translate_button->SetVisibleState(true);
    } else if (floating_translate_button) {
        floating_translate_button->SetVisibleState(false);
    }
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
    QtCommon::emu_thread->SetRunning(false);
    play_time_manager->Stop();
    UpdateMenuState();
    AllowOSSleep();
    Common::FeralGamemode::Stop();
}

void MainWindow::OnPauseContinueGame() {
    if (emulation_running) {
        if (QtCommon::emu_thread->IsRunning()) {
            OnPauseGame();
        } else {
            OnStartGame();
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
    //     const auto& caps = g_cpu_caps;
    //     const bool has_fma = caps.fma;
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
    OpenURL(QUrl(QStringLiteral("https://github.com/eden-emulator/yuzu-mod-archive")));
}

void MainWindow::OnOpenQuickstartGuide() {
    OpenURL(QUrl(QStringLiteral("https://yuzu-mirror.github.io/help/quickstart/")));
}

void MainWindow::OnOpenFAQ() {
    OpenURL(QUrl(QStringLiteral("https://yuzu-mirror.github.io/help")));
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
    ui->action_Carousel_View->setChecked(mode == Settings::GameListMode::CarouselView);

    UISettings::values.game_list_mode = mode;
    ui->action_Show_Game_Name->setEnabled(mode != Settings::GameListMode::TreeView);

    CheckIconSize();
    game_list->ResetViewMode();
}

void MainWindow::SetGridView() {
    SetGameListMode(Settings::GameListMode::GridView);
}

void MainWindow::SetTreeView() {
    SetGameListMode(Settings::GameListMode::TreeView);
}

void MainWindow::SetCarouselView() {
    SetGameListMode(Settings::GameListMode::CarouselView);
}

void MainWindow::CheckIconSize() {
    // When in grid/carousel view mode, with text off
    // there is no point in having icons turned off
    auto actions = game_size_actions->actions();
    if (UISettings::values.game_list_mode.GetValue() != Settings::GameListMode::TreeView &&
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

    game_list->UpdateIconSizes();
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
        config->SaveAllValues();
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

    if (floating_translate_button) {
        floating_translate_button->SetVisibleState(UISettings::values.enable_floating_translate_button.GetValue() && emulation_running);
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
    Settings::values.audio_muted.SetValue(!Settings::values.audio_muted.GetValue());
    UpdateVolumeUI();
    UpdateMuteButton();
}

void MainWindow::OnDecreaseVolume() {
    Settings::values.audio_muted.SetValue(false);
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
    UpdateMuteButton();
}

void MainWindow::OnIncreaseVolume() {
    Settings::values.audio_muted.SetValue(false);
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
    UpdateMuteButton();
}

void MainWindow::OnToggleAdaptingFilter() {
    auto filter = Settings::values.scaling_filter.GetValue();
    filter = Settings::ScalingFilter(u32(filter) + 1);
    if (u32(filter) > u32(Settings::EnumMetadata<Settings::ScalingFilter>::GetLast()))
        filter = Settings::EnumMetadata<Settings::ScalingFilter>::GetFirst();
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

void MainWindow::OnModManagerDialog() {
    u64 title_id = 0;
    QString game_path;
    QString game_name;

    if (QtCommon::system->IsPoweredOn()) {
        title_id = QtCommon::system->GetApplicationProcessProgramID();
        game_path = current_game_path;
        game_name = m_current_addons_game_name;
    } else if (m_current_addons_title_id != 0) {
        title_id = m_current_addons_title_id;
        game_path = QString::fromStdString(m_current_addons_game_path);
        game_name = m_current_addons_game_name;
    }

    if (title_id == 0) {
        QMessageBox::information(this, tr("Менеджер модов"),
                                 tr("Выберите игру из списка или запустите эмуляцию для управления модами."));
        return;
    }

    ModManagerDialog dialog(this, *QtCommon::system, title_id, game_path, game_name);
    dialog.exec();
}

void MainWindow::OnCheatsDialog() {
    u64 title_id = 0;
    QString game_path;

    if (QtCommon::system->IsPoweredOn()) {
        title_id = QtCommon::system->GetApplicationProcessProgramID();
        game_path = current_game_path;
    } else if (m_current_addons_title_id != 0) {
        title_id = m_current_addons_title_id;
        game_path = QString::fromStdString(m_current_addons_game_path);
    }

    if (title_id == 0) {
        QMessageBox::information(this, tr("Чит-коды"),
                                 tr("Выберите игру из списка или запустите эмуляцию для настройки чит-кодов."));
        return;
    }

    CheatsDialog dialog(this, *QtCommon::system, title_id, game_path);
    dialog.exec();
}

void MainWindow::StartSilentCheatsSync() {
    // Non-blocking silent background cheats sync for games in library
    const QString cheats_root = QString::fromStdString(Common::FS::PathToUTF8String(
        Common::FS::GetEdenPath(Common::FS::EdenPath::EdenDir) / "cheats"));
    QDir().mkpath(cheats_root);

    const auto& content_provider = QtCommon::system->GetContentProvider();
    const auto entries = content_provider.ListEntriesFilter(FileSys::TitleType::Application,
                                                           FileSys::ContentRecordType::Program);

    for (const auto& entry : entries) {
        const u64 tid = entry.title_id;
        if (tid == 0) continue;

        const QString tid_hex = QString(QStringLiteral("%1")).arg(tid, 16, 16, QLatin1Char('0')).toUpper();
        const QString game_cheats_dir = QDir(cheats_root).filePath(tid_hex);

        bool has_cheats = false;
        QDir dir(game_cheats_dir);
        if (dir.exists()) {
            const auto files = dir.entryInfoList(QStringList() << QStringLiteral("*.txt"), QDir::Files);
            for (const auto& file : files) {
                if (file.size() > 0) {
                    has_cheats = true;
                    break;
                }
            }
        }

        if (!has_cheats) {
            auto* nam = new QNetworkAccessManager(this);
            const QString url_str = QString(QStringLiteral("https://raw.githubusercontent.com/HamletDuFromage/switch-cheats-db/master/cheats/%1.json"))
                                        .arg(tid_hex);
            QNetworkRequest request{QUrl(url_str)};
            request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("STORM_EDEN_Emulator/4.2.9"));
            request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

            auto* reply = nam->get(request);
            connect(reply, &QNetworkReply::finished, this, [reply, nam, game_cheats_dir, tid_hex]() {
                reply->deleteLater();
                nam->deleteLater();

                if (reply->error() == QNetworkReply::NoError &&
                    reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
                    const QByteArray data = reply->readAll();
                    QJsonDocument doc = QJsonDocument::fromJson(data);
                    if (doc.isObject()) {
                        const QJsonObject root = doc.object();
                        QString combined_cheats;
                        for (auto it = root.begin(); it != root.end(); ++it) {
                            const QString bid_key = it.key();
                            const QJsonObject bid_cheats = it.value().toObject();
                            QString bid_file_content;
                            for (auto c_it = bid_cheats.begin(); c_it != bid_cheats.end(); ++c_it) {
                                const QString cheat_body = c_it.value().toString();
                                if (!cheat_body.trimmed().isEmpty()) {
                                    bid_file_content += cheat_body.trimmed() + QStringLiteral("\n\n");
                                    combined_cheats += cheat_body.trimmed() + QStringLiteral("\n\n");
                                }
                            }
                            if (!bid_file_content.isEmpty()) {
                                QDir().mkpath(game_cheats_dir);
                                QFile bid_file(QDir(game_cheats_dir).filePath(QString(QStringLiteral("%1.txt")).arg(bid_key.toUpper())));
                                if (bid_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                                    QTextStream stream(&bid_file);
                                    stream << bid_file_content;
                                    bid_file.close();
                                }
                            }
                        }
                        if (!combined_cheats.isEmpty()) {
                            QDir().mkpath(game_cheats_dir);
                            QFile all_file(QDir(game_cheats_dir).filePath(QStringLiteral("cheats.txt")));
                            if (all_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                                QTextStream stream(&all_file);
                                stream << combined_cheats;
                                all_file.close();
                            }
                        }
                    }
                }
            });
        }
    }
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
    if (QtCommon::emu_thread == nullptr || !QtCommon::emu_thread->IsRunning() ||
        is_amiibo_file_select_active)
        return;

    auto* virtual_amiibo = input_subsystem->GetVirtualAmiibo();

    // If an amiibo is currently attached, ask if user wants to disconnect/remove it or load another
    if (virtual_amiibo->GetCurrentState() == InputCommon::VirtualAmiibo::State::TagNearby) {
        const auto reply = QMessageBox::question(
            this, tr("Amiibo"),
            tr("An Amiibo is currently active.\nDo you want to disconnect/remove it?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (reply == QMessageBox::Yes) {
            virtual_amiibo->CloseAmiibo();
            QMessageBox::information(this, tr("Amiibo"), tr("The current amiibo has been disconnected and removed."));
            return;
        }
    }

    if (virtual_amiibo->GetCurrentState() != InputCommon::VirtualAmiibo::State::WaitingForAmiibo) {
        QMessageBox::warning(this, tr("Error"), tr("The current game is not looking for amiibos"));
        return;
    }

    is_amiibo_file_select_active = true;
    const QString extensions{QStringLiteral("*.bin")};
    const QString file_filter = tr("Amiibo File (%1);; All Files (*.*)").arg(extensions);
    const QString filename = QFileDialog::getOpenFileName(this, tr("Load Amiibo"), {}, file_filter);
    is_amiibo_file_select_active = false;

    if (filename.isEmpty()) {
        return;
    }

    LoadAmiibo(filename);
}

void MainWindow::OnAmiiboOnlineDatabase() {
    AmiiboBrowserDialog dialog(this, *QtCommon::system);
    connect(&dialog, &AmiiboBrowserDialog::AmiiboSelectedForLoading, this, [this](const QString& path) {
        if (QtCommon::emu_thread && QtCommon::emu_thread->IsRunning()) {
            LoadAmiibo(path);
        }
    });
    connect(&dialog, &AmiiboBrowserDialog::AmiiboRemoveRequested, this, [this]() {
        auto* virtual_amiibo = input_subsystem->GetVirtualAmiibo();
        if (virtual_amiibo) {
            virtual_amiibo->CloseAmiibo();
        }
    });
    dialog.exec();
}

void MainWindow::OnTranslateScreen() {
    if (!m_game_translator) {
        m_game_translator = new GameTranslator(*QtCommon::system, this);
    }
    m_game_translator->show();
    m_game_translator->raise();
    m_game_translator->activateWindow();

    if (render_window && QtCommon::system && QtCommon::system->IsPoweredOn()) {
        QImage lastFrame = render_window->GetLastFrame();
        if (!lastFrame.isNull()) {
            m_game_translator->TranslateFrame(lastFrame);
        }
        QPointer<GameTranslator> safeTranslator = m_game_translator;
        render_window->CaptureFrame([safeTranslator](const QImage& frame) {
            if (safeTranslator && !frame.isNull()) {
                safeTranslator->TranslateFrame(frame);
            }
        });
    }
}

void MainWindow::OnOpenTranslatorSettings() {
    if (!m_game_translator) {
        m_game_translator = new GameTranslator(*QtCommon::system, this);
    }
    m_game_translator->show();
    m_game_translator->raise();
    m_game_translator->activateWindow();
    if (floating_translate_button) {
        connect(m_game_translator, &QDialog::finished, this, [this](int) {
            if (floating_translate_button) {
                const bool enable_floating = UISettings::values.enable_floating_translate_button.GetValue();
                floating_translate_button->SetVisibleState(enable_floating && emulation_running);
            }
        }, Qt::UniqueConnection);
    }
    m_game_translator->show();
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
    
    // Automatically close existing amiibo before injecting new one
    if (virtual_amiibo->GetCurrentState() == InputCommon::VirtualAmiibo::State::TagNearby) {
        virtual_amiibo->CloseAmiibo();
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

void MainWindow::OnInstallFirmware() {
    QtCommon::Content::InstallFirmware();
    OnCheckFirmwareDecryption();
}

void MainWindow::OnInstallFirmwareFromZIP() {
    QtCommon::Content::InstallFirmwareZip();
    OnCheckFirmwareDecryption();
}

// TODO(crueter): QtCommon this: game list populate can be a signal?
void MainWindow::OnInstallDecryptionKeys() {
    // Don't do this while emulation is running.
    if (QtCommon::emu_thread != nullptr && QtCommon::emu_thread->IsRunning())
        return;

    QtCommon::Content::InstallKeys();

    game_list->PopulateAsync(UISettings::values.game_dirs);
    OnCheckFirmwareDecryption();
}

void MainWindow::OnCheckUpdates(bool manual_check) {
    auto* nam = new QNetworkAccessManager(this);
    QNetworkRequest request(QUrl(QStringLiteral("https://api.github.com/repos/ReiKatari/STORM_EDEN/releases/latest")));
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("STORM_EDEN-Updater"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    auto* reply = nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, nam, manual_check]() {
        reply->deleteLater();
        nam->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            if (manual_check) {
                QMessageBox::warning(this, tr("Проверка обновлений"),
                                     tr("Не удалось проверить наличие обновлений.\nОшибка: %1").arg(reply->errorString()));
            }
            return;
        }

        const auto doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) {
            if (manual_check) {
                QMessageBox::warning(this, tr("Проверка обновлений"), tr("Получен некорректный ответ от сервера обновлений."));
            }
            return;
        }

        const auto obj = doc.object();
        const QString tag_name = obj[QStringLiteral("tag_name")].toString().trimmed();
        const QString release_name = obj[QStringLiteral("name")].toString().trimmed();
        const QString release_body = obj[QStringLiteral("body")].toString();
        const QString html_url = obj[QStringLiteral("html_url")].toString().trimmed();

        QString clean_tag = tag_name;
        if (clean_tag.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
            clean_tag = clean_tag.mid(1);
        }

        QString current_ver = QString::fromStdString(Common::g_build_version).trimmed();
        if (current_ver.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
            current_ver = current_ver.mid(1);
        }

        auto parse_version = [](const QString& ver_str) -> std::vector<int> {
            std::vector<int> parts;
            const auto list = ver_str.split(QLatin1Char('.'), Qt::SkipEmptyParts);
            for (const auto& s : list) {
                parts.push_back(s.toInt());
            }
            return parts;
        };

        const auto remote_parts = parse_version(clean_tag);
        const auto local_parts = parse_version(current_ver);

        bool is_newer = false;
        for (size_t i = 0; i < std::max(remote_parts.size(), local_parts.size()); ++i) {
            const int r = i < remote_parts.size() ? remote_parts[i] : 0;
            const int l = i < local_parts.size() ? local_parts[i] : 0;
            if (r > l) {
                is_newer = true;
                break;
            } else if (r < l) {
                break;
            }
        }

        if (is_newer) {
            QDialog dlg(this);
            dlg.setWindowTitle(tr("Доступно обновление STORM EDEN"));
            dlg.resize(650, 460);
            dlg.setStyleSheet(QStringLiteral(
                "QDialog { background-color: #0b0f19; color: #ffffff; font-family: 'Segoe UI', sans-serif; }"
                "QLabel { color: #ffffff; }"
                "QTextEdit { background-color: #121826; color: #e2e8f0; border: 1px solid #1e283d; border-radius: 8px; padding: 10px; font-size: 9pt; }"
                "QPushButton { background-color: #161e30; color: #ffffff; border: 1px solid #23314a; border-radius: 6px; padding: 8px 18px; font-weight: bold; font-size: 9pt; }"
                "QPushButton:hover { background-color: #00e5ff; color: #000000; border-color: #00e5ff; }"
                "QPushButton#DownloadBtn { background-color: #00e5ff; color: #000000; border: 1px solid #00e5ff; }"
                "QPushButton#DownloadBtn:hover { background-color: #33ebff; }"
            ));

            auto* layout = new QVBoxLayout(&dlg);
            layout->setContentsMargins(20, 20, 20, 20);
            layout->setSpacing(14);

            auto* title = new QLabel(tr("<h2 style='margin:0; color:#00e5ff;'>⚡ Доступна новая версия: %1</h2>")
                                         .arg(release_name.isEmpty() ? tag_name : release_name), &dlg);
            auto* sub_info = new QLabel(tr("<span style='color:#94a3b8;'>Текущая версия: <b>%1</b> &nbsp;|&nbsp; Новая версия: <b>%2</b></span>")
                                            .arg(current_ver, clean_tag), &dlg);

            auto* notes_box = new QTextEdit(&dlg);
            notes_box->setReadOnly(true);
            notes_box->setPlainText(release_body.isEmpty() ? tr("Информация об изменениях не указана.") : release_body);

            auto* btn_layout = new QHBoxLayout();
            auto* download_btn = new QPushButton(tr("🚀 Скачать обновление"), &dlg);
            download_btn->setObjectName(QStringLiteral("DownloadBtn"));
            auto* close_btn = new QPushButton(tr("Позже"), &dlg);

            connect(download_btn, &QPushButton::clicked, [&dlg, html_url]() {
                const QString target_url = html_url.isEmpty()
                    ? QStringLiteral("https://github.com/ReiKatari/STORM_EDEN/releases")
                    : html_url;
                QDesktopServices::openUrl(QUrl(target_url));
                dlg.accept();
            });
            connect(close_btn, &QPushButton::clicked, &dlg, &QDialog::reject);

            btn_layout->addStretch();
            btn_layout->addWidget(close_btn);
            btn_layout->addWidget(download_btn);

            layout->addWidget(title);
            layout->addWidget(sub_info);
            layout->addWidget(notes_box);
            layout->addLayout(btn_layout);

            dlg.exec();
        } else if (manual_check) {
            QMessageBox::information(this, tr("STORM EDEN — Обновления"),
                                     tr("У вас установлена последняя актуальная версия STORM EDEN (v%1).\nОбновлений не найдено.").arg(current_ver));
        }
    });
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
    if (ui->action_Show_Filter_Bar->isChecked())
        game_list->SetFilterFocus();
    else
        game_list->ClearFilter();
}

void MainWindow::OnToggleStatusBar() {
    statusBar()->setVisible(ui->action_Show_Status_Bar->isChecked());
}

void MainWindow::OnTogglePerfOverlay() {
    if (perf_overlay)
        perf_overlay->setVisible(ui->action_Show_Performance_Overlay->isChecked());
}

void MainWindow::OnGameListRefresh() {
    // Resets metadata cache and reloads
    QtCommon::Game::ResetMetadata(false);
    game_list->RefreshGameDirectory();
    game_list->RefreshExternalContent();
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
    if (QtCommon::emu_thread == nullptr || !QtCommon::emu_thread->IsRunning())
        return;

    const u64 title_id = QtCommon::system->GetApplicationProcessProgramID();
    const auto screenshot_path =
        QString::fromStdString(Common::FS::GetEdenPathString(Common::FS::EdenPath::ScreenshotsDir));
    const auto date =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_hh-mm-ss-zzz"));
    QString filename = QStringLiteral("%1/%2_%3.png")
                           .arg(screenshot_path)
                           .arg(title_id, 16, 16, QLatin1Char{'0'})
                           .arg(date);

    if (!Common::FS::CreateDir(screenshot_path.toStdString()))
        return;

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

void MainWindow::UpdateWindowTitle(std::string_view filename, std::string_view display_version,
                                   std::string_view internal_version, std::string_view gpu_vendor) {
    static const std::string window_title =
        fmt::format("{} {}", std::string{Common::g_build_name}, std::string{Common::g_build_version});

    if (filename.empty()) {
        setWindowTitle(QString::fromStdString(window_title));
    } else {
        const std::string upper_gpu = QString::fromStdString(std::string{gpu_vendor}).toUpper().toStdString();
        const auto run_title = [filename, display_version, internal_version, upper_gpu]() {
            std::string disp_ver = std::string{display_version};
            if (disp_ver.empty()) {
                disp_ver = "1.0.0";
            }
            std::string intern_ver = std::string{internal_version};
            if (intern_ver.empty()) {
                intern_ver = "0";
            }
            return fmt::format("{} {} | {} | {} | {} | {}",
                               std::string{Common::g_build_name},
                               std::string{Common::g_build_version},
                               filename,
                               disp_ver,
                               intern_ver,
                               upper_gpu.empty() ? "GPU" : upper_gpu);
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
    if (QtCommon::emu_thread == nullptr || !QtCommon::system->IsPoweredOn()) {
        status_bar_update_timer.stop();
        return;
    }

    if (Settings::values.tas_enable)
        tas_label->setText(GetTasStateDescription());
    else
        tas_label->clear();

    auto results = QtCommon::system->GetAndResetPerfStats();
    auto& shader_notify = QtCommon::system->GPU().ShaderNotify();
    const int shaders_building = shader_notify.ShadersBuilding();

    emit statsUpdated(results, shader_notify);

    if (shaders_building > 0) {
        shader_building_label->setText(tr("Компиляция: %n шейдер(ов)", "", shaders_building));
        shader_building_label->setStyleSheet(QStringLiteral(
            "QLabel { background-color: rgba(255, 64, 129, 0.12); color: #ff4081; border: 1px solid rgba(255, 64, 129, 0.35); "
            "border-radius: 4px; padding: 2px 6px; font-size: 7.2pt; font-weight: 700; }"));
        shader_building_label->setVisible(true);
    } else {
        shader_building_label->setVisible(false);
    }

    res_scale_label->setVisible(false);

    if (Settings::values.use_speed_limit.GetValue()) {
        emu_speed_label->setText(tr("Скорость: %1% / %2%")
                                     .arg(results.emulation_speed * 100.0, 0, 'f', 0)
                                     .arg(Settings::SpeedLimit()));
    } else {
        emu_speed_label->setText(tr("Скорость: %1%").arg(results.emulation_speed * 100.0, 0, 'f', 0));
    }
    emu_speed_label->setStyleSheet(QStringLiteral(
        "QLabel { background-color: rgba(0, 230, 118, 0.10); color: #00e676; border: 1px solid rgba(0, 230, 118, 0.30); "
        "border-radius: 4px; padding: 2px 6px; font-size: 7.2pt; font-weight: 700; }"));

    QString fpsText = tr("Игра: %1 FPS").arg(std::round(results.average_game_fps), 0, 'f', 0);
    if (!m_fpsSuffix.isEmpty())
        fpsText = fpsText % QStringLiteral(" (%1)").arg(m_fpsSuffix);

    game_fps_label->setText(fpsText);
    game_fps_label->setStyleSheet(QStringLiteral(
        "QLabel { background-color: rgba(0, 229, 255, 0.10); color: #00e5ff; border: 1px solid rgba(0, 229, 255, 0.30); "
        "border-radius: 4px; padding: 2px 6px; font-size: 7.2pt; font-weight: 700; }"));

    emu_frametime_label->setText(tr("Кадр: %1 мс").arg(results.frametime * 1000.0, 0, 'f', 2));
    emu_frametime_label->setStyleSheet(QStringLiteral(
        "QLabel { background-color: rgba(255, 202, 40, 0.10); color: #ffca28; border: 1px solid rgba(255, 202, 40, 0.30); "
        "border-radius: 4px; padding: 2px 6px; font-size: 7.2pt; font-weight: 700; }"));

    emu_speed_label->setVisible(!Settings::values.use_multi_core.GetValue());
    game_fps_label->setVisible(true);
    emu_frametime_label->setVisible(true);
}

QString MainWindow::CleanDisplayString(const QString& str) {
    QString res = str;
    int idx = res.indexOf(QLatin1Char('('));
    if (idx > 0) {
        res = res.left(idx).trimmed();
    }
    return res;
}

void MainWindow::UpdateGPUAccuracyButton() {
    if (!gpu_accuracy_button) return;
    const auto gpu_accuracy = Settings::values.gpu_accuracy.GetValue();
    QString text = (gpu_accuracy == Settings::GpuAccuracy::Low) ? tr("Быстрый") : tr("Высокая точность");
    gpu_accuracy_button->setText(tr("ТОЧНОСТЬ ГПУ:\n%1").arg(text));
    gpu_accuracy_button->setChecked(gpu_accuracy != Settings::GpuAccuracy::Low);
}

void MainWindow::UpdateDockedButton() {
    if (!dock_status_button) return;
    const auto console_mode = Settings::values.use_docked_mode.GetValue();
    dock_status_button->setChecked(Settings::IsDockedMode());
    dock_status_button->setText(
        tr("РЕЖИМ:\n%1").arg(console_mode == Settings::ConsoleMode::Docked ? tr("В ДОКЕ") : tr("ПОРТАТИВ")));
}

void MainWindow::UpdateAPIText() {
    if (!renderer_status_button) return;
    const auto api = Settings::values.renderer_backend.GetValue();
    const auto renderer_status_text =
        ConfigurationShared::renderer_backend_texts_map.find(api)->second;
    renderer_status_button->setText(tr("РЕНДЕР:\n%1").arg(renderer_status_text.toUpper()));
}

void MainWindow::UpdateFilterText() {
    if (!filter_status_button) return;
    const auto filter = Settings::values.scaling_filter.GetValue();
    const auto it = ConfigurationShared::scaling_filter_texts_map.find(filter);
    const auto filter_text = it != ConfigurationShared::scaling_filter_texts_map.end() ? it->second : QStringLiteral("FSR");
    filter_status_button->setText(tr("ФИЛЬТР:\n%1").arg(filter_text.toUpper()));
}

void MainWindow::UpdateAAText() {
    if (!aa_status_button) return;
    const auto aa_mode = Settings::values.anti_aliasing.GetValue();
    const auto it = ConfigurationShared::anti_aliasing_texts_map.find(aa_mode);
    const auto aa_text = it != ConfigurationShared::anti_aliasing_texts_map.end() ? it->second : QStringLiteral("None");
    aa_status_button->setText(tr("СГЛАЖИВАНИЕ:\n%1").arg(aa_mode == Settings::AntiAliasing::None
                                  ? tr("ВЫКЛ")
                                  : aa_text.toUpper()));
}

void MainWindow::UpdateVolumeUI() {
    if (!volume_button || !volume_slider) return;
    const auto volume_value = static_cast<int>(Settings::values.volume.GetValue());
    volume_slider->setValue(volume_value);
    if (volume_val_label) {
        volume_val_label->setText(tr("Громкость: %1%").arg(volume_value));
    }
    if (Settings::values.audio_muted.GetValue()) {
        volume_button->setChecked(false);
        volume_button->setText(tr("ГРОМКОСТЬ:\nВЫКЛ"));
    } else {
        volume_button->setChecked(true);
        volume_button->setText(tr("ГРОМКОСТЬ:\n%1%").arg(volume_value));
    }
}

void MainWindow::UpdateAspectText() {
    if (!aspect_ratio_button) return;
    QString val_text = QStringLiteral("16:9");
    const auto combo_map = ConfigurationShared::ComboboxEnumeration(this);
    const auto it = combo_map->find(Settings::EnumMetadata<Settings::AspectRatio>::Index());
    if (it != combo_map->end()) {
        const u32 val = static_cast<u32>(Settings::values.aspect_ratio.GetValue());
        for (const auto& item : it->second) {
            if (item.first == val) {
                val_text = CleanDisplayString(item.second);
                if (val_text.contains(QStringLiteral("Растянуть")) || val_text.contains(QStringLiteral("Stretch"))) {
                    val_text = tr("Растянуть");
                }
                break;
            }
        }
    }
    aspect_ratio_button->setText(tr("СООТНОШЕНИЕ:\n%1").arg(val_text));
}

void MainWindow::UpdateDmaText() {
    if (!dma_accuracy_button) return;
    QString val_text;
    switch (Settings::values.dma_accuracy.GetValue()) {
    case Settings::DmaAccuracy::Default:
        val_text = tr("По умолчанию");
        break;
    case Settings::DmaAccuracy::Normal:
        val_text = tr("Нормально");
        break;
    case Settings::DmaAccuracy::Unsafe:
        val_text = tr("Небезопасно");
        break;
    case Settings::DmaAccuracy::Safe:
        val_text = tr("Безопасно");
        break;
    default:
        val_text = tr("По умолчанию");
        break;
    }
    dma_accuracy_button->setText(tr("DMA:\n%1").arg(val_text));
    dma_accuracy_button->setToolTip(tr("Точность прямого доступа к памяти DMA"));
}

void MainWindow::UpdateGpuFenceText() {
    if (!gpu_fence_button) return;
    QString val_text;
    switch (Settings::values.gpu_fence_behavior.GetValue()) {
    case Settings::GpuFenceBehavior::Default:
        val_text = tr("По умолчанию");
        break;
    case Settings::GpuFenceBehavior::Immediate:
        val_text = tr("Немедленно");
        break;
    case Settings::GpuFenceBehavior::Balanced:
        val_text = tr("Сбалансированно");
        break;
    case Settings::GpuFenceBehavior::Accurate:
        val_text = tr("Точно");
        break;
    case Settings::GpuFenceBehavior::Strict:
        val_text = tr("Строго");
        break;
    default:
        val_text = tr("По умолчанию");
        break;
    }
    gpu_fence_button->setText(tr("БАРЬЕРЫ ГПУ:\n%1").arg(val_text));
    gpu_fence_button->setToolTip(tr("Поведение барьеров ГПУ — синхронизация команд рендера"));
}

void MainWindow::UpdateVramText() {
    if (!vram_mode_button) return;
    QString val_text;
    switch (Settings::values.vram_usage_mode.GetValue()) {
    case Settings::VramUsageMode::Conservative:
        val_text = tr("Экономный");
        break;
    case Settings::VramUsageMode::Normal:
        val_text = tr("Нормальный");
        break;
    case Settings::VramUsageMode::Aggressive:
        val_text = tr("Агрессивный");
        break;
    default:
        val_text = tr("Нормальный");
        break;
    }
    vram_mode_button->setText(tr("VRAM:\n%1").arg(val_text));
}

void MainWindow::UpdateAnisotropyText() {
    if (!anisotropy_button) return;
    QString val_text;
    switch (Settings::values.max_anisotropy.GetValue()) {
    case Settings::AnisotropyMode::Automatic:
        val_text = tr("Автоматически");
        break;
    case Settings::AnisotropyMode::Default:
        val_text = tr("По умолчанию");
        break;
    case Settings::AnisotropyMode::X2:
        val_text = QStringLiteral("2x");
        break;
    case Settings::AnisotropyMode::X4:
        val_text = QStringLiteral("4x");
        break;
    case Settings::AnisotropyMode::X8:
        val_text = QStringLiteral("8x");
        break;
    case Settings::AnisotropyMode::X16:
        val_text = QStringLiteral("16x");
        break;
    case Settings::AnisotropyMode::X32:
        val_text = QStringLiteral("32x");
        break;
    case Settings::AnisotropyMode::X64:
        val_text = QStringLiteral("64x");
        break;
    case Settings::AnisotropyMode::None:
        val_text = tr("Отключено");
        break;
    default:
        val_text = tr("Автоматически");
        break;
    }
    anisotropy_button->setText(tr("АНИЗОТРОПИЯ:\n%1").arg(val_text));
}

void MainWindow::UpdateAstcDecodeText() {
    if (!astc_decode_button) return;
    QString val_text = QStringLiteral("ЦП Асинх.");
    switch (Settings::values.accelerate_astc.GetValue()) {
    case Settings::AstcDecodeMode::CpuAsynchronous:
        val_text = QStringLiteral("ЦП Асинх.");
        break;
    case Settings::AstcDecodeMode::Cpu:
        val_text = QStringLiteral("ЦП");
        break;
    case Settings::AstcDecodeMode::Gpu:
        val_text = QStringLiteral("ГПУ");
        break;
    }
    astc_decode_button->setText(tr("ДЕКОД. ASTC:\n%1").arg(val_text));
}

void MainWindow::UpdateAstcRecompressText() {
    if (!astc_recompress_button) return;
    QString val_text = QStringLiteral("Без сжатия");
    switch (Settings::values.astc_recompression.GetValue()) {
    case Settings::AstcRecompression::Uncompressed:
        val_text = QStringLiteral("Без сжатия");
        break;
    case Settings::AstcRecompression::Bc1:
        val_text = QStringLiteral("BC1");
        break;
    case Settings::AstcRecompression::Bc3:
        val_text = QStringLiteral("BC3");
        break;
    case Settings::AstcRecompression::Bc5:
        val_text = QStringLiteral("BC5");
        break;
    }
    astc_recompress_button->setText(tr("ПЕРЕСЖ. ASTC:\n%1").arg(val_text));
}

void MainWindow::UpdateResScaleText() {
    if (!res_scale_button) return;
    QString val_text = QStringLiteral("1X");
    switch (Settings::values.resolution_setup.GetValue()) {
    case Settings::ResolutionSetup::Res1_4X: val_text = QStringLiteral("0.25X"); break;
    case Settings::ResolutionSetup::Res1_2X: val_text = QStringLiteral("0.5X"); break;
    case Settings::ResolutionSetup::Res3_4X: val_text = QStringLiteral("0.75X"); break;
    case Settings::ResolutionSetup::Res1X: val_text = QStringLiteral("1X"); break;
    case Settings::ResolutionSetup::Res5_4X: val_text = QStringLiteral("1.25X"); break;
    case Settings::ResolutionSetup::Res3_2X: val_text = QStringLiteral("1.5X"); break;
    case Settings::ResolutionSetup::Res2X: val_text = QStringLiteral("2X"); break;
    case Settings::ResolutionSetup::Res3X: val_text = QStringLiteral("3X"); break;
    case Settings::ResolutionSetup::Res4X: val_text = QStringLiteral("4X"); break;
    case Settings::ResolutionSetup::Res5X: val_text = QStringLiteral("5X"); break;
    case Settings::ResolutionSetup::Res6X: val_text = QStringLiteral("6X"); break;
    case Settings::ResolutionSetup::Res7X: val_text = QStringLiteral("7X"); break;
    case Settings::ResolutionSetup::Res8X: val_text = QStringLiteral("8X"); break;
    default: break;
    }
    res_scale_button->setText(tr("МАСШТАБ:\n%1").arg(val_text));
}

void MainWindow::UpdateAddonsStatusButton(u64 title_id, const QString& game_name) {
    if (!addons_status_button) return;
    if (title_id != 0) {
        m_current_addons_title_id = title_id;
    }
    if (!game_name.isEmpty()) {
        m_current_addons_game_name = game_name;
    }
    if (m_current_addons_title_id == 0) {
        addons_status_button->setText(tr("ДОПОЛНЕНИЯ:\nНет"));
        addons_status_button->setStyleSheet(QString{});
        addons_status_button->setToolTip(tr("Выберите или запустите игру для просмотра дополнений"));
        return;
    }

    const u64 cur_tid = m_current_addons_title_id;
    const QString cur_name = m_current_addons_game_name;
    const std::string cur_path = m_current_addons_game_path;

    auto apply_button_style = [this, cur_tid, cur_name](int file_dlc_count, int tinfoil_dlc_count) {
        if (m_current_addons_title_id != cur_tid) return;
        const QString dlc_display_text = QStringLiteral("%1 / %2").arg(file_dlc_count).arg(tinfoil_dlc_count);
        addons_status_button->setText(tr("ДОПОЛНЕНИЯ:\n%1").arg(dlc_display_text));

        if (file_dlc_count >= tinfoil_dlc_count && (file_dlc_count > 0 || tinfoil_dlc_count > 0)) {
            addons_status_button->setStyleSheet(QStringLiteral(
                "QPushButton#TogglableStatusBarButton, QPushButton {"
                "  background-color: #0c2618;"
                "  color: #00e676;"
                "  border: 1px solid #00e676;"
                "  border-radius: 4px;"
                "  padding: 2px 5px;"
                "  font-size: 6.8pt;"
                "  font-weight: 800;"
                "  min-height: 26px;"
                "  min-width: 44px;"
                "}"
                "QPushButton#TogglableStatusBarButton:hover, QPushButton:hover {"
                "  background-color: #00e676;"
                "  color: #000000;"
                "}"
            ));
        } else if (file_dlc_count < tinfoil_dlc_count) {
            addons_status_button->setStyleSheet(QStringLiteral(
                "QPushButton#TogglableStatusBarButton, QPushButton {"
                "  background-color: #2e1014;"
                "  color: #ff5252;"
                "  border: 1px solid #ff5252;"
                "  border-radius: 4px;"
                "  padding: 2px 5px;"
                "  font-size: 6.8pt;"
                "  font-weight: 800;"
                "  min-height: 26px;"
                "  min-width: 44px;"
                "}"
                "QPushButton#TogglableStatusBarButton:hover, QPushButton:hover {"
                "  background-color: #ff5252;"
                "  color: #000000;"
                "}"
            ));
        } else {
            addons_status_button->setStyleSheet(QString{});
        }

        addons_status_button->setToolTip(tr(
            "Дополнения (DLC):\n"
            "• В файле игры: %1\n"
            "• В базе Tinfoil: %2\n"
            "Игра: %3 (ID: 0x%4)\n\n"
            "Нажмите для просмотра подробного списка всех дополнений и модов"
        ).arg(file_dlc_count)
         .arg(tinfoil_dlc_count)
         .arg(cur_name.isEmpty() ? tr("игре") : cur_name,
              QStringLiteral("%1").arg(cur_tid, 16, 16, QLatin1Char('0')).toUpper()));
    };

    {
        std::lock_guard<std::mutex> lock(m_addons_cache_mutex);
        auto it = m_addons_dlc_cache.find(cur_tid);
        if (it != m_addons_dlc_cache.end()) {
            apply_button_style(it->second.first, it->second.second);
            return;
        }
    }

    // Show quick status immediately without blocking
    addons_status_button->setText(tr("ДОПОЛНЕНИЯ:\n..."));

    // Compute in background thread
    std::thread([this, cur_tid, cur_name, cur_path, apply_button_style]() {
        std::set<u64> seen_dlc_ids;

        if (QtCommon::system) {
            const auto aoc_data = QtCommon::system->GetContentProvider().ListEntriesFilter(
                FileSys::TitleType::AOC, FileSys::ContentRecordType::Data);
            for (const auto& entry : aoc_data) {
                if ((entry.title_id & 0xFFFFFFFFFFFFF000) == (cur_tid & 0xFFFFFFFFFFFFF000) ||
                    (entry.title_id >= cur_tid + 1 && entry.title_id < cur_tid + 0x2000)) {
                    seen_dlc_ids.insert(entry.title_id);
                }
            }

            const FileSys::PatchManager patch_manager(cur_tid, QtCommon::system->GetFileSystemController(), QtCommon::system->GetContentProvider());
            const auto patches = patch_manager.GetPatches();
            for (const auto& patch : patches) {
                if (patch.type == FileSys::PatchType::DLC) {
                    const QStringList dlc_indices = QString::fromStdString(patch.version).split(QLatin1Char(','), Qt::SkipEmptyParts);
                    for (const auto& idx_str : dlc_indices) {
                        const u32 idx_val = idx_str.trimmed().toUInt();
                        const u64 generated_tid = (cur_tid & 0xFFFFFFFFFFFFF000) | (idx_val > 0 ? (0x1000 | (idx_val & 0x7FF)) : 0x1001);
                        seen_dlc_ids.insert(generated_tid);
                    }
                    if (dlc_indices.isEmpty()) {
                        seen_dlc_ids.insert((cur_tid & 0xFFFFFFFFFFFFF000) | 0x1001);
                    }
                }
            }
        }

        if (!cur_path.empty()) {
            static const QRegularExpression fn_dlc_tag{QStringLiteral(R"(\+([0-9]+)D\b)"), QRegularExpression::CaseInsensitiveOption};
            const auto dm = fn_dlc_tag.match(QString::fromStdString(cur_path));
            if (dm.hasMatch() && dm.hasCaptured(1)) {
                const int tag_count = dm.captured(1).toInt();
                for (int i = 1; i <= tag_count; ++i) {
                    seen_dlc_ids.insert((cur_tid & 0xFFFFFFFFFFFFF000) | (0x1000 + i));
                }
            }
        }

        const int file_dlc_count = static_cast<int>(seen_dlc_ids.size());
        TitleDB::TitleDatabase::Instance().WaitLoaded(std::chrono::milliseconds(3000));
        const int tinfoil_dlc_count = TitleDB::TitleDatabase::Instance().GetDlcCount(cur_tid);

        {
            std::lock_guard<std::mutex> lock(m_addons_cache_mutex);
            m_addons_dlc_cache[cur_tid] = {file_dlc_count, tinfoil_dlc_count};
        }

        QMetaObject::invokeMethod(this, [apply_button_style, file_dlc_count, tinfoil_dlc_count]() {
            apply_button_style(file_dlc_count, tinfoil_dlc_count);
        }, Qt::QueuedConnection);
    }).detach();
}

void MainWindow::UpdateAirplaneModeButton() {
    if (!airplane_mode_button) return;
    const bool airplane = Settings::values.airplane_mode.GetValue();
    airplane_mode_button->setText(airplane ? tr("САМОЛЁТ:\nВКЛ") : tr("САМОЛЁТ:\nВЫКЛ"));
    airplane_mode_button->setToolTip(tr("Режим полёта (отключение сетевых функций Switch)"));
}

void MainWindow::UpdateVSyncText() {
    if (!vsync_mode_button) return;
    QString val_text = QStringLiteral("FIFO");
    const auto vsync = Settings::values.vsync_mode.GetValue();
    switch (vsync) {
    case Settings::VSyncMode::Immediate: val_text = tr("ВЫКЛ"); break;
    case Settings::VSyncMode::Mailbox: val_text = QStringLiteral("MAILBOX"); break;
    case Settings::VSyncMode::Fifo: val_text = QStringLiteral("FIFO"); break;
    case Settings::VSyncMode::FifoRelaxed: val_text = QStringLiteral("RELAXED"); break;
    default: break;
    }
    vsync_mode_button->setText(tr("VSYNC:\n%1").arg(val_text));
}

void MainWindow::UpdateSpeedLimitText() {
    if (!speed_limit_button) return;
    if (!Settings::values.use_speed_limit.GetValue()) {
        speed_limit_button->setText(tr("СКОРОСТЬ:\nБЕЗ ЛИМИТА"));
    } else {
        const auto limit = Settings::values.speed_limit.GetValue();
        speed_limit_button->setText(tr("СКОРОСТЬ:\n%1%").arg(limit));
    }
}

void MainWindow::UpdateNvdecText() {
    if (!nvdec_status_button) return;
    QString val_text = QStringLiteral("ГПУ");
    const auto nvdec = Settings::values.nvdec_emulation.GetValue();
    switch (nvdec) {
    case Settings::NvdecEmulation::Off: val_text = tr("ВЫКЛ"); break;
    case Settings::NvdecEmulation::Cpu: val_text = QStringLiteral("ЦП"); break;
    case Settings::NvdecEmulation::Gpu: val_text = QStringLiteral("ГПУ"); break;
    default: break;
    }
    nvdec_status_button->setText(tr("NVDEC:\n%1").arg(val_text));
}

void MainWindow::UpdateCpuAccuracyText() {
    if (!cpu_accuracy_button) return;
    QString val_text = QStringLiteral("АВТО");
    const auto cpu_acc = Settings::values.cpu_accuracy.GetValue();
    switch (cpu_acc) {
    case Settings::CpuAccuracy::Auto: val_text = QStringLiteral("АВТО"); break;
    case Settings::CpuAccuracy::Accurate: val_text = tr("ТОЧНО"); break;
    case Settings::CpuAccuracy::Unsafe: val_text = tr("НЕБЕЗОПАСНО"); break;
    default: break;
    }
    cpu_accuracy_button->setText(tr("ТОЧНОСТЬ ЦП:\n%1").arg(val_text));
}

void MainWindow::UpdateDiskCacheText() {
    if (!disk_cache_button) return;
    const bool enabled = Settings::values.use_disk_shader_cache.GetValue();
    disk_cache_button->setText(enabled ? tr("КЭШ ШЕЙДЕРОВ:\nВКЛ") : tr("КЭШ ШЕЙДЕРОВ:\nВЫКЛ"));
}

void MainWindow::UpdateFullscreenButton() {
    if (!fullscreen_button) return;
    const bool is_full = ui->action_Fullscreen->isChecked();
    fullscreen_button->setText(is_full ? tr("ЭКРАН:\nПОЛНЫЙ") : tr("ЭКРАН:\nОКНО"));
}

void MainWindow::UpdateMuteButton() {
    if (!mute_button) return;
    const bool muted = Settings::values.audio_muted.GetValue();
    mute_button->setText(muted ? tr("ЗВУК:\nМУТ") : tr("ЗВУК:\nВКЛ"));
}

void MainWindow::SaveFooterSettings() {
    QStringList hidden;
    for (int i = 0; i < static_cast<int>(m_status_groups.size()); ++i) {
        if (m_status_groups[i] && !m_status_groups[i]->isVisible()) {
            hidden.append(QStringLiteral("grp_%1").arg(i));
        }
    }
    const std::vector<std::pair<QString, QWidget*>> btns = {
        {QStringLiteral("refresh"), refresh_button},
        {QStringLiteral("addons"), addons_status_button},
        {QStringLiteral("fullscreen"), fullscreen_button},
        {QStringLiteral("renderer"), renderer_status_button},
        {QStringLiteral("gpu_acc"), gpu_accuracy_button},
        {QStringLiteral("cpu_acc"), cpu_accuracy_button},
        {QStringLiteral("vsync"), vsync_mode_button},
        {QStringLiteral("dma"), dma_accuracy_button},
        {QStringLiteral("gpu_fence"), gpu_fence_button},
        {QStringLiteral("nvdec"), nvdec_status_button},
        {QStringLiteral("aa"), aa_status_button},
        {QStringLiteral("filter"), filter_status_button},
        {QStringLiteral("aspect"), aspect_ratio_button},
        {QStringLiteral("res_scale"), res_scale_button},
        {QStringLiteral("vram"), vram_mode_button},
        {QStringLiteral("anisotropy"), anisotropy_button},
        {QStringLiteral("disk_cache"), disk_cache_button},
        {QStringLiteral("astc_decode"), astc_decode_button},
        {QStringLiteral("astc_recompress"), astc_recompress_button},
        {QStringLiteral("dock"), dock_status_button},
        {QStringLiteral("airplane"), airplane_mode_button},
        {QStringLiteral("speed_limit"), speed_limit_button},
        {QStringLiteral("volume"), volume_button},
        {QStringLiteral("mute"), mute_button},
        {QStringLiteral("firmware"), firmware_label},
    };
    for (const auto& [id, w] : btns) {
        if (w && !w->isVisible()) {
            hidden.append(id);
        }
    }
    UISettings::values.hidden_footer_items.SetValue(hidden.join(QLatin1Char(';')).toStdString());
}

void MainWindow::LoadFooterSettings() {
    const std::string saved = UISettings::values.hidden_footer_items.GetValue();
    if (saved.empty()) return;
    const QStringList hidden = QString::fromStdString(saved).split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (int i = 0; i < static_cast<int>(m_status_groups.size()); ++i) {
        if (m_status_groups[i] && hidden.contains(QStringLiteral("grp_%1").arg(i))) {
            m_status_groups[i]->setVisible(false);
        }
    }
    const std::vector<std::pair<QString, QWidget*>> btns = {
        {QStringLiteral("refresh"), refresh_button},
        {QStringLiteral("addons"), addons_status_button},
        {QStringLiteral("fullscreen"), fullscreen_button},
        {QStringLiteral("renderer"), renderer_status_button},
        {QStringLiteral("gpu_acc"), gpu_accuracy_button},
        {QStringLiteral("cpu_acc"), cpu_accuracy_button},
        {QStringLiteral("vsync"), vsync_mode_button},
        {QStringLiteral("dma"), dma_accuracy_button},
        {QStringLiteral("gpu_fence"), gpu_fence_button},
        {QStringLiteral("nvdec"), nvdec_status_button},
        {QStringLiteral("aa"), aa_status_button},
        {QStringLiteral("filter"), filter_status_button},
        {QStringLiteral("aspect"), aspect_ratio_button},
        {QStringLiteral("res_scale"), res_scale_button},
        {QStringLiteral("vram"), vram_mode_button},
        {QStringLiteral("anisotropy"), anisotropy_button},
        {QStringLiteral("disk_cache"), disk_cache_button},
        {QStringLiteral("astc_decode"), astc_decode_button},
        {QStringLiteral("astc_recompress"), astc_recompress_button},
        {QStringLiteral("dock"), dock_status_button},
        {QStringLiteral("airplane"), airplane_mode_button},
        {QStringLiteral("speed_limit"), speed_limit_button},
        {QStringLiteral("volume"), volume_button},
        {QStringLiteral("mute"), mute_button},
        {QStringLiteral("firmware"), firmware_label},
    };
    for (const auto& [id, w] : btns) {
        if (w && hidden.contains(id)) {
            w->setVisible(false);
        }
    }
}

void MainWindow::ShowFooterCustomizeMenu() {
    QMenu context_menu(this);
    context_menu.setTitle(tr("Настройка панели подвала"));

    auto* header_act = context_menu.addAction(tr("--- РАЗДЕЛЫ ПОДВАЛА ---"));
    header_act->setEnabled(false);

    struct GroupInfo {
        QString name;
        QWidget* widget;
    };

    const std::vector<GroupInfo> groups = {
        {tr("Раздел: УПРАВЛЕНИЕ"), m_status_groups.size() > 0 ? m_status_groups[0] : nullptr},
        {tr("Раздел: ДОПОЛНЕНИЯ"), m_status_groups.size() > 1 ? m_status_groups[1] : nullptr},
        {tr("Раздел: РЕНДЕР"), m_status_groups.size() > 2 ? m_status_groups[2] : nullptr},
        {tr("Раздел: ГРАФИКА"), m_status_groups.size() > 3 ? m_status_groups[3] : nullptr},
        {tr("Раздел: ASTC"), m_status_groups.size() > 4 ? m_status_groups[4] : nullptr},
        {tr("Раздел: РЕЖИМ"), m_status_groups.size() > 5 ? m_status_groups[5] : nullptr},
        {tr("Раздел: СИСТЕМА"), m_status_groups.size() > 6 ? m_status_groups[6] : nullptr},
        {tr("Раздел: СЕТЬ"), m_status_groups.size() > 7 ? m_status_groups[7] : nullptr},
    };

    for (const auto& g : groups) {
        if (!g.widget) continue;
        auto* act = context_menu.addAction(g.name, [this, g] {
            g.widget->setVisible(!g.widget->isVisible());
            SaveFooterSettings();
        });
        act->setCheckable(true);
        act->setChecked(g.widget->isVisible());
    }

    context_menu.addSeparator();
    auto* btn_header = context_menu.addAction(tr("--- ОТДЕЛЬНЫЕ КНОПКИ ---"));
    btn_header->setEnabled(false);

    struct ButtonInfo {
        QString name;
        QWidget* widget;
    };

    const std::vector<ButtonInfo> buttons = {
        {tr("Обновить список"), refresh_button},
        {tr("Дополнения"), addons_status_button},
        {tr("Полный экран"), fullscreen_button},
        {tr("Рендер API"), renderer_status_button},
        {tr("Точность ГПУ"), gpu_accuracy_button},
        {tr("Точность ЦП"), cpu_accuracy_button},
        {tr("VSync"), vsync_mode_button},
        {tr("DMA"), dma_accuracy_button},
        {tr("Барьеры ГПУ"), gpu_fence_button},
        {tr("NVDEC"), nvdec_status_button},
        {tr("Сглаживание"), aa_status_button},
        {tr("Фильтр масштабирования"), filter_status_button},
        {tr("Соотношение сторон"), aspect_ratio_button},
        {tr("Масштаб разрешения"), res_scale_button},
        {tr("VRAM"), vram_mode_button},
        {tr("Анизотропия"), anisotropy_button},
        {tr("Кэш шейдеров"), disk_cache_button},
        {tr("Декод. ASTC"), astc_decode_button},
        {tr("Пересж. ASTC"), astc_recompress_button},
        {tr("Режим ТВ / Портал"), dock_status_button},
        {tr("Режим полёта"), airplane_mode_button},
        {tr("Ограничение скорости"), speed_limit_button},
        {tr("Громкость"), volume_button},
        {tr("Отключение звука"), mute_button},
        {tr("Прошивка"), firmware_label},
    };

    for (const auto& b : buttons) {
        if (!b.widget) continue;
        auto* act = context_menu.addAction(b.name, [this, b] {
            b.widget->setVisible(!b.widget->isVisible());
            SaveFooterSettings();
        });
        act->setCheckable(true);
        act->setChecked(b.widget->isVisible());
    }

    context_menu.addSeparator();
    context_menu.addAction(tr("Показать все разделы и элементы"), [this, groups, buttons] {
        for (const auto& g : groups) {
            if (g.widget) g.widget->setVisible(true);
        }
        for (const auto& b : buttons) {
            if (b.widget) b.widget->setVisible(true);
        }
        SaveFooterSettings();
    });

    context_menu.exec(QCursor::pos());
}

void MainWindow::ShowMenuAtWidget(QMenu& menu, QWidget* widget) {
    if (!widget) {
        menu.exec(QCursor::pos());
        return;
    }
    menu.ensurePolished();
    const QSize menu_size = menu.sizeHint();
    const QPoint global_pos = widget->mapToGlobal(QPoint(0, 0));
    int x = global_pos.x();
    int y = global_pos.y() - menu_size.height();

    const auto* screen = widget->screen();
    if (screen) {
        const QRect screen_geo = screen->availableGeometry();
        if (y < screen_geo.top()) {
            y = global_pos.y() + widget->height();
        }
        if (x + menu_size.width() > screen_geo.right()) {
            x = screen_geo.right() - menu_size.width();
        }
        if (x < screen_geo.left()) {
            x = screen_geo.left();
        }
    }
    menu.exec(QPoint(x, y));
}

void MainWindow::ShowGroupMenu(const QString& title, QWidget* group_widget) {
    if (title == tr("ДОПОЛНЕНИЯ")) {
        if (m_current_addons_title_id == 0 && game_list) {
            const auto [tid, path] = game_list->GetSelectedGameInfo();
            if (tid != 0) {
                m_current_addons_title_id = tid;
                m_current_addons_game_path = path.toStdString();
                m_current_addons_game_name = QFileInfo(path).completeBaseName();
                UpdateAddonsStatusButton(m_current_addons_title_id, m_current_addons_game_name);
            }
        }
        ShowDLCDialog(m_current_addons_title_id, m_current_addons_game_name);
        return;
    }
    QMenu context_menu(this);
    if (title == tr("ASTC")) {
        auto* header_act = context_menu.addAction(tr("🎨 Управление текстурами ASTC"));
        header_act->setEnabled(false);
        context_menu.addSeparator();

        auto* decode_menu = context_menu.addMenu(tr("⚙️ Декодирование ASTC"));
        const auto cur_dec = Settings::values.accelerate_astc.GetValue();
        const std::vector<std::pair<Settings::AstcDecodeMode, QString>> dec_options = {
            {Settings::AstcDecodeMode::CpuAsynchronous, tr("ЦП Асинхронно (Рекомендуется)")},
            {Settings::AstcDecodeMode::Cpu, tr("ЦП (Синхронно)")},
            {Settings::AstcDecodeMode::Gpu, tr("ГПУ (Аппаратное декодирование)")},
        };
        for (const auto& opt : dec_options) {
            auto* act = decode_menu->addAction(opt.second, [this, opt] {
                Settings::values.accelerate_astc.SetValue(opt.first);
                UpdateAstcDecodeText();
            });
            act->setCheckable(true);
            act->setChecked(opt.first == cur_dec);
        }

        auto* recomp_menu = context_menu.addMenu(tr("📦 Пересжатие ASTC"));
        const auto cur_rec = Settings::values.astc_recompression.GetValue();
        const std::vector<std::pair<Settings::AstcRecompression, QString>> rec_options = {
            {Settings::AstcRecompression::Uncompressed, tr("Без сжатия (Лучшее качество)")},
            {Settings::AstcRecompression::Bc1, tr("BC1 (Низкое качество)")},
            {Settings::AstcRecompression::Bc3, tr("BC3 (Среднее качество)")},
            {Settings::AstcRecompression::Bc5, tr("BC5 (Высокое качество)")},
        };
        for (const auto& opt : rec_options) {
            auto* act = recomp_menu->addAction(opt.second, [this, opt] {
                Settings::values.astc_recompression.SetValue(opt.first);
                UpdateAstcRecompressText();
            });
            act->setCheckable(true);
            act->setChecked(opt.first == cur_rec);
        }
    } else if (title == tr("РЕНДЕР")) {
        auto* header_act = context_menu.addAction(tr("⚡ Параметры рендера"));
        header_act->setEnabled(false);
        context_menu.addSeparator();

        auto* api_menu = context_menu.addMenu(tr("🎮 Графический API"));
        const auto cur_api = Settings::values.renderer_backend.GetValue();
        for (const auto& pair : ConfigurationShared::renderer_backend_texts_map) {
            if (pair.first == Settings::RendererBackend::Null) continue;
            auto* act = api_menu->addAction(pair.second, [this, pair] {
                Settings::values.renderer_backend.SetValue(pair.first);
                UpdateAPIText();
            });
            act->setCheckable(true);
            act->setChecked(pair.first == cur_api);
        }

        auto* gpu_acc_menu = context_menu.addMenu(tr("🎯 Точность GPU"));
        const auto cur_gpu_acc = Settings::values.gpu_accuracy.GetValue();
        for (const auto& pair : ConfigurationShared::gpu_accuracy_texts_map) {
            auto* act = gpu_acc_menu->addAction(pair.second, [this, pair] {
                Settings::values.gpu_accuracy.SetValue(pair.first);
                UpdateGPUAccuracyButton();
            });
            act->setCheckable(true);
            act->setChecked(pair.first == cur_gpu_acc);
        }

        auto* cpu_acc_menu = context_menu.addMenu(tr("🧠 Точность CPU"));
        const auto cur_cpu = Settings::values.cpu_accuracy.GetValue();
        const std::vector<std::pair<Settings::CpuAccuracy, QString>> cpu_options = {
            {Settings::CpuAccuracy::Auto, tr("Авто")},
            {Settings::CpuAccuracy::Accurate, tr("Точно")},
            {Settings::CpuAccuracy::Unsafe, tr("Небезопасно")},
        };
        for (const auto& opt : cpu_options) {
            auto* act = cpu_acc_menu->addAction(opt.second, [this, opt] {
                Settings::values.cpu_accuracy.SetValue(opt.first);
                UpdateCpuAccuracyText();
            });
            act->setCheckable(true);
            act->setChecked(opt.first == cur_cpu);
        }

        auto* vsync_menu = context_menu.addMenu(tr("⏱️ Синхронизация кадров"));
        const auto cur_vsync = Settings::values.vsync_mode.GetValue();
        const std::vector<std::pair<Settings::VSyncMode, QString>> vsync_options = {
            {Settings::VSyncMode::Fifo, tr("FIFO")},
            {Settings::VSyncMode::FifoRelaxed, tr("FIFO Relaxed")},
            {Settings::VSyncMode::Mailbox, tr("Mailbox")},
            {Settings::VSyncMode::Immediate, tr("Immediate")},
        };
        for (const auto& opt : vsync_options) {
            auto* act = vsync_menu->addAction(opt.second, [this, opt] {
                Settings::values.vsync_mode.SetValue(opt.first);
                UpdateVSyncText();
            });
            act->setCheckable(true);
            act->setChecked(opt.first == cur_vsync);
        }

        auto* nvdec_menu = context_menu.addMenu(tr("🎬 Декодирование видео"));
        const auto cur_nvdec = Settings::values.nvdec_emulation.GetValue();
        const std::vector<std::pair<Settings::NvdecEmulation, QString>> nvdec_options = {
            {Settings::NvdecEmulation::Gpu, tr("ГПУ")},
            {Settings::NvdecEmulation::Cpu, tr("ЦП")},
            {Settings::NvdecEmulation::Off, tr("Выключено")},
        };
        for (const auto& opt : nvdec_options) {
            auto* act = nvdec_menu->addAction(opt.second, [this, opt] {
                Settings::values.nvdec_emulation.SetValue(opt.first);
                UpdateNvdecText();
            });
            act->setCheckable(true);
            act->setChecked(opt.first == cur_nvdec);
        }

        auto* dma_menu = context_menu.addMenu(tr("⚡ Точность DMA"));
        const auto cur_dma = Settings::values.dma_accuracy.GetValue();
        const std::vector<std::pair<Settings::DmaAccuracy, QString>> dma_options = {
            {Settings::DmaAccuracy::Default, tr("По умолчанию")},
            {Settings::DmaAccuracy::Normal, tr("Нормально")},
            {Settings::DmaAccuracy::Unsafe, tr("Небезопасно")},
            {Settings::DmaAccuracy::Safe, tr("Безопасно")},
        };
        for (const auto& item : dma_options) {
            auto* act = dma_menu->addAction(item.second, [this, item] {
                Settings::values.dma_accuracy.SetValue(item.first);
                UpdateDmaText();
            });
            act->setCheckable(true);
            act->setChecked(item.first == cur_dma);
        }

        auto* fence_menu = context_menu.addMenu(tr("🛡️ Поведение барьеров ГПУ"));
        const auto cur_fence = Settings::values.gpu_fence_behavior.GetValue();
        const std::vector<std::pair<Settings::GpuFenceBehavior, QString>> fence_options = {
            {Settings::GpuFenceBehavior::Default, tr("По умолчанию")},
            {Settings::GpuFenceBehavior::Immediate, tr("Немедленно")},
            {Settings::GpuFenceBehavior::Balanced, tr("Сбалансированно")},
            {Settings::GpuFenceBehavior::Accurate, tr("Точно")},
            {Settings::GpuFenceBehavior::Strict, tr("Строго")},
        };
        for (const auto& item : fence_options) {
            auto* act = fence_menu->addAction(item.second, [this, item] {
                Settings::values.gpu_fence_behavior.SetValue(item.first);
                UpdateGpuFenceText();
            });
            act->setCheckable(true);
            act->setChecked(item.first == cur_fence);
        }
    } else if (title == tr("ГРАФИКА")) {
        auto* header_act = context_menu.addAction(tr("🖼️ Графика и масштабирование"));
        header_act->setEnabled(false);
        context_menu.addSeparator();

        auto* res_menu = context_menu.addMenu(tr("📐 Разрешение"));
        const auto cur_res = Settings::values.resolution_setup.GetValue();
        const std::vector<std::pair<Settings::ResolutionSetup, QString>> res_options = {
            {Settings::ResolutionSetup::Res1_2X, tr("0.5X")},
            {Settings::ResolutionSetup::Res3_4X, tr("0.75X")},
            {Settings::ResolutionSetup::Res1X, tr("1X")},
            {Settings::ResolutionSetup::Res3_2X, tr("1.5X")},
            {Settings::ResolutionSetup::Res2X, tr("2X")},
            {Settings::ResolutionSetup::Res3X, tr("3X")},
            {Settings::ResolutionSetup::Res4X, tr("4X")},
        };
        for (const auto& opt : res_options) {
            auto* act = res_menu->addAction(opt.second, [this, opt] {
                Settings::values.resolution_setup.SetValue(opt.first);
                UpdateResScaleText();
            });
            act->setCheckable(true);
            act->setChecked(opt.first == cur_res);
        }

        auto* vram_menu = context_menu.addMenu(tr("💾 Видеопамять"));
        const auto cur_vram = Settings::values.vram_usage_mode.GetValue();
        const std::vector<std::pair<Settings::VramUsageMode, QString>> vram_options = {
            {Settings::VramUsageMode::Conservative, tr("Экономный")},
            {Settings::VramUsageMode::Normal, tr("Нормальный")},
            {Settings::VramUsageMode::Aggressive, tr("Агрессивный")},
        };
        for (const auto& opt : vram_options) {
            auto* act = vram_menu->addAction(opt.second, [this, opt] {
                Settings::values.vram_usage_mode.SetValue(opt.first);
                UpdateVramText();
            });
            act->setCheckable(true);
            act->setChecked(opt.first == cur_vram);
        }

        auto* aniso_menu = context_menu.addMenu(tr("🔍 Анизотропная фильтрация"));
        const auto cur_aniso = Settings::values.max_anisotropy.GetValue();
        const std::vector<std::pair<Settings::AnisotropyMode, QString>> aniso_options = {
            {Settings::AnisotropyMode::Automatic, tr("Автоматически")},
            {Settings::AnisotropyMode::Default, tr("По умолчанию")},
            {Settings::AnisotropyMode::X2, QStringLiteral("2x")},
            {Settings::AnisotropyMode::X4, QStringLiteral("4x")},
            {Settings::AnisotropyMode::X8, QStringLiteral("8x")},
            {Settings::AnisotropyMode::X16, QStringLiteral("16x")},
            {Settings::AnisotropyMode::X32, QStringLiteral("32x")},
            {Settings::AnisotropyMode::X64, QStringLiteral("64x")},
            {Settings::AnisotropyMode::None, tr("Отключено")},
        };
        for (const auto& opt : aniso_options) {
            auto* act = aniso_menu->addAction(opt.second, [this, opt] {
                Settings::values.max_anisotropy.SetValue(opt.first);
                UpdateAnisotropyText();
            });
            act->setCheckable(true);
            act->setChecked(opt.first == cur_aniso);
        }

        auto* aa_menu = context_menu.addMenu(tr("✨ Сглаживание"));
        const auto cur_aa = Settings::values.anti_aliasing.GetValue();
        for (const auto& pair : ConfigurationShared::anti_aliasing_texts_map) {
            auto* act = aa_menu->addAction(pair.second, [this, pair] {
                Settings::values.anti_aliasing.SetValue(pair.first);
                UpdateAAText();
            });
            act->setCheckable(true);
            act->setChecked(pair.first == cur_aa);
        }

        auto* filter_menu = context_menu.addMenu(tr("🔬 Фильтрация масштабирования"));
        const auto cur_filter = Settings::values.scaling_filter.GetValue();
        for (const auto& pair : ConfigurationShared::scaling_filter_texts_map) {
            auto* act = filter_menu->addAction(pair.second, [this, pair] {
                Settings::values.scaling_filter.SetValue(pair.first);
                UpdateFilterText();
            });
            act->setCheckable(true);
            act->setChecked(pair.first == cur_filter);
        }

        auto* disk_act = context_menu.addAction(tr("💽 Кэш шейдеров на диске"), [this] {
            Settings::values.use_disk_shader_cache.SetValue(!Settings::values.use_disk_shader_cache.GetValue());
            UpdateDiskCacheText();
        });
        disk_act->setCheckable(true);
        disk_act->setChecked(Settings::values.use_disk_shader_cache.GetValue());
    } else if (title == tr("РЕЖИМ")) {
        auto* header_act = context_menu.addAction(tr("🕹️ Режимы работы консоли"));
        header_act->setEnabled(false);
        context_menu.addSeparator();

        auto* dock_menu = context_menu.addMenu(tr("📺 Режим консоли"));
        const auto cur_dock = Settings::values.use_docked_mode.GetValue();
        for (const auto& pair : ConfigurationShared::use_docked_mode_texts_map) {
            auto* act = dock_menu->addAction(pair.second, [this, pair] {
                if (pair.first != Settings::values.use_docked_mode.GetValue()) {
                    OnToggleDockedMode();
                }
            });
            act->setCheckable(true);
            act->setChecked(pair.first == cur_dock);
        }

        auto* speed_menu = context_menu.addMenu(tr("⚡ Ограничение скорости"));
        speed_menu->addAction(tr("100%"), [this] {
            Settings::values.use_speed_limit.SetValue(true);
            Settings::values.speed_limit.SetValue(100);
            UpdateSpeedLimitText();
        });
        speed_menu->addAction(tr("150%"), [this] {
            Settings::values.use_speed_limit.SetValue(true);
            Settings::values.speed_limit.SetValue(150);
            UpdateSpeedLimitText();
        });
        speed_menu->addAction(tr("200%"), [this] {
            Settings::values.use_speed_limit.SetValue(true);
            Settings::values.speed_limit.SetValue(200);
            UpdateSpeedLimitText();
        });
        speed_menu->addAction(tr("300%"), [this] {
            Settings::values.use_speed_limit.SetValue(true);
            Settings::values.speed_limit.SetValue(300);
            UpdateSpeedLimitText();
        });
        speed_menu->addAction(tr("Без лимита скорости"), [this] {
            Settings::values.use_speed_limit.SetValue(false);
            UpdateSpeedLimitText();
        });

        auto* airplane_act = context_menu.addAction(tr("✈️ Режим полёта"), [this] {
            Settings::values.airplane_mode.SetValue(!Settings::values.airplane_mode.GetValue());
            UpdateAirplaneModeButton();
        });
        airplane_act->setCheckable(true);
        airplane_act->setChecked(Settings::values.airplane_mode.GetValue());

        auto* mute_act = context_menu.addAction(tr("🔇 Отключить звук"), [this] {
            OnMute();
        });
        mute_act->setCheckable(true);
        mute_act->setChecked(Settings::values.audio_muted.GetValue());
    } else if (title == tr("УПРАВЛЕНИЕ")) {
        context_menu.addAction(tr("🔄 Обновить список игр"), this, &MainWindow::OnGameListRefresh);
        context_menu.addAction(tr("🖥️ Полноэкранный режим"), this, [this] {
            ui->action_Fullscreen->setChecked(!ui->action_Fullscreen->isChecked());
            ToggleFullscreen();
            UpdateFullscreenButton();
        });
    } else if (title == tr("СИСТЕМА")) {
        auto* header_act = context_menu.addAction(tr("🛠️ Системные компоненты"));
        header_act->setEnabled(false);
        context_menu.addSeparator();
        context_menu.addAction(tr("📦 Установить прошивку из папки..."), this, &MainWindow::OnInstallFirmware);
        context_menu.addAction(tr("📦 Установить прошивку из ZIP..."), this, &MainWindow::OnInstallFirmwareFromZIP);
        context_menu.addAction(tr("🔑 Установить ключи (prod.keys)..."), this, &MainWindow::OnInstallDecryptionKeys);
        context_menu.addAction(tr("📁 Открыть папку NAND..."), this, &MainWindow::OnOpenNANDFolder);
    } else if (title == tr("СЕТЬ")) {
        auto* header_act = context_menu.addAction(tr("🌐 Сетевые функции"));
        header_act->setEnabled(false);
        context_menu.addSeparator();
        context_menu.addAction(tr("🌐 Мультиплеер (Обзор комнат)..."), multiplayer_state, &MultiplayerState::OnOpenNetworkRoom);
        context_menu.addAction(tr("🔗 Прямое подключение к комнате..."), multiplayer_state, &MultiplayerState::OnDirectConnectToRoom);
        context_menu.addAction(tr("⚙️ Настройки сети..."), this, [this] {
            OnConfigure();
        });
    }
    context_menu.addSeparator();
    context_menu.addAction(tr("⚙️ Настроить подвал..."), this, &MainWindow::ShowFooterCustomizeMenu);

    ShowMenuAtWidget(context_menu, group_widget);
}

void MainWindow::UpdateStatusButtons() {
    if (renderer_status_button) {
        renderer_status_button->setChecked(Settings::values.renderer_backend.GetValue() ==
                                           Settings::RendererBackend::Vulkan);
    }
    UpdateAPIText();
    UpdateGPUAccuracyButton();
    UpdateDockedButton();
    UpdateFilterText();
    UpdateAAText();
    UpdateAspectText();
    UpdateDmaText();
    UpdateGpuFenceText();
    UpdateVramText();
    UpdateAnisotropyText();
    UpdateAstcDecodeText();
    UpdateAstcRecompressText();
    UpdateResScaleText();
    UpdateAirplaneModeButton();
    UpdateVSyncText();
    UpdateSpeedLimitText();
    UpdateNvdecText();
    UpdateCpuAccuracyText();
    UpdateDiskCacheText();
    UpdateFullscreenButton();
    UpdateMuteButton();
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
    if (!input_subsystem)
        return;
    input_subsystem->PumpEvents();
}

void MainWindow::HideMouseCursor() {
    if (QtCommon::emu_thread == nullptr && UISettings::values.hide_mouse) {
        mouse_hide_timer.stop();
        ShowMouseCursor();
        return;
    }
    render_window->setCursor(QCursor(Qt::BlankCursor));
}

void MainWindow::ShowMouseCursor() {
    render_window->unsetCursor();
    if (QtCommon::emu_thread != nullptr && UISettings::values.hide_mouse)
        mouse_hide_timer.start();
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
        firmware_label->setAlignment(Qt::AlignCenter);
        firmware_label->setText(tr("ПРОШИВКА:\nНЕТ"));
        firmware_label->setToolTip(tr("Прошивка не установлена (Инструменты -> Установить прошивку)"));
        firmware_label->setVisible(true);
        return;
    }

    firmware_label->setVisible(true);
    ui->menu_Applets->setEnabled(true);
    ui->menu_Create_Shortcuts->setEnabled(true);

    const std::string display_version(firmware_data.display_version.data());
    const std::string display_title(firmware_data.display_title.data());

    LOG_INFO(Frontend, "Installed firmware: {}", display_version);

    firmware_label->setAlignment(Qt::AlignCenter);
    firmware_label->setText(tr("ПРОШИВКА:\n%1").arg(QString::fromStdString(display_version)));
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
    if (QtCommon::emu_thread == nullptr ||
        UISettings::values.confirm_before_stopping.GetValue() == ConfirmStop::Ask_Never)
        return true;

    if (!QtCommon::system->GetExitLocked() &&
        UISettings::values.confirm_before_stopping.GetValue() == ConfirmStop::Ask_Based_On_Game)
        return true;

    const auto text = tr("Вы действительно хотите закрыть STORM EDEN?");
    return question(this, tr("STORM EDEN"), text);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (!ConfirmClose()) {
        event->ignore();
        return;
    }

    UpdateUISettings();
    game_list->SaveInterfaceLayout();
    SaveFooterSettings();
    UISettings::SaveWindowState();
    hotkey_registry.SaveHotkeys();

    // Unload controllers early
    controller_dialog->UnloadController();
    game_list->UnloadController();

    // Shutdown session if the emu thread is active...
    if (QtCommon::emu_thread != nullptr)
        ShutdownGame();

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

void MainWindow::ShowDLCDialog(u64 title_id, const QString& game_name) {
    if (title_id == 0) {
        QMessageBox::information(this, QStringLiteral("STORM EDEN"), tr("Нет выделенной или запущенной игры."));
        return;
    }

    const FileSys::PatchManager patch_manager(title_id, QtCommon::system->GetFileSystemController(), QtCommon::system->GetContentProvider());
    const auto patches = patch_manager.GetPatches();
    const auto& provider = QtCommon::system->GetContentProvider();
    TitleDB::TitleDatabase::Instance().WaitLoaded(std::chrono::milliseconds(3000));

    const int parent_width = this->width();
    const int parent_height = this->height();
    const int target_width = std::clamp(static_cast<int>(parent_width * 0.96), 1450, 2400);
    const int target_height = std::clamp(static_cast<int>(parent_height * 0.90), 680, 1200);

    QDialog dlg(this);
    dlg.setWindowTitle(tr("STORM EDEN — Менеджер дополнений"));
    dlg.resize(target_width, target_height);
    dlg.setMinimumSize(1300, 600);
    dlg.setStyleSheet(QStringLiteral(
        "QDialog { background-color: #0b0f19; color: #ffffff; font-family: 'Segoe UI', sans-serif; }"
        "QLabel { color: #ffffff; }"
        "QLineEdit { background-color: #141b2a; color: #ffffff; border: 1px solid #23314a; border-radius: 6px; padding: 7px 12px; font-size: 9pt; }"
        "QLineEdit:focus { border: 1px solid #00e5ff; }"
        "QTableWidget { background-color: #0f1422; color: #ffffff; gridline-color: #1b2438; border: 1px solid #1c273c; border-radius: 8px; selection-background-color: rgba(0, 229, 255, 0.25); selection-color: #ffffff; font-size: 9pt; }"
        "QHeaderView::section { background-color: #161e30; color: #00e5ff; font-weight: bold; border: 1px solid #1c273c; padding: 7px; font-size: 9pt; }"
        "QPushButton { background-color: #161e30; color: #ffffff; border: 1px solid #23314a; border-radius: 6px; padding: 7px 16px; font-weight: bold; font-size: 8.5pt; }"
        "QPushButton:hover { background-color: #00e5ff; color: #000000; border-color: #00e5ff; }"
        "QPushButton#CopyBtn { background-color: #1b273d; color: #00e5ff; border: 1px solid #00e5ff; }"
        "QPushButton#CopyBtn:hover { background-color: #00e5ff; color: #000000; }"
        "QMenu { background-color: #121826; color: #ffffff; border: 1px solid #1e283d; padding: 4px; }"
        "QMenu::item:selected { background-color: #00e5ff; color: #000000; font-weight: bold; border-radius: 4px; }"
    ));

    auto* main_layout = new QVBoxLayout(&dlg);
    main_layout->setContentsMargins(18, 18, 18, 18);
    main_layout->setSpacing(12);

    QString full_display_name;
    if (!m_current_addons_game_path.empty()) {
        const QFileInfo fi(QString::fromStdString(m_current_addons_game_path));
        full_display_name = fi.completeBaseName();
    }
    if (full_display_name.isEmpty()) {
        full_display_name = game_name.isEmpty() ? QString::fromStdString(fmt::format("{:016X}", title_id)) : game_name;
    }

    const QString tid_str = QString::fromStdString(fmt::format("{:016X}", title_id));

    QString base_clean_name;
    const auto base_tdb = TitleDB::TitleDatabase::Instance().Lookup(title_id);
    if (base_tdb.has_value() && !base_tdb->name.empty()) {
        base_clean_name = QString::fromStdString(base_tdb->name).trimmed();
    }
    if (base_clean_name.isEmpty() && !game_name.isEmpty()) {
        base_clean_name = game_name.trimmed();
    }

    const auto tdb_dlcs = TitleDB::TitleDatabase::Instance().GetDlcs(title_id);

    auto clean_item_name = [&](QString raw_name) -> QString {
        raw_name = raw_name.trimmed();
        if (raw_name.isEmpty()) return QString{};

        if (!base_clean_name.isEmpty()) {
            if (raw_name.startsWith(base_clean_name, Qt::CaseInsensitive)) {
                raw_name = raw_name.mid(base_clean_name.length()).trimmed();
            } else {
                const QStringList parts = base_clean_name.split(QRegularExpression(QStringLiteral("[:—–\\-]")), Qt::SkipEmptyParts);
                for (const auto& part : parts) {
                    const QString trimmed_part = part.trimmed();
                    if (trimmed_part.length() >= 4) {
                        const int idx = raw_name.indexOf(trimmed_part, 0, Qt::CaseInsensitive);
                        if (idx >= 0) {
                            raw_name.remove(idx, trimmed_part.length());
                            raw_name = raw_name.trimmed();
                        }
                    }
                }
            }
        }

        if (raw_name.startsWith(QLatin1Char('T')) && raw_name.length() > 1 && (raw_name[1] == QLatin1Char(':') || raw_name[1] == QLatin1Char(' '))) {
            raw_name = raw_name.mid(1).trimmed();
        }

        while (!raw_name.isEmpty() && (raw_name.startsWith(QLatin1Char(':')) ||
                                       raw_name.startsWith(QLatin1Char('-')) ||
                                       raw_name.startsWith(QStringLiteral("—")) ||
                                       raw_name.startsWith(QStringLiteral("–")) ||
                                       raw_name.startsWith(QLatin1Char('|')) ||
                                       raw_name.startsWith(QLatin1Char('~')) ||
                                       raw_name.startsWith(QLatin1Char(' ')))) {
            raw_name = raw_name.mid(1).trimmed();
        }
        return raw_name;
    };

    auto resolve_dlc_name = [&](u64 dlc_tid, int dlc_order, const QString& nacp_fallback = QString{}) -> QString {
        const int dlc_num = static_cast<int>(dlc_tid & 0x7FF);
        const int num_to_show = dlc_order > 0 ? dlc_order : (dlc_num > 0 ? dlc_num : 1);

        const auto tdb = TitleDB::TitleDatabase::Instance().Lookup(dlc_tid);
        if (tdb.has_value() && !tdb->name.empty()) {
            QString name_str = QString::fromStdString(tdb->name).trimmed();
            QString cleaned = clean_item_name(name_str);
            if (cleaned.isEmpty()) cleaned = name_str;
            if (!cleaned.isEmpty() && !cleaned.startsWith(QStringLiteral("Дополнение #"))) {
                return QStringLiteral("Дополнение #%1: %2").arg(num_to_show).arg(cleaned);
            }
            if (!name_str.isEmpty()) return name_str;
        }

        const std::string d_hex = fmt::format("{:016X}", dlc_tid);
        for (const auto& d : tdb_dlcs) {
            if (d.id == d_hex && !d.name.empty()) {
                QString name_str = QString::fromStdString(d.name).trimmed();
                QString cleaned = clean_item_name(name_str);
                if (cleaned.isEmpty()) cleaned = name_str;
                if (!cleaned.isEmpty() && !cleaned.startsWith(QStringLiteral("Дополнение #"))) {
                    return QStringLiteral("Дополнение #%1: %2").arg(num_to_show).arg(cleaned);
                }
                return name_str;
            }
        }

        if (dlc_order > 0 && dlc_order <= static_cast<int>(tdb_dlcs.size())) {
            const auto& d = tdb_dlcs[dlc_order - 1];
            if (!d.name.empty()) {
                QString name_str = QString::fromStdString(d.name).trimmed();
                QString cleaned = clean_item_name(name_str);
                if (cleaned.isEmpty()) cleaned = name_str;
                if (!cleaned.isEmpty() && !cleaned.startsWith(QStringLiteral("Дополнение #"))) {
                    return QStringLiteral("Дополнение #%1: %2").arg(num_to_show).arg(cleaned);
                }
                return name_str;
            }
        }

        if (!nacp_fallback.trimmed().isEmpty()) {
            const QString cleaned = clean_item_name(nacp_fallback);
            if (!cleaned.isEmpty() && !cleaned.startsWith(QStringLiteral("Дополнение #"))) {
                return QStringLiteral("Дополнение #%1: %2").arg(num_to_show).arg(cleaned);
            }
            return nacp_fallback;
        }

        return tr("Дополнение #%1").arg(num_to_show);
    };

    auto resolve_dlc_desc = [&](u64 dlc_tid, int dlc_order) -> QString {
        const auto tdb = TitleDB::TitleDatabase::Instance().Lookup(dlc_tid);
        if (tdb.has_value() && !tdb->description.empty()) {
            const QString desc = QString::fromStdString(tdb->description).trimmed();
            if (!base_tdb.has_value() || desc != QString::fromStdString(base_tdb->description).trimmed()) {
                return desc;
            }
        }
        if (dlc_order > 0 && dlc_order <= static_cast<int>(tdb_dlcs.size())) {
            const auto& d = tdb_dlcs[dlc_order - 1];
            if (!d.description.empty()) {
                const QString desc = QString::fromStdString(d.description).trimmed();
                if (!base_tdb.has_value() || desc != QString::fromStdString(base_tdb->description).trimmed()) {
                    return desc;
                }
            }
        }
        return tr("Официальный загружаемый контент (DLC). Включает дополнительные игровые материалы, бонусы или сценарии.");
    };

    struct RowItem {
        QString type;
        QString tid;
        QString name;
        QString desc;
        QString ver;
        QString internal_ver;
        QString status;
    };
    std::vector<RowItem> rows;

    int total_dlcs = 0;
    int total_updates = 0;
    int total_mods = 0;

    QString update_ver_str;
    QString update_internal_ver_str = QStringLiteral("0");

    // 1. Try PatchManager control metadata
    if (const auto nacp = patch_manager.GetControlMetadata().first; nacp != nullptr) {
        const auto ver = nacp->GetVersionString();
        if (!ver.empty() && ver != "0") {
            update_ver_str = QString::fromStdString(ver);
        }
    }

    // 2. Try provider update version
    const u32 prov_update_num = provider.GetEntryVersion(FileSys::GetUpdateTitleID(title_id)).value_or(0);
    if (prov_update_num > 0) {
        update_internal_ver_str = QString::number(prov_update_num);
        if (update_ver_str.isEmpty() || update_ver_str == QStringLiteral("1.0.0") || update_ver_str == QStringLiteral("0")) {
            update_ver_str = QString::number(prov_update_num);
        }
    }

    // 3. Try reading NACP directly from the running file
    if ((update_ver_str.isEmpty() || update_ver_str == QStringLiteral("1.0.0") || update_ver_str == QStringLiteral("0")) && !m_current_addons_game_path.empty()) {
        const auto v_file = Core::GetGameFileFromPath(QtCommon::vfs, m_current_addons_game_path);
        if (v_file) {
            const auto file_loader = Loader::GetLoader(*QtCommon::system, v_file);
            if (file_loader) {
                FileSys::NACP file_nacp;
                if (file_loader->ReadControlData(file_nacp) == Loader::ResultStatus::Success) {
                    const auto ver = file_nacp.GetVersionString();
                    if (!ver.empty() && ver != "0") {
                        update_ver_str = QString::fromStdString(ver);
                    }
                }
            }
        }
    }

    // 4. Try matching version pair (e.g. "(1.0.7 - 393216 - ...") from filename
    if (!m_current_addons_game_path.empty()) {
        static const QRegularExpression fn_pair_ver_regex{QStringLiteral(R"(\(([0-9]+\.[0-9]+(?:\.[0-9]+)*)\s*-\s*([0-9]+))")};
        const auto fm = fn_pair_ver_regex.match(QString::fromStdString(m_current_addons_game_path));
        if (fm.hasMatch() && !fm.captured(1).isEmpty()) {
            update_ver_str = fm.captured(1);
            update_internal_ver_str = fm.captured(2);
        } else {
            static const QRegularExpression fn_ver_regex{QStringLiteral(R"((?:[\(\[\s]v?|\b)([0-9]+\.[0-9]+(?:\.[0-9]+)*)(?!\s*(?:GB|MB|KB|TB|ГБ|МБ|КБ|Б|B)\b))")};
            const auto m = fn_ver_regex.match(QString::fromStdString(m_current_addons_game_path));
            if (m.hasMatch() && m.hasCaptured(1) && (update_ver_str.isEmpty() || update_ver_str == QStringLiteral("1.0.0"))) {
                update_ver_str = m.captured(1);
            }
            static const QRegularExpression fn_vnum_regex{QStringLiteral(R"(\[v([0-9]+)\])")};
            const auto vm = fn_vnum_regex.match(QString::fromStdString(m_current_addons_game_path));
            if (vm.hasMatch() && update_internal_ver_str == QStringLiteral("0")) {
                update_internal_ver_str = vm.captured(1);
            }
        }
    }

    while (update_ver_str.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
        update_ver_str.remove(0, 1);
    }
    update_ver_str = update_ver_str.trimmed();

    if (update_ver_str.isEmpty() || update_ver_str == QStringLiteral("0")) {
        update_ver_str = QStringLiteral("1.0.0");
    }

    // 1. Collect individual DLCs from ContentProvider
    const auto aoc_data = provider.ListEntriesFilter(
        FileSys::TitleType::AOC, FileSys::ContentRecordType::Data);
    std::set<u64> seen_dlc_ids;
    std::set<u64> seen_update_ids;

    for (const auto& entry : aoc_data) {
        if (FileSys::GetBaseTitleID(entry.title_id) == FileSys::GetBaseTitleID(title_id) ||
            (entry.title_id >= title_id + 1 && entry.title_id < title_id + 0x2000)) {
            seen_dlc_ids.insert(entry.title_id);
            total_dlcs++;

            QString nacp_title;
            const auto dlc_ctrl = provider.GetEntry(entry.title_id, FileSys::ContentRecordType::Control);
            if (dlc_ctrl) {
                const auto [nacp, icon] = patch_manager.ParseControlNCA(*dlc_ctrl);
                if (nacp) {
                    nacp_title = QString::fromStdString(nacp->GetApplicationName());
                }
            }

            const QString dlc_name = resolve_dlc_name(entry.title_id, total_dlcs, nacp_title);
            const QString dlc_desc = resolve_dlc_desc(entry.title_id, total_dlcs);
            const u32 dlc_ver = provider.GetEntryVersion(entry.title_id).value_or(0);

            rows.push_back({
                tr("Дополнение"),
                QString::fromStdString(fmt::format("{:016X}", entry.title_id)),
                dlc_name,
                dlc_desc,
                QStringLiteral("1.0.0"),
                QString::number(dlc_ver),
                tr("✓ В файле / Активно"),
            });
        }
    }

    // 2. Fallback for DLCs listed in PatchManager if not separately indexed in provider
    for (const auto& p : patches) {
        if (p.type == FileSys::PatchType::DLC) {
            const QStringList dlc_indices = QString::fromStdString(p.version).split(QLatin1Char(','), Qt::SkipEmptyParts);
            for (const auto& idx_str : dlc_indices) {
                const u32 idx_val = idx_str.trimmed().toUInt();
                const u64 generated_tid = (title_id & 0xFFFFFFFFFFFFF000) | (idx_val > 0 ? (0x1000 | (idx_val & 0x7FF)) : 0x1001);
                if (seen_dlc_ids.find(generated_tid) == seen_dlc_ids.end()) {
                    seen_dlc_ids.insert(generated_tid);
                    total_dlcs++;

                    const QString dlc_name = resolve_dlc_name(generated_tid, total_dlcs);
                    const QString dlc_desc = resolve_dlc_desc(generated_tid, total_dlcs);

                    rows.push_back({
                        tr("Дополнение"),
                        QString::fromStdString(fmt::format("{:016X}", generated_tid)),
                        dlc_name,
                        dlc_desc,
                        QStringLiteral("1.0.0"),
                        QStringLiteral("0"),
                        tr("✓ В файле / Активно"),
                    });
                }
            }
        } else if (p.type == FileSys::PatchType::Update) {
            seen_update_ids.insert(p.title_id);
            total_updates++;
            u32 update_num = provider.GetEntryVersion(FileSys::GetUpdateTitleID(title_id)).value_or(0);
            if (update_num == 0 && update_internal_ver_str != QStringLiteral("0")) {
                update_num = update_internal_ver_str.toUInt();
            }
            const QString upd_name = tr("Пакет обновления игры");
            const QString upd_desc = tr("Накопительный пакет обновлений. Включает оптимизацию производительности, исправления ошибок и актуальные игровые данные.");

            rows.push_back({
                tr("Обновление"),
                QString::fromStdString(fmt::format("{:016X}", p.title_id)),
                upd_name,
                upd_desc,
                update_ver_str,
                update_num > 0 ? QString::number(update_num) : update_internal_ver_str,
                p.enabled ? tr("✓ В файле / Активно") : tr("Отключено"),
            });
        } else if (p.type == FileSys::PatchType::Mod) {
            total_mods++;
            rows.push_back({
                tr("Мод"),
                QString::fromStdString(fmt::format("{:016X}", p.title_id)),
                QString::fromStdString(p.name),
                tr("Пользовательская модификация игры (LayeredFS)"),
                QStringLiteral("-"),
                QStringLiteral("-"),
                p.enabled ? tr("✓ Установлен") : tr("Отключен"),
            });
        }
    }

    // 3. Fallback scan for DLCs and Updates directly inside the running file / container if not registered in provider
    if (!m_current_addons_game_path.empty()) {
        const auto game_vfs = Core::GetGameFileFromPath(QtCommon::vfs, m_current_addons_game_path);
        if (game_vfs) {
            const auto nsp = std::make_shared<FileSys::NSP>(game_vfs);
            if (nsp && nsp->GetStatus() == Loader::ResultStatus::Success) {
                for (const auto& [nca_tid, nca_map] : nsp->GetNCAs()) {
                    // Embedded Update NCA check (ends in 0x800)
                    if ((nca_tid & 0x800) != 0 && (nca_tid & 0xFFFFFFFFFFFFF000) == (title_id & 0xFFFFFFFFFFFFF000)) {
                        if (seen_update_ids.find(nca_tid) == seen_update_ids.end()) {
                            seen_update_ids.insert(nca_tid);
                            total_updates++;

                            const QString upd_name = tr("Пакет обновления игры");
                            const QString upd_desc = tr("Накопительный пакет обновлений, вшитый в файл игры. Включает исправления и игровые ресурсы.");

                            rows.push_back({
                                tr("Обновление"),
                                QString::fromStdString(fmt::format("{:016X}", nca_tid)),
                                upd_name,
                                upd_desc,
                                update_ver_str,
                                update_internal_ver_str,
                                tr("✓ В файле / Активно"),
                            });
                        }
                    }
                    // Embedded DLC NCA check
                    else if (((nca_tid & 0xFFFFFFFFFFFFF000) == (title_id & 0xFFFFFFFFFFFFF000) ||
                         (nca_tid >= title_id + 1 && nca_tid < title_id + 0x2000)) &&
                        nca_tid != title_id) {
                        if (seen_dlc_ids.find(nca_tid) == seen_dlc_ids.end()) {
                            seen_dlc_ids.insert(nca_tid);
                            total_dlcs++;

                            const QString dlc_name = resolve_dlc_name(nca_tid, total_dlcs);
                            const QString dlc_desc = resolve_dlc_desc(nca_tid, total_dlcs);

                            rows.push_back({
                                tr("Дополнение"),
                                QString::fromStdString(fmt::format("{:016X}", nca_tid)),
                                dlc_name,
                                dlc_desc,
                                QStringLiteral("1.0.0"),
                                QStringLiteral("0"),
                                tr("✓ В файле / Активно"),
                            });
                        }
                    }
                }
            }
        }

        // Check filename tag +1U or (1G+1U)
        static const QRegularExpression fn_u_tag{QStringLiteral(R"(\+([0-9]+)U\b)"), QRegularExpression::CaseInsensitiveOption};
        const auto um = fn_u_tag.match(QString::fromStdString(m_current_addons_game_path));
        if (um.hasMatch() && seen_update_ids.empty()) {
            const u64 upd_tid = FileSys::GetUpdateTitleID(title_id);
            seen_update_ids.insert(upd_tid);
            total_updates++;

            const QString upd_name = tr("Пакет обновления игры");
            const QString upd_desc = tr("Накопительный пакет обновлений, вшитый в файл игры. Включает исправления и игровые ресурсы.");

            rows.push_back({
                tr("Обновление"),
                QString::fromStdString(fmt::format("{:016X}", upd_tid)),
                upd_name,
                upd_desc,
                update_ver_str,
                update_internal_ver_str,
                tr("✓ В файле / Активно"),
            });
        }

        // Check filename tags like +1D, +2D, (1G+1U+1D)
        static const QRegularExpression fn_dlc_tag{QStringLiteral(R"(\+([0-9]+)D\b)"), QRegularExpression::CaseInsensitiveOption};
        const auto dm = fn_dlc_tag.match(QString::fromStdString(m_current_addons_game_path));
        if (dm.hasMatch() && dm.hasCaptured(1)) {
            const int tag_count = dm.captured(1).toInt();
            for (int i = 1; i <= tag_count; ++i) {
                const u64 generated_tid = (title_id & 0xFFFFFFFFFFFFF000) | (0x1000 + i);
                if (seen_dlc_ids.find(generated_tid) == seen_dlc_ids.end()) {
                    seen_dlc_ids.insert(generated_tid);
                    total_dlcs++;

                    const QString dlc_name = resolve_dlc_name(generated_tid, i);
                    const QString dlc_desc = resolve_dlc_desc(generated_tid, i);

                    rows.push_back({
                        tr("Дополнение"),
                        QString::fromStdString(fmt::format("{:016X}", generated_tid)),
                        dlc_name,
                        dlc_desc,
                        QStringLiteral("1.0.0"),
                        QStringLiteral("0"),
                        tr("✓ В файле / Активно"),
                    });
                }
            }
        }
    }

    auto format_dlc_count_ru = [](int count) -> QString {
        const int mod10 = count % 10;
        const int mod100 = count % 100;
        if (mod100 >= 11 && mod100 <= 19) {
            return QStringLiteral("%1 дополнений").arg(count);
        }
        if (mod10 == 1) {
            return QStringLiteral("%1 дополнение").arg(count);
        }
        if (mod10 >= 2 && mod10 <= 4) {
            return QStringLiteral("%1 дополнения").arg(count);
        }
        return QStringLiteral("%1 дополнений").arg(count);
    };

    const int tinfoil_dlc_count = TitleDB::TitleDatabase::Instance().GetDlcCount(title_id);
    const QString tinfoil_badge_text = tinfoil_dlc_count > 0 ? format_dlc_count_ru(tinfoil_dlc_count) : (base_tdb.has_value() ? QStringLiteral("0 дополнений") : tr("Не найдено"));

    auto* header_card = new QWidget(&dlg);
    header_card->setStyleSheet(QStringLiteral("background-color: #121826; border: 1px solid #1e283d; border-radius: 8px; padding: 6px;"));
    auto* header_layout = new QVBoxLayout(header_card);
    header_layout->setContentsMargins(14, 10, 14, 10);
    header_layout->setSpacing(6);

    auto* title_label = new QLabel(tr("<h2 style='margin:0; color:#00e5ff;'>🎮 %1</h2>").arg(full_display_name), header_card);
    auto* badges_label = new QLabel(tr(
        "<span style='background:#1b2438; color:#94a3b8; padding:4px 9px; border-radius:5px; font-weight:bold;'>ID: %1</span> &nbsp; "
        "<span style='background:#143324; color:#00e676; padding:4px 9px; border-radius:5px; font-weight:bold;'>📦 Дополнений в файле: %2</span> &nbsp; "
        "<span style='background:#142738; color:#00e5ff; padding:4px 9px; border-radius:5px; font-weight:bold;'>🗃️ В базе Tinfoil: %3</span> &nbsp; "
        "<span style='background:#2d2915; color:#ffca28; padding:4px 9px; border-radius:5px; font-weight:bold;'>🆙 Версия игры: %4</span> &nbsp; "
        "<span style='background:#2e183a; color:#e040fb; padding:4px 9px; border-radius:5px; font-weight:bold;'>⚡ Модов: %5</span>")
        .arg(tid_str, QString::number(total_dlcs), tinfoil_badge_text, update_ver_str, QString::number(total_mods)), header_card);

    header_layout->addWidget(title_label);
    header_layout->addWidget(badges_label);
    main_layout->addWidget(header_card);

    auto* search_box = new QLineEdit(&dlg);
    search_box->setPlaceholderText(tr("🔍 Поиск по названию, описанию или Title ID..."));
    search_box->setClearButtonEnabled(true);
    main_layout->addWidget(search_box);

    auto* table = new QTableWidget(&dlg);
    table->setColumnCount(8);
    table->setHorizontalHeaderLabels({
        tr("№"), tr("Тип"), tr("Title ID"), tr("Полное название"),
        tr("Описание"), tr("Версия"), tr("Внутренняя версия"), tr("Статус")
    });
    table->setWordWrap(true);
    table->setTextElideMode(Qt::ElideNone);
    table->setSelectionBehavior(QAbstractItemView::SelectItems);
    table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setContextMenuPolicy(Qt::CustomContextMenu);

    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
    table->setColumnWidth(3, 400);
    table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Interactive);
    table->setColumnWidth(5, 95);
    table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Interactive);
    table->setColumnWidth(6, 140);
    table->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    main_layout->addWidget(table);

    int row_idx = 0;
    QStringList copy_lines;
    copy_lines << QStringLiteral("============================================================");
    copy_lines << QStringLiteral("STORM EDEN — Список дополнений и обновлений");
    copy_lines << QStringLiteral("Игра: %1 (ID: %2)").arg(full_display_name, tid_str);
    copy_lines << QStringLiteral("Версия игры: %1 | Дополнений в файле: %2 | В базе Tinfoil: %3 | Модов: %4")
        .arg(update_ver_str, QString::number(total_dlcs), QString::number(tinfoil_dlc_count), QString::number(total_mods));
    copy_lines << QStringLiteral("------------------------------------------------------------");

    for (const auto& r : rows) {
        table->insertRow(row_idx);

        auto* item0 = new QTableWidgetItem(QString::number(row_idx + 1));
        item0->setTextAlignment(Qt::AlignCenter);
        table->setItem(row_idx, 0, item0);

        auto* item1 = new QTableWidgetItem(r.type);
        item1->setTextAlignment(Qt::AlignCenter);
        table->setItem(row_idx, 1, item1);

        auto* item2 = new QTableWidgetItem(r.tid);
        item2->setTextAlignment(Qt::AlignCenter);
        table->setItem(row_idx, 2, item2);

        table->setItem(row_idx, 3, new QTableWidgetItem(r.name));
        table->setItem(row_idx, 4, new QTableWidgetItem(r.desc));

        auto* item5 = new QTableWidgetItem(r.ver);
        item5->setTextAlignment(Qt::AlignCenter);
        table->setItem(row_idx, 5, item5);

        auto* item6 = new QTableWidgetItem(r.internal_ver);
        item6->setTextAlignment(Qt::AlignCenter);
        table->setItem(row_idx, 6, item6);

        auto* item7 = new QTableWidgetItem(r.status);
        item7->setTextAlignment(Qt::AlignCenter);
        table->setItem(row_idx, 7, item7);

        copy_lines << QStringLiteral("%1. [%2] %3 — %4 | %5 | Версия: %6 (Внутр: %7) | %8")
            .arg(QString::number(row_idx + 1), r.type, r.tid, r.name, r.desc, r.ver, r.internal_ver, r.status);
        row_idx++;
    }

    if (row_idx == 0) {
        table->insertRow(0);
        for (int c = 0; c < 8; ++c) {
            table->setItem(0, c, new QTableWidgetItem(c == 3 ? tr("Дополнения или обновления не обнаружены.") : QStringLiteral("-")));
        }
        copy_lines << tr("Дополнения не найдены.");
    } else {
        copy_lines << QStringLiteral("============================================================");
        copy_lines << QStringLiteral("Всего элементов: %1").arg(row_idx);
    }

    connect(search_box, &QLineEdit::textChanged, [table](const QString& text) {
        for (int r = 0; r < table->rowCount(); ++r) {
            bool match = false;
            for (int c = 0; c < table->columnCount(); ++c) {
                auto* item = table->item(r, c);
                if (item && item->text().contains(text, Qt::CaseInsensitive)) {
                    match = true;
                    break;
                }
            }
            table->setRowHidden(r, !match);
        }
    });

    // Custom Context Menu for individual field copy
    connect(table, &QTableWidget::customContextMenuRequested, [table](const QPoint& pos) {
        QTableWidgetItem* item = table->itemAt(pos);
        if (!item) return;
        QMenu menu(table);
        menu.addAction(QObject::tr("📋 Копировать ячейку"), [item] {
            QGuiApplication::clipboard()->setText(item->text());
        });
        const int row = item->row();
        auto* tid_item = table->item(row, 2);
        if (tid_item) {
            menu.addAction(QObject::tr("📋 Копировать Title ID"), [tid_item] {
                QGuiApplication::clipboard()->setText(tid_item->text());
            });
        }
        auto* name_item = table->item(row, 3);
        if (name_item) {
            menu.addAction(QObject::tr("📋 Копировать название"), [name_item] {
                QGuiApplication::clipboard()->setText(name_item->text());
            });
        }
        auto* desc_item = table->item(row, 4);
        if (desc_item) {
            menu.addAction(QObject::tr("📋 Копировать описание"), [desc_item] {
                QGuiApplication::clipboard()->setText(desc_item->text());
            });
        }
        menu.addSeparator();
        menu.addAction(QObject::tr("📋 Копировать строку целиком"), [table, row] {
            QStringList cell_texts;
            for (int col = 0; col < table->columnCount(); ++col) {
                auto* cell = table->item(row, col);
                if (cell) cell_texts << cell->text();
            }
            QGuiApplication::clipboard()->setText(cell_texts.join(QStringLiteral(" | ")));
        });
        menu.exec(table->viewport()->mapToGlobal(pos));
    });

    // Ctrl+C Shortcut for cell selection
    auto* copy_shortcut = new QShortcut(QKeySequence::Copy, table);
    connect(copy_shortcut, &QShortcut::activated, [table] {
        const auto selected_items = table->selectedItems();
        if (selected_items.isEmpty()) return;
        if (selected_items.size() == 1) {
            QGuiApplication::clipboard()->setText(selected_items.first()->text());
            return;
        }
        QMap<int, QMap<int, QString>> row_col_map;
        for (auto* item : selected_items) {
            row_col_map[item->row()][item->column()] = item->text();
        }
        QStringList rows_str;
        for (auto row_it = row_col_map.begin(); row_it != row_col_map.end(); ++row_it) {
            QStringList cols_str;
            for (auto col_it = row_it.value().begin(); col_it != row_it.value().end(); ++col_it) {
                cols_str << col_it.value();
            }
            rows_str << cols_str.join(QLatin1Char('\t'));
        }
        QGuiApplication::clipboard()->setText(rows_str.join(QLatin1Char('\n')));
    });

    main_layout->addWidget(table);

    auto* btn_layout = new QHBoxLayout();
    auto* copy_btn = new QPushButton(tr("📋 Копировать список"), &dlg);
    copy_btn->setObjectName(QStringLiteral("CopyBtn"));
    connect(copy_btn, &QPushButton::clicked, [copy_lines, copy_btn] {
        QGuiApplication::clipboard()->setText(copy_lines.join(QLatin1Char('\n')));
        copy_btn->setText(QCoreApplication::translate("MainWindow", "✅ Скопировано в буфер обмена!"));
        QTimer::singleShot(2000, [copy_btn] {
            if (copy_btn) copy_btn->setText(QCoreApplication::translate("MainWindow", "📋 Копировать список"));
        });
    });

    auto* manage_btn = new QPushButton(tr("⚙️ Управление дополнениями..."), &dlg);
    connect(manage_btn, &QPushButton::clicked, [this, title_id, &dlg] {
        dlg.accept();
        OpenPerGameConfiguration(title_id, m_current_addons_game_path);
    });

    auto* close_btn = new QPushButton(tr("Закрыть"), &dlg);
    connect(close_btn, &QPushButton::clicked, &dlg, &QDialog::accept);

    btn_layout->addWidget(copy_btn);
    btn_layout->addWidget(manage_btn);
    btn_layout->addStretch();
    btn_layout->addWidget(close_btn);

    main_layout->addLayout(btn_layout);

    dlg.exec();
}

bool MainWindow::ConfirmChangeGame() {
    if (QtCommon::emu_thread == nullptr)
        return true;

    // Use custom question to link controller navigation
    return question(
        this, QStringLiteral("STORM EDEN"),
        tr("Вы действительно хотите остановить эмуляцию?\nВсе несохраненные данные будут потеряны."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
}

bool MainWindow::ConfirmForceLockedExit() {
    if (QtCommon::emu_thread == nullptr)
        return true;

    const auto text = tr("Запущенное приложение запросило запрет на выход из STORM EDEN.\n\n"
                         "Вы действительно хотите принудительно завершить работу и выйти?");

    return question(this, QStringLiteral("STORM EDEN"), text);
}

void MainWindow::RequestGameExit() {
    if (!QtCommon::system->IsPoweredOn())
        return;

    QtCommon::system->SetExitRequested(true);
    QtCommon::system->GetAppletManager().RequestExit();
}

void MainWindow::filterBarSetChecked(bool state) {
    ui->action_Show_Filter_Bar->setChecked(state);
    emit(OnToggleFilterBar());
}

static void AdjustLinkColor() {
    QPalette new_pal(qApp->palette());

    if (UISettings::IsDarkTheme())
        new_pal.setColor(QPalette::Link, QColor(0, 190, 255, 255));
    else
        new_pal.setColor(QPalette::Link, QColor(0, 140, 200, 255));

    if (qApp->palette().color(QPalette::Link) != new_pal.color(QPalette::Link))
        qApp->setPalette(new_pal);
}

void MainWindow::UpdateUITheme() {
    const QString default_theme = QString::fromUtf8(UISettings::themes[size_t(UISettings::default_theme)].second);
    QString current_theme = QString::fromStdString(UISettings::values.theme);

    if (current_theme.isEmpty())
        current_theme = default_theme;

#ifdef _WIN32
    QIcon::setThemeName(current_theme);
    AdjustLinkColor();
#else
    QIcon::setThemeName(current_theme);
    QIcon::setThemeSearchPaths(QStringList(QStringLiteral(":/icons")));
    AdjustLinkColor();
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
        const QString stylesheet = ts.readAll();
        qApp->setStyleSheet(stylesheet);
        setStyleSheet(stylesheet);
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

    if (loaded)
        qApp->installTranslator(&translator);
    else
        UISettings::values.language = std::string("en");
}

void MainWindow::OnLanguageChanged(const QString& locale) {
    if (UISettings::values.language.GetValue() != std::string("en"))
        qApp->removeTranslator(&translator);

    QList<QAction*> actions = game_size_actions->actions();
    for (size_t i = 0; i < default_game_icon_sizes.size(); i++) {
        actions.at(i)->setText(GetTranslatedGameIconSize(i));
    }

    UISettings::values.language = locale.toStdString();
    LoadTranslation();
    ui->retranslateUi(this);
    multiplayer_state->retranslateUi();
    UpdateWindowTitle();
}

void MainWindow::SetDiscordEnabled([[maybe_unused]] bool state) {
#ifdef USE_DISCORD_PRESENCE
    if (state)
        discord_rpc = std::make_unique<DiscordRPC::DiscordImpl>(*QtCommon::system);
    else
        discord_rpc = std::make_unique<DiscordRPC::NullImpl>();
#else
    discord_rpc = std::make_unique<DiscordRPC::NullImpl>();
#endif
    discord_rpc->Update();
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

#ifdef main
#undef main
#endif

#if !defined(QT_STATICPLUGIN) || defined(__APPLE__)
#define VMA_IMPLEMENTATION
#include "video_core/vulkan_common/vma.h"
#endif
