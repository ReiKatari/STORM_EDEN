// SPDX-FileCopyrightText: Copyright 2026 STORM EDEN Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <chrono>
#include "common/common_types.h"

namespace Vulkan {

class Device;
class Scheduler;

class AdaptiveScaler {
public:
    explicit AdaptiveScaler();
    ~AdaptiveScaler();

    void Initialize(float target_framerate_fps = 60.0f);
    void ReportFrameTime(double frame_time_ms);

    /// Compute dynamic scaling factor between 0.75 and 1.0 based on current GPU load
    [[nodiscard]] float GetCurrentScaleFactor() const;

    /// Checks if upscaling pass is needed
    [[nodiscard]] bool IsUpscalingActive() const;

    /// GLSL code for Sobel edge-aware spatial upscaler
    [[nodiscard]] static const char* GetEdgeAwareUpscaleShaderSource();

private:
    float target_frame_time_ms{16.666f};
    std::atomic<float> current_scale{1.0f};
    std::atomic<double> smoothed_frametime{16.0};
    u32 consecutive_overbudget_frames{0};
    u32 consecutive_underbudget_frames{0};
};

} // namespace Vulkan
