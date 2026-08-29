// SPDX-FileCopyrightText: Copyright 2026 STORM EDEN Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "video_core/draw_call_sorter.h"

namespace VideoCore {

DrawCallSorter::DrawCallSorter() = default;

DrawCallSorter::~DrawCallSorter() = default;

void DrawCallSorter::BeginPass(bool depth_test_enabled, bool blending_enabled) {
    draw_queue.clear();
    // Reordering is strictly safe when blending order is commutative or when depth write preserves order
    is_depth_independent = !blending_enabled;
}

void DrawCallSorter::PushDraw(const DrawCallCommand& command) {
    draw_queue.push_back(command);
}

void DrawCallSorter::Flush(const std::function<void(const DrawCallCommand&)>& execute_draw_cb) {
    if (draw_queue.empty()) {
        return;
    }

    if (is_depth_independent && draw_queue.size() > 1) {
        // Sort by state key: pipeline_hash -> descriptor_set_hash -> vertex_layout_id
        std::stable_sort(draw_queue.begin(), draw_queue.end(),
                         [](const DrawCallCommand& a, const DrawCallCommand& b) {
                             if (a.pipeline_hash != b.pipeline_hash) {
                                 return a.pipeline_hash < b.pipeline_hash;
                             }
                             if (a.descriptor_set_hash != b.descriptor_set_hash) {
                                 return a.descriptor_set_hash < b.descriptor_set_hash;
                             }
                             return a.vertex_layout_id < b.vertex_layout_id;
                         });
    }

    u64 last_pipe = 0;
    u64 last_desc = 0;

    for (const auto& cmd : draw_queue) {
        if (last_pipe != 0 && last_pipe == cmd.pipeline_hash && last_desc == cmd.descriptor_set_hash) {
            avoided_state_switches++;
        }
        last_pipe = cmd.pipeline_hash;
        last_desc = cmd.descriptor_set_hash;

        execute_draw_cb(cmd);
    }

    draw_queue.clear();
}

size_t DrawCallSorter::GetBufferedDrawCount() const {
    return draw_queue.size();
}

size_t DrawCallSorter::GetAvoidedStateSwitches() const {
    return avoided_state_switches;
}

} // namespace VideoCore
