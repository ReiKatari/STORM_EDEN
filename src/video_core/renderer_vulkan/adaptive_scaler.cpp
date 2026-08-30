// SPDX-FileCopyrightText: Copyright 2026 STORM EDEN Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include "video_core/renderer_vulkan/adaptive_scaler.h"
#include "common/logging.h"

namespace Vulkan {

AdaptiveScaler::AdaptiveScaler() = default;

AdaptiveScaler::~AdaptiveScaler() = default;

void AdaptiveScaler::Initialize(float target_framerate_fps) {
    if (target_framerate_fps <= 0.0f) {
        target_framerate_fps = 60.0f;
    }
    target_frame_time_ms = 1000.0f / target_framerate_fps;
    current_scale = 1.0f;
    smoothed_frametime = target_frame_time_ms;
    consecutive_overbudget_frames = 0;
    consecutive_underbudget_frames = 0;

    LOG_INFO(Render_Vulkan, "AdaptiveScaler: Initialized for target {} FPS (budget {:.2f} ms)",
             target_framerate_fps, target_frame_time_ms);
}

void AdaptiveScaler::ReportFrameTime(double frame_time_ms) {
    // Exponential moving average (alpha = 0.1)
    const double current_avg = smoothed_frametime.load();
    const double new_avg = (current_avg * 0.9) + (frame_time_ms * 0.1);
    smoothed_frametime.store(new_avg);

    if (new_avg > target_frame_time_ms * 1.1) {
        consecutive_overbudget_frames++;
        consecutive_underbudget_frames = 0;
        if (consecutive_overbudget_frames >= 5) {
            // Drop scale by 10% down to 0.75 floor
            const float new_scale = std::max(0.75f, current_scale.load() - 0.10f);
            current_scale.store(new_scale);
            consecutive_overbudget_frames = 0;
        }
    } else if (new_avg < target_frame_time_ms * 0.85) {
        consecutive_underbudget_frames++;
        consecutive_overbudget_frames = 0;
        if (consecutive_underbudget_frames >= 30) {
            // Gradually recover scale up to 1.0 ceiling
            const float new_scale = std::min(1.0f, current_scale.load() + 0.05f);
            current_scale.store(new_scale);
            consecutive_underbudget_frames = 0;
        }
    }
}

float AdaptiveScaler::GetCurrentScaleFactor() const {
    return current_scale.load();
}

bool AdaptiveScaler::IsUpscalingActive() const {
    return current_scale.load() < 0.99f;
}

const char* AdaptiveScaler::GetEdgeAwareUpscaleShaderSource() {
    return R"(#version 450
layout(local_size_x = 16, local_size_y = 16) in;

layout(binding = 0) uniform sampler2D inputTexture;
layout(binding = 1, rgba8) uniform writeonly image2D outputImage;

layout(push_constant) uniform PushConstants {
    vec2 inputResolution;
    vec2 outputResolution;
    float edgeThreshold;
};

void main() {
    ivec2 outCoord = ivec2(gl_GlobalInvocationID.xy);
    if (outCoord.x >= int(outputResolution.x) || outCoord.y >= int(outputResolution.y)) {
        return;
    }

    vec2 uv = (vec2(outCoord) + 0.5) / outputResolution;
    vec2 texel = 1.0 / inputResolution;

    // 3x3 Sobel edge detection on luminance
    vec3 c00 = texture(inputTexture, uv + vec2(-texel.x, -texel.y)).rgb;
    vec3 c10 = texture(inputTexture, uv + vec2(0.0,      -texel.y)).rgb;
    vec3 c20 = texture(inputTexture, uv + vec2( texel.x, -texel.y)).rgb;
    vec3 c01 = texture(inputTexture, uv + vec2(-texel.x,  0.0)).rgb;
    vec3 c11 = texture(inputTexture, uv).rgb;
    vec3 c21 = texture(inputTexture, uv + vec2( texel.x,  0.0)).rgb;
    vec3 c02 = texture(inputTexture, uv + vec2(-texel.x,  texel.y)).rgb;
    vec3 c12 = texture(inputTexture, uv + vec2(0.0,       texel.y)).rgb;
    vec3 c22 = texture(inputTexture, uv + vec2( texel.x,  texel.y)).rgb;

    const vec3 luma = vec3(0.299, 0.587, 0.114);
    float l00 = dot(c00, luma); float l10 = dot(c10, luma); float l20 = dot(c20, luma);
    float l01 = dot(c01, luma); float l11 = dot(c11, luma); float l21 = dot(c21, luma);
    float l02 = dot(c02, luma); float l12 = dot(c12, luma); float l22 = dot(c22, luma);

    float gx = (l20 + 2.0 * l21 + l22) - (l00 + 2.0 * l01 + l02);
    float gy = (l02 + 2.0 * l12 + l22) - (l00 + 2.0 * l10 + l20);
    float edgeMagnitude = sqrt(gx * gx + gy * gy);

    vec3 finalColor = c11;
    if (edgeMagnitude > edgeThreshold) {
        // High-frequency edge: sharpen via unsharp-masking kernel
        vec3 blurred = (c10 + c01 + c21 + c12) * 0.25;
        finalColor = clamp(c11 + (c11 - blurred) * 0.5, 0.0, 1.0);
    }

    imageStore(outputImage, outCoord, vec4(finalColor, 1.0));
}
)";
}

} // namespace Vulkan
