// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QProxyStyle>
#include <QStyleOptionMenuItem>
#include <QPainter>
#include <QtPlugin>
#include <exception>
#include <csignal>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#include <psapi.h>
#include <SDL3/SDL.h>
#pragma comment(lib, "dbghelp.lib")
#endif

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
#include <psapi.h>
#pragma comment(lib, "psapi.lib")

LONG WINAPI GlobalCrashHandler(EXCEPTION_POINTERS* ExceptionInfo) {
    // Write to the exe directory so we can always find the file
    char exe_path[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exe_path, MAX_PATH);
    std::string crash_file(exe_path);
    auto last_sep = crash_file.find_last_of("\\/");
    if (last_sep != std::string::npos) {
        crash_file = crash_file.substr(0, last_sep + 1);
    }
    crash_file += "crash_dump_global.txt";

    std::ofstream os(crash_file, std::ios::app);
    os << "=== HARD CRASH DETECTED ===\n";
    os << "Thread ID: " << std::dec << GetCurrentThreadId() << "\n";
    os << "Exception Code: 0x" << std::hex << ExceptionInfo->ExceptionRecord->ExceptionCode << "\n";
    os << "Exception Address: 0x" << (uintptr_t)ExceptionInfo->ExceptionRecord->ExceptionAddress << "\n";

    // For Access Violations, show what address was being accessed and how
    if (ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
        ExceptionInfo->ExceptionRecord->NumberParameters >= 2) {
        const char* av_type = "unknown";
        switch (ExceptionInfo->ExceptionRecord->ExceptionInformation[0]) {
            case 0: av_type = "READ"; break;
            case 1: av_type = "WRITE"; break;
            case 8: av_type = "DEP"; break;
        }
        os << "AV Type: " << av_type << "\n";
        os << "AV Target Address: 0x" << std::hex
           << ExceptionInfo->ExceptionRecord->ExceptionInformation[1] << "\n";
    }

    // Dump key registers from crash context
#ifdef _M_X64
    if (ExceptionInfo->ContextRecord) {
        auto* ctx = ExceptionInfo->ContextRecord;
        os << "Registers:\n";
        os << "  RIP=0x" << std::hex << ctx->Rip << "  RSP=0x" << ctx->Rsp << "  RBP=0x" << ctx->Rbp << "\n";
        os << "  RAX=0x" << ctx->Rax << "  RBX=0x" << ctx->Rbx << "  RCX=0x" << ctx->Rcx << "\n";
        os << "  RDX=0x" << ctx->Rdx << "  RSI=0x" << ctx->Rsi << "  RDI=0x" << ctx->Rdi << "\n";
        os << "  R8=0x"  << ctx->R8  << "  R9=0x"  << ctx->R9  << "  R10=0x" << ctx->R10 << "\n";
        os << "  R11=0x" << ctx->R11 << "  R12=0x" << ctx->R12 << "  R13=0x" << ctx->R13 << "\n";
        os << "  R14=0x" << ctx->R14 << "  R15=0x" << ctx->R15 << "\n";
    }
#endif

    // Walk the stack using CaptureStackBackTrace
    void* stack[64] = {};
    USHORT frames = CaptureStackBackTrace(0, 64, stack, nullptr);
    os << "Stack frames captured: " << std::dec << frames << "\n";

    // Enumerate loaded modules to resolve addresses to module+offset
    HANDLE process = GetCurrentProcess();
    HMODULE modules[256] = {};
    DWORD needed = 0;
    EnumProcessModules(process, modules, sizeof(modules), &needed);
    DWORD num_modules = needed / sizeof(HMODULE);

    for (USHORT i = 0; i < frames; i++) {
        uintptr_t addr = (uintptr_t)stack[i];
        // Find which module this address belongs to
        const char* mod_name = "???";
        uintptr_t mod_offset = addr;
        char mod_filename[MAX_PATH] = {};
        for (DWORD m = 0; m < num_modules; m++) {
            MODULEINFO mi = {};
            GetModuleInformation(process, modules[m], &mi, sizeof(mi));
            uintptr_t base = (uintptr_t)mi.lpBaseOfDll;
            if (addr >= base && addr < base + mi.SizeOfImage) {
                GetModuleFileNameA(modules[m], mod_filename, MAX_PATH);
                // Extract just the filename
                const char* p = strrchr(mod_filename, '\\');
                mod_name = p ? p + 1 : mod_filename;
                mod_offset = addr - base;
                break;
            }
        }
        os << "  [" << std::dec << i << "] " << mod_name << " + 0x" << std::hex << mod_offset << "\n";
    }

    os << "=== END CRASH ===\n\n";
    os.flush();

    LOG_CRITICAL(Frontend, "=== HARD CRASH DETECTED ===");
    LOG_CRITICAL(Frontend, "Exception Code: 0x{:x}, Address: 0x{:x}", ExceptionInfo->ExceptionRecord->ExceptionCode, (uintptr_t)ExceptionInfo->ExceptionRecord->ExceptionAddress);
    STORM_TRACE("CRASH DETECTED in GlobalCrashHandler: Exception Code 0x{:x}, Address 0x{:x}", ExceptionInfo->ExceptionRecord->ExceptionCode, (uintptr_t)ExceptionInfo->ExceptionRecord->ExceptionAddress);
    Common::Log::Stop();

    return EXCEPTION_CONTINUE_SEARCH;
}

#endif



int main(int argc, char* argv[]) {
    STORM_TRACE("=== STORM EDEN STARTUP: main() entered ===");
    std::atexit([] {
        STORM_TRACE("=== STORM EDEN SHUTDOWN: atexit() triggered ===");
    });

    // Force software rendering for Qt UI to prevent OpenGL driver crashes (e.g. nvoglv64.dll)
    qputenv("QT_OPENGL", "software");
    qputenv("QT_OPENGL_BUGLIST", ":/disable_gpu");

#ifdef _WIN32
    SetUnhandledExceptionFilter(GlobalCrashHandler);

    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

    std::set_terminate([] {
        char exe_path[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, exe_path, MAX_PATH);
        std::string crash_file(exe_path);
        auto last_sep = crash_file.find_last_of("\\/");
        if (last_sep != std::string::npos) {
            crash_file = crash_file.substr(0, last_sep + 1);
        }
        crash_file += "crash_dump_global.txt";

        std::ofstream os(crash_file, std::ios::app);
        os << "=== std::terminate() CALLED ===\n";

        // Try to get the current exception
        try {
            auto eptr = std::current_exception();
            if (eptr) {
                std::rethrow_exception(eptr);
            } else {
                os << "No active exception (terminate called directly)\n";
            }
        } catch (const std::exception& e) {
            os << "Unhandled std::exception: " << e.what() << "\n";
        } catch (...) {
            os << "Unhandled unknown exception (not std::exception)\n";
        }

        // Capture stack trace
        void* stack[64] = {};
        USHORT frames = CaptureStackBackTrace(0, 64, stack, nullptr);
        os << "Stack frames: " << std::dec << frames << "\n";

        HANDLE process = GetCurrentProcess();
        HMODULE modules[256] = {};
        DWORD needed = 0;
        EnumProcessModules(process, modules, sizeof(modules), &needed);
        DWORD num_modules = needed / sizeof(HMODULE);

        for (USHORT i = 0; i < frames; i++) {
            uintptr_t addr = (uintptr_t)stack[i];
            const char* mod_name = "???";
            uintptr_t mod_offset = addr;
            char mod_filename[MAX_PATH] = {};
            for (DWORD m = 0; m < num_modules; m++) {
                MODULEINFO mi = {};
                GetModuleInformation(process, modules[m], &mi, sizeof(mi));
                uintptr_t base = (uintptr_t)mi.lpBaseOfDll;
                if (addr >= base && addr < base + mi.SizeOfImage) {
                    GetModuleFileNameA(modules[m], mod_filename, MAX_PATH);
                    const char* p = strrchr(mod_filename, '\\');
                    mod_name = p ? p + 1 : mod_filename;
                    mod_offset = addr - base;
                    break;
                }
            }
            os << "  [" << std::dec << i << "] " << mod_name << " + 0x" << std::hex << mod_offset << "\n";
        }

        os << "=== END TERMINATE ===\n\n";
        os.flush();
        LOG_CRITICAL(Frontend, "=== std::terminate CALLED ===");
        STORM_TRACE("CRASH: std::terminate() was invoked directly by C++ runtime!");
        Common::Log::Stop();
        std::abort();
    });
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
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_WGI, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_THREAD, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT_CORRELATE_XINPUT, "0");
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
