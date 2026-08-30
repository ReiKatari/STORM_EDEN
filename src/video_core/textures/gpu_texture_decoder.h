// SPDX-FileCopyrightText: Copyright 2026 STORM EDEN Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>
#include <span>
#include "common/common_types.h"

namespace VideoCore::Textures {

struct DetileParameters {
    u32 width{0};
    u32 height{0};
    u32 depth{1};
    u32 block_height_log2{0};
    u32 bytes_per_pixel{4};
    u64 guest_raw_address{0};
    size_t input_size_bytes{0};
};

class GPUTextureDecoder {
public:
    explicit GPUTextureDecoder();
    ~GPUTextureDecoder();

    /// Decodes Tegra Block Linear GOB swizzled memory into linear RGBA8/BC on GPU
    void DecodeBlockLinearGOB(const DetileParameters& params,
                              std::span<const u8> input_raw_memory,
                              std::span<u8> output_linear_memory);

    /// Compute shader GLSL code for GPU-side Block Linear to Linear unswizzle
    [[nodiscard]] static const char* GetBlockLinearDetileComputeShaderSource();

    /// Compute shader GLSL code for GPU ASTC HDR decompression
    [[nodiscard]] static const char* GetASTCDecompressionComputeShaderSource();
};

} // namespace VideoCore::Textures
