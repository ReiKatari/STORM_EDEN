# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2017 Citra Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

add_library(eden-room STATIC EXCLUDE_FROM_ALL
    eden_room.cpp
    eden_room.h
    eden_room.rc
)

target_link_libraries(eden-room PRIVATE common network)
if (ENABLE_WEB_SERVICE)
    target_compile_definitions(eden-room PRIVATE ENABLE_WEB_SERVICE)
    target_link_libraries(eden-room PRIVATE web_service)
endif()

target_link_libraries(eden-room PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto)

if (MSVC)
    target_link_libraries(eden-room PRIVATE getopt)
endif()
target_link_libraries(eden-room PRIVATE ${PLATFORM_LIBRARIES} Threads::Threads)

create_target_directory_groups(eden-room)
