// SPDX-FileCopyrightText: Copyright 2026 STORM EDEN Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <cmath>
#include "video_core/renderer_vulkan/mipmap_generator.h"

namespace Vulkan {

MipMapGenerator::MipMapGenerator() = default;

MipMapGenerator::~MipMapGenerator() = default;

bool MipMapGenerator::NeedsMipGeneration(u32 width, u32 height, u32 mip_levels) {
    return mip_levels == 1 && (width > 256 || height > 256);
}

u32 MipMapGenerator::CalculateMipLevels(u32 width, u32 height) {
    return 1u + static_cast<u32>(std::floor(std::log2(std::max(width, height))));
}

const char* MipMapGenerator::GetBoxFilterDownsampleShaderSource() {
    return R"(#version 450
layout(local_size_x = 8, local_size_y = 8) in;

layout(binding = 0) uniform sampler2D sourceMip;
layout(binding = 1, rgba8) uniform writeonly image2D destinationMip;

layout(push_constant) uniform MipPushConstants {
    vec2 destinationExtent;
};

void main() {
    ivec2 destCoord = ivec2(gl_GlobalInvocationID.xy);
    if (destCoord.x >= int(destinationExtent.x) || destCoord.y >= int(destinationExtent.y)) {
        return;
    }

    vec2 uv = (vec2(destCoord) + 0.5) / destinationExtent;
    vec2 texelSize = 1.0 / (destinationExtent * 2.0);

    // 4-tap box filter downsample
    vec4 s00 = texture(sourceMip, uv + vec2(-texelSize.x, -texelSize.y) * 0.5);
    vec4 s10 = texture(sourceMip, uv + vec2( texelSize.x, -texelSize.y) * 0.5);
    vec4 s01 = texture(sourceMip, uv + vec2(-texelSize.x,  texelSize.y) * 0.5);
    vec4 s11 = texture(sourceMip, uv + vec2( texelSize.x,  texelSize.y) * 0.5);

    vec4 filteredColor = (s00 + s10 + s01 + s11) * 0.25;
    imageStore(destinationMip, destCoord, filteredColor);
}
)";
}

} // namespace Vulkan
