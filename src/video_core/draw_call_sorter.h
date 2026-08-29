// SPDX-FileCopyrightText: Copyright 2026 STORM EDEN Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>
#include <functional>
#include <algorithm>
#include "common/common_types.h"

namespace VideoCore {

struct DrawCallCommand {
    u64 pipeline_hash{0};
    u64 descriptor_set_hash{0};
    u32 vertex_layout_id{0};
    u32 first_vertex{0};
    u32 vertex_count{0};
    u32 first_index{0};
    u32 index_count{0};
    u32 first_instance{0};
    u32 instance_count{1};
    bool is_indexed{false};
    void* user_data{nullptr};
};

class DrawCallSorter {
public:
    explicit DrawCallSorter();
    ~DrawCallSorter();

    void BeginPass(bool depth_test_enabled, bool blending_enabled);
    void PushDraw(const DrawCallCommand& command);
    void Flush(const std::function<void(const DrawCallCommand&)>& execute_draw_cb);

    [[nodiscard]] size_t GetBufferedDrawCount() const;
    [[nodiscard]] size_t GetAvoidedStateSwitches() const;

private:
    std::vector<DrawCallCommand> draw_queue;
    bool is_depth_independent{true};
    size_t avoided_state_switches{0};
};

} // namespace VideoCore
