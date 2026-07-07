// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <vector>

#include "common/common_types.h"
#include "common/string_util.h"
#include "common/fs/path_util.h"
#include "core/core.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/control_metadata.h"
#include "core/file_sys/nca_metadata.h"
#include "core/file_sys/patch_manager.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/romfs_factory.h"
#include "core/file_sys/partition_filesystem.h"
#include "core/file_sys/submission_package.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/loader/deconstructed_rom_directory.h"
#include "core/loader/nca.h"
#include "core/loader/nsp.h"
#include "core/file_sys/vfs/vfs_vector.h"

namespace Loader {

AppLoader_NSP::AppLoader_NSP(FileSys::VirtualFile file_,
                             const Service::FileSystem::FileSystemController& fsc,
                             const FileSys::ContentProvider& content_provider, u64 program_id,
                             std::size_t program_index)
    : AppLoader(file_), nsp(std::make_unique<FileSys::NSP>(file_, program_id, program_index)) {

    if (nsp->GetStatus() != ResultStatus::Success) {
        return;
    }

    if (nsp->IsExtractedType()) {
        secondary_loader = std::make_unique<AppLoader_DeconstructedRomDirectory>(
            nsp->GetExeFS(), false, file->GetName() == "hbl.nsp");
    } else {
        const auto control_nca =
            nsp->GetNCA(nsp->GetProgramTitleID(), FileSys::ContentRecordType::Control);
        if (control_nca == nullptr) {
            LOG_ERROR(Loader, "NSP/NSZ ICON DEBUG: Control NCA is nullptr for title {:016X}", nsp->GetProgramTitleID());
            if (std::FILE* dbg = nullptr) {
                std::fprintf(dbg, "[ICON DEBUG] Control NCA is nullptr for title %016llX\n",
                             static_cast<unsigned long long>(nsp->GetProgramTitleID()));
                std::fflush(dbg); std::fclose(dbg);
            }
        } else {
            bool has_romfs = control_nca->GetRomFS() != nullptr;
            bool has_exefs = control_nca->GetExeFS() != nullptr;
            LOG_INFO(Loader, "NSP/NSZ ICON DEBUG: Control NCA found. Status={}, Type={}, TitleID={:016X}, RomFS={}, ExeFS={}",
                     static_cast<int>(control_nca->GetStatus()),
                     static_cast<int>(control_nca->GetType()),
                     control_nca->GetTitleId(),
                     has_romfs, has_exefs);
            if (std::FILE* dbg = nullptr) {
                std::fprintf(dbg, "[ICON DEBUG] Control NCA found. Status=%d, Type=%d, TitleID=%016llX, RomFS=%d, ExeFS=%d\n",
                             static_cast<int>(control_nca->GetStatus()),
                             static_cast<int>(control_nca->GetType()),
                             static_cast<unsigned long long>(control_nca->GetTitleId()),
                             has_romfs, has_exefs);
                // Dump RomFS contents if available
                if (has_romfs) {
                    auto romfs = control_nca->GetRomFS();
                    std::fprintf(dbg, "[ICON DEBUG] RomFS size=%zu, IsNcz=%d\n",
                                 romfs->GetSize(), romfs->IsNczFile());
                    // Try to read first 16 bytes of RomFS to check if data is valid
                    std::vector<u8> header(16);
                    auto bytes_read = romfs->Read(header.data(), 16, 0);
                    std::fprintf(dbg, "[ICON DEBUG] RomFS first 16 bytes (read=%zu): ", bytes_read);
                    for (size_t i = 0; i < bytes_read && i < 16; i++)
                        std::fprintf(dbg, "%02X ", header[i]);
                    std::fprintf(dbg, "\n");
                }
                std::fflush(dbg); std::fclose(dbg);
            }
            if (control_nca->GetStatus() == ResultStatus::Success) {
                std::tie(nacp_file, icon_file) = [this, &content_provider, &control_nca, &fsc] {
                    const FileSys::PatchManager pm{nsp->GetProgramTitleID(), fsc, content_provider};
                    return pm.ParseControlNCA(*control_nca);
                }();
                if (std::FILE* dbg = nullptr) {
                    std::fprintf(dbg, "[ICON DEBUG] After ParseControlNCA: nacp=%d, icon=%d, icon_size=%zu\n",
                                 nacp_file != nullptr, icon_file != nullptr,
                                 icon_file ? icon_file->GetSize() : 0);
                    std::fflush(dbg); std::fclose(dbg);
                }
            } else {
                if (std::FILE* dbg = nullptr) {
                    std::fprintf(dbg, "[ICON DEBUG] Control NCA status FAILED (%d), skipping icon\n",
                                 static_cast<int>(control_nca->GetStatus()));
                    std::fflush(dbg); std::fclose(dbg);
                }
            }
        }
        auto nca_file = nsp->GetNCAFile(nsp->GetProgramTitleID(), FileSys::ContentRecordType::Program);
        auto update_nca_file = nsp->GetNCAFile(nsp->GetProgramTitleID() | 0x800, FileSys::ContentRecordType::Program, FileSys::TitleType::Update);
        
        if (update_nca_file) {
            LOG_INFO(Loader, "NSP: Found Update Program NCA, using for ExeFS loading");
            secondary_loader = std::make_unique<AppLoader_NCA>(update_nca_file);
        } else {
            secondary_loader = std::make_unique<AppLoader_NCA>(nca_file);
        }
    }
}

AppLoader_NSP::~AppLoader_NSP() = default;

FileType AppLoader_NSP::IdentifyType(const FileSys::VirtualFile& nsp_file) {
    LOG_DEBUG(Loader, "NSP DEBUG: IdentifyType called for file {}", nsp_file->GetName());
    
    const auto pfs = std::make_shared<FileSys::PartitionFilesystem>(nsp_file);
    if (pfs->GetStatus() != ResultStatus::Success) {
        LOG_DEBUG(Loader, "NSP DEBUG: IdentifyType: PFS GetStatus failed with {}", static_cast<int>(pfs->GetStatus()));
        return FileType::Error;
    }

    const auto extension = Common::ToLower(std::string(Common::FS::GetExtensionFromFilename(nsp_file->GetName())));
    const auto file_type = (extension == "nsz") ? FileType::NSZ : FileType::NSP;

    // Extracted Type case
    if (FileSys::IsDirectoryExeFS(pfs)) {
        return file_type;
    }

    // Check if PFS0 contains any .nca or .ncz files
    for (const auto& entry : pfs->GetFiles()) {
        if (entry == nullptr) {
            continue;
        }

        const auto& name = entry->GetName();
        if (name.size() >= 4 && (name.ends_with(".nca") || name.ends_with(".ncz") || name.ends_with(".NCA") || name.ends_with(".NCZ"))) {
            return file_type;
        }
    }

    if (file_type == FileType::NSZ || file_type == FileType::NSP) {
        return file_type;
    }

    return FileType::Error;
}

AppLoader_NSP::LoadResult AppLoader_NSP::Load(Kernel::KProcess& process, Core::System& system) {
    if (is_loaded) {
        return {ResultStatus::ErrorAlreadyLoaded, {}};
    }

    const auto title_id = nsp->GetProgramTitleID();

    if (!nsp->IsExtractedType() && title_id == 0) {
        LOG_ERROR(Loader, "NSP/NSZ {} has no program title ID.", file->GetName());
        return {ResultStatus::ErrorNSPMissingProgramNCA, {}};
    }

    const auto nsp_status = nsp->GetStatus();
    if (nsp_status != ResultStatus::Success) {
        LOG_ERROR(Loader, "NSP/NSZ {} container parse failed: {}.", file->GetName(), GetResultStatusString(nsp_status));
        return {nsp_status, {}};
    }

    const auto nsp_program_status = nsp->GetProgramStatus();
    if (nsp_program_status != ResultStatus::Success) {
        LOG_ERROR(Loader, "NSP/NSZ {} program status error: {}.", file->GetName(), GetResultStatusString(nsp_program_status));
        return {nsp_program_status, {}};
    }

    if (!nsp->IsExtractedType() &&
        nsp->GetNCA(title_id, FileSys::ContentRecordType::Program) == nullptr) {
        if (!Core::Crypto::KeyManager::KeyFileExists(false)) {
            return {ResultStatus::ErrorMissingProductionKeyFile, {}};
        }

        return {ResultStatus::ErrorNSPMissingProgramNCA, {}};
    }

    FileSys::VirtualFile update_raw;
    if (ReadUpdateRaw(update_raw) == ResultStatus::Success && update_raw != nullptr) {
        if (secondary_loader->GetFileType() == FileType::NCA) {
            auto app_loader_nca = static_cast<AppLoader_NCA*>(secondary_loader.get());
            app_loader_nca->SetUpdateRaw(update_raw);
        }
    }

    const auto result = secondary_loader->Load(process, system);
    if (result.first != ResultStatus::Success) {
        LOG_ERROR(Loader, "NSP/NSZ {} secondary loader failed: {}.", file->GetName(), GetResultStatusString(result.first));
        return result;
    }

    if (nsp->IsExtractedType()) {
        system.GetFileSystemController().RegisterProcess(
            process.GetProcessId(), {},
            std::make_shared<FileSys::RomFSFactory>(*this, system.GetContentProvider(),
                                                    system.GetFileSystemController()));
    }

    if (update_raw != nullptr) {
        system.GetFileSystemController().SetPackedUpdate(process.GetProcessId(),
                                                         std::move(update_raw));
    }

    is_loaded = true;
    return result;
}

ResultStatus AppLoader_NSP::VerifyIntegrity(std::function<bool(size_t, size_t)> progress_callback) {
    // Extracted-type NSPs can't be verified.
    if (nsp->IsExtractedType()) {
        return ResultStatus::ErrorIntegrityVerificationNotImplemented;
    }

    // Get list of all NCAs.
    const auto ncas = nsp->GetNCAsCollapsed();

    size_t total_size = 0;
    size_t processed_size = 0;

    // Loop over NCAs, collecting the total size to verify.
    for (const auto& nca : ncas) {
        total_size += nca->GetBaseFile()->GetSize();
    }

    // Loop over NCAs again, verifying each.
    for (const auto& nca : ncas) {
        AppLoader_NCA loader_nca(nca->GetBaseFile());

        const auto NcaProgressCallback = [&](size_t nca_processed_size, size_t nca_total_size) {
            return progress_callback(processed_size + nca_processed_size, total_size);
        };

        const auto verification_result = loader_nca.VerifyIntegrity(NcaProgressCallback);
        if (verification_result != ResultStatus::Success) {
            return verification_result;
        }

        processed_size += nca->GetBaseFile()->GetSize();
    }

    return ResultStatus::Success;
}

ResultStatus AppLoader_NSP::ReadRomFS(FileSys::VirtualFile& out_file) {
    if (nsp->IsExtractedType()) {
        if (!secondary_loader) {
            return ResultStatus::ErrorNotInitialized;
        }
        return secondary_loader->ReadRomFS(out_file);
    }

    const auto nca = nsp->GetNCA(nsp->GetProgramTitleID(), FileSys::ContentRecordType::Program);
    if (nca == nullptr) {
        return ResultStatus::ErrorNoRomFS;
    }

    out_file = nca->GetRomFS();
    return out_file == nullptr ? ResultStatus::ErrorNoRomFS : ResultStatus::Success;
}

ResultStatus AppLoader_NSP::ReadUpdateRaw(FileSys::VirtualFile& out_file) {
    if (nsp->IsExtractedType()) {
        return ResultStatus::ErrorNoPackedUpdate;
    }

    auto read = nsp->GetNCAFile(FileSys::GetUpdateTitleID(nsp->GetProgramTitleID()),
                                FileSys::ContentRecordType::Program, FileSys::TitleType::Update);

    if (read == nullptr) {
        // Fallback for custom repacked NSPs
        read = nsp->GetNCAFile(FileSys::GetUpdateTitleID(nsp->GetProgramTitleID()),
                               FileSys::ContentRecordType::Program, FileSys::TitleType::Application);
    }

    if (read == nullptr) {
        return ResultStatus::ErrorNoPackedUpdate;
    }

    const auto nca_test = std::make_shared<FileSys::NCA>(read);
    const auto status = nca_test->GetStatus();
    if (status != ResultStatus::ErrorMissingBKTRBaseRomFS && status != ResultStatus::Success) {
        return status;
    }

    // If the update NCA comes from an NCZ, it must be fully decompressed into memory
    // before BKTR patching, because BKTR (AesCtrEx) requires random access which NCZ
    // streaming decompression cannot support.
    if (read->IsNczFile()) {
        const std::size_t total_size = read->GetSize();
        LOG_INFO(Loader, "Update NCA is NCZ-compressed ({} bytes). Decompressing fully for BKTR...", total_size);

        std::vector<u8> decompressed(total_size);
        const std::size_t bytes_read = read->Read(decompressed.data(), total_size, 0);
        if (bytes_read != total_size) {
            LOG_ERROR(Loader, "Failed to fully decompress update NCZ. Read {} of {} bytes.", bytes_read, total_size);
            return ResultStatus::ErrorNoPackedUpdate;
        }

        out_file = std::make_shared<FileSys::VectorVfsFile>(std::move(decompressed), read->GetName());
        LOG_INFO(Loader, "Update NCA decompressed successfully ({} bytes).", total_size);
    } else {
        out_file = read;
    }
    return ResultStatus::Success;
}


ResultStatus AppLoader_NSP::ReadProgramId(u64& out_program_id) {
    out_program_id = nsp->GetProgramTitleID();
    if (out_program_id == 0) {
        return ResultStatus::ErrorNotInitialized;
    }
    return ResultStatus::Success;
}

ResultStatus AppLoader_NSP::ReadProgramIds(std::vector<u64>& out_program_ids) {
    out_program_ids = nsp->GetProgramTitleIDs();
    return ResultStatus::Success;
}

ResultStatus AppLoader_NSP::ReadIcon(std::vector<u8>& buffer) {
    if (icon_file == nullptr) {
        return ResultStatus::ErrorNoControl;
    }

    buffer = icon_file->ReadAllBytes();
    return ResultStatus::Success;
}

ResultStatus AppLoader_NSP::ReadTitle(std::string& title) {
    if (nacp_file == nullptr) {
        return ResultStatus::ErrorNoControl;
    }

    title = nacp_file->GetApplicationName();
    return ResultStatus::Success;
}

ResultStatus AppLoader_NSP::ReadControlData(FileSys::NACP& nacp) {
    if (nacp_file == nullptr) {
        return ResultStatus::ErrorNoControl;
    }

    nacp = *nacp_file;
    return ResultStatus::Success;
}

ResultStatus AppLoader_NSP::ReadManualRomFS(FileSys::VirtualFile& out_file) {
    const auto nca =
        nsp->GetNCA(nsp->GetProgramTitleID(), FileSys::ContentRecordType::HtmlDocument);
    if (nsp->GetStatus() != ResultStatus::Success || nca == nullptr) {
        return ResultStatus::ErrorNoRomFS;
    }

    out_file = nca->GetRomFS();
    return out_file == nullptr ? ResultStatus::ErrorNoRomFS : ResultStatus::Success;
}

ResultStatus AppLoader_NSP::ReadBanner(std::vector<u8>& buffer) {
    if (!secondary_loader) {
        return ResultStatus::ErrorNotInitialized;
    }
    auto status = secondary_loader->ReadBanner(buffer);
    if (status == ResultStatus::Success) {
        return status;
    }
    
    // Fallback for custom/repacked NSPs: search other NCAs for a banner
    for (u64 title_id : nsp->GetProgramTitleIDs()) {
        const auto nca = nsp->GetNCAFile(title_id, FileSys::ContentRecordType::Program);
        if (nca != nullptr) {
            AppLoader_NCA loader(nca);
            if (loader.ReadBanner(buffer) == ResultStatus::Success) {
                return ResultStatus::Success;
            }
        }
    }
    return status;
}

ResultStatus AppLoader_NSP::ReadLogo(std::vector<u8>& buffer) {
    if (!secondary_loader) {
        return ResultStatus::ErrorNotInitialized;
    }
    auto status = secondary_loader->ReadLogo(buffer);
    if (status == ResultStatus::Success) {
        return status;
    }
    
    // Fallback for custom/repacked NSPs: search other NCAs for a logo
    for (u64 title_id : nsp->GetProgramTitleIDs()) {
        const auto nca = nsp->GetNCAFile(title_id, FileSys::ContentRecordType::Program);
        if (nca != nullptr) {
            AppLoader_NCA loader(nca);
            if (loader.ReadLogo(buffer) == ResultStatus::Success) {
                return ResultStatus::Success;
            }
        }
    }
    return status;
}

ResultStatus AppLoader_NSP::ReadNSOModules(Modules& modules) {
    if (!secondary_loader) {
        return ResultStatus::ErrorNotInitialized;
    }
    return secondary_loader->ReadNSOModules(modules);
}

} // namespace Loader
