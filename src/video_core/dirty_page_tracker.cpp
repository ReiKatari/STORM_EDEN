// SPDX-FileCopyrightText: Copyright 2026 STORM EDEN Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include "common/logging.h"
#include "video_core/dirty_page_tracker.h"

namespace VideoCore {

DirtyPageTracker::DirtyPageTracker() = default;

DirtyPageTracker::~DirtyPageTracker() = default;

void DirtyPageTracker::Initialize(size_t total_memory_bytes) {
    std::unique_lock lock(mutex);
    max_pages = (total_memory_bytes + DIRTY_PAGE_SIZE - 1) >> DIRTY_PAGE_SHIFT;
    page_bitmap.assign(max_pages, 0);
    dirty_pages_count = 0;

    LOG_INFO(Render_Vulkan, "DirtyPageTracker: Initialized with {} pages (16KB granularity, {} MB total)",
             max_pages, (total_memory_bytes >> 20));
}

void DirtyPageTracker::Reset() {
    std::unique_lock lock(mutex);
    std::fill(page_bitmap.begin(), page_bitmap.end(), 0);
    dirty_pages_count = 0;
}

void DirtyPageTracker::MarkDirty(u64 address, size_t size) {
    if (size == 0 || max_pages == 0) {
        return;
    }

    const size_t start_page = PageIndex(address);
    const size_t end_page = PageIndex(address + size - 1);

    std::unique_lock lock(mutex);
    const size_t clamped_end = std::min(end_page + 1, max_pages);
    for (size_t i = start_page; i < clamped_end; ++i) {
        if (!page_bitmap[i]) {
            page_bitmap[i] = 1;
            dirty_pages_count++;
        }
    }
}

bool DirtyPageTracker::IsRangeDirty(u64 address, size_t size) const {
    if (size == 0 || max_pages == 0) {
        return false;
    }

    const size_t start_page = PageIndex(address);
    const size_t end_page = PageIndex(address + size - 1);

    std::unique_lock lock(mutex);
    const size_t clamped_end = std::min(end_page + 1, max_pages);
    for (size_t i = start_page; i < clamped_end; ++i) {
        if (page_bitmap[i]) {
            return true;
        }
    }
    return false;
}

std::vector<DirtyRange> DirtyPageTracker::SynchronizeAndClearRange(u64 address, size_t size) {
    if (size == 0 || max_pages == 0) {
        return {};
    }

    const size_t start_page = PageIndex(address);
    const size_t end_page = PageIndex(address + size - 1);

    std::vector<DirtyRange> dirty_ranges;
    std::unique_lock lock(mutex);
    const size_t clamped_end = std::min(end_page + 1, max_pages);

    size_t run_start = static_cast<size_t>(-1);
    for (size_t i = start_page; i < clamped_end; ++i) {
        if (page_bitmap[i]) {
            if (run_start == static_cast<size_t>(-1)) {
                run_start = i;
            }
            page_bitmap[i] = 0;
            dirty_pages_count--;
        } else {
            if (run_start != static_cast<size_t>(-1)) {
                const u64 range_addr = static_cast<u64>(run_start) << DIRTY_PAGE_SHIFT;
                const size_t range_size = (i - run_start) << DIRTY_PAGE_SHIFT;
                dirty_ranges.push_back({range_addr, range_size});
                run_start = static_cast<size_t>(-1);
            }
        }
    }

    if (run_start != static_cast<size_t>(-1)) {
        const u64 range_addr = static_cast<u64>(run_start) << DIRTY_PAGE_SHIFT;
        const size_t range_size = (clamped_end - run_start) << DIRTY_PAGE_SHIFT;
        dirty_ranges.push_back({range_addr, range_size});
    }

    return dirty_ranges;
}

void DirtyPageTracker::MarkClean(u64 address, size_t size) {
    if (size == 0 || max_pages == 0) {
        return;
    }

    const size_t start_page = PageIndex(address);
    const size_t end_page = PageIndex(address + size - 1);

    std::unique_lock lock(mutex);
    const size_t clamped_end = std::min(end_page + 1, max_pages);
    for (size_t i = start_page; i < clamped_end; ++i) {
        if (page_bitmap[i]) {
            page_bitmap[i] = 0;
            dirty_pages_count--;
        }
    }
}

size_t DirtyPageTracker::GetTotalTrackedPages() const {
    return max_pages;
}

size_t DirtyPageTracker::GetDirtyPageCount() const {
    return dirty_pages_count.load();
}

} // namespace VideoCore
