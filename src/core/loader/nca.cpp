// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <utility>
#include <openssl/err.h>
#include <openssl/evp.h>

#include "common/hex_util.h"
#include "common/scope_exit.h"
#include "core/core.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/nca_metadata.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/romfs_factory.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/loader/deconstructed_rom_directory.h"
#include "core/loader/nca.h"
#include "common/literals.h"
#include <mutex>
#include <filesystem>
#include "common/fs/file.h"
#include "common/fs/path_util.h"
#include "core/file_sys/ncz_virtual_file.h"

namespace Settings {
extern bool is_booting;
}

namespace Loader {

static u32 CalculatePointerBufferSize(size_t heap_size) {
    if (heap_size > 1073741824) { // Games with 1 GiB
        return 0x10000;
    } else if (heap_size > 536870912) { // Games with 512 MiB
        return 0xC000;
    } else {
        return 0x8000; // Default for all other games
    }
}

namespace {
class DiskVfsFile : public FileSys::VfsFile {
public:
    DiskVfsFile(std::filesystem::path path_, std::string name_)
        : path(std::move(path_)), name(std::move(name_)) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        file.Open(path, Common::FS::FileAccessMode::Read, Common::FS::FileType::BinaryFile, Common::FS::FileShareFlag::ShareReadOnly);
    }

    std::string GetName() const override { return name; }
    std::string GetExtension() const override { return name.substr(name.find_last_of('.') + 1); }
    std::size_t GetSize() const override { return file.IsOpen() ? file.GetSize() : 0; }
    bool Resize(std::size_t new_size) override { return false; }
    FileSys::VirtualDir GetContainingDirectory() const override { return nullptr; }
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



static FileSys::VirtualFile DecompressIfNCZ(FileSys::VirtualFile file) {
    if (file == nullptr) return nullptr;
    auto ncz_file = file->IsNczFile() ? std::static_pointer_cast<FileSys::NCZVirtualFile>(file) : nullptr;
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

        if (cache_valid) {
            LOG_INFO(Loader, "Using cached decompressed base NCA");
            return std::make_shared<DiskVfsFile>(cache_path, file->GetName());
        }

        if (!Settings::is_booting) return file;

        LOG_INFO(Loader, "Decompressing base NCZ NCA ({} bytes) to disk cache...", ncz_file->GetSize());
        if (ncz_file->DecompressSolidTo(cache_path)) {
            LOG_INFO(Loader, "Base NCZ NCA decompressed and cached to disk successfully.");
            return std::make_shared<DiskVfsFile>(cache_path, file->GetName());
        } else {
            LOG_ERROR(Loader, "Failed to decompress base NCZ NCA");
            return file;
        }
    }
    return file;
}
}

AppLoader_NCA::AppLoader_NCA(FileSys::VirtualFile file_)
    : AppLoader(DecompressIfNCZ(std::move(file_))), nca(std::make_unique<FileSys::NCA>(file)) {}

AppLoader_NCA::~AppLoader_NCA() = default;

FileType AppLoader_NCA::IdentifyType(const FileSys::VirtualFile& nca_file) {
    const FileSys::NCA nca(nca_file);

    if (nca.GetStatus() == ResultStatus::Success &&
        nca.GetType() == FileSys::NCAContentType::Program) {
        return FileType::NCA;
    }

    return FileType::Error;
}

AppLoader_NCA::LoadResult AppLoader_NCA::Load(Kernel::KProcess& process, Core::System& system) {
    if (is_loaded) {
        return {ResultStatus::ErrorAlreadyLoaded, {}};
    }

    const auto result = nca->GetStatus();
    if (result != ResultStatus::Success) {
        return {result, {}};
    }

    if (nca->GetType() != FileSys::NCAContentType::Program) {
        return {ResultStatus::ErrorNCANotProgram, {}};
    }

    auto exefs = nca->GetExeFS();

    // If we have a packed update (NSZ/NSP), prefer its ExeFS!
    if (update_raw != nullptr) {
        LOG_INFO(Loader, "Checking packed update NCA for ExeFS...");
        FileSys::NCA update_nca(update_raw, nca.get());
        if (update_nca.GetStatus() == ResultStatus::Success) {
            auto update_exefs = update_nca.GetExeFS();
            if (update_exefs != nullptr) {
                exefs = update_exefs;
                LOG_INFO(Loader, "Using ExeFS from packed update NCA");
            }
        }
    }

    if (exefs == nullptr) {
        LOG_INFO(Loader, "No ExeFS found in NCA, looking for ExeFS from update");

        const auto& installed = system.GetContentProvider();
        const auto update_nca = installed.GetEntry(FileSys::GetUpdateTitleID(nca->GetTitleId()),
                                                   FileSys::ContentRecordType::Program);

        if (update_nca) {
            exefs = update_nca->GetExeFS();
        }

        if (exefs == nullptr) {
            LOG_CRITICAL(Loader, "NCA DEBUG: ErrorNoExeFS returned because update_nca->GetExeFS() was also null");
            return {ResultStatus::ErrorNoExeFS, {}};
        }
    }

    directory_loader = std::make_unique<AppLoader_DeconstructedRomDirectory>(exefs, true);

    // Read heap size from main.npdm in ExeFS
    u64 heap_size = 0;

    if (exefs) {
        const auto npdm_file = exefs->GetFile("main.npdm");
        if (npdm_file) {
            auto npdm_data = npdm_file->ReadAllBytes();
            if (npdm_data.size() >= 0x30) {
                heap_size = *reinterpret_cast<const u64*>(&npdm_data[0x28]);
                LOG_INFO(Loader, "Read heap size {:#x} bytes from main.npdm", heap_size);
            } else {
                LOG_WARNING(Loader, "main.npdm too small to read heap size!");
            }
        } else {
            LOG_WARNING(Loader, "No main.npdm found in ExeFS!");
        }
    }

    // Set pointer buffer size based on heap size
    process.SetPointerBufferSize(CalculatePointerBufferSize(heap_size));

    // Load modules
    const auto load_result = directory_loader->Load(process, system);
    if (load_result.first != ResultStatus::Success) {
        return load_result;
    }

    LOG_INFO(Loader, "Set pointer buffer size to {:#x} bytes for ProgramID {:#018x} (Heap size: {:#x})",
             process.GetPointerBufferSize(), nca->GetTitleId(), heap_size);

    // Register the process in the file system controller
    system.GetFileSystemController().RegisterProcess(
        process.GetProcessId(), nca->GetTitleId(),
        std::make_shared<FileSys::RomFSFactory>(*this, system.GetContentProvider(),
                                                system.GetFileSystemController()));

    is_loaded = true;
    return load_result;
}

ResultStatus AppLoader_NCA::VerifyIntegrity(std::function<bool(size_t, size_t)> progress_callback) {
    using namespace Common::Literals;

    constexpr size_t NcaFileNameWithHashLength = 36;
    constexpr size_t NcaFileNameHashLength = 32;
    constexpr size_t NcaSha256HashLength = 32;
    constexpr size_t NcaSha256HalfHashLength = NcaSha256HashLength / 2;

    // Get the file name.
    const auto name = file->GetName();

    // We won't try to verify meta NCAs.
    if (name.ends_with(".cnmt.nca"))
        return ResultStatus::Success;

    // Check if we can verify this file. NCAs should be named after their hashes.
    if (!name.ends_with(".nca") || name.size() != NcaFileNameWithHashLength) {
        LOG_WARNING(Loader, "Unable to validate NCA with name {}", name);
        return ResultStatus::ErrorIntegrityVerificationNotImplemented;
    }

    // Get the expected truncated hash of the NCA.
    const auto input_hash =
        Common::HexStringToVector(file->GetName().substr(0, NcaFileNameHashLength), false);

    // Declare buffer to read into.
    std::vector<u8> buffer(4_MiB);

    // Initialize sha256 verification context.
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx)
        return ResultStatus::ErrorNotInitialized;

    // Ensure we maintain a clean state on exit.
    SCOPE_EXIT {
        EVP_MD_CTX_free(ctx);
    };

    if (!EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr))
        return ResultStatus::ErrorIntegrityVerificationFailed;

    // Declare counters.
    const size_t total_size = file->GetSize();
    size_t processed_size = 0;

    // Begin iterating the file.
    while (processed_size < total_size) {
        // Refill the buffer.
        const size_t intended_read_size = (std::min)(buffer.size(), total_size - processed_size);
        const size_t read_size = file->Read(buffer.data(), intended_read_size, processed_size);

        // Update the hash function with the buffer contents.
        if (!EVP_DigestUpdate(ctx, buffer.data(), read_size)) {
            return ResultStatus::ErrorIntegrityVerificationFailed;
        }

        // Update counters.
        processed_size += read_size;

        // Call the progress function.
        if (!progress_callback(processed_size, total_size)) {
            return ResultStatus::ErrorIntegrityVerificationFailed;
        }
    }

    // Finalize context and compute the output hash.
    std::array<u8, NcaSha256HashLength> output_hash;
    unsigned int output_len = 0;
    if (!EVP_DigestFinal_ex(ctx, output_hash.data(), &output_len)) {
        return ResultStatus::ErrorIntegrityVerificationFailed;
    }

    // Compare to expected.
    if (std::memcmp(input_hash.data(), output_hash.data(), NcaSha256HalfHashLength) != 0) {
        LOG_ERROR(Loader, "NCA hash mismatch detected for file {}", name);
        return ResultStatus::ErrorIntegrityVerificationFailed;
    }

    // File verified.
    return ResultStatus::Success;
}

ResultStatus AppLoader_NCA::ReadRomFS(FileSys::VirtualFile& dir) {
    if (nca == nullptr) {
        return ResultStatus::ErrorNotInitialized;
    }

    if (nca->GetRomFS() == nullptr || nca->GetRomFS()->GetSize() == 0) {
        return ResultStatus::ErrorNoRomFS;
    }

    dir = nca->GetRomFS();
    return ResultStatus::Success;
}

ResultStatus AppLoader_NCA::ReadProgramId(u64& out_program_id) {
    if (nca == nullptr || nca->GetStatus() != ResultStatus::Success) {
        return ResultStatus::ErrorNotInitialized;
    }

    out_program_id = nca->GetTitleId();
    return ResultStatus::Success;
}

ResultStatus AppLoader_NCA::ReadBanner(std::vector<u8>& buffer) {
    if (nca == nullptr || nca->GetStatus() != ResultStatus::Success) {
        return ResultStatus::ErrorNotInitialized;
    }

    const auto logo = nca->GetLogoPartition();
    if (logo == nullptr) {
        return ResultStatus::ErrorNoIcon;
    }

    auto icon_file = logo->GetFile("StartupMovie.gif");
    if (icon_file == nullptr) {
        return ResultStatus::ErrorNoIcon;
    }
    buffer = icon_file->ReadAllBytes();
    return ResultStatus::Success;
}

ResultStatus AppLoader_NCA::ReadLogo(std::vector<u8>& buffer) {
    if (nca == nullptr || nca->GetStatus() != ResultStatus::Success) {
        return ResultStatus::ErrorNotInitialized;
    }

    const auto logo = nca->GetLogoPartition();
    if (logo == nullptr) {
        return ResultStatus::ErrorNoIcon;
    }

    auto icon_file = logo->GetFile("NintendoLogo.png");
    if (icon_file == nullptr) {
        return ResultStatus::ErrorNoIcon;
    }
    buffer = icon_file->ReadAllBytes();
    return ResultStatus::Success;
}

ResultStatus AppLoader_NCA::ReadNSOModules(Modules& modules) {
    if (directory_loader == nullptr) {
        return ResultStatus::ErrorNotInitialized;
    }

    return directory_loader->ReadNSOModules(modules);
}

} // namespace Loader
