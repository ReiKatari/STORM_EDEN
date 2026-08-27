// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include "common/common_types.h"

namespace Core {

struct GameFixProfile {
    u64 title_id;
    std::string game_name;
    std::string issues_ru;
    std::string issues_en;
    std::string fixes_ru;
    std::string fixes_en;
    
    // Per-game config parameters
    std::unordered_map<std::string, std::string> ini_settings;
};

class GameFixDatabase {
public:
    static const GameFixProfile* GetProfile(u64 title_id);
    static bool HasProfile(u64 title_id);
    static const std::vector<GameFixProfile>& GetAllProfiles();
    static bool ApplyProfileToPerGameConfig(u64 title_id, const std::string& config_file_path);
};

} // namespace Core
