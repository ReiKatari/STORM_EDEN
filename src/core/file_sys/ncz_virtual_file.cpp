// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/file_sys/ncz_virtual_file.h"
#include "common/zstd_compression.h"
#include "common/logging.h"
#include "common/fs/file.h"

#include <cstring>
#include <algorithm>
#include <mutex>
#include "core/file_sys/fssystem/fssystem_utility.h"
#include <vector>
#include <span>
#include <zstd.h>

namespace {
class ZstdContextPool {
private:
    std::mutex mutex;
    std::vector<ZSTD_DCtx*> pool;
public:
    static ZstdContextPool& Get() {
        static ZstdContextPool instance;
        return instance;
    }

    ZSTD_DCtx* Acquire() {
        std::lock_guard<std::mutex> lock(mutex);
        if (!pool.empty()) {
            ZSTD_DCtx* ctx = pool.back();
            pool.pop_back();
            ZSTD_DCtx_reset(ctx, ZSTD_reset_session_only);
            return ctx;
        }
        return ZSTD_createDCtx();
    }

    void Release(ZSTD_DCtx* ctx) {
        if (!ctx) return;
        std::lock_guard<std::mutex> lock(mutex);
        if (pool.size() < 16) {
            pool.push_back(ctx);
        } else {
            ZSTD_freeDCtx(ctx);
        }
    }

    ~ZstdContextPool() {
        for (auto* ctx : pool) {
            ZSTD_freeDCtx(ctx);
        }
    }
};
}

namespace FileSys {

constexpr u64 MAGIC_NCZBLOCK = 0x4B434F4C425A434E; // 'NCZBLOCK'

NCZVirtualFile::NCZVirtualFile(VirtualFile file_) 
    : file(std::move(file_)) {
    if (!file) return;


    constexpr u64 MAGIC_NCZSECTN = 0x4E544345535A434E; // 'NCZSECTN'

    u64 magic = 0;
    std::size_t offset = 0;
    file->ReadObject(&magic, 0);

    if (magic != MAGIC_NCZBLOCK && magic != MAGIC_NCZSECTN) {
        file->ReadObject(&magic, 0x4000);
        if (magic == MAGIC_NCZBLOCK || magic == MAGIC_NCZSECTN) {
            offset = 0x4000;
            LOG_DEBUG(Service_FS, "Found NCZ magic at offset 0x4000 for {}", file->GetName());
            is_header_uncompressed = true;
        } else {
            LOG_INFO(Service_FS, "No NCZ magic in file {}, treating as raw NCA pass-through", file->GetName());
            decompressed_size = file->GetSize();
            is_valid = true;
            is_raw_nca = true;
            return;
        }
    } else {
        LOG_DEBUG(Service_FS, "Found NCZ magic at offset 0 for {}", file->GetName());
    }

    if (magic == MAGIC_NCZSECTN) {
        // is_header_uncompressed already set above if offset is 0x4000
        u64 section_count = 0;
        file->ReadObject(&section_count, offset + 8);
        
        // Some older tools (like StormSwitchBox) write section_count as a 32-bit integer (4 bytes)
        // instead of 64-bit integer (8 bytes), making sections start at offset + 12.
        // We detect this by checking if the upper 32-bits are non-zero (which happens if the next field is non-zero),
        // or by reading the first section and checking if crypto_type is valid.
        u64 sections_start = offset + 16;
        u32 count_32 = static_cast<u32>(section_count & 0xFFFFFFFF);
        
        if ((section_count >> 32) != 0) {
            // Upper bits are not zero, likely means it's a 4-byte count and we read into the next field.
            section_count = count_32;
            sections_start = offset + 12;
        } else {
            // Could still be a 4-byte count if the next 4 bytes happen to be zero (e.g. sec.offset == 0).
            // Let's validate the first section at offset + 16.
            NCZSection test_sec{};
            file->ReadBytes(&test_sec, sizeof(NCZSection), offset + 16);
            if (test_sec.crypto_type > 4) {
                // Invalid crypto type, try offset + 12
                file->ReadBytes(&test_sec, sizeof(NCZSection), offset + 12);
                if (test_sec.crypto_type <= 4) {
                    sections_start = offset + 12;
                }
            }
        }

        sections.resize(section_count);
        file->ReadBytes(sections.data(), section_count * sizeof(NCZSection), sections_start);
        
        s64 current_solid_offset = 0;
        section_solid_offsets.reserve(section_count);
        for (std::size_t i = 0; i < sections.size(); i++) {
            auto& sec = sections[i];
            section_solid_offsets.push_back(current_solid_offset);
            
            u8* bytes = reinterpret_cast<u8*>(&sec);
            LOG_DEBUG(Service_FS, "NCZSECTN Section [{}]: offset={:016X}, size={:016X}, crypto_type={}, solid_offset={:016X}", i, sec.offset, sec.size, sec.crypto_type, current_solid_offset);
            std::string raw_bytes = "";
            for (int j = 0; j < 32; j++) {
                raw_bytes += fmt::format("{:02X} ", bytes[j]);
            }
            LOG_DEBUG(Service_FS, "Raw Bytes [{}]: {}", i, raw_bytes);
            
            // For header_uncompressed, only the portion of the section
            // after 0x4000 is in the ZSTD stream
            if (is_header_uncompressed) {
                s64 sec_end = static_cast<s64>(sec.offset) + static_cast<s64>(sec.size);
                s64 zstd_start = std::max(static_cast<s64>(sec.offset), static_cast<s64>(0x4000));
                s64 in_zstd = std::max(static_cast<s64>(0), sec_end - zstd_start);
                current_solid_offset += in_zstd;
            } else {
                current_solid_offset += sec.size;
            }
        }
        
        offset = sections_start + section_count * sizeof(NCZSection);
        
        // Dynamically find NCZBLOCK magic in next 4096 bytes
        std::vector<u8> search_buffer(4096);
        file->ReadBytes(search_buffer.data(), search_buffer.size(), offset);
        
        bool found = false;
        for (size_t i = 0; i < search_buffer.size() - 8; i++) {
            u64 m;
            std::memcpy(&m, search_buffer.data() + i, sizeof(u64));
            if (m == MAGIC_NCZBLOCK) {
                offset += i;
                found = true;
                break;
            }
        }
        
        if (!found) {
            // It's a Solid ZSTD stream without NCZBLOCK
            is_solid_stream = true;
            
            u32 zstd_magic = 0xFD2FB528;
            bool zstd_found = false;
            for (size_t i = 0; i < search_buffer.size() - 4; i++) {
                u32 m;
                std::memcpy(&m, search_buffer.data() + i, sizeof(u32));
                if (m == zstd_magic) {
                    offset += i;
                    zstd_found = true;
                    break;
                }
            }
            
            if (!zstd_found) {
                LOG_ERROR(Service_FS, "Failed to find Zstd magic in file {}", file->GetName());
                return;
            }
            
            decompressed_size = 0;
            for (const auto& sec : sections) {
                u64 end = static_cast<u64>(sec.offset) + static_cast<u64>(sec.size);
                if (end > decompressed_size) decompressed_size = end;
            }
            
            std::size_t compressed_size = file->GetSize() - offset;
            
            solid_compressed_offset = offset;
            solid_compressed_size = compressed_size;
            is_valid = true;
            LOG_INFO(Service_FS, "Successfully mapped Solid NCZSECTN stream. Virtual NCA Size: {}", decompressed_size);
            return;
        }
    }

    NCZBlockHeader header{};
    file->ReadObject(&header, offset);
    header_size = offset + sizeof(NCZBlockHeader);

    packed_size = header.decompressed_size;
    block_size = 1ULL << header.block_size_exponent;

    if (!sections.empty()) {
        decompressed_size = 0;
        for (const auto& sec : sections) {
            u64 end = static_cast<u64>(sec.offset) + static_cast<u64>(sec.size);
            if (end > decompressed_size) decompressed_size = end;
        }
    } else {
        decompressed_size = packed_size;
        if (is_header_uncompressed) {
            decompressed_size += 0x4000;
        }
    }

    std::vector<u32> compressed_sizes(header.number_of_blocks);
    file->ReadBytes(compressed_sizes.data(), compressed_sizes.size() * sizeof(u32), header_size);
    header_size += compressed_sizes.size() * sizeof(u32);

    u64 current_offset = header_size;
    blocks.reserve(header.number_of_blocks);
    for (u32 size : compressed_sizes) {
        blocks.push_back({current_offset, size});
        current_offset += size;
    }

    is_valid = true;
    LOG_DEBUG(Service_FS, "NCZBLOCK: file={}, packed_size={:X}, block_size={:X}, block_count={}, decompressed_size={:X}, is_header_uncompressed={}, sections_count={}",
                 file->GetName(), packed_size, block_size, blocks.size(), decompressed_size, is_header_uncompressed, sections.size());
}

void NCZVirtualFile::RegisterPartition(s32 fs_index, s64 virtual_offset, s64 virtual_size, s64 physical_offset, s64 physical_size) {
    std::lock_guard<std::mutex> lock(solid_mutex);
    for (const auto& part : registered_partitions) {
        if (part.fs_index == fs_index) return;
    }
    registered_partitions.push_back({fs_index, virtual_offset, virtual_size, physical_offset, physical_size});
    partitions_registered = true;
    LOG_DEBUG(Service_FS, "NCZ RegisterPartition: fs_index={}, v_offset={:016X}, v_size={:016X}, p_offset={:016X}, p_size={:016X}",
                 fs_index, virtual_offset, virtual_size, physical_offset, physical_size);
}

void NCZVirtualFile::SetDecryptedHeader(const u8* data, std::size_t size) {
    decrypted_header.resize(size);
    std::memcpy(decrypted_header.data(), data, size);
    LOG_INFO(Service_FS, "NCZ: Cached decrypted header ({} bytes) for {}", size, GetName());
}

NCZVirtualFile::~NCZVirtualFile() {
    if (solid_dctx) {
        ZstdContextPool::Get().Release(static_cast<ZSTD_DCtx*>(solid_dctx));
    }
}

std::string NCZVirtualFile::GetName() const {
    return file->GetName();
}

std::size_t NCZVirtualFile::GetSize() const {
    // Return the pre-calculated uncompressed size
    return decompressed_size;
}

bool NCZVirtualFile::Resize(std::size_t new_size) {
    return false;
}

VirtualDir NCZVirtualFile::GetContainingDirectory() const {
    return file->GetContainingDirectory();
}

bool NCZVirtualFile::IsWritable() const {
    return false;
}

bool NCZVirtualFile::IsReadable() const {
    return true;
}

std::size_t NCZVirtualFile::Read(u8* data, std::size_t length, std::size_t offset) const {
    if (!is_valid || length == 0 || offset >= decompressed_size) {
        if (!is_valid) LOG_ERROR(Service_FS, "Read called on invalid NCZVirtualFile!");
        return 0;
    }
    


    // Global read_mutex removed for concurrent I/O
    
    auto SafeRead = [](const VirtualFile& vfs_file, u8* buffer, std::size_t size, std::size_t off) -> std::size_t {
        try {
            std::size_t b_read = vfs_file->Read(buffer, size, off);
            if (b_read == 0 && size > 0) {
                LOG_ERROR(Service_FS, "SafeRead: Failed to read {} bytes at {} (Disk disconnected?)", size, off);
            } else if (b_read < size) {
                LOG_ERROR(Service_FS, "SafeRead: Incomplete read! Read {} out of {} bytes at offset {}", b_read, size, off);
            }
            return b_read;
        } catch (const std::exception& e) {
            LOG_CRITICAL(Service_FS, "SafeRead: Exception during I/O! {}", e.what());
            return 0;
        } catch (...) {
            LOG_CRITICAL(Service_FS, "SafeRead: Unknown exception during I/O!");
            return 0;
        }
    };

    std::size_t bytes_read = 0;
    std::size_t remaining = std::min<std::size_t>(length, decompressed_size - offset);
    std::size_t current_offset = offset;

    while (remaining > 0) {
        if (current_offset < 0x4000 && !decrypted_header.empty()) {
            std::size_t to_read = std::min<std::size_t>(remaining, 0x4000 - current_offset);
            std::size_t copy_len = std::min<std::size_t>(to_read, decrypted_header.size() - current_offset);
            std::memcpy(data + bytes_read, decrypted_header.data() + current_offset, copy_len);
            std::size_t read = copy_len;
            if (read < to_read) {
                std::size_t extra = to_read - read;
                std::size_t extra_read = SafeRead(file, data + bytes_read + read, extra, current_offset + read);
                read += extra_read;
            }
            if (read == 0) break;
            bytes_read += read;
            current_offset += read;
            remaining -= read;
            continue;
        }

        if (is_raw_nca) {
            std::size_t read = SafeRead(file, data + bytes_read, remaining, current_offset);
            if (read == 0) break;
            bytes_read += read;
            current_offset += read;
            remaining -= read;
            continue;
        }

        if (is_header_uncompressed && current_offset < 0x4000) {
            std::size_t to_read = std::min<std::size_t>(remaining, 0x4000 - current_offset);
            std::size_t read = SafeRead(file, data + bytes_read, to_read, current_offset);
            if (read == 0) break;
            bytes_read += read;
            current_offset += read;
            remaining -= read;
            continue;
        }

        std::size_t copy_size = remaining;
        u64 mapped_offset = current_offset;
        const NCZSection* active_section = nullptr;

        u64 virtual_offset_in_nca = current_offset;
        std::size_t active_section_idx = 0;
        
        if (!sections.empty()) {
            bool found = false;
            for (std::size_t i = 0; i < sections.size(); ++i) {
                const auto& sec = sections[i];
                u64 sec_offset = static_cast<u64>(sec.offset);
                u64 sec_size = static_cast<u64>(sec.size);
                if (virtual_offset_in_nca >= sec_offset && virtual_offset_in_nca < sec_offset + sec_size) {
                    std::size_t remaining_in_section = (sec_offset + sec_size) - virtual_offset_in_nca;
                    if (copy_size > remaining_in_section) copy_size = remaining_in_section;
                    active_section = &sec;
                    active_section_idx = i;
                    found = true;
                    break;
                }
            }
            if (active_section) {
                if (is_solid_stream) {
                    u64 zstd_sec_start = is_header_uncompressed
                        ? std::max(static_cast<u64>(active_section->offset), static_cast<u64>(0x4000))
                        : static_cast<u64>(active_section->offset);
                    u64 offset_in_sec = virtual_offset_in_nca - zstd_sec_start;
                    mapped_offset = static_cast<u64>(section_solid_offsets[active_section_idx]) + offset_in_sec;
                } else {
                    mapped_offset = virtual_offset_in_nca;
                }
            } else if (found) {
                // fallthrough
            } else {
                std::memset(data + bytes_read, 0, copy_size);
                bytes_read += copy_size;
                current_offset += copy_size;
                remaining -= copy_size;
                continue;
            }
        } else {
            mapped_offset = virtual_offset_in_nca;
        }

        if (is_solid_stream && !solid_decompressed) {
            std::lock_guard<std::mutex> solid_lock(solid_mutex);
            std::size_t required_size = mapped_offset + copy_size;
            
            // Check if we can just serve from cache
            if (required_size <= solid_header_cache.size()) {
                std::memcpy(data + bytes_read, solid_header_cache.data() + mapped_offset, copy_size);
                bytes_read += copy_size;
                current_offset += copy_size;
                remaining -= copy_size;
                continue;
            }

            // Correctly copy cached prefix if the request crosses the boundary
            if (mapped_offset < solid_header_cache.size()) {
                std::size_t cached_copy = std::min<std::size_t>(copy_size, solid_header_cache.size() - mapped_offset);
                std::memcpy(data + bytes_read, solid_header_cache.data() + mapped_offset, cached_copy);
                bytes_read += cached_copy;
                current_offset += cached_copy;
                remaining -= cached_copy;
                continue;
            }

            // If we need data before the current stream position, rewind.
            if (solid_dctx && solid_decomp_offset > mapped_offset) {
                ZstdContextPool::Get().Release(static_cast<ZSTD_DCtx*>(solid_dctx));
                solid_dctx = nullptr;
            }

            if (!solid_dctx) {
                solid_dctx = ZstdContextPool::Get().Acquire();
                solid_comp_offset = solid_compressed_offset;
                solid_remaining_comp = solid_compressed_size;
                solid_comp_chunk.resize(1024 * 1024);
                solid_carry_over = 0;
                solid_decomp_offset = 0;
            }

            std::vector<u8> decomp_buffer(1024 * 1024);
            std::size_t actual_copied = 0;

            while (solid_decomp_offset < required_size) {
                std::size_t to_read = std::min<std::size_t>(solid_comp_chunk.size() - solid_carry_over, solid_remaining_comp);
                if (to_read > 0) {
                    std::size_t r = SafeRead(file, solid_comp_chunk.data() + solid_carry_over, to_read, solid_comp_offset);
                    if (r == 0) break;
                    to_read = r;
                }
                
                ZSTD_inBuffer input = { solid_comp_chunk.data(), to_read + solid_carry_over, 0 };
                ZSTD_outBuffer output = { decomp_buffer.data(), decomp_buffer.size(), 0 };
                
                std::size_t ret = ZSTD_decompressStream(static_cast<ZSTD_DCtx*>(solid_dctx), &output, &input);
                if (ZSTD_isError(ret)) {
                    LOG_ERROR(Service_FS, "Failed to stream solid stream! ZSTD Error: {}", ZSTD_getErrorName(ret));
                    break;
                }
                
                std::size_t consumed = input.pos;
                std::size_t produced = output.pos;
                
                if (consumed == 0 && produced == 0 && to_read == 0) {
                    break;
                }
                
                std::size_t chunk_start = solid_decomp_offset;
                std::size_t chunk_end = solid_decomp_offset + produced;

                // 1. Cache the entire decompressed stream (Zero-Copy emulation concept)
                if (produced > 0) {
                    if (chunk_end <= MAX_SOLID_CACHE_SIZE) {
                        if (solid_header_cache.size() < chunk_end) {
                            solid_header_cache.resize(chunk_end);
                        }
                        std::memcpy(solid_header_cache.data() + chunk_start, decomp_buffer.data(), produced);
                    } else if (chunk_start < MAX_SOLID_CACHE_SIZE) {
                        // Partially cache the part that fits
                        std::size_t fit_produced = MAX_SOLID_CACHE_SIZE - chunk_start;
                        if (solid_header_cache.size() < MAX_SOLID_CACHE_SIZE) {
                            solid_header_cache.resize(MAX_SOLID_CACHE_SIZE);
                        }
                        std::memcpy(solid_header_cache.data() + chunk_start, decomp_buffer.data(), fit_produced);
                    }
                }

                // 2. Copy to user buffer if it overlaps
                std::size_t req_start = mapped_offset;
                std::size_t req_end = mapped_offset + copy_size;
                
                std::size_t overlap_start = std::max(chunk_start, req_start);
                std::size_t overlap_end = std::min(chunk_end, req_end);
                
                if (overlap_start < overlap_end) {
                    std::size_t overlap_size = overlap_end - overlap_start;
                    std::size_t src_offset = overlap_start - chunk_start;
                    std::size_t dst_offset = overlap_start - req_start;
                    
                    std::memcpy(data + bytes_read + dst_offset, decomp_buffer.data() + src_offset, overlap_size);
                    actual_copied += overlap_size;
                }

                solid_decomp_offset += produced;
                
                solid_comp_offset += to_read;
                solid_remaining_comp -= to_read;
                
                solid_carry_over = input.size - consumed;
                if (solid_carry_over > 0) {
                    std::memmove(solid_comp_chunk.data(), solid_comp_chunk.data() + consumed, solid_carry_over);
                }
            }
            if (actual_copied < copy_size) {
                std::memset(data + bytes_read, 0, copy_size - actual_copied);
                actual_copied = copy_size;
            }
            
            bytes_read += actual_copied;
            current_offset += actual_copied;
            remaining -= actual_copied;
            
            continue;
        }

        std::size_t block_index = 0;
        std::size_t block_offset = 0;
        
        if (!is_solid_stream) {
            u64 relative_offset = mapped_offset;
            if (is_header_uncompressed) {
                if (mapped_offset < 0x4000) {
                    // Header area - should have been handled above by decrypted_header or direct read
                    std::memset(data + bytes_read, 0, copy_size);
                    bytes_read += copy_size;
                    current_offset += copy_size;
                    remaining -= copy_size;
                    continue;
                }
                relative_offset = mapped_offset - 0x4000;
            }
            
            block_index = relative_offset / block_size;
            block_offset = relative_offset % block_size;
            if (block_index >= blocks.size()) {
                LOG_ERROR(Service_FS, "block_index {} out of range (blocks count: {}) for physical offset {}, relative {}",
                          block_index, blocks.size(), mapped_offset, relative_offset);
                break;
            }
            copy_size = std::min<std::size_t>(copy_size, block_size - block_offset);
        }

        if (!is_solid_stream) {
            std::unique_lock<std::mutex> cache_lock(cache_mutex);
            
            auto cache_it = std::find_if(block_cache.begin(), block_cache.end(),
                [block_index](const BlockCacheEntry& entry) { return entry.index == block_index; });
                
            if (cache_it == block_cache.end() && prefetching_block_index == block_index && prefetch_future.valid()) {
                std::shared_future<void> fut = prefetch_future;
                cache_lock.unlock();
                fut.wait();
                cache_lock.lock();
                
                cache_it = std::find_if(block_cache.begin(), block_cache.end(),
                    [block_index](const BlockCacheEntry& entry) { return entry.index == block_index; });
            }

            const auto& block = blocks[block_index];
            std::size_t expected_decompressed_size = block_size;
            if (block_index == blocks.size() - 1) {
                // packed_size is the total ZSTD decompressed size (excludes 0x4000 NCA header per NCZ spec)
                expected_decompressed_size = packed_size % block_size;
                if (expected_decompressed_size == 0) expected_decompressed_size = block_size;
            }

            if (block.compressed_size == expected_decompressed_size) {
                std::size_t read = SafeRead(file, data + bytes_read, copy_size, block.offset + block_offset);
                if (read == 0) break;
                copy_size = read;
            } else {
                const std::vector<u8>* decompressed_ptr = nullptr;
                
                if (cache_it != block_cache.end()) {
                    if (cache_it != block_cache.begin()) {
                        std::rotate(block_cache.begin(), cache_it, cache_it + 1);
                    }
                    decompressed_ptr = &block_cache.front().data;
                } else {
                    thread_local std::vector<u8> compressed_data;
                    compressed_data.resize(block.compressed_size);
                    if (SafeRead(file, compressed_data.data(), block.compressed_size, block.offset) != block.compressed_size) {
                        break;
                    }

                    std::vector<u8> decomp = Common::Compression::DecompressDataZSTD(compressed_data);
                    if (decomp.empty()) {
                        LOG_ERROR(Service_FS, "ZSTD decompression failed at block {}", block_index);
                        break;
                    }
                    if (decomp.size() != expected_decompressed_size) {
                        LOG_CRITICAL(Service_FS, "ZSTD block {} size mismatch! Expected: {}, Got: {}, compressed_size: {}, file: {}",
                                     block_index, expected_decompressed_size, decomp.size(), block.compressed_size, file->GetName());
                    }

                    if (block_cache.size() >= 32) {
                        block_cache.pop_back();
                    }
                    block_cache.insert(block_cache.begin(), {block_index, std::move(decomp)});
                    
                    // Async Read-ahead: prefetch next block if sequential access pattern
                    if (last_accessed_block != SIZE_MAX && block_index == last_accessed_block + 1 &&
                        block_index + 1 < blocks.size()) {
                        
                        std::size_t next_idx = block_index + 1;
                        auto prefetch_it = std::find_if(block_cache.begin(), block_cache.end(),
                            [next_idx](const BlockCacheEntry& entry) { return entry.index == next_idx; });
                            
                        if (prefetch_it == block_cache.end() && prefetching_block_index != next_idx) {
                            if (prefetch_future.valid()) {
                                std::shared_future<void> old_fut = prefetch_future;
                                cache_lock.unlock();
                                old_fut.wait();
                                cache_lock.lock();
                            }

                            // Recheck block_cache in case it was loaded while we were unlocked/waiting
                            prefetch_it = std::find_if(block_cache.begin(), block_cache.end(),
                                [next_idx](const BlockCacheEntry& entry) { return entry.index == next_idx; });

                            if (prefetch_it == block_cache.end() && prefetching_block_index != next_idx) {
                                const auto& next_block = blocks[next_idx];
                                std::size_t next_expected_size = block_size;
                                if (next_idx == blocks.size() - 1) {
                                    next_expected_size = packed_size % block_size;
                                    if (next_expected_size == 0) next_expected_size = block_size;
                                }
                                
                                auto f = file;
                                auto block_offset_val = next_block.offset;
                                auto block_comp_size = next_block.compressed_size;
                                
                                prefetching_block_index = next_idx;
                                prefetch_future = std::async(std::launch::async, [this, f, next_idx, block_offset_val, block_comp_size, next_expected_size]() {
                                    std::vector<u8> comp(block_comp_size);
                                    if (f->Read(comp.data(), block_comp_size, block_offset_val) == block_comp_size) {
                                        std::vector<u8> decomp = Common::Compression::DecompressDataZSTD(comp);
                                        if (!decomp.empty()) {
                                            if (decomp.size() != next_expected_size) {
                                                LOG_CRITICAL(Service_FS, "ZSTD block {} size mismatch! Expected: {}, Got: {}, compressed_size: {}, file: {}",
                                                             next_idx, next_expected_size, decomp.size(), block_comp_size, f->GetName());
                                            }
                                            std::lock_guard<std::mutex> lock(cache_mutex);
                                            auto it = std::find_if(block_cache.begin(), block_cache.end(),
                                                [next_idx](const BlockCacheEntry& entry) { return entry.index == next_idx; });
                                            if (it == block_cache.end()) {
                                                if (block_cache.size() >= 32) {
                                                    block_cache.pop_back();
                                                }
                                                block_cache.push_back({next_idx, std::move(decomp)});
                                            }
                                        }
                                    }
                                }).share();
                            }
                        }
                    }
                    last_accessed_block = block_index;
                    decompressed_ptr = &block_cache.front().data;
                }

                std::size_t available = (decompressed_ptr->size() > block_offset) ? (decompressed_ptr->size() - block_offset) : 0;
                copy_size = std::min<std::size_t>(copy_size, available);
                if (copy_size == 0) break;
                
                std::memcpy(data + bytes_read, decompressed_ptr->data() + block_offset, copy_size);
            }
        }

        bytes_read += copy_size;
        current_offset += copy_size;
        remaining -= copy_size;
    }

    return bytes_read;
}

std::size_t NCZVirtualFile::Write(const u8* data, std::size_t length, std::size_t offset) {
    return 0;
}

bool NCZVirtualFile::HasDecryptedSections() const {
    // Solid NCZ files (NCZSECTN) contain pre-decrypted data that bypasses NCA decryption.
    // Block NCZ files (NCZBLOCK) compress the NCA blocks directly (often ciphertext), 
    // so they MUST be decrypted by the NCA loader like normal NCAs.
    return is_valid && !is_raw_nca && !sections.empty();
}

bool NCZVirtualFile::Rename(std::string_view name) {
    return file->Rename(name);
}

bool NCZVirtualFile::DecompressSolidTo(const std::filesystem::path& dest_path) const {
    if (!is_solid_stream) return false;

    Common::FS::IOFile out_file(dest_path, Common::FS::FileAccessMode::Write, Common::FS::FileType::BinaryFile);
    if (!out_file.IsOpen()) return false;

    // 1. Write the decrypted header if we have one
    if (!decrypted_header.empty()) {
        out_file.WriteSpan(std::span<const u8>(decrypted_header.data(), decrypted_header.size()));
    } else {
        // Fallback: copy first 0x4000 bytes from raw file
        std::vector<u8> temp(0x4000);
        std::size_t r = file->Read(temp.data(), 0x4000, 0);
        if (r > 0) {
            out_file.WriteSpan(std::span<const u8>(temp.data(), r));
        }
    }

    // 2. Initialize ZSTD stream decompression
    std::unique_ptr<ZSTD_DCtx, decltype(&ZSTD_freeDCtx)> dctx(ZSTD_createDCtx(), &ZSTD_freeDCtx);
    if (!dctx) return false;

    std::size_t comp_offset = solid_compressed_offset;
    std::size_t remaining_comp = solid_compressed_size;
    std::vector<u8> comp_chunk(16 * 1024 * 1024);
    std::vector<u8> decomp_buffer(16 * 1024 * 1024);
    std::size_t carry_over = 0;

    auto SafeRead = [](const VirtualFile& f, u8* d, std::size_t l, std::size_t o) -> std::size_t {
        try {
            return f->Read(d, l, o);
        } catch (...) {
            return 0;
        }
    };

    u64 total_decompressed_size = 0;
    for (const auto& sec : sections) {
        total_decompressed_size += sec.size;
    }
    u64 total_decompressed_so_far = 0;
    u64 last_log_progress = 0;

    // 3. Decompress each section sequentially
    for (const auto& sec : sections) {
        u64 sec_offset = static_cast<u64>(sec.offset);
        u64 sec_size = static_cast<u64>(sec.size);

        u64 zstd_sec_start = is_header_uncompressed
            ? std::max<u64>(sec_offset, 0x4000)
            : sec_offset;
        
        // Write the uncompressed prefix if needed
        if (sec_offset < zstd_sec_start) {
            u64 prefix_size = zstd_sec_start - sec_offset;
            if (!decrypted_header.empty() && sec_offset < decrypted_header.size()) {
                std::size_t copy_size = std::min<std::size_t>(
                    static_cast<std::size_t>(prefix_size),
                    decrypted_header.size() - static_cast<std::size_t>(sec_offset)
                );
                if (out_file.Seek(static_cast<s64>(sec_offset))) {
                    out_file.WriteSpan(std::span<const u8>(decrypted_header.data() + sec_offset, copy_size));
                    total_decompressed_so_far += copy_size;
                }
            }
        }

        u64 remaining_in_sec = (sec_offset + sec_size) > zstd_sec_start
            ? (sec_offset + sec_size) - zstd_sec_start
            : 0;

        u64 write_offset = zstd_sec_start;
        if (remaining_in_sec > 0) {
            if (!out_file.Seek(static_cast<s64>(write_offset))) {
                return false;
            }
        }

        while (remaining_in_sec > 0) {
            // Fill input buffer if needed
            std::size_t to_read = std::min<std::size_t>(comp_chunk.size() - carry_over, remaining_comp);
            if (to_read > 0) {
                std::size_t r = SafeRead(file, comp_chunk.data() + carry_over, to_read, comp_offset);
                if (r > 0) {
                    comp_offset += r;
                    remaining_comp -= r;
                    carry_over += r;
                }
            }

            ZSTD_inBuffer input = { comp_chunk.data(), carry_over, 0 };
            ZSTD_outBuffer output = { decomp_buffer.data(), std::min<std::size_t>(decomp_buffer.size(), remaining_in_sec), 0 };

            std::size_t ret = ZSTD_decompressStream(dctx.get(), &output, &input);
            if (ZSTD_isError(ret)) {
                LOG_ERROR(Service_FS, "DecompressSolidTo: ZSTD Error: {}", ZSTD_getErrorName(ret));
                return false;
            }

            std::size_t consumed = input.pos;
            std::size_t produced = output.pos;

            if (produced > 0) {
                out_file.WriteSpan(std::span<const u8>(decomp_buffer.data(), produced));
                remaining_in_sec -= produced;
                total_decompressed_so_far += produced;
                
                if (total_decompressed_so_far - last_log_progress >= 512ULL * 1024 * 1024 || total_decompressed_so_far == total_decompressed_size) {
                    double progress_pct = (double)total_decompressed_so_far / total_decompressed_size * 100.0;
                    LOG_INFO(Service_FS, "DecompressSolidTo: Decompressed {:.1f}% ({:.2f} / {:.2f} GB)...",
                             progress_pct, (double)total_decompressed_so_far / (1024*1024*1024),
                             (double)total_decompressed_size / (1024*1024*1024));
                    last_log_progress = total_decompressed_so_far;
                }
            }

            if (consumed > 0) {
                carry_over -= consumed;
                if (carry_over > 0) {
                    std::memmove(comp_chunk.data(), comp_chunk.data() + consumed, carry_over);
                }
            }

            if (consumed == 0 && produced == 0 && to_read == 0) {
                // Done or stuck
                break;
            }
        }
    }

    return true;
}

} // namespace FileSys
