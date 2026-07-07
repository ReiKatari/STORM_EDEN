// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef _WIN32
#include <windows.h>
#include <wincon.h>
#include <cstdio>
#include <iostream>
#endif

#include "common/logging.h"
#include "eden/debugger/console.h"
#include "qt_common/config/uisettings.h"

namespace Debugger {
void ToggleConsole() {
    static bool console_shown = false;
    const bool want_shown = UISettings::values.show_console.GetValue();

    if (console_shown == want_shown) {
        return;
    }
    console_shown = want_shown;

    using namespace Common::Log;

#if defined(_WIN32) && !defined(_DEBUG)
    if (want_shown) {
        // Allocate a console window if one doesn't exist yet.
        // This must happen BEFORE we enable the logging backend,
        // otherwise the logger thread will try to write to a
        // non-existent console and crash.
        if (GetConsoleWindow() == nullptr) {
            if (!AllocConsole()) {
                // AllocConsole failed — don't enable the backend.
                console_shown = false;
                return;
            }
            // Configure the new console
            SetConsoleOutputCP(65001);
            SetConsoleTitleW(L"STORM EDEN - Console Log");

            // Reopen standard streams to avoid C runtime crashes on console writes
            FILE* fp;
            freopen_s(&fp, "CONOUT$", "w", stdout);
            freopen_s(&fp, "CONOUT$", "w", stderr);
            freopen_s(&fp, "CONIN$", "r", stdin);
            std::ios::sync_with_stdio();

            // Enable VT100 escape sequences for colored output
            HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
            if (hOut != nullptr && hOut != INVALID_HANDLE_VALUE) {
                DWORD mode = 0;
                GetConsoleMode(hOut, &mode);
                SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
            }
        }
        // Console is ready — show it and enable logging
        HWND hw = GetConsoleWindow();
        if (hw) {
            ShowWindow(hw, SW_SHOW);
        }
        SetColorConsoleBackendEnabled(true);
    } else {
        // Disable logging FIRST, then hide the window.
        // This prevents the logger thread from writing while
        // we're hiding the console.
        SetColorConsoleBackendEnabled(false);
        HWND hw = GetConsoleWindow();
        if (hw) {
            ShowWindow(hw, SW_HIDE);
        }
    }
#else
    SetColorConsoleBackendEnabled(want_shown);
#endif
}
} // namespace Debugger
