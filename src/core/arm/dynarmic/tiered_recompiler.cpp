// SPDX-FileCopyrightText: Copyright 2026 STORM EDEN Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/logging.h"
#include "core/arm/dynarmic/tiered_recompiler.h"

namespace Core::Arm {

TieredRecompiler::TieredRecompiler() = default;

TieredRecompiler::~TieredRecompiler() {
    Shutdown();
}

void TieredRecompiler::Initialize() {
    std::unique_lock lock(mutex);
    is_running = true;
    block_map.clear();
    pending_recompile_queue.clear();
    tier2_optimized_count = 0;

    background_thread = std::thread(&TieredRecompiler::BackgroundCompilationLoop, this);
    LOG_INFO(Core_ARM, "TieredRecompiler: JIT Profiler & Tier-2 Optimizer Initialized (Threshold = {} executions)",
             TIER2_RECOMPILATION_THRESHOLD);
}

void TieredRecompiler::Shutdown() {
    if (!is_running) {
        return;
    }
    is_running = false;
    cv.notify_all();

    if (background_thread.joinable()) {
        background_thread.join();
    }
}

void* TieredRecompiler::ReportExecution(u64 guest_vaddr, size_t code_size, void* current_ptr) {
    std::unique_lock lock(mutex);
    auto& profile = block_map[guest_vaddr];
    if (profile.guest_vaddr == 0) {
        profile.guest_vaddr = guest_vaddr;
        profile.code_size = code_size;
        profile.tier_level = 1;
        profile.compiled_func_ptr = current_ptr;
    }

    profile.execution_count++;
    if (profile.tier_level == 1 && profile.execution_count >= TIER2_RECOMPILATION_THRESHOLD) {
        profile.tier_level = 2; // Marked as pending Tier-2
        pending_recompile_queue.push_back(guest_vaddr);
        cv.notify_one();
    }

    return profile.compiled_func_ptr;
}

void TieredRecompiler::SetTier2RecompileCallback(
    const std::function<void*(u64, size_t)>& callback) {
    std::unique_lock lock(mutex);
    tier2_callback = callback;
}

size_t TieredRecompiler::GetHotBlockCount() const {
    std::unique_lock lock(mutex);
    return block_map.size();
}

size_t TieredRecompiler::GetTier2OptimizedBlockCount() const {
    return tier2_optimized_count.load();
}

void TieredRecompiler::BackgroundCompilationLoop() {
    while (is_running) {
        u64 vaddr = 0;
        size_t size = 0;
        {
            std::unique_lock lock(mutex);
            cv.wait(lock, [this] {
                return !is_running || !pending_recompile_queue.empty();
            });

            if (!is_running) {
                break;
            }

            vaddr = pending_recompile_queue.front();
            pending_recompile_queue.pop_front();
            size = block_map[vaddr].code_size;
        }

        if (tier2_callback) {
            void* new_ptr = tier2_callback(vaddr, size);
            if (new_ptr != nullptr) {
                std::unique_lock lock(mutex);
                block_map[vaddr].compiled_func_ptr = new_ptr;
                block_map[vaddr].tier_level = 3; // Fully Tier-2 Optimized
                tier2_optimized_count++;
            }
        }
    }
}

} // namespace Core::Arm
