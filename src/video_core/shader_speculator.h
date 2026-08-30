// SPDX-FileCopyrightText: Copyright 2026 STORM EDEN Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "common/common_types.h"

namespace VideoCore {

enum class ShaderStageType : u32 {
    Vertex = 0,
    TessellationControl = 1,
    TessellationEval = 2,
    Geometry = 3,
    Fragment = 4,
    Compute = 5,
};

struct ShaderPermutationRecord {
    u64 shader_hash{0};
    u32 stage_mask{0};
    u32 execution_count{0};
    u32 sample_rate{0};
    bool is_compute{false};
};

class ShaderSpeculator {
public:
    explicit ShaderSpeculator();
    ~ShaderSpeculator();

    void Initialize(u64 title_id);
    void Shutdown();

    /// Register a newly encountered shader permutation into the game profile
    void RecordShaderExecution(u64 shader_hash, u32 stage_mask, bool is_compute);

    /// Launch background speculative precompilation of the top-N predicted permutations
    void LaunchSpeculativePrecompilation(
        const std::function<void(u64 hash, u32 stage_mask, bool is_compute)>& compile_callback);

    /// Check if a lightweight fallback pipeline is requested while asynchronous building finishes
    [[nodiscard]] bool ShouldUseFallbackPipeline(u64 shader_hash) const;

    /// Notify that full asynchronous compilation has finished for a shader hash
    void MarkCompilationComplete(u64 shader_hash);

    /// Query compilation statistics
    [[nodiscard]] size_t GetTrackedShaderCount() const;
    [[nodiscard]] size_t GetPrecompiledCount() const;
    [[nodiscard]] size_t GetPendingQueueSize() const;

private:
    void WorkerThreadLoop();

    u64 current_title_id{0};
    mutable std::mutex mutex;
    std::condition_variable cv;
    std::atomic<bool> is_running{false};

    std::unordered_map<u64, ShaderPermutationRecord> profile_map;
    std::unordered_set<u64> compiled_hashes;
    std::unordered_set<u64> building_hashes;
    std::deque<ShaderPermutationRecord> compilation_queue;

    std::function<void(u64, u32, bool)> compiler_callback;
    std::vector<std::thread> worker_threads;
    std::atomic<size_t> precompiled_counter{0};
};

} // namespace VideoCore
