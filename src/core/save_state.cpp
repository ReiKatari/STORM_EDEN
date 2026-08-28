// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <chrono>
#include <fstream>
#include "common/fs/path_util.h"
#include "common/logging.h"
#include "common/zstd_compression.h"
#include "core/core.h"
#include "core/device_memory.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_thread.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/board/nintendo/nx/k_system_control.h"
#include "core/save_state.h"

namespace Core {

SaveStateResult CreateSaveState(System& system, const std::filesystem::path& path) {
    if (!system.IsPoweredOn()) {
        return SaveStateResult::ErrorSystemNotRunning;
    }

    LOG_INFO(Core, "Creating save state: {}", path.string());

    // Pause emulation
    system.StallApplication();

    // Get DRAM data
    auto& device_memory = system.DeviceMemory();
    const u8* dram_ptr = device_memory.buffer.BackingBasePointer();
    const u64 dram_size = Kernel::Board::Nintendo::Nx::KSystemControl::Init::GetIntendedMemorySize();

    // Compress DRAM with Zstd (level 1 for speed)
    LOG_INFO(Core, "Compressing {} MB of DRAM...", dram_size / (1024 * 1024));
    auto compressed = Common::Compression::CompressDataZSTD(dram_ptr, dram_size, 1);
    if (compressed.empty()) {
        system.UnstallApplication();
        return SaveStateResult::ErrorCompress;
    }
    LOG_INFO(Core, "Compressed to {} MB ({:.1f}% ratio)",
             compressed.size() / (1024 * 1024),
             100.0 * compressed.size() / dram_size);

    // Collect thread contexts
    auto& kernel = system.Kernel();
    std::vector<ThreadSnapshot> thread_snapshots;

    auto* process = kernel.ApplicationProcess();
    if (process) {
        auto& thread_list = process->GetThreadList();
        for (auto& thread : thread_list) {
            ThreadSnapshot snap{};
            snap.thread_id = static_cast<u32>(thread.GetThreadId());
            snap.core_id = static_cast<u32>(thread.GetActiveCore());
            snap.tls_address = GetInteger(thread.GetTlsAddress());

            const auto& ctx = thread.GetContext();
            std::memcpy(snap.context_data, &ctx, std::min(sizeof(ctx), sizeof(snap.context_data)));

            thread_snapshots.push_back(snap);
        }
    }

    // Build header
    SaveStateHeader header{};
    header.title_id = system.GetApplicationProcessProgramID();
    header.dram_size = dram_size;
    header.dram_compressed_size = compressed.size();
    header.num_threads = static_cast<u32>(thread_snapshots.size());
    header.compression_level = 1;
    header.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();

    // Write to file
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        system.UnstallApplication();
        return SaveStateResult::ErrorFileCreate;
    }

    file.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // Write thread contexts
    const u32 thread_data_size = static_cast<u32>(thread_snapshots.size() * sizeof(ThreadSnapshot));
    file.write(reinterpret_cast<const char*>(&thread_data_size), sizeof(thread_data_size));
    if (!thread_snapshots.empty()) {
        file.write(reinterpret_cast<const char*>(thread_snapshots.data()), thread_data_size);
    }

    // Write compressed DRAM
    file.write(reinterpret_cast<const char*>(compressed.data()), compressed.size());
    file.close();

    system.UnstallApplication();

    LOG_INFO(Core, "Save state created successfully: {} threads, {} MB compressed",
             thread_snapshots.size(), compressed.size() / (1024 * 1024));

    return SaveStateResult::Success;
}

SaveStateResult LoadSaveState(System& system, const std::filesystem::path& path) {
    if (!system.IsPoweredOn()) {
        return SaveStateResult::ErrorSystemNotRunning;
    }

    LOG_INFO(Core, "Loading save state: {}", path.string());

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return SaveStateResult::ErrorFileRead;
    }

    // Read header
    SaveStateHeader header{};
    file.read(reinterpret_cast<char*>(&header), sizeof(header));

    if (header.magic != SAVE_STATE_MAGIC) {
        return SaveStateResult::ErrorInvalidMagic;
    }
    if (header.version != SAVE_STATE_VERSION) {
        return SaveStateResult::ErrorVersionMismatch;
    }

    // Pause emulation
    system.StallApplication();

    // Read thread contexts
    u32 thread_data_size = 0;
    file.read(reinterpret_cast<char*>(&thread_data_size), sizeof(thread_data_size));

    std::vector<ThreadSnapshot> thread_snapshots;
    if (thread_data_size > 0) {
        const u32 num_threads = thread_data_size / sizeof(ThreadSnapshot);
        thread_snapshots.resize(num_threads);
        file.read(reinterpret_cast<char*>(thread_snapshots.data()), thread_data_size);
    }

    // Read compressed DRAM
    std::vector<u8> compressed(header.dram_compressed_size);
    file.read(reinterpret_cast<char*>(compressed.data()), header.dram_compressed_size);
    file.close();

    // Decompress
    LOG_INFO(Core, "Decompressing {} MB...", compressed.size() / (1024 * 1024));
    auto decompressed = Common::Compression::DecompressDataZSTD(compressed);
    if (decompressed.empty() || decompressed.size() != header.dram_size) {
        system.UnstallApplication();
        return SaveStateResult::ErrorDecompress;
    }

    // Restore DRAM
    auto& device_memory = system.DeviceMemory();
    u8* dram_ptr = device_memory.buffer.BackingBasePointer();
    std::memcpy(dram_ptr, decompressed.data(), header.dram_size);

    // Restore thread contexts
    auto& kernel = system.Kernel();
    auto* process = kernel.ApplicationProcess();
    if (process && !thread_snapshots.empty()) {
        auto& thread_list = process->GetThreadList();
        for (auto& thread : thread_list) {
            for (const auto& snap : thread_snapshots) {
                if (static_cast<u32>(thread.GetThreadId()) == snap.thread_id) {
                    auto& ctx = thread.GetContext();
                    std::memcpy(&ctx, snap.context_data,
                                std::min(sizeof(ctx), sizeof(snap.context_data)));
                    break;
                }
            }
        }
    }

    system.UnstallApplication();

    LOG_INFO(Core, "Save state loaded successfully");
    return SaveStateResult::Success;
}

std::filesystem::path GetSaveStatePath(u64 title_id, u32 slot) {
    auto save_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::SaveDir) / "savestates";
    std::filesystem::create_directories(save_dir);
    return save_dir / fmt::format("{:016X}_slot{}.ses", title_id, slot);
}

bool GetSaveStateInfo(const std::filesystem::path& path, SaveStateHeader& header_out) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    file.read(reinterpret_cast<char*>(&header_out), sizeof(header_out));
    return header_out.magic == SAVE_STATE_MAGIC;
}

} // namespace Core
