// SPDX-FileCopyrightText: Copyright 2026 STORM EDEN Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>
#include "common/common_types.h"

namespace Vulkan {

class Device;
class Scheduler;

class MipMapGenerator {
public:
    explicit MipMapGenerator();
    ~MipMapGenerator();

    /// Checks if a texture needs on-the-fly mip generation (size > 256x256, mips == 1)
    [[nodiscard]] static bool NeedsMipGeneration(u32 width, u32 height, u32 mip_levels);

    /// Compute total mip levels for given extent
    [[nodiscard]] static u32 CalculateMipLevels(u32 width, u32 height);

    /// Compute shader code for box-filter downsampling
    [[nodiscard]] static const char* GetBoxFilterDownsampleShaderSource();
};

} // namespace Vulkan
