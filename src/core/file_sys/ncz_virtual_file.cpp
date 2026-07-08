// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/file_sys/ncz_virtual_file.h"
#include "common/zstd_compression.h"
#include "common/logging.h"

#include <cstring>
#include <algorithm>
#include <mutex>
#include <atomic>
#include <chrono>
#include "core/file_sys/fssystem/fssystem_utility.h"
#include <vector>
#include <span>
#include <zstd.h>

namespace {
class ZstdContextPool {
public:
    static ZstdContextPool& Get() {
        static ZstdContextPool instance;
        return instance;
    }

    ZSTD_DCtx* Acquire() {
        thread_local std::vector<ZSTD_DCtx*> local_pool;
        if (!local_pool.empty()) {
            ZSTD_DCtx* ctx = local_pool.back();
            local_pool.pop_back();
            ZSTD_DCtx_reset(ctx, ZSTD_reset_session_only);
            return ctx;
        }
        return ZSTD_createDCtx();
    }

    void Release(ZSTD_DCtx* ctx) {
        if (!ctx) return;
        thread_local std::vector<ZSTD_DCtx*> local_pool;
        if (local_pool.size() < 8) {
            local_pool.push_back(ctx);
        } else {
            ZSTD_freeDCtx(ctx);
        }
    }

    ~ZstdContextPool() {}
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
            std::lock_guard<std::mutex> cache_lock(cache_mutex);
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
                auto cache_it = std::find_if(block_cache.begin(), block_cache.end(),
                    [block_index](const BlockCacheEntry& entry) { return entry.index == block_index; });
                
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
                    // Read-ahead: prefetch next block if sequential access pattern
                    if (last_accessed_block != SIZE_MAX && block_index == last_accessed_block + 1 &&
                        block_index + 1 < blocks.size()) {
                        auto prefetch_it = std::find_if(block_cache.begin(), block_cache.end(),
                            [next = block_index + 1](const BlockCacheEntry& entry) { return entry.index == next; });
                        if (prefetch_it == block_cache.end()) {
                            const auto& next_block = blocks[block_index + 1];
                            thread_local std::vector<u8> prefetch_comp;
                            prefetch_comp.resize(next_block.compressed_size);
                            if (file->Read(prefetch_comp.data(), next_block.compressed_size, next_block.offset) == next_block.compressed_size) {
                                auto prefetch_decomp = Common::Compression::DecompressDataZSTD(prefetch_comp);
                                if (!prefetch_decomp.empty()) {
                                    if (block_cache.size() >= 32) {
                                        block_cache.pop_back();
                                    }
                                    block_cache.push_back({block_index + 1, std::move(prefetch_decomp)});
                                }
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

CachedOnDemandVfsFile::CachedOnDemandVfsFile(VirtualFile source_, std::filesystem::path cache_dir_)
    : source(std::move(source_)) {
    
    std::error_code ec;
    std::filesystem::create_directories(cache_dir_, ec);
    
    total_size = source->GetSize();
    
    std::string safe_name = source->GetName();
    for (char& c : safe_name) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            c = '_';
        }
    }
    
    final_path = cache_dir_ / fmt::format("{}.decompressed", safe_name);
    
    static std::atomic<u64> temp_counter{0};
    u64 timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    tmp_path = cache_dir_ / fmt::format("{}_{}_{}.decompressed.tmp", safe_name, timestamp, temp_counter.fetch_add(1));
    
    if (std::filesystem::exists(final_path, ec) && std::filesystem::file_size(final_path, ec) == total_size) {
        decompressed_until = total_size;
        is_tmp = false;
        cache_file.Open(final_path, Common::FS::FileAccessMode::Read, Common::FS::FileType::BinaryFile);
    } else {
        {
            Common::FS::IOFile creator(tmp_path, Common::FS::FileAccessMode::Write, Common::FS::FileType::BinaryFile);
        }
        
        decompressed_until = 0;
        is_tmp = true;
        cache_file.Open(tmp_path, Common::FS::FileAccessMode::ReadWrite, Common::FS::FileType::BinaryFile);
    }
}

CachedOnDemandVfsFile::~CachedOnDemandVfsFile() {
    std::lock_guard<std::mutex> lock(io_mutex);
    cache_file.Close();
    if (is_tmp) {
        std::error_code ec;
        (void)std::filesystem::remove(tmp_path, ec);
    }
}

std::string CachedOnDemandVfsFile::GetName() const {
    return source->GetName();
}

std::string CachedOnDemandVfsFile::GetExtension() const {
    return source->GetExtension();
}

std::size_t CachedOnDemandVfsFile::GetSize() const {
    return total_size;
}

bool CachedOnDemandVfsFile::Resize(std::size_t new_size) {
    return false;
}

VirtualDir CachedOnDemandVfsFile::GetContainingDirectory() const {
    return nullptr;
}

bool CachedOnDemandVfsFile::IsWritable() const {
    return false;
}

bool CachedOnDemandVfsFile::IsReadable() const {
    return true;
}

std::size_t CachedOnDemandVfsFile::Read(u8* data, std::size_t length, std::size_t offset) const {
    if (length == 0 || offset >= total_size) return 0;
    
    std::size_t real_length = std::min(length, total_size - offset);
    std::size_t target_end = offset + real_length;
    
    std::lock_guard<std::mutex> lock(io_mutex);
    
    if (target_end > decompressed_until) {
        constexpr std::size_t CHUNK_SIZE = 4ULL * 1024 * 1024;
        std::vector<u8> buffer(CHUNK_SIZE);
        
        while (decompressed_until < target_end) {
            std::size_t to_read = std::min(CHUNK_SIZE, total_size - decompressed_until);
            std::size_t read_bytes = source->Read(buffer.data(), to_read, decompressed_until);
            if (read_bytes == 0) {
                break;
            }
            
            (void)cache_file.Seek(decompressed_until);
            (void)cache_file.WriteSpan(std::span<const u8>(buffer.data(), read_bytes));
            
            decompressed_until += read_bytes;
        }
        
        if (decompressed_until == total_size && is_tmp) {
            cache_file.Close();
            std::error_code ec;
            if (!std::filesystem::exists(final_path, ec)) {
                (void)std::filesystem::rename(tmp_path, final_path, ec);
            } else {
                (void)std::filesystem::remove(tmp_path, ec);
            }
            is_tmp = false;
            cache_file.Open(final_path, Common::FS::FileAccessMode::Read, Common::FS::FileType::BinaryFile);
        }
    }
    
    if (!cache_file.Seek(offset)) return 0;
    return cache_file.ReadSpan(std::span<u8>(data, real_length));
}

std::size_t CachedOnDemandVfsFile::Write(const u8* data, std::size_t length, std::size_t offset) {
    return 0;
}

bool CachedOnDemandVfsFile::Rename(std::string_view name) {
    return false;
}

} // namespace FileSys
