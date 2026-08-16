// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "common/common_types.h"

namespace TitleDB {

struct Entry {
    std::string id;
    std::string name;
    std::string description;
    std::string version;
    std::string release_date;
};

class TitleDatabase {
public:
    static TitleDatabase& Instance();

    void EnsureLoaded();
    void WaitLoaded(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));
    bool IsLoaded() const;

    std::optional<Entry> Lookup(u64 title_id);
    std::optional<Entry> Lookup(std::string_view hex_id);
    std::vector<Entry> GetDlcs(u64 base_title_id);
    int GetDlcCount(u64 base_title_id);

private:
    TitleDatabase();
    ~TitleDatabase();

    void LoadDataAsync();
    void LoadDataSync();

    std::unordered_map<std::string, Entry> db;
    std::unordered_map<u64, Entry> db_by_id;
    std::unordered_map<u64, std::vector<Entry>> base_to_dlcs;
    std::unordered_map<u64, int> base_to_dlc_count;

    std::atomic<bool> is_loaded{false};
    std::atomic<bool> is_loading{false};
    std::mutex db_mutex;
    std::condition_variable cv;
};

} // namespace TitleDB
