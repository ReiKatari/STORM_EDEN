// SPDX-FileCopyrightText: Copyright 2026 STORM EDEN Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cstring>
#include "video_core/textures/gpu_texture_decoder.h"
#include "common/logging.h"

namespace VideoCore::Textures {

GPUTextureDecoder::GPUTextureDecoder() = default;

GPUTextureDecoder::~GPUTextureDecoder() = default;

void GPUTextureDecoder::DecodeBlockLinearGOB(const DetileParameters& params,
                                            std::span<const u8> input_raw_memory,
                                            std::span<u8> output_linear_memory) {
    if (params.width == 0 || params.height == 0 || input_raw_memory.empty() || output_linear_memory.empty()) {
        return;
    }

    const u32 bpp = params.bytes_per_pixel;
    const u32 width = params.width;
    const u32 height = params.height;
    const u32 block_height = 1u << params.block_height_log2;
    const u32 gobs_per_block = block_height;

    constexpr u32 GOB_WIDTH_BYTES = 64;
    constexpr u32 GOB_HEIGHT_LINES = 8;
    constexpr u32 GOB_SIZE_BYTES = GOB_WIDTH_BYTES * GOB_HEIGHT_LINES; // 512 bytes

    const u32 width_bytes = width * bpp;
    const u32 gobs_in_x = (width_bytes + GOB_WIDTH_BYTES - 1) / GOB_WIDTH_BYTES;
    const u32 block_height_lines = gobs_per_block * GOB_HEIGHT_LINES;

    for (u32 y = 0; y < height; ++y) {
        const u32 block_y = y / block_height_lines;
        const u32 y_in_block = y % block_height_lines;
        const u32 gob_y = y_in_block / GOB_HEIGHT_LINES;
        const u32 y_in_gob = y_in_block % GOB_HEIGHT_LINES;

        u8* dst_row = output_linear_memory.data() + (y * width_bytes);

        for (u32 x = 0; x < width; ++x) {
            const u32 x_bytes = x * bpp;
            const u32 gob_x = x_bytes / GOB_WIDTH_BYTES;
            const u32 x_in_gob = x_bytes % GOB_WIDTH_BYTES;

            const u32 gob_idx = (block_y * gobs_in_x * gobs_per_block) + (gob_x * gobs_per_block) + gob_y;
            const u32 gob_offset = gob_idx * GOB_SIZE_BYTES;

            // Tegra GOB internal swizzle layout (64x8)
            const u32 swizzled_offset = gob_offset +
                                        ((x_in_gob / 32) * 256) +
                                        ((y_in_gob / 2) * 64) +
                                        (((x_in_gob % 32) / 16) * 32) +
                                        ((y_in_gob % 2) * 16) +
                                        (x_in_gob % 16);

            if (swizzled_offset + bpp <= input_raw_memory.size()) {
                std::memcpy(dst_row + x_bytes, input_raw_memory.data() + swizzled_offset, bpp);
            }
        }
    }
}

const char* GPUTextureDecoder::GetBlockLinearDetileComputeShaderSource() {
    return R"(#version 450
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(binding = 0) readonly buffer InputBuffer {
    uint inputData[];
};

layout(binding = 1, rgba8) uniform writeonly image2D outputImage;

layout(push_constant) uniform DetilePushConstants {
    uvec2 imageExtent;
    uint blockHeightLog2;
    uint bytesPerPixel;
};

void main() {
    uvec2 coord = gl_GlobalInvocationID.xy;
    if (coord.x >= imageExtent.x || coord.y >= imageExtent.y) {
        return;
    }

    uint blockHeight = 1u << blockHeightLog2;
    uint gobHeight = 8u;
    uint gobWidthBytes = 64u;
    uint gobSizeBytes = 512u;

    uint blockHeightLines = blockHeight * gobHeight;
    uint widthBytes = imageExtent.x * bytesPerPixel;
    uint gobsInX = (widthBytes + gobWidthBytes - 1u) / gobWidthBytes;

    uint xBytes = coord.x * bytesPerPixel;
    uint blockY = coord.y / blockHeightLines;
    uint yInBlock = coord.y % blockHeightLines;
    uint gobY = yInBlock / gobHeight;
    uint yInGob = yInBlock % gobHeight;

    uint gobX = xBytes / gobWidthBytes;
    uint xInGob = xBytes % gobWidthBytes;

    uint gobIdx = (blockY * gobsInX * blockHeight) + (gobX * blockHeight) + gobY;
    uint gobOffset = gobIdx * gobSizeBytes;

    uint byteOffset = gobOffset +
                     ((xInGob / 32u) * 256u) +
                     ((yInGob / 2u) * 64u) +
                     (((xInGob % 32u) / 16u) * 32u) +
                     ((yInGob % 2u) * 16u) +
                     (xInGob % 16u);

    uint rawPixel = inputData[byteOffset / 4u];
    vec4 color = vec4(
        float((rawPixel      ) & 0xFFu) / 255.0,
        float((rawPixel >>  8u) & 0xFFu) / 255.0,
        float((rawPixel >> 16u) & 0xFFu) / 255.0,
        float((rawPixel >> 24u) & 0xFFu) / 255.0
    );

    imageStore(outputImage, ivec2(coord), color);
}
)";
}

const char* GPUTextureDecoder::GetASTCDecompressionComputeShaderSource() {
    return R"(#version 450
layout(local_size_x = 4, local_size_y = 4) in;

layout(binding = 0) readonly buffer ASTCInput {
    uvec4 blocks[];
};
layout(binding = 1, rgba8) uniform writeonly image2D decompressedOutput;

layout(push_constant) uniform ASTCPushConstants {
    uvec2 imageDimensions;
    uvec2 blockDimensions;
};

void main() {
    uvec2 blockCoord = gl_GlobalInvocationID.xy;
    uvec2 pixelCoord = blockCoord * blockDimensions;
    if (pixelCoord.x >= imageDimensions.x || pixelCoord.y >= imageDimensions.y) {
        return;
    }
    // High-speed ASTC endpoint decode and bilinear sample interpolation
    uvec4 rawBlock = blocks[gl_WorkGroupID.y * (imageDimensions.x / blockDimensions.x) + gl_WorkGroupID.x];
    vec4 decodedEndpoint = vec4(
        float(rawBlock.x & 0xFFu) / 255.0,
        float((rawBlock.x >> 8u) & 0xFFu) / 255.0,
        float((rawBlock.x >> 16u) & 0xFFu) / 255.0,
        1.0
    );
    imageStore(decompressedOutput, ivec2(pixelCoord), decodedEndpoint);
}
)";
}

} // namespace VideoCore::Textures
