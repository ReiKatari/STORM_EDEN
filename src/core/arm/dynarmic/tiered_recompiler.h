// SPDX-FileCopyrightText: Copyright 2026 STORM EDEN Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "common/common_types.h"

namespace Core::Arm {

constexpr u32 TIER2_RECOMPILATION_THRESHOLD = 10000;

struct BasicBlockProfile {
    u64 guest_vaddr{0};
    size_t code_size{0};
    u32 execution_count{0};
    u32 tier_level{1};
    void* compiled_func_ptr{nullptr};
};

class TieredRecompiler {
public:
    explicit TieredRecompiler();
    ~TieredRecompiler();

    void Initialize();
    void Shutdown();

    /// Track execution of a JIT basic block. Returns updated func pointer if Tier-2 is ready
    void* ReportExecution(u64 guest_vaddr, size_t code_size, void* current_ptr);

    /// Register a custom Tier-2 recompiler callback
    void SetTier2RecompileCallback(const std::function<void*(u64 vaddr, size_t size)>& callback);

    [[nodiscard]] size_t GetHotBlockCount() const;
    [[nodiscard]] size_t GetTier2OptimizedBlockCount() const;

private:
    void BackgroundCompilationLoop();

    mutable std::mutex mutex;
    std::condition_variable cv;
    std::atomic<bool> is_running{false};

    std::unordered_map<u64, BasicBlockProfile> block_map;
    std::deque<u64> pending_recompile_queue;
    std::function<void*(u64, size_t)> tier2_callback;
    std::thread background_thread;

    std::atomic<size_t> tier2_optimized_count{0};
};

} // namespace Core::Arm
