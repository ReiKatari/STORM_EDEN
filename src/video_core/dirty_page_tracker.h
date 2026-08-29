// SPDX-FileCopyrightText: Copyright 2026 STORM EDEN Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <span>

#include "common/common_types.h"

namespace VideoCore {

constexpr size_t DIRTY_PAGE_SIZE = 16 * 1024; // 16 KB page granularity
constexpr size_t DIRTY_PAGE_SHIFT = 14;

struct DirtyRange {
    u64 address{0};
    size_t size{0};
};

class DirtyPageTracker {
public:
    explicit DirtyPageTracker();
    ~DirtyPageTracker();

    void Initialize(size_t total_memory_bytes);
    void Reset();

    /// Mark a range of guest memory as dirty (e.g. from CPU/DMA write)
    void MarkDirty(u64 address, size_t size);

    /// Check if a memory range has any dirty pages
    [[nodiscard]] bool IsRangeDirty(u64 address, size_t size) const;

    /// Collect and clear dirty subranges within a specified buffer boundary
    [[nodiscard]] std::vector<DirtyRange> SynchronizeAndClearRange(u64 address, size_t size);

    /// Mark an entire range as clean
    void MarkClean(u64 address, size_t size);

    /// Query statistics
    [[nodiscard]] size_t GetTotalTrackedPages() const;
    [[nodiscard]] size_t GetDirtyPageCount() const;

private:
    [[nodiscard]] size_t PageIndex(u64 address) const {
        return static_cast<size_t>(address >> DIRTY_PAGE_SHIFT);
    }

    mutable std::mutex mutex;
    size_t max_pages{0};
    std::vector<u8> page_bitmap; // 1 = dirty, 0 = clean
    std::atomic<size_t> dirty_pages_count{0};
};

} // namespace VideoCore
