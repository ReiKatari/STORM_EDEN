// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <mutex>
#include <vector>
#include <filesystem>
#include <future>
#include "common/common_types.h"
#include "common/swap.h"
#include "core/file_sys/vfs/vfs.h"

namespace FileSys {

class NCZVirtualFile : public VfsFile {
public:
    static constexpr std::size_t MAX_SOLID_CACHE_SIZE = 256ULL * 1024 * 1024; // 256MB

    explicit NCZVirtualFile(VirtualFile file);
    ~NCZVirtualFile() override;

    std::string GetName() const override;
    std::size_t GetSize() const override;
    bool Resize(std::size_t new_size) override;
    VirtualDir GetContainingDirectory() const override;
    bool IsWritable() const override;
    bool IsReadable() const override;
    std::size_t Read(u8* data, std::size_t length, std::size_t offset) const override;
    std::size_t Write(const u8* data, std::size_t length, std::size_t offset) override;
    bool Rename(std::string_view name) override;
    bool IsNczFile() const override { return is_valid && !is_raw_nca; }
    NCZVirtualFile* GetNczFilePointer() override { return this; }

    bool DecompressSolidTo(const std::filesystem::path& dest_path) const;
    
    struct PartitionInfo {
        s32 fs_index;
        s64 virtual_offset;
        s64 virtual_size;
        s64 physical_offset;
        s64 physical_size;
    };
    std::vector<PartitionInfo> registered_partitions;
    bool partitions_registered = false;
    void RegisterPartition(s32 fs_index, s64 virtual_offset, s64 virtual_size, s64 physical_offset, s64 physical_size);
    void SetDecryptedHeader(const u8* data, std::size_t size);
    bool HasDecryptedSections() const;


#pragma pack(push, 1)
    struct NCZBlockHeader {
        u64 magic; // 'NCZBLOCK' -> 0x4B434F4C425A434E
        u8 version;
        u8 type;
        u8 unused;
        u8 block_size_exponent;
        u32 number_of_blocks;
        u64 decompressed_size;
    };

    struct NCZSection {
        u64 offset; // little-endian 64-bit virtual offset
        u64 size;   // little-endian 64-bit section size
        u8 crypto_type;
        std::array<u8, 7> padding1;
        u64 padding2; // reserved
        std::array<u8, 16> crypto_key;
        std::array<u8, 16> crypto_counter;
    };
#pragma pack(pop)

    const std::vector<NCZSection>& GetSections() const { return sections; }

    VirtualFile file;
    std::size_t decompressed_size = 0;
    std::size_t packed_size = 0;
    std::size_t header_size = 0;
    std::size_t block_size = 0;
    
    struct Block {
        u64 offset;
        u32 compressed_size;
    };
    std::vector<Block> blocks;
    bool is_valid = false;
    bool is_raw_nca = false;
    bool is_header_uncompressed = false;
    
    std::vector<NCZSection> sections;
    std::vector<s64> section_solid_offsets;
    
    bool is_solid_stream = false;
    std::size_t solid_compressed_offset = 0;
    std::size_t solid_compressed_size = 0;
    mutable bool solid_decompressed = false;
    mutable std::vector<u8> solid_cache;
    mutable bool solid_header_decompressed = false;
    mutable std::vector<u8> solid_header_cache;
    
    mutable void* solid_dctx = nullptr;
    mutable std::size_t solid_comp_offset = 0;
    mutable std::size_t solid_remaining_comp = 0;
    mutable std::vector<u8> solid_comp_chunk;
    mutable std::size_t solid_carry_over = 0;
    mutable std::size_t solid_decomp_offset = 0;
    
    mutable std::shared_ptr<VfsFile> disk_cache_file;
    mutable bool disk_cache_checked = false;
    
    struct BlockCacheEntry {
        std::size_t index;
        std::vector<u8> data;
    };
    mutable std::vector<BlockCacheEntry> block_cache;
    
    mutable std::mutex cache_mutex;
    mutable std::mutex solid_mutex;
    mutable std::size_t last_accessed_block = SIZE_MAX;
    mutable std::shared_future<void> prefetch_future;
    mutable std::size_t prefetching_block_index = SIZE_MAX;
    
    // Decrypted NCA header cache (0x0-0x3FFF) - set by NCA driver after header decryption.
    // NCZ files store the raw encrypted NCA header, but body data is pre-decrypted.
    // This cache allows reads at < 0x4000 to return decrypted header bytes.
    std::vector<u8> decrypted_header;
};

} // namespace FileSys
