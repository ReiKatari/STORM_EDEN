# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

if (WIN32)
    set(EDEN_ROOM_RC "../dedicated_room/eden_room.rc")
else()
    set(EDEN_ROOM_RC "")
endif()

add_executable(eden_room_standalone eden_room_standalone.cpp ${EDEN_ROOM_RC})

set_target_properties(eden_room_standalone PROPERTIES OUTPUT_NAME "eden-room")

target_link_libraries(eden_room_standalone PRIVATE eden-room)

if(UNIX AND NOT APPLE)
    install(TARGETS eden_room_standalone RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
endif()

if (YUZU_STATIC_ROOM)
    target_link_options(eden_room_standalone PRIVATE "-static")
endif()

create_target_directory_groups(eden_room_standalone)
