# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(yuzu_room_standalone yuzu_room_standalone.cpp)

set_target_properties(yuzu_room_standalone PROPERTIES OUTPUT_NAME "storm_eden-room")

target_link_libraries(yuzu_room_standalone PRIVATE yuzu-room)

if(UNIX AND NOT APPLE)
    install(TARGETS yuzu_room_standalone RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
endif()

if(WIN32)
    if(MSVC)
        target_link_libraries(yuzu_room_standalone PRIVATE delayimp.lib)
        set_target_properties(yuzu_room_standalone PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:CONSOLE,6.01 /DELAYLOAD:MSVCP140_ATOMIC_WAIT.dll")
        set_target_properties(yuzu_room_standalone PROPERTIES LINK_FLAGS_RELWITHDEBINFO "/SUBSYSTEM:CONSOLE,6.01 /DELAYLOAD:MSVCP140_ATOMIC_WAIT.dll")
    elseif(MINGW)
        set_target_properties(yuzu_room_standalone PROPERTIES LINK_FLAGS_RELEASE "-Wl,--subsystem,console")
    endif()
endif()

create_target_directory_groups(yuzu_room_standalone)
