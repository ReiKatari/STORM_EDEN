// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <vector>

#include "common/common_types.h"
#include "core/core.h"
#include "core/file_sys/card_image.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/control_metadata.h"
#include "core/file_sys/patch_manager.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/romfs_factory.h"
#include "core/file_sys/submission_package.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/loader/nca.h"
#include "core/loader/xci.h"

namespace Loader {

AppLoader_XCI::AppLoader_XCI(FileSys::VirtualFile file_,
                             const Service::FileSystem::FileSystemController& fsc,
                             const FileSys::ContentProvider& content_provider, u64 program_id,
                             std::size_t program_index)
    : AppLoader(file_), xci(std::make_unique<FileSys::XCI>(file_, program_id, program_index)),
      nca_loader(std::make_unique<AppLoader_NCA>(xci->GetProgramNCAFile())) {
    if (xci->GetStatus() != ResultStatus::Success) {
        return;
    }

    const auto control_nca = xci->GetNCAByType(FileSys::NCAContentType::Control);
    if (control_nca != nullptr && control_nca->GetStatus() == ResultStatus::Success) {
        std::tie(nacp_file, icon_file) = [this, &content_provider, &control_nca, &fsc] {
            const FileSys::PatchManager pm{xci->GetProgramTitleID(), fsc, content_provider};
            return pm.ParseControlNCA(*control_nca);
        }();
    } else if (xci->GetSecurePartitionNSP() != nullptr) {
        const auto nca_file = xci->GetSecurePartitionNSP()->GetNCA(xci->GetProgramTitleID(), FileSys::ContentRecordType::Control);
        if (nca_file != nullptr && nca_file->GetStatus() == ResultStatus::Success) {
            const FileSys::PatchManager pm{xci->GetProgramTitleID(), fsc, content_provider};
            std::tie(nacp_file, icon_file) = pm.ParseControlNCA(*nca_file);
        }
    }
}

AppLoader_XCI::~AppLoader_XCI() = default;

FileType AppLoader_XCI::IdentifyType(const FileSys::VirtualFile& xci_file) {
    const FileSys::XCI xci(xci_file);

    if (xci.GetStatus() != ResultStatus::Success) {
        return FileType::Error;
    }

    const bool is_xcz = xci_file && (xci_file->GetName().ends_with(".xcz") || xci_file->GetName().ends_with(".XCZ"));
    const FileType return_type = is_xcz ? FileType::XCZ : FileType::XCI;

    // Identify XCI as a valid container even when it does not include a bootable Program NCA.
    // Bootability is handled by AppLoader_XCI::Load().
    if (xci.GetSecurePartitionNSP() != nullptr) {
        return return_type;
    }

    return FileType::Error;
}

AppLoader_XCI::LoadResult AppLoader_XCI::Load(Kernel::KProcess& process, Core::System& system) {
    if (is_loaded) {
        return {ResultStatus::ErrorAlreadyLoaded, {}};
    }

    if (xci->GetStatus() != ResultStatus::Success) {
        return {xci->GetStatus(), {}};
    }

    if (xci->GetProgramNCAStatus() != ResultStatus::Success) {
        return {xci->GetProgramNCAStatus(), {}};
    }

    if (!xci->HasProgramNCA() && !Core::Crypto::KeyManager::KeyFileExists(false)) {
        return {ResultStatus::ErrorMissingProductionKeyFile, {}};
    }

    const auto result = nca_loader->Load(process, system);
    if (result.first != ResultStatus::Success) {
        return result;
    }

    u64 program_id{};
    ReadProgramId(program_id);
    if (program_id == 0 && xci) {
        program_id = xci->GetProgramTitleID();
    }

    system.GetFileSystemController().RegisterProcess(
        process.GetProcessId(), program_id,
        std::make_shared<FileSys::RomFSFactory>(*this, system.GetContentProvider(),
                                                system.GetFileSystemController()));

    FileSys::VirtualFile update_raw;
    if (ReadUpdateRaw(update_raw) == ResultStatus::Success && update_raw != nullptr) {
        system.GetFileSystemController().SetPackedUpdate(process.GetProcessId(),
                                                         std::move(update_raw));
    }

    is_loaded = true;
    return result;
}

ResultStatus AppLoader_XCI::VerifyIntegrity(std::function<bool(size_t, size_t)> progress_callback) {
    // Verify secure partition, as it is the only thing we can process.
    auto secure_partition = xci->GetSecurePartitionNSP();

    // Get list of all NCAs.
    const auto ncas = secure_partition->GetNCAsCollapsed();

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

ResultStatus AppLoader_XCI::ReadRomFS(FileSys::VirtualFile& out_file) {
    if (xci == nullptr) {
        return ResultStatus::ErrorNotInitialized;
    }
    auto base_nca = xci->GetProgramNCA();
    if (base_nca != nullptr && base_nca->GetRomFS() != nullptr) {
        out_file = base_nca->GetRomFS();
        return ResultStatus::Success;
    }
    if (xci->GetSecurePartitionNSP() != nullptr) {
        for (const auto& nca_item : xci->GetSecurePartitionNSP()->GetNCAsCollapsed()) {
            if (nca_item && nca_item->GetType() == FileSys::NCAContentType::Program &&
                (nca_item->GetTitleId() & 0x800) == 0 &&
                nca_item->GetRomFS() != nullptr) {
                out_file = nca_item->GetRomFS();
                return ResultStatus::Success;
            }
        }
        for (const auto& nca_item : xci->GetSecurePartitionNSP()->GetNCAsCollapsed()) {
            if (nca_item && nca_item->GetType() == FileSys::NCAContentType::Program &&
                nca_item->GetRomFS() != nullptr) {
                out_file = nca_item->GetRomFS();
                return ResultStatus::Success;
            }
        }
    }
    if (nca_loader) {
        return nca_loader->ReadRomFS(out_file);
    }
    return ResultStatus::ErrorNoRomFS;
}

ResultStatus AppLoader_XCI::ReadUpdateRaw(FileSys::VirtualFile& out_file) {
    u64 program_id{};
    nca_loader->ReadProgramId(program_id);
    if (program_id == 0 && xci) {
        program_id = xci->GetProgramTitleID();
    }
    if (program_id == 0) {
        return ResultStatus::ErrorXCIMissingProgramNCA;
    }

    if (xci->GetSecurePartitionNSP() != nullptr) {
        const auto read = xci->GetSecurePartitionNSP()->GetNCAFile(
            FileSys::GetUpdateTitleID(program_id), FileSys::ContentRecordType::Program);
        if (read != nullptr) {
            const auto nca_test = std::make_shared<FileSys::NCA>(read);
            if (nca_test->GetStatus() == ResultStatus::Success ||
                nca_test->GetStatus() == ResultStatus::ErrorMissingBKTRBaseRomFS) {
                out_file = read;
                return ResultStatus::Success;
            }
        }
        for (const auto& nca_item : xci->GetSecurePartitionNSP()->GetNCAsCollapsed()) {
            if (nca_item && nca_item->GetType() == FileSys::NCAContentType::Program &&
                ((nca_item->GetTitleId() & 0x800) != 0 ||
                 nca_item->GetStatus() == ResultStatus::ErrorMissingBKTRBaseRomFS)) {
                out_file = nca_item->GetBaseFile();
                return ResultStatus::Success;
            }
        }
    }

    return ResultStatus::ErrorNoPackedUpdate;
}

std::shared_ptr<FileSys::NCA> AppLoader_XCI::GetNCA() const {
    if (xci == nullptr) {
        return nullptr;
    }
    auto base_nca = xci->GetProgramNCA();
    if (base_nca != nullptr && base_nca->GetRomFS() != nullptr) {
        return base_nca;
    }
    if (xci->GetSecurePartitionNSP() != nullptr) {
        for (const auto& nca_item : xci->GetSecurePartitionNSP()->GetNCAsCollapsed()) {
            if (nca_item && nca_item->GetType() == FileSys::NCAContentType::Program &&
                (nca_item->GetTitleId() & 0x800) == 0 &&
                nca_item->GetRomFS() != nullptr) {
                return nca_item;
            }
        }
        for (const auto& nca_item : xci->GetSecurePartitionNSP()->GetNCAsCollapsed()) {
            if (nca_item && nca_item->GetType() == FileSys::NCAContentType::Program &&
                nca_item->GetRomFS() != nullptr) {
                return nca_item;
            }
        }
    }
    return base_nca;
}

ResultStatus AppLoader_XCI::ReadProgramId(u64& out_program_id) {
    if (nca_loader && nca_loader->ReadProgramId(out_program_id) == ResultStatus::Success && out_program_id != 0) {
        return ResultStatus::Success;
    }
    if (xci && xci->GetStatus() == ResultStatus::Success) {
        out_program_id = xci->GetProgramTitleID();
        if (out_program_id != 0) {
            return ResultStatus::Success;
        }
        const auto ids = xci->GetProgramTitleIDs();
        if (!ids.empty() && ids[0] != 0) {
            out_program_id = ids[0];
            return ResultStatus::Success;
        }
    }
    return ResultStatus::ErrorXCIMissingProgramNCA;
}

ResultStatus AppLoader_XCI::ReadProgramIds(std::vector<u64>& out_program_ids) {
    if (xci && xci->GetStatus() == ResultStatus::Success) {
        out_program_ids = xci->GetProgramTitleIDs();
        if (!out_program_ids.empty()) {
            return ResultStatus::Success;
        }
    }
    if (nca_loader) {
        return nca_loader->ReadProgramIds(out_program_ids);
    }
    return ResultStatus::ErrorXCIMissingProgramNCA;
}

ResultStatus AppLoader_XCI::ReadIcon(std::vector<u8>& buffer) {
    if (icon_file != nullptr) {
        buffer = icon_file->ReadAllBytes();
        if (!buffer.empty()) {
            return ResultStatus::Success;
        }
    }
    return ResultStatus::ErrorNoControl;
}

ResultStatus AppLoader_XCI::ReadTitle(std::string& title) {
    if (nacp_file != nullptr) {
        title = nacp_file->GetApplicationName();
        if (!title.empty()) {
            return ResultStatus::Success;
        }
    }
    return ResultStatus::ErrorNoControl;
}

ResultStatus AppLoader_XCI::ReadControlData(FileSys::NACP& control) {
    if (nacp_file != nullptr) {
        control = *nacp_file;
        return ResultStatus::Success;
    }
    return ResultStatus::ErrorNoControl;
}

ResultStatus AppLoader_XCI::ReadManualRomFS(FileSys::VirtualFile& out_file) {
    const auto nca =
        xci->GetSecurePartitionNSP()->GetNCA(xci->GetSecurePartitionNSP()->GetProgramTitleID(),
                                             FileSys::ContentRecordType::HtmlDocument);
    if (xci->GetStatus() != ResultStatus::Success || nca == nullptr) {
        return ResultStatus::ErrorXCIMissingPartition;
    }

    out_file = nca->GetRomFS();
    return out_file == nullptr ? ResultStatus::ErrorNoRomFS : ResultStatus::Success;
}

ResultStatus AppLoader_XCI::ReadBanner(std::vector<u8>& buffer) {
    return nca_loader->ReadBanner(buffer);
}

ResultStatus AppLoader_XCI::ReadLogo(std::vector<u8>& buffer) {
    return nca_loader->ReadLogo(buffer);
}

ResultStatus AppLoader_XCI::ReadNSOModules(Modules& modules) {
    return nca_loader->ReadNSOModules(modules);
}

} // namespace Loader
