// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>

namespace Util {

enum class HardwareTier {
    LowEnd,      // i3 / old Ryzen 3 / GTX 1050 / integrated / <8GB RAM
    MidRange,    // i5 / Ryzen 5 / GTX 1060 - 1660 / RTX 2060 / 3050 / 8-16GB RAM
    HighEnd,     // i7 / Ryzen 7 / RTX 3060 - 4070 / RX 6700 / 16-32GB RAM
    Enthusiast   // i9 / Ryzen 9 / RTX 4080 - 4090 / RX 7900 XTX / 32GB+ RAM
};

struct HardwareInfo {
    std::string cpu_name;
    int cpu_cores;
    std::string gpu_name;
    double ram_gb;
    HardwareTier tier;
};

class HardwareAnalyzer {
public:
    // Retrives current PC specs and determines tier
    static HardwareInfo GetHardwareInfo();
    
    // Calculates tier based on rules
    static HardwareTier DetermineTier(const std::string& cpu, int cores, const std::string& gpu, double ram_gb);
    
    // Returns max safe resolution scale (1 = 1x, 2 = 2x, etc) based on tier
    static int GetMaxRecommendedResolution(HardwareTier tier);
};

} // namespace Util
