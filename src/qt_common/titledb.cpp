// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>
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

TitleDatabase::TitleDatabase() {
    EnsureLoaded();
}

TitleDatabase::~TitleDatabase() = default;

void TitleDatabase::EnsureLoaded() {
    if (is_loaded.load(std::memory_order_relaxed)) {
        return;
    }
    bool expected = false;
    if (is_loading.compare_exchange_strong(expected, true)) {
        std::thread([this]() {
            LoadDataSync();
        }).detach();
    }
}

void TitleDatabase::WaitLoaded(std::chrono::milliseconds timeout) {
    if (is_loaded.load(std::memory_order_acquire)) {
        return;
    }
    EnsureLoaded();
    std::unique_lock<std::mutex> lock(db_mutex);
    cv.wait_for(lock, timeout, [this]() {
        return is_loaded.load(std::memory_order_acquire);
    });
}

bool TitleDatabase::IsLoaded() const {
    return is_loaded.load(std::memory_order_acquire);
}

static std::string ExtractDlcTitleFromDesc(std::string_view desc) {
    if (desc.empty()) return {};

    // 1. Look for leading quotes: "The Champions' Ballad" or \"The Champions' Ballad\"
    size_t start_q = desc.find('"');
    if (start_q != std::string_view::npos && start_q < 40) {
        size_t end_q = desc.find('"', start_q + 1);
        if (end_q != std::string_view::npos && end_q > start_q + 1) {
            std::string title(desc.substr(start_q + 1, end_q - (start_q + 1)));
            if (!title.empty() && title.length() < 80) {
                return title;
            }
        }
    }

    // 2. Look for first line before newline
    size_t newline = desc.find('\n');
    std::string_view first_line = (newline != std::string_view::npos) ? desc.substr(0, newline) : desc;

    size_t will_inc = first_line.find(" will include");
    if (will_inc == std::string_view::npos) {
        will_inc = first_line.find(" includes ");
    }
    if (will_inc != std::string_view::npos) {
        std::string_view candidate = first_line.substr(0, will_inc);
        while (!candidate.empty() && (candidate.front() == '"' || candidate.front() == ' ' || candidate.front() == '\\')) {
            candidate.remove_prefix(1);
        }
        while (!candidate.empty() && (candidate.back() == '"' || candidate.back() == ' ' || candidate.back() == '\\')) {
            candidate.remove_suffix(1);
        }
        if (!candidate.empty() && candidate.length() < 80) return std::string(candidate);
    }

    return {};
}

void TitleDatabase::LoadDataSync() {
    std::vector<std::filesystem::path> candidate_paths;

    // 1. Check userprofile ~/.switch/titledb.json (from STORM SWITCH BOX)
#ifdef _WIN32
    const char* userprofile = std::getenv("USERPROFILE");
    if (userprofile) {
        candidate_paths.push_back(std::filesystem::path(userprofile) / ".switch" / "titledb.json");
        candidate_paths.push_back(std::filesystem::path(userprofile) / ".switch" / "titles.json");
    }
#else
    const char* home = std::getenv("HOME");
    if (home) {
        candidate_paths.push_back(std::filesystem::path(home) / ".switch" / "titledb.json");
        candidate_paths.push_back(std::filesystem::path(home) / ".switch" / "titles.json");
    }
#endif

    // 2. Check emulator config, keys & cache paths
    candidate_paths.push_back(Common::FS::GetEdenPath(Common::FS::EdenPath::ConfigDir) / "titledb" / "titledb.json");
    candidate_paths.push_back(Common::FS::GetEdenPath(Common::FS::EdenPath::ConfigDir) / "titledb" / "titles.json");
    candidate_paths.push_back(Common::FS::GetEdenPath(Common::FS::EdenPath::KeysDir) / "titledb.json");
    candidate_paths.push_back(Common::FS::GetEdenPath(Common::FS::EdenPath::KeysDir) / "titles.json");
    candidate_paths.push_back(Common::FS::GetEdenPath(Common::FS::EdenPath::CacheDir) / "titledb.json");
    candidate_paths.push_back(Common::FS::GetEdenPath(Common::FS::EdenPath::CacheDir) / "titles.json");

    std::filesystem::path found_path;
    for (const auto& p : candidate_paths) {
        if (std::filesystem::exists(p)) {
            found_path = p;
            break;
        }
    }

    if (found_path.empty()) {
        LOG_DEBUG(Frontend, "TitleDB database file not found in search paths");
        is_loaded.store(true, std::memory_order_release);
        is_loading.store(false, std::memory_order_release);
        cv.notify_all();
        return;
    }

    LOG_INFO(Frontend, "Loading TitleDB in background from: {}", Common::FS::PathToUTF8String(found_path));

    try {
        std::FILE* f =
#ifdef _WIN32
            _wfopen(found_path.c_str(), L"rb");
#else
            std::fopen(found_path.c_str(), "rb");
#endif
        if (!f) {
            is_loaded.store(true, std::memory_order_release);
            is_loading.store(false, std::memory_order_release);
            cv.notify_all();
            return;
        }

        std::fseek(f, 0, SEEK_END);
        const long sz = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);

        if (sz <= 0) {
            std::fclose(f);
            is_loaded.store(true, std::memory_order_release);
            is_loading.store(false, std::memory_order_release);
            cv.notify_all();
            return;
        }

        std::string json_buffer(static_cast<size_t>(sz), '\0');
        const size_t read_bytes = std::fread(json_buffer.data(), 1, sz, f);
        std::fclose(f);

        if (read_bytes != static_cast<size_t>(sz)) {
            is_loaded.store(true, std::memory_order_release);
            is_loading.store(false, std::memory_order_release);
            cv.notify_all();
            return;
        }

        const auto json_data = nlohmann::json::parse(json_buffer, nullptr, false);
        if (json_data.is_discarded() || !json_data.is_object()) {
            LOG_WARNING(Frontend, "Failed to parse TitleDB JSON");
            is_loaded.store(true, std::memory_order_release);
            is_loading.store(false, std::memory_order_release);
            cv.notify_all();
            return;
        }

        std::unordered_map<std::string, Entry> temp_db;
        std::unordered_map<u64, Entry> temp_by_id;
        std::unordered_map<u64, std::vector<Entry>> temp_base_dlcs;
        std::unordered_map<u64, int> temp_base_dlc_counts;

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
            if (entry.name.empty() && !entry.description.empty()) {
                const std::string extracted = ExtractDlcTitleFromDesc(entry.description);
                if (!extracted.empty()) {
                    entry.name = extracted;
                }
            }
            if (val.contains("version") && !val["version"].is_null()) {
                if (val["version"].is_string()) {
                    entry.version = val["version"].get<std::string>();
                } else if (val["version"].is_number()) {
                    entry.version = std::to_string(val["version"].get<u64>());
                }
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

            u64 tid_val = 0;
            if (key.length() == 16) {
                try {
                    tid_val = std::stoull(key, nullptr, 16);
                } catch (...) {
                    tid_val = 0;
                }
            }

            if (tid_val != 0) {
                temp_by_id.emplace(tid_val, entry);

                // Index DLCs
                if ((tid_val & 0x1FFF) >= 0x1000 || ((tid_val & 0xFFF) > 0 && (tid_val & 0x800) == 0)) {
                    const u64 base_id = (tid_val & ~0x1000ULL) & ~0xFFFULL;
                    const u64 base_masked = tid_val & 0xFFFFFFFFFFFFE000;
                    temp_base_dlcs[base_id].push_back(entry);
                    temp_base_dlc_counts[base_id]++;
                    if (base_masked != base_id) {
                        temp_base_dlcs[base_masked].push_back(entry);
                        temp_base_dlc_counts[base_masked]++;
                    }
                }
            }

            temp_db.emplace(std::move(key), std::move(entry));
        }

        // Sort DLC lists by Title ID
        for (auto& [bid, dlc_list] : temp_base_dlcs) {
            std::sort(dlc_list.begin(), dlc_list.end(), [](const Entry& a, const Entry& b) {
                return a.id < b.id;
            });
        }

        {
            std::lock_guard<std::mutex> lock(db_mutex);
            db = std::move(temp_db);
            db_by_id = std::move(temp_by_id);
            base_to_dlcs = std::move(temp_base_dlcs);
            base_to_dlc_count = std::move(temp_base_dlc_counts);
        }

        LOG_INFO(Frontend, "TitleDB loaded successfully with {} entries in background", db.size());
    } catch (const std::exception& e) {
        LOG_WARNING(Frontend, "Exception while parsing TitleDB: {}", e.what());
    }

    is_loaded.store(true, std::memory_order_release);
    is_loading.store(false, std::memory_order_release);
    cv.notify_all();
}

std::optional<Entry> TitleDatabase::Lookup(u64 title_id) {
    std::lock_guard<std::mutex> lock(db_mutex);
    auto it = db_by_id.find(title_id);
    if (it != db_by_id.end()) {
        if (!it->second.name.empty()) {
            return it->second;
        }
        if (!it->second.description.empty()) {
            const std::string extracted = ExtractDlcTitleFromDesc(it->second.description);
            if (!extracted.empty()) {
                it->second.name = extracted;
                return it->second;
            }
        }
    }

    // Check if it's an Update or DLC
    const bool is_update = ((title_id & 0x800) != 0);
    const bool is_dlc = ((title_id & 0x1FFF) >= 0x1000);

    u64 base_tid = 0;
    if (is_update) {
        base_tid = title_id - 0x800;
    } else {
        base_tid = (title_id & ~0x1000ULL) & ~0xFFFULL;
    }

    auto base_it = db_by_id.find(base_tid);
    if (base_it == db_by_id.end()) {
        base_tid = title_id & 0xFFFFFFFFFFFFE000;
        base_it = db_by_id.find(base_tid);
    }

    if (base_it != db_by_id.end() && !base_it->second.name.empty()) {
        Entry fallback_entry;
        const std::string exact_key = fmt::format("{:016X}", title_id);
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

    return std::nullopt;
}

std::optional<Entry> TitleDatabase::Lookup(std::string_view hex_id) {
    try {
        u64 tid = std::stoull(std::string(hex_id), nullptr, 16);
        return Lookup(tid);
    } catch (...) {
        std::string key(hex_id);
        std::transform(key.begin(), key.end(), key.begin(), ::toupper);

        std::lock_guard<std::mutex> lock(db_mutex);
        auto it = db.find(key);
        if (it != db.end()) {
            return it->second;
        }
        return std::nullopt;
    }
}

std::vector<Entry> TitleDatabase::GetDlcs(u64 base_title_id) {
    std::lock_guard<std::mutex> lock(db_mutex);
    const u64 base_clean = (base_title_id & ~0x1000ULL) & ~0xFFFULL;
    auto it = base_to_dlcs.find(base_clean);
    if (it != base_to_dlcs.end()) {
        return it->second;
    }
    const u64 base_masked = base_title_id & 0xFFFFFFFFFFFFE000;
    auto it2 = base_to_dlcs.find(base_masked);
    if (it2 != base_to_dlcs.end()) {
        return it2->second;
    }
    return {};
}

int TitleDatabase::GetDlcCount(u64 base_title_id) {
    std::lock_guard<std::mutex> lock(db_mutex);
    const u64 base_clean = (base_title_id & ~0x1000ULL) & ~0xFFFULL;
    auto it = base_to_dlc_count.find(base_clean);
    if (it != base_to_dlc_count.end()) {
        return it->second;
    }
    const u64 base_masked = base_title_id & 0xFFFFFFFFFFFFE000;
    auto it2 = base_to_dlc_count.find(base_masked);
    if (it2 != base_to_dlc_count.end()) {
        return it2->second;
    }
    return 0;
}

} // namespace TitleDB
