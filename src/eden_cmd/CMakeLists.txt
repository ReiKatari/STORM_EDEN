# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2018 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

# Credits to Samantas5855 and others for this function.
function(create_resource file output filename)
    # Read hex data from file
    file(READ ${file} filedata HEX)
    # Convert hex data for C compatibility
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," filedata ${filedata})
    # Write data to output file
    set(RESOURCES_DIR "${PROJECT_BINARY_DIR}/dist" PARENT_SCOPE)
    file(WRITE "${PROJECT_BINARY_DIR}/dist/${output}" "const unsigned char ${filename}[] = {${filedata}};\nconst unsigned ${filename}_size = sizeof(${filename});\n")
endfunction()

if (ENABLE_OPENGL)
    list(APPEND OPENGL_SOURCES
        emu_window/emu_window_sdl3_gl.cpp
        emu_window/emu_window_sdl3_gl.h
    )
else()
    set(OPENGL_SOURCES "")
endif()

add_executable(eden-cmd
    emu_window/emu_window_sdl3.cpp
    emu_window/emu_window_sdl3.h
    emu_window/emu_window_sdl3_null.cpp
    emu_window/emu_window_sdl3_null.h
    emu_window/emu_window_sdl3_vk.cpp
    emu_window/emu_window_sdl3_vk.h
    sdl_config.cpp
    sdl_config.h
    eden.cpp
    eden.rc
    ${OPENGL_SOURCES}
)

target_link_libraries(eden-cmd PRIVATE common core input_common frontend_common video_core)
if (ENABLE_OPENGL)
    target_link_libraries(eden-cmd PRIVATE glad)
endif()
if (MSVC)
    target_link_libraries(eden-cmd PRIVATE getopt)
endif()
target_link_libraries(eden-cmd PRIVATE ${PLATFORM_LIBRARIES} Threads::Threads)

create_resource("../../dist/eden.bmp" "eden_cmd/eden_icon.h" "eden_icon")
target_include_directories(eden-cmd PRIVATE ${RESOURCES_DIR})

target_link_libraries(eden-cmd PRIVATE SDL3::SDL3)

if(UNIX AND NOT APPLE)
    install(TARGETS eden-cmd)
endif()

if(WIN32)
    # compile as a win32 gui application instead of a console application
    if(MSVC)
        set_target_properties(eden-cmd PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
    elseif(MINGW)
        set_target_properties(eden-cmd PROPERTIES LINK_FLAGS_RELEASE "-Wl,--subsystem,windows")
    endif()
endif()

create_target_directory_groups(eden-cmd)

# needed for vma
if (NOT MSVC)
    target_compile_options(eden-cmd PRIVATE
        $<$<COMPILE_LANGUAGE:C,CXX>:-Wno-conversion>
        $<$<COMPILE_LANGUAGE:C,CXX>:-Wno-unused-variable>
        $<$<COMPILE_LANGUAGE:C,CXX>:-Wno-unused-parameter>
        $<$<COMPILE_LANGUAGE:C,CXX>:-Wno-missing-field-initializers>)
endif()
