// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "common/hex_util.h"
#include "common/logging.h"
#include "core/crypto/key_manager.h"
#include "qt_common/titledb.h"

namespace TitleDB {

TitleDatabase& TitleDatabase::Instance() {
    static TitleDatabase instance;
    return instance;
}

TitleDatabase::TitleDatabase() = default;
TitleDatabase::~TitleDatabase() = default;

void TitleDatabase::EnsureLoaded() {
    std::lock_guard<std::mutex> lock(load_mutex);
    if (is_loaded) {
        return;
    }

    std::vector<std::filesystem::path> candidate_paths;

    // 1. Check userprofile ~/.switch/titledb.json (from STORM SWITCH BOX)
#ifdef _WIN32
    const char* userprofile = std::getenv("USERPROFILE");
    if (userprofile) {
        candidate_paths.push_back(std::filesystem::path(userprofile) / ".switch" / "titledb.json");
    }
#else
    const char* home = std::getenv("HOME");
    if (home) {
        candidate_paths.push_back(std::filesystem::path(home) / ".switch" / "titledb.json");
    }
#endif

    // 2. Check emulator config & cache paths
    candidate_paths.push_back(Common::FS::GetEdenPath(Common::FS::EdenPath::ConfigDir) / "titledb" / "titledb.json");
    candidate_paths.push_back(Common::FS::GetEdenPath(Common::FS::EdenPath::ConfigDir) / "titledb" / "titles.json");
    candidate_paths.push_back(Common::FS::GetEdenPath(Common::FS::EdenPath::CacheDir) / "titledb.json");

    std::filesystem::path found_path;
    for (const auto& p : candidate_paths) {
        if (std::filesystem::exists(p)) {
            found_path = p;
            break;
        }
    }

    if (found_path.empty()) {
        LOG_DEBUG(Frontend, "TitleDB database file not found in search paths");
        is_loaded = true;
        return;
    }

    LOG_INFO(Frontend, "Loading TitleDB from: {}", Common::FS::PathToUTF8String(found_path));

    try {
        std::ifstream file(found_path);
        if (!file.is_open()) {
            is_loaded = true;
            return;
        }

        const auto json_data = nlohmann::json::parse(file, nullptr, false);
        if (json_data.is_discarded() || !json_data.is_object()) {
            LOG_WARNING(Frontend, "Failed to parse TitleDB JSON");
            is_loaded = true;
            return;
        }

        for (auto it = json_data.begin(); it != json_data.end(); ++it) {
            std::string key = it.key();
            std::transform(key.begin(), key.end(), key.begin(), ::toupper);

            const auto& val = it.value();
            if (!val.is_object()) continue;

            Entry entry;
            entry.id = key;
            if (val.contains("name") && !val["name"].is_null()) {
                entry.name = val["name"].get<std::string>();
            }
            if (val.contains("description") && !val["description"].is_null()) {
                entry.description = val["description"].get<std::string>();
            }
            if (val.contains("key") && !val["key"].is_null() && val["key"].is_string()) {
                const std::string key_hex = val["key"].get<std::string>();
                if (key_hex.length() == 32) {
                    std::string rid_hex = (val.contains("rightsId") && !val["rightsId"].is_null() && val["rightsId"].is_string())
                                              ? val["rightsId"].get<std::string>()
                                              : (entry.id + "0000000000000000");
                    if (rid_hex.length() == 32) {
                        const auto rights_id_raw = Common::HexStringToArray<16>(rid_hex);
                        u128 rights_id{};
                        std::memcpy(rights_id.data(), rights_id_raw.data(), rights_id_raw.size());
                        const Core::Crypto::Key128 key_data = Common::HexStringToArray<16>(key_hex);
                        Core::Crypto::KeyManager::Instance().SetKey(Core::Crypto::S128KeyType::Titlekey, key_data, rights_id[1], rights_id[0]);
                    }
                }
            }

            db.emplace(std::move(key), std::move(entry));
        }

        LOG_INFO(Frontend, "TitleDB loaded successfully with {} entries", db.size());
    } catch (const std::exception& e) {
        LOG_WARNING(Frontend, "Exception while parsing TitleDB: {}", e.what());
    }

    is_loaded = true;
}

std::optional<Entry> TitleDatabase::Lookup(u64 title_id) {
    EnsureLoaded();
    const std::string exact_key = fmt::format("{:016X}", title_id);
    auto it = db.find(exact_key);
    if (it != db.end() && !it->second.name.empty()) {
        return it->second;
    }

    // Check if it's an Update or DLC
    const bool is_update = ((title_id & 0x800) != 0);
    const bool is_dlc = ((title_id & 0x1FFF) >= 0x1000);

    u64 base_tid = 0;
    if (is_update) {
        // Update Title ID is base + 0x800
        base_tid = title_id - 0x800;
    } else {
        // DLC Title ID: clear bit 12 (0x1000) and lower 12 bits (0xFFF)
        base_tid = (title_id & ~0x1000ULL) & ~0xFFFULL;
    }

    std::string base_key = fmt::format("{:016X}", base_tid);
    auto base_it = db.find(base_key);
    if (base_it == db.end()) {
        // Fallback mask 0xFFFFFFFFFFFFE000
        base_tid = title_id & 0xFFFFFFFFFFFFE000;
        base_key = fmt::format("{:016X}", base_tid);
        base_it = db.find(base_key);
    }

    if (base_it != db.end() && !base_it->second.name.empty()) {
        Entry fallback_entry;
        fallback_entry.id = exact_key;
        const auto& base = base_it->second;
        const u32 dlc_num = static_cast<u32>(title_id & 0x7FF);

        if (is_update) {
            fallback_entry.name = "Пакет обновления игры";
            fallback_entry.description = "Накопительный пакет обновлений. Включает оптимизацию производительности, исправления ошибок и актуальные игровые данные.";
            fallback_entry.version = base.version;
        } else if (is_dlc) {
            fallback_entry.name = fmt::format("Дополнение #{}", dlc_num > 0 ? dlc_num : 1);
            fallback_entry.description = "Официальный загружаемый контент (DLC). Включает дополнительные игровые материалы, бонусы или сценарии.";
            fallback_entry.version = "0";
        } else {
            fallback_entry.name = base.name;
            fallback_entry.description = base.description;
            fallback_entry.version = base.version;
        }
        return fallback_entry;
    }

    if (it != db.end()) {
        return it->second;
    }

    return std::nullopt;
}

std::optional<Entry> TitleDatabase::Lookup(std::string_view hex_id) {
    try {
        u64 tid = std::stoull(std::string(hex_id), nullptr, 16);
        return Lookup(tid);
    } catch (...) {
        EnsureLoaded();
        std::string key(hex_id);
        std::transform(key.begin(), key.end(), key.begin(), ::toupper);

        auto it = db.find(key);
        if (it != db.end()) {
            return it->second;
        }
        return std::nullopt;
    }
}

std::vector<Entry> TitleDatabase::GetDlcs(u64 base_title_id) {
    EnsureLoaded();
    const u64 base_masked = base_title_id & 0xFFFFFFFFFFFFE000;
    std::vector<Entry> dlcs;
    for (const auto& [k, v] : db) {
        if (k.length() == 16) {
            try {
                u64 k_val = std::stoull(k, nullptr, 16);
                if ((k_val & 0xFFFFFFFFFFFFE000) == base_masked && (k_val & 0x1FFF) >= 0x1000) {
                    dlcs.push_back(v);
                }
            } catch (...) {}
        }
    }
    std::sort(dlcs.begin(), dlcs.end(), [](const Entry& a, const Entry& b) {
        return a.id < b.id;
    });
    return dlcs;
}

int TitleDatabase::GetDlcCount(u64 base_title_id) {
    return static_cast<int>(GetDlcs(base_title_id).size());
}

} // namespace TitleDB
