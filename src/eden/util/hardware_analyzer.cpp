// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <thread>
#include <regex>

#include "common/common_funcs.h"
#include "common/settings.h"
#include "common/host_memory.h"
#include "common/x64/cpu_detect.h"
#include "common/memory_detect.h"
#include "eden/util/hardware_analyzer.h"
#include "eden/vk_device_info.h"

namespace Util {

HardwareInfo HardwareAnalyzer::GetHardwareInfo() {
    HardwareInfo info{};
    
    // 1. CPU Name
#ifdef ARCHITECTURE_x86_64
    info.cpu_name = Common::GetCPUCaps().cpu_string;
#else
    info.cpu_name = "Generic CPU";
#endif

    // 2. CPU Cores
    if (std::optional<int> processor_core = Common::GetProcessorCount()) {
        info.cpu_cores = *processor_core;
    } else {
        info.cpu_cores = std::thread::hardware_concurrency();
    }

    // 3. RAM GB
    info.ram_gb = Common::GetMemInfo().TotalPhysicalMemory / static_cast<double>(1024ULL * 1024 * 1024);

    // 4. GPU Name
    std::vector<VkDeviceInfo::Record> records;
    VkDeviceInfo::PopulateRecords(records, nullptr);
    int selected_gpu_index = Settings::values.vulkan_device.GetValue();
    if (selected_gpu_index >= 0 && selected_gpu_index < static_cast<int>(records.size())) {
        info.gpu_name = records[selected_gpu_index].name;
    } else if (!records.empty()) {
        info.gpu_name = records[0].name;
    } else {
        info.gpu_name = "Unknown GPU";
    }

    // 5. Tier
    info.tier = DetermineTier(info.cpu_name, info.cpu_cores, info.gpu_name, info.ram_gb);
    
    return info;
}

HardwareTier HardwareAnalyzer::DetermineTier(const std::string& cpu, int cores, const std::string& gpu, double ram_gb) {
    int score = 0; // 0 = low, 1 = mid, 2 = high, 3 = enthusiast

    std::string cpu_lower = cpu;
    std::transform(cpu_lower.begin(), cpu_lower.end(), cpu_lower.begin(), ::tolower);
    std::string gpu_lower = gpu;
    std::transform(gpu_lower.begin(), gpu_lower.end(), gpu_lower.begin(), ::tolower);

    // CPU Scoring
    if (cpu_lower.find("i9") != std::string::npos || cpu_lower.find("ryzen 9") != std::string::npos || cores >= 16) {
        score += 3;
    } else if (cpu_lower.find("i7") != std::string::npos || cpu_lower.find("ryzen 7") != std::string::npos || cores >= 8) {
        score += 2;
    } else if (cpu_lower.find("i5") != std::string::npos || cpu_lower.find("ryzen 5") != std::string::npos || cores >= 6) {
        score += 1;
    }

    // GPU Scoring
    if (gpu_lower.find("rtx 4090") != std::string::npos || gpu_lower.find("rtx 4080") != std::string::npos || gpu_lower.find("rx 7900") != std::string::npos) {
        score += 4;
    } else if (gpu_lower.find("rtx 4070") != std::string::npos || gpu_lower.find("rtx 3080") != std::string::npos || gpu_lower.find("rtx 3090") != std::string::npos || gpu_lower.find("rx 6800") != std::string::npos || gpu_lower.find("rx 6900") != std::string::npos) {
        score += 3;
    } else if (gpu_lower.find("rtx 3060") != std::string::npos || gpu_lower.find("rtx 2080") != std::string::npos || gpu_lower.find("rtx 4060") != std::string::npos || gpu_lower.find("rx 6700") != std::string::npos) {
        score += 2;
    } else if (gpu_lower.find("rtx") != std::string::npos || gpu_lower.find("gtx 1660") != std::string::npos || gpu_lower.find("gtx 1080") != std::string::npos || gpu_lower.find("rx 5700") != std::string::npos) {
        score += 1;
    } else {
        score += 0; // Low end or unknown
    }

    // RAM Scoring
    if (ram_gb >= 31.0) score += 2;
    else if (ram_gb >= 15.0) score += 1;

    // Determine final tier based on total score (Max theoretical score: 3+4+2 = 9)
    if (score >= 7) return HardwareTier::Enthusiast;
    if (score >= 4) return HardwareTier::HighEnd;
    if (score >= 2) return HardwareTier::MidRange;
    return HardwareTier::LowEnd;
}

int HardwareAnalyzer::GetMaxRecommendedResolution(HardwareTier tier) {
    switch (tier) {
        case HardwareTier::LowEnd: return 1;    // 1x (720p/1080p)
        case HardwareTier::MidRange: return 2;  // 2x (1440p/2160p)
        case HardwareTier::HighEnd: return 3;   // 3x (4K+)
        case HardwareTier::Enthusiast: return 6;// up to 6x+
        default: return 1;
    }
}

} // namespace Util
