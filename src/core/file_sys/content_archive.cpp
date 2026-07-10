// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cstring>
#include <optional>
#include <utility>

#include "common/logging.h"
#include <ranges>
#include "core/crypto/aes_util.h"
#include "core/crypto/ctr_encryption_layer.h"
#include "core/crypto/key_manager.h"
#include "core/file_sys/content_archive.h"
#include <mutex>
#include <filesystem>
#include "common/fs/file.h"
#include "common/fs/path_util.h"
#include "core/file_sys/ncz_virtual_file.h"
#include "core/file_sys/partition_filesystem.h"
#include "core/file_sys/vfs/vfs_offset.h"
#include "core/loader/loader.h"

#include "core/file_sys/fssystem/fssystem_compression_configuration.h"
#include "core/file_sys/fssystem/fssystem_crypto_configuration.h"
#include "core/file_sys/fssystem/fssystem_nca_file_system_driver.h"

namespace Settings {
extern bool is_booting;
}

namespace FileSys {

static u8 MasterKeyIdForKeyGeneration(u8 key_generation) {
    return std::max<u8>(key_generation, 1) - 1;
}

namespace {
class DiskVfsFile : public VfsFile {
public:
    DiskVfsFile(std::filesystem::path path_, std::string name_)
        : path(std::move(path_)), name(std::move(name_)) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        file.Open(path, Common::FS::FileAccessMode::Read, Common::FS::FileType::BinaryFile);
    }

    std::string GetName() const override { return name; }
    std::string GetExtension() const override { return name.substr(name.find_last_of('.') + 1); }
    std::size_t GetSize() const override { return file.IsOpen() ? file.GetSize() : 0; }
    bool Resize(std::size_t new_size) override { return false; }
    VirtualDir GetContainingDirectory() const override { return nullptr; }
    bool IsWritable() const override { return false; }
    bool IsReadable() const override { return file.IsOpen(); }
    bool Rename(std::string_view name_) override { return false; }

    std::size_t Read(u8* data, std::size_t length, std::size_t offset) const override {
        if (!file.IsOpen()) return 0;
        std::lock_guard<std::mutex> lock(io_mutex);
        if (!file.Seek(static_cast<s64>(offset))) return 0;
        return file.ReadSpan(std::span<u8>(data, length));
    }

    std::size_t Write(const u8* data, std::size_t length, std::size_t offset) override {
        return 0;
    }

private:
    std::filesystem::path path;
    std::string name;
    mutable Common::FS::IOFile file;
    mutable std::mutex io_mutex;
};



static VirtualFile DecompressIfNCZ(VirtualFile file) {
    if (file == nullptr) return nullptr;
    if (!Settings::is_booting) return file;
    auto ncz_file = file->IsNczFile() ? std::static_pointer_cast<NCZVirtualFile>(file) : nullptr;
    if (ncz_file && ncz_file->is_solid_stream) {
        std::filesystem::path temp_dir = std::filesystem::path("user") / "cache";
        std::error_code ec;
        std::filesystem::create_directories(temp_dir, ec);
        std::filesystem::path cache_path = temp_dir / (file->GetName() + ".decompressed_cache");

        bool cache_valid = false;
        if (std::filesystem::exists(cache_path, ec)) {
            std::size_t disk_size = std::filesystem::file_size(cache_path, ec);
            if (disk_size == ncz_file->GetSize()) {
                cache_valid = true;
            }
        }

        if (!cache_valid) {
            LOG_INFO(Loader, "NCA: Decompressing solid NCZ NCA ({} bytes) to disk cache...", ncz_file->GetSize());
            if (ncz_file->DecompressSolidTo(cache_path)) {
                LOG_INFO(Loader, "NCA: Solid NCZ NCA decompressed and cached to disk successfully.");
            } else {
                LOG_ERROR(Loader, "NCA: Failed to decompress solid NCZ NCA");
                return file;
            }
        } else {
            LOG_INFO(Loader, "NCA: Using cached decompressed solid NCA");
        }

        return std::make_shared<DiskVfsFile>(cache_path, file->GetName());
    }
    return file;
}
}

NCA::NCA(VirtualFile file_, const NCA* base_nca)
    : file(DecompressIfNCZ(std::move(file_))), keys{Core::Crypto::KeyManager::Instance()} {
    if (file == nullptr) {
        status = Loader::ResultStatus::ErrorNullFile;
        return;
    }

    reader = std::make_shared<NcaReader>();
    if (Result rc = reader->Initialize(file, GetCryptoConfiguration(), GetNcaCompressionConfiguration()); R_FAILED(rc)) {
        LOG_CRITICAL(Loader, "NcaReader Initialize failed with rc={:08X}", rc.raw);
        status = Loader::ResultStatus::ErrorBadNCAHeader;
        return;
    }

    // Ensure we have the proper key area keys to continue.
    const u8 master_key_id = MasterKeyIdForKeyGeneration(reader->GetKeyGeneration());
    if (!keys.HasKey(Core::Crypto::S128KeyType::KeyArea, master_key_id, reader->GetKeyIndex())) {
        status = Loader::ResultStatus::ErrorMissingKeyAreaKey;
        return;
    }

    RightsId rights_id{};
    reader->GetRightsId(rights_id.data(), rights_id.size());
    if (rights_id != RightsId{}) {
        // External decryption key required; provide it here.
        u128 rights_id_u128;
        std::memcpy(rights_id_u128.data(), rights_id.data(), sizeof(rights_id));

        auto titlekey =
            keys.GetKey(Core::Crypto::S128KeyType::Titlekey, rights_id_u128[1], rights_id_u128[0]);
        bool is_fallback_key = false;
        
        if (titlekey == Core::Crypto::Key128{}) {
            // STORM SWITCH BOX God Mode: Assume rights_id IS the TitleKey!
            std::memcpy(titlekey.data(), rights_id.data(), rights_id.size());
            LOG_WARNING(Loader, "TitleKey not found! Falling back to using RightsId as TitleKey for STORM SWITCH BOX!");
            is_fallback_key = true;
        }

        if (!is_fallback_key) {
            if (!keys.HasKey(Core::Crypto::S128KeyType::Titlekek, master_key_id)) {
                status = Loader::ResultStatus::ErrorMissingTitlekek;
                return;
            }

            auto titlekek = keys.GetKey(Core::Crypto::S128KeyType::Titlekek, master_key_id);
            Core::Crypto::AESCipher<Core::Crypto::Key128> cipher(titlekek, Core::Crypto::Mode::ECB);
            cipher.Transcode(titlekey.data(), titlekey.size(), titlekey.data(),
                             Core::Crypto::Op::Decrypt);
        }

        reader->SetExternalDecryptionKey(titlekey.data(), titlekey.size());
    }

    const s32 fs_count = reader->GetFsCount();
    NcaFileSystemDriver fs(base_nca ? base_nca->reader : nullptr, reader);
    std::vector<VirtualFile> filesystems(fs_count);
    for (s32 i = 0; i < fs_count; i++) {
        NcaFsHeaderReader header_reader;
        if (Result rc = fs.OpenStorage(&filesystems[i], &header_reader, i); R_FAILED(rc)) {
            LOG_CRITICAL(Loader, "NcaFileSystemDriver OpenStorage failed for fs_index={} with rc={:08X}", i, rc.raw);
            continue;
        }

        if (header_reader.GetFsType() == NcaFsHeader::FsType::RomFs) {
            files.push_back(filesystems[i]);
            romfs = files.back();
        }

        if (header_reader.GetFsType() == NcaFsHeader::FsType::PartitionFs) {
            partition_files.push_back(filesystems[i]);
        }

        if (header_reader.GetEncryptionType() == NcaFsHeader::EncryptionType::AesCtrEx) {
            is_update = true;
        }
    }

    if (is_update && base_nca == nullptr) {
        status = Loader::ResultStatus::ErrorMissingBKTRBaseRomFS;
    } else {
        status = Loader::ResultStatus::Success;
    }
}

NCA::~NCA() = default;

Loader::ResultStatus NCA::GetStatus() const {
    return status;
}

std::vector<VirtualFile> NCA::GetFiles() const {
    if (status != Loader::ResultStatus::Success) {
        return {};
    }
    return files;
}

std::vector<VirtualDir> NCA::GetSubdirectories() const {
    if (status != Loader::ResultStatus::Success) {
        return {};
    }
    InitializeDirs();
    return dirs;
}

std::string NCA::GetName() const {
    return file->GetName();
}

VirtualDir NCA::GetParentDirectory() const {
    return file->GetContainingDirectory();
}

NCAContentType NCA::GetType() const {
    return static_cast<NCAContentType>(reader->GetContentType());
}

u64 NCA::GetTitleId() const {
    if (is_update) {
        return reader->GetProgramId() | 0x800;
    }
    return reader->GetProgramId();
}

RightsId NCA::GetRightsId() const {
    RightsId result;
    reader->GetRightsId(result.data(), result.size());
    return result;
}

u32 NCA::GetSDKVersion() const {
    return reader->GetSdkAddonVersion();
}

u8 NCA::GetKeyGeneration() const {
    return reader->GetKeyGeneration();
}

bool NCA::IsUpdate() const {
    return is_update;
}

VirtualFile NCA::GetRomFS() const {
    return romfs;
}

VirtualDir NCA::GetExeFS() const {
    InitializeDirs();
    if (exefs == nullptr) {
        LOG_DEBUG(Loader, "NCA DEBUG: GetExeFS: exefs is null after InitializeDirs! (NCA: {}, partition_files.size={})", file->GetName(), partition_files.size());
        for (size_t i = 0; i < partition_files.size(); i++) {
            LOG_CRITICAL(Loader, "  partition_files[{}]: {}", i, partition_files[i]->GetName());
        }
    }
    return exefs;
}

VirtualFile NCA::GetBaseFile() const {
    return file;
}

VirtualDir NCA::GetLogoPartition() const {
    InitializeDirs();
    return logo;
}

void NCA::InitializeDirs() const {
    if (dirs_initialized) {
        return;
    }
    dirs_initialized = true;

    for (const auto& part_file : partition_files) {
        auto npfs = std::make_shared<PartitionFilesystem>(part_file);
        if (npfs->GetStatus() == Loader::ResultStatus::Success) {
            dirs.push_back(npfs);
            if (IsDirectoryExeFS(npfs)) {
                exefs = dirs.back();
            } else if (IsDirectoryLogoPartition(npfs)) {
                logo = dirs.back();
            }
        }
    }
}

} // namespace FileSys
