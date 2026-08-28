// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include "common/common_types.h"

namespace Core {

class System;

/// Save state file format version
constexpr u32 SAVE_STATE_VERSION = 1;

/// Save state magic header
constexpr u32 SAVE_STATE_MAGIC = 0x53455353; // "SESS" (Storm Eden Save State)

/// Header for save state files (.ses)
struct SaveStateHeader {
    u32 magic{SAVE_STATE_MAGIC};
    u32 version{SAVE_STATE_VERSION};
    u64 title_id{};
    u64 dram_size{};
    u64 dram_compressed_size{};
    u32 num_threads{};
    u32 compression_level{};
    u64 timestamp{};
    u8 reserved[32]{};
};

static_assert(sizeof(SaveStateHeader) == 80, "SaveStateHeader must be 80 bytes");

/// Thread context snapshot for save states
struct ThreadSnapshot {
    u32 thread_id{};
    u32 core_id{};
    u64 tls_address{};
    // Full Svc::ThreadContext is 0x320 bytes (ARM64 registers + FPU)
    u8 context_data[0x320]{};
};

/// Result of save state operations
enum class SaveStateResult {
    Success,
    ErrorSystemNotRunning,
    ErrorFileCreate,
    ErrorFileRead,
    ErrorCompress,
    ErrorDecompress,
    ErrorVersionMismatch,
    ErrorCorruptedFile,
    ErrorInvalidMagic,
};

/// Create a save state from the current emulation state
SaveStateResult CreateSaveState(System& system, const std::filesystem::path& path);

/// Load a save state and restore the emulation state
SaveStateResult LoadSaveState(System& system, const std::filesystem::path& path);

/// Get the default save state directory for a given title
std::filesystem::path GetSaveStatePath(u64 title_id, u32 slot);

/// Get info about a save state file without loading it
bool GetSaveStateInfo(const std::filesystem::path& path, SaveStateHeader& header_out);

} // namespace Core
