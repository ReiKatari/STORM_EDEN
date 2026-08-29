// SPDX-FileCopyrightText: Copyright 2026 STORM EDEN Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "common/common_types.h"

namespace Vulkan {

struct UIDrawHeuristicInfo {
    bool depth_test_enabled{false};
    bool depth_write_enabled{false};
    bool blending_enabled{false};
    bool is_orthographic{false};
    u32 vertex_count{0};
    u32 instance_count{1};
};

class UILayerDetector {
public:
    explicit UILayerDetector();
    ~UILayerDetector();

    /// Evaluates if current draw characteristics qualify as a 2D UI overlay layer
    [[nodiscard]] static bool IsUILayerDraw(const UIDrawHeuristicInfo& info);

    /// Checks whether orthographic projection matrix pattern matches 2D screen bounds
    [[nodiscard]] static bool IsOrthographicMatrix(const float* matrix_4x4);
};

} // namespace Vulkan
