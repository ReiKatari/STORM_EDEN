// SPDX-FileCopyrightText: Copyright 2026 STORM EDEN Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include "common/logging.h"
#include "video_core/shader_speculator.h"

namespace VideoCore {

ShaderSpeculator::ShaderSpeculator() = default;

ShaderSpeculator::~ShaderSpeculator() {
    Shutdown();
}

void ShaderSpeculator::Initialize(u64 title_id) {
    std::unique_lock lock(mutex);
    current_title_id = title_id;
    is_running = true;
    profile_map.clear();
    compiled_hashes.clear();
    building_hashes.clear();
    compilation_queue.clear();
    precompiled_counter = 0;

    LOG_INFO(Render_Vulkan, "ShaderSpeculator: Initialized for Title ID {:016X}", title_id);

    // Launch worker threads
    const u32 thread_count = std::max(2u, std::min(4u, std::thread::hardware_concurrency() / 2));
    for (u32 i = 0; i < thread_count; ++i) {
        worker_threads.emplace_back(&ShaderSpeculator::WorkerThreadLoop, this);
    }
}

void ShaderSpeculator::Shutdown() {
    if (!is_running) {
        return;
    }
    is_running = false;
    cv.notify_all();

    for (auto& thread : worker_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    worker_threads.clear();
}

void ShaderSpeculator::RecordShaderExecution(u64 shader_hash, u32 stage_mask, bool is_compute) {
    if (shader_hash == 0) {
        return;
    }
    std::unique_lock lock(mutex);
    auto& record = profile_map[shader_hash];
    record.shader_hash = shader_hash;
    record.stage_mask = stage_mask;
    record.is_compute = is_compute;
    record.execution_count++;
}

void ShaderSpeculator::LaunchSpeculativePrecompilation(
    const std::function<void(u64, u32, bool)>& compile_callback) {
    std::unique_lock lock(mutex);
    compiler_callback = compile_callback;

    // Sort recorded permutations by execution frequency
    std::vector<ShaderPermutationRecord> sorted_records;
    sorted_records.reserve(profile_map.size());
    for (const auto& [hash, record] : profile_map) {
        if (compiled_hashes.find(hash) == compiled_hashes.end() &&
            building_hashes.find(hash) == building_hashes.end()) {
            sorted_records.push_back(record);
        }
    }

    std::sort(sorted_records.begin(), sorted_records.end(),
              [](const ShaderPermutationRecord& a, const ShaderPermutationRecord& b) {
                  return a.execution_count > b.execution_count;
              });

    for (const auto& record : sorted_records) {
        compilation_queue.push_back(record);
        building_hashes.insert(record.shader_hash);
    }

    LOG_INFO(Render_Vulkan, "ShaderSpeculator: Queued {} speculative shaders for background precompilation",
             compilation_queue.size());

    cv.notify_all();
}

bool ShaderSpeculator::ShouldUseFallbackPipeline(u64 shader_hash) const {
    std::unique_lock lock(mutex);
    return compiled_hashes.find(shader_hash) == compiled_hashes.end();
}

void ShaderSpeculator::MarkCompilationComplete(u64 shader_hash) {
    std::unique_lock lock(mutex);
    compiled_hashes.insert(shader_hash);
    building_hashes.erase(shader_hash);
    precompiled_counter++;
}

size_t ShaderSpeculator::GetTrackedShaderCount() const {
    std::unique_lock lock(mutex);
    return profile_map.size();
}

size_t ShaderSpeculator::GetPrecompiledCount() const {
    return precompiled_counter.load();
}

size_t ShaderSpeculator::GetPendingQueueSize() const {
    std::unique_lock lock(mutex);
    return compilation_queue.size();
}

void ShaderSpeculator::WorkerThreadLoop() {
    while (is_running) {
        ShaderPermutationRecord item;
        {
            std::unique_lock lock(mutex);
            cv.wait(lock, [this] {
                return !is_running || !compilation_queue.empty();
            });

            if (!is_running) {
                break;
            }

            item = compilation_queue.front();
            compilation_queue.pop_front();
        }

        if (compiler_callback) {
            try {
                compiler_callback(item.shader_hash, item.stage_mask, item.is_compute);
            } catch (...) {
                LOG_ERROR(Render_Vulkan, "ShaderSpeculator: Exception occurred during speculative compilation of hash {:016X}",
                          item.shader_hash);
            }
        }

        MarkCompilationComplete(item.shader_hash);
    }
}

} // namespace VideoCore
