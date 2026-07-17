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
#include "core/file_sys/program_metadata.h"
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
    return 0x10000;
}

AppLoader_NCA::AppLoader_NCA(FileSys::VirtualFile file_)
    : AppLoader(std::move(file_)), nca(std::make_unique<FileSys::NCA>(file)) {}

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
    bool exefs_from_update = false;

    LOG_INFO(Loader, "NCA Load: base NCA ExeFS = {}, base NCA title_id = {:016X}",
             exefs != nullptr ? "present" : "null", nca->GetTitleId());

    // If we have a packed update (NSZ/NSP), prefer its ExeFS!
    if (update_raw != nullptr) {
        LOG_INFO(Loader, "Checking packed update NCA for ExeFS... (update_raw size: {})",
                 update_raw->GetSize());
        update_nca_ptr = std::make_unique<FileSys::NCA>(update_raw, nca.get());
        LOG_INFO(Loader, "Update NCA status: {}, type: {}, is_update: {}",
                 static_cast<int>(update_nca_ptr->GetStatus()),
                 static_cast<int>(update_nca_ptr->GetType()),
                 update_nca_ptr->IsUpdate());
        if (update_nca_ptr->GetStatus() == ResultStatus::Success) {
            auto update_exefs = update_nca_ptr->GetExeFS();
            if (update_exefs != nullptr) {
                // Log update ExeFS contents for diagnostics
                LOG_INFO(Loader, "Update ExeFS contents:");
                for (const auto& f : update_exefs->GetFiles()) {
                    LOG_INFO(Loader, "  {} ({} bytes)", f->GetName(), f->GetSize());
                }

                // Verify main NSO is readable
                auto main_nso = update_exefs->GetFile("main");
                if (main_nso) {
                    std::vector<u8> header(4);
                    auto read = main_nso->Read(header.data(), 4, 0);
                    LOG_INFO(Loader, "  main NSO header: {:02X} {:02X} {:02X} {:02X} (read {} bytes)",
                             header[0], header[1], header[2], header[3], read);
                }

                exefs = update_exefs;
                exefs_from_update = true;
                LOG_INFO(Loader, "Using ExeFS from packed update NCA");
            } else {
                LOG_WARNING(Loader, "Update NCA has no ExeFS!");
            }
        } else {
            LOG_WARNING(Loader, "Update NCA status is not Success: {}",
                        static_cast<int>(update_nca_ptr->GetStatus()));
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

    // When ExeFS already comes from the packed update NCA, tell PatchExeFS to skip
    // applying the update again from the content provider (prevents double-patching).
    directory_loader = std::make_unique<AppLoader_DeconstructedRomDirectory>(exefs, true,
                                                                            false, exefs_from_update);

    // Read system resource size from main.npdm in ExeFS using the proper parser
    u64 heap_size = 0;

    if (exefs) {
        const auto npdm_file = exefs->GetFile("main.npdm");
        if (npdm_file) {
            FileSys::ProgramMetadata npdm_meta;
            if (npdm_meta.Load(npdm_file) == ResultStatus::Success) {
                heap_size = npdm_meta.GetSystemResourceSize();
                LOG_INFO(Loader, "Read system resource size {:#x} bytes from main.npdm", heap_size);
            } else {
                LOG_WARNING(Loader, "Failed to parse main.npdm for system resource size");
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
