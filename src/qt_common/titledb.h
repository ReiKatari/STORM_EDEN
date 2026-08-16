// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
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
    std::optional<Entry> Lookup(u64 title_id);
    std::optional<Entry> Lookup(std::string_view hex_id);
    std::vector<Entry> GetDlcs(u64 base_title_id);
    int GetDlcCount(u64 base_title_id);

private:
    TitleDatabase();
    ~TitleDatabase();

    std::unordered_map<std::string, Entry> db;
    bool is_loaded{false};
    std::mutex load_mutex;
};

} // namespace TitleDB
