// SPDX-FileCopyrightText: Copyright 2026 STORM EDEN Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cmath>
#include "video_core/renderer_vulkan/ui_layer_detector.h"

namespace Vulkan {

UILayerDetector::UILayerDetector() = default;

UILayerDetector::~UILayerDetector() = default;

bool UILayerDetector::IsUILayerDraw(const UIDrawHeuristicInfo& info) {
    // 2D UI criteria: No depth write, alpha blending enabled, and orthographic or small quad vertex counts
    if (info.depth_write_enabled) {
        return false;
    }

    if (!info.blending_enabled) {
        return false;
    }

    if (info.is_orthographic) {
        return true;
    }

    // Quad or 2-triangle UI draw
    return (info.vertex_count == 4 || info.vertex_count == 6);
}

bool UILayerDetector::IsOrthographicMatrix(const float* matrix_4x4) {
    if (matrix_4x4 == nullptr) {
        return false;
    }
    // In orthographic projection, matrix[3][3] == 1.0f and perspective row [3][0..2] == 0.0f
    return std::abs(matrix_4x4[15] - 1.0f) < 0.001f &&
           std::abs(matrix_4x4[3]) < 0.001f &&
           std::abs(matrix_4x4[7]) < 0.001f &&
           std::abs(matrix_4x4[11]) < 0.001f;
}

} // namespace Vulkan
