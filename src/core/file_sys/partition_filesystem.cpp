// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <utility>

#include "common/logging.h"
#include "core/file_sys/partition_filesystem.h"
#include "core/file_sys/vfs/vfs_offset.h"
#include "core/loader/loader.h"

namespace FileSys {

bool PartitionFilesystem::Header::HasValidMagicValue() const {
    return magic == Common::MakeMagic('H', 'F', 'S', '0') ||
           magic == Common::MakeMagic('P', 'F', 'S', '0');
}

PartitionFilesystem::PartitionFilesystem(VirtualFile file) {
    if (std::FILE* dbg = nullptr) {
        std::string name = file->GetName();
        if (name.empty()) name = "<empty_name>";
        std::fprintf(dbg, "[PFS DEBUG] Opening PFS: %s (size=%zu)\n", name.c_str(), file->GetSize());
        std::fflush(dbg); std::fclose(dbg);
    }
    // At least be as large as the header
    if (file->GetSize() < sizeof(Header)) {
        if (std::FILE* dbg = nullptr) {
            std::fprintf(dbg, "[PFS DEBUG] Error: file size too small\n");
            std::fflush(dbg); std::fclose(dbg);
        }
        status = Loader::ResultStatus::ErrorBadPFSHeader;
        return;
    }

    // For cartridges, HFSs can get very large, so we need to calculate the size up to
    // the actual content itself instead of just blindly reading in the entire file.
    if (sizeof(Header) != file->ReadObject(&pfs_header)) {
        if (std::FILE* dbg = nullptr) {
            std::fprintf(dbg, "[PFS DEBUG] Error: failed to read header\n");
            std::fflush(dbg); std::fclose(dbg);
        }
        status = Loader::ResultStatus::ErrorBadPFSHeader;
        return;
    }

    if (!pfs_header.HasValidMagicValue()) {
        // Dump first 64 bytes for diagnosis
        std::vector<u8> diag(64, 0);
        file->Read(diag.data(), diag.size(), 0);
        std::string hex_dump;
        for (size_t i = 0; i < diag.size(); i++) {
            char buf[4];
            std::sprintf(buf, "%02X ", diag[i]);
            hex_dump += buf;
        }
        LOG_CRITICAL(Loader, "PartitionFilesystem failed: Invalid Magic {:08X} '{}' file={} size={} first_64_bytes=[{}]", 
            pfs_header.magic, 
            std::string(reinterpret_cast<const char*>(&pfs_header.magic), 4),
            file->GetName(), file->GetSize(), hex_dump);
        LOG_DEBUG(Loader, "PFS DEBUG: Error: Invalid magic {:08X}, file={}, size={}\n  Bytes: {}", 
            pfs_header.magic, file->GetName(), file->GetSize(), hex_dump);
        status = Loader::ResultStatus::ErrorBadPFSHeader;
        return;
    }

    is_hfs = pfs_header.magic == Common::MakeMagic('H', 'F', 'S', '0');

    std::size_t entry_size = is_hfs ? sizeof(HFSEntry) : sizeof(PFSEntry);
    std::size_t metadata_size =
        sizeof(Header) + (pfs_header.num_entries * entry_size) + pfs_header.strtab_size;

    // Actually read in now...
    std::vector<u8> file_data = file->ReadBytes(metadata_size);
    const std::size_t total_size = file_data.size();
    file_data.push_back(0);

    if (total_size != metadata_size) {
        LOG_DEBUG(Loader, "PFS DEBUG: Error: read bytes size {} != metadata_size {}", total_size, metadata_size);
        status = Loader::ResultStatus::ErrorIncorrectPFSFileSize;
        return;
    }

    LOG_DEBUG(Loader, "PFS DEBUG: Read metadata successfully. Num entries: {}", pfs_header.num_entries);

    std::size_t entries_offset = sizeof(Header);
    std::size_t strtab_offset = entries_offset + (pfs_header.num_entries * entry_size);
    content_offset = strtab_offset + pfs_header.strtab_size;
    for (u16 i = 0; i < pfs_header.num_entries; i++) {
        FSEntry entry;

        memcpy(&entry, &file_data[entries_offset + (i * entry_size)], sizeof(FSEntry));
        std::string name(
            reinterpret_cast<const char*>(&file_data[strtab_offset + entry.strtab_offset]));

        offsets.insert_or_assign(name, content_offset + entry.offset);
        sizes.insert_or_assign(name, entry.size);

        pfs_files.emplace_back(std::make_shared<OffsetVfsFile>(
            file, entry.size, content_offset + entry.offset, std::move(name)));
    }

    status = Loader::ResultStatus::Success;
}

PartitionFilesystem::~PartitionFilesystem() = default;

Loader::ResultStatus PartitionFilesystem::GetStatus() const {
    return status;
}

std::map<std::string, u64> PartitionFilesystem::GetFileOffsets() const {
    return offsets;
}

std::map<std::string, u64> PartitionFilesystem::GetFileSizes() const {
    return sizes;
}

std::vector<VirtualFile> PartitionFilesystem::GetFiles() const {
    return pfs_files;
}

std::vector<VirtualDir> PartitionFilesystem::GetSubdirectories() const {
    return {};
}

std::string PartitionFilesystem::GetName() const {
    return is_hfs ? "HFS0" : "PFS0";
}

VirtualDir PartitionFilesystem::GetParentDirectory() const {
    // TODO(DarkLordZach): Add support for nested containers.
    return nullptr;
}

} // namespace FileSys
