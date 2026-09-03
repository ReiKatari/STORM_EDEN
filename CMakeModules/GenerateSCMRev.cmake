# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2019 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

# generate git/build information
include(GetSCMRev)

function(get_timestamp _var)
    string(TIMESTAMP timestamp UTC)
    set(${_var} "${timestamp}" PARENT_SCOPE)
endfunction()

get_timestamp(BUILD_DATE)

set(REPO_NAME "STORM SWITCH")
if (DEFINED GIT_RELEASE AND NOT GIT_RELEASE STREQUAL "")
    set(BUILD_VERSION "${GIT_RELEASE}")
    set(GIT_TAG "${GIT_RELEASE}")
elseif (DEFINED GIT_TAG AND NOT GIT_TAG STREQUAL "")
    set(BUILD_VERSION "${GIT_TAG}")
else()
    set(BUILD_VERSION "7.2.2")
    set(GIT_TAG "7.2.2")
endif()
set(GIT_REFSPEC "main")
set(IS_DEV_BUILD false)
set(IS_NIGHTLY_BUILD false)

set(BUILD_TAG "v${BUILD_VERSION}")
set(BUILD_ID "${BUILD_VERSION}")
set(BUILD_FULLNAME "${REPO_NAME} ${BUILD_VERSION}")
set(GIT_DESC "${BUILD_VERSION}")

# Generate cpp with Git revision from template

set(BUILD_AUTO_UPDATE_STABLE_REPO "ReiKatari/STORM_SWITCH")
set(BUILD_AUTO_UPDATE_STABLE_API "api.github.com")
set(BUILD_AUTO_UPDATE_STABLE_API_PATH "/repos/")

set(BUILD_AUTO_UPDATE_WEBSITE "https://github.com")
set(BUILD_AUTO_UPDATE_API "api.github.com")
set(BUILD_AUTO_UPDATE_API_PATH "/repos/ReiKatari/STORM_SWITCH/releases/latest")
set(BUILD_AUTO_UPDATE_REPO "ReiKatari/STORM_SWITCH")

set(TITLE_BAR_FORMAT_IDLE "STORM SWITCH ${BUILD_VERSION}")
set(TITLE_BAR_FORMAT_RUNNING "STORM SWITCH ${BUILD_VERSION} | {3} | {1} | {2}")
set(CXX_COMPILER "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

configure_file(scm_rev.cpp.in scm_rev.cpp @ONLY)