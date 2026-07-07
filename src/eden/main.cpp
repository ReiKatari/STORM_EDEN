// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QProxyStyle>
#include <QStyleOptionMenuItem>
#include <QPainter>
#include <QtPlugin>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
Q_IMPORT_PLUGIN(QGifPlugin)
#else
Q_IMPORT_PLUGIN(QGifPlugin)
#endif
#include "startup_checks.h"

#if YUZU_ROOM
#include <cstring>
#include "dedicated_room/eden_room.h"
#endif

#include <common/detached_tasks.h>

#ifdef __unix__
#include "qt_common/gui_settings.h"
#endif

#include "main_window.h"

class MenuProxyStyle : public QProxyStyle {
public:
    using QProxyStyle::QProxyStyle;

    void drawControl(ControlElement element, const QStyleOption* option, QPainter* painter,
                     const QWidget* widget) const override {
        if (element == CE_MenuItem) {
            if (const auto* menu_item = qstyleoption_cast<const QStyleOptionMenuItem*>(option)) {
                QStyleOptionMenuItem my_menu_item = *menu_item;
                my_menu_item.text = QString();
                QProxyStyle::drawControl(element, &my_menu_item, painter, widget);

                QRect rect = menu_item->rect;
                QString original_text = menu_item->text;
                QString text = original_text;
                QString shortcut;
                int tab_index = original_text.indexOf(QLatin1Char('\t'));
                if (tab_index != -1) {
                    text = original_text.left(tab_index);
                    shortcut = original_text.mid(tab_index + 1);
                }

                int left_padding = 10;
                if (menu_item->maxIconWidth > 0) {
                    left_padding += menu_item->maxIconWidth + 10;
                } else {
                    left_padding += 20;
                }
                int right_padding = 20;

                QRect text_rect = rect.adjusted(left_padding, 0, -right_padding, 0);

                painter->save();
                painter->setRenderHint(QPainter::TextAntialiasing);

                QColor text_color;
                if (!(menu_item->state & QStyle::State_Enabled)) {
                    text_color = QColor(75, 85, 99); // #4b5563
                } else if (menu_item->state & QStyle::State_Selected) {
                    text_color = QColor(0, 0, 0); // black
                } else {
                    text_color = QColor(255, 255, 255); // white
                }
                painter->setPen(text_color);

                QFont font = painter->font();
                painter->setFont(font);
                painter->drawText(text_rect, Qt::AlignLeft | Qt::AlignVCenter, text);

                if (!shortcut.isEmpty()) {
                    QFont bold_font = font;
                    bold_font.setBold(true);
                    painter->setFont(bold_font);
                    painter->drawText(text_rect, Qt::AlignRight | Qt::AlignVCenter, shortcut);
                }

                painter->restore();
                return;
            }
        }
        QProxyStyle::drawControl(element, option, painter, widget);
    }
};

#ifdef _WIN32
#include <QScreen>

static void OverrideWindowsFont() {
    QFont modern_font(QStringLiteral("Century Gothic"), 9, QFont::Normal);
    QApplication::setFont(modern_font);
}
#endif

static Qt::HighDpiScaleFactorRoundingPolicy GetHighDpiRoundingPolicy() {
#ifdef _WIN32
    // For Windows, we want to avoid scaling artifacts on fractional scaling ratios.
    // This is done by setting the optimal scaling policy for the primary screen.

    // Create a temporary QApplication.
    int temp_argc = 0;
    char** temp_argv = nullptr;
    QApplication temp{temp_argc, temp_argv};

    // Get the current screen geometry.
    const QScreen* primary_screen = QGuiApplication::primaryScreen();
    if (primary_screen == nullptr) {
        return Qt::HighDpiScaleFactorRoundingPolicy::PassThrough;
    }

    const QRect screen_rect = primary_screen->geometry();
    const qreal real_ratio = primary_screen->devicePixelRatio();
    const qreal real_width = std::trunc(screen_rect.width() * real_ratio);
    const qreal real_height = std::trunc(screen_rect.height() * real_ratio);

    // Recommended minimum width and height for proper window fit.
    // Any screen with a lower resolution than this will still have a scale of 1.
    constexpr qreal minimum_width = 1350.0;
    constexpr qreal minimum_height = 900.0;

    const qreal width_ratio = std::max(1.0, real_width / minimum_width);
    const qreal height_ratio = std::max(1.0, real_height / minimum_height);

    // Get the lower of the 2 ratios and truncate, this is the maximum integer scale.
    const qreal max_ratio = std::trunc(std::min(width_ratio, height_ratio));
    return max_ratio > real_ratio ? Qt::HighDpiScaleFactorRoundingPolicy::Round
                                  : Qt::HighDpiScaleFactorRoundingPolicy::Floor;
#else
    // Other OSes should be better than Windows at fractional scaling.
    return Qt::HighDpiScaleFactorRoundingPolicy::PassThrough;
#endif
}

#ifdef _WIN32
#include <windows.h>
#include <fstream>
LONG WINAPI GlobalCrashHandler(EXCEPTION_POINTERS* ExceptionInfo) {
    std::ofstream os("crash_dump_global.txt", std::ios::app);
    os << "HARD CRASH DETECTED!\n";
    os << "Exception Code: " << std::hex << ExceptionInfo->ExceptionRecord->ExceptionCode << "\n";
    os << "Exception Address: " << ExceptionInfo->ExceptionRecord->ExceptionAddress << "\n";
    os.flush();
    return EXCEPTION_CONTINUE_SEARCH; // Let Windows still show the WER dialog if configured
}
#endif



int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetUnhandledExceptionFilter(GlobalCrashHandler);
#endif


#if YUZU_ROOM
    bool launch_room = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--room") == 0) {
            launch_room = true;
        }
    }

    if (launch_room) {
        LaunchRoom(argc, argv, true);
        return 0;
    }
#endif


    bool has_broken_vulkan = false;
    bool is_child = false;
    if (CheckEnvVars(&is_child)) {
        return 0;
    }


    if (StartupChecks(argv[0], &has_broken_vulkan,
                      Settings::values.perform_vulkan_check.GetValue())) {
        return 0;
    }


#ifdef YUZU_CRASH_DUMPS
    Breakpad::InstallCrashHandler();
#endif

    Common::DetachedTasks detached_tasks;

    // Init settings params
    QCoreApplication::setOrganizationName(QStringLiteral("eden"));
    QCoreApplication::setApplicationName(QStringLiteral("eden"));

#ifdef _WIN32
    // Increases the maximum open file limit to 8192
    _setmaxstdio(8192);
#elif defined(__APPLE__)
    // If you start a bundle (binary) on OSX without the Terminal, the working directory is "/".
    // But since we require the working directory to be the executable path for the location of
    // the user folder in the Qt Frontend, we need to cd into that working directory
    const auto bin_path = Common::FS::GetBundleDirectory() / "..";
    chdir(Common::FS::PathToUTF8String(bin_path).c_str());
#endif

#ifdef __unix__
    // Set the DISPLAY variable in order to open web browsers
    // TODO (lat9nq): Find a better solution for AppImages to start external applications
    if (QString::fromLocal8Bit(qgetenv("DISPLAY")).isEmpty()) {
        qputenv("DISPLAY", ":0");
    }

    if (GraphicsBackend::GetForceX11() && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "xcb");

    // Fix the Wayland appId. This needs to match the name of the .desktop file without the .desktop
    // suffix.
    QGuiApplication::setDesktopFileName(QStringLiteral("dev.eden_emu.eden"));
#endif

    auto rounding_policy = GetHighDpiRoundingPolicy();
    QApplication::setHighDpiScaleFactorRoundingPolicy(rounding_policy);

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Disables the "?" button on all dialogs. Disabled by default on Qt6.
    QCoreApplication::setAttribute(Qt::AA_DisableWindowContextHelpButton);
#endif

    // Enables the core to make the qt created contexts current on std::threads
    QCoreApplication::setAttribute(Qt::AA_DontCheckOpenGLContextThreadAffinity);

#ifdef _WIN32
    QApplication::setStyle(QStringLiteral("windowsvista"));
#endif

    QApplication app(argc, argv);
    app.setStyle(new MenuProxyStyle(app.style()));

#ifdef _WIN32
    OverrideWindowsFont();
#endif

#ifdef _WIN32
#endif

    // Workaround for QTBUG-85409, for Suzhou numerals the number 1 is actually \u3021
    // so we can see if we get \u3008 instead
    // TL;DR all other number formats are consecutive in unicode code points
    // This bug is fixed in Qt6, specifically 6.0.0-alpha1
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    const QLocale locale = QLocale::system();
    if (QStringLiteral("\u3008") == locale.toString(1)) {
        QLocale::setDefault(QLocale::system().name());
    }
#endif

    // Qt changes the locale and causes issues in float conversion using std::to_string() when
    // generating shaders
    setlocale(LC_ALL, "C");

    MainWindow main_window{has_broken_vulkan};
    // After settings have been loaded by GMainWindow, apply the filter
    main_window.show();

    app.connect(&app, &QGuiApplication::applicationStateChanged, &main_window,
                &MainWindow::OnAppFocusStateChanged);

    int result = app.exec();
    detached_tasks.WaitForAllTasks();
    return result;
}
