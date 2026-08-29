// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <string>

#include <fmt/ostream.h>

#include "common/logging.h"
#include "core/crypto/key_manager.h"
#include "core/file_sys/card_image.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/nca_metadata.h"
#include "core/file_sys/partition_filesystem.h"
#include "core/file_sys/submission_package.h"
#include "core/file_sys/ncz_virtual_file.h"
#include "core/file_sys/vfs/vfs_offset.h"
#include "core/file_sys/vfs/vfs_vector.h"
#include "core/loader/loader.h"

namespace FileSys {

constexpr u64 GAMECARD_CERTIFICATE_OFFSET = 0x7000;
constexpr std::array partition_names{
    "update",
    "normal",
    "secure",
    "logo",
};

XCI::XCI(VirtualFile file_, u64 program_id, size_t program_index)
    : file(std::move(file_)), program_nca_status{Loader::ResultStatus::ErrorXCIMissingProgramNCA},
      partitions(partition_names.size()),
      partitions_raw(partition_names.size()), keys{Core::Crypto::KeyManager::Instance()} {
    const auto header_status = TryReadHeader();
    if (header_status != Loader::ResultStatus::Success) {
        status = header_status;
        return;
    }

    PartitionFilesystem main_hfs(std::make_shared<OffsetVfsFile>(
        file, file->GetSize() - header.hfs_offset, header.hfs_offset));

    update_normal_partition_end = main_hfs.GetFileOffsets()["secure"];

    if (main_hfs.GetStatus() != Loader::ResultStatus::Success) {
        status = main_hfs.GetStatus();
        return;
    }

    for (XCIPartition partition :
         {XCIPartition::Update, XCIPartition::Normal, XCIPartition::Secure, XCIPartition::Logo}) {
        const auto partition_idx = static_cast<std::size_t>(partition);
        auto raw = main_hfs.GetFile(partition_names[partition_idx]);

        partitions_raw[static_cast<std::size_t>(partition)] = std::move(raw);
    }

    secure_partition = std::make_shared<NSP>(
        main_hfs.GetFile(partition_names[static_cast<std::size_t>(XCIPartition::Secure)]),
        program_id, program_index);

    if (secure_partition != nullptr && secure_partition->GetStatus() == Loader::ResultStatus::Success) {
        ncas = secure_partition->GetNCAsCollapsed();
        program =
            secure_partition->GetNCA(secure_partition->GetProgramTitleID(), ContentRecordType::Program);
        program_nca_status = secure_partition->GetProgramStatus();
        if (program_nca_status == Loader::ResultStatus::ErrorNSPMissingProgramNCA) {
            program_nca_status = Loader::ResultStatus::ErrorXCIMissingProgramNCA;
        }
    }

    // Optional partitions (e.g. Normal, Logo, Update in trimmed or converted dumps)
    AddNCAFromPartition(XCIPartition::Normal);
    if (GetFormatVersion() >= 0x2) {
        AddNCAFromPartition(XCIPartition::Logo);
    }

    if (secure_partition == nullptr || secure_partition->GetStatus() != Loader::ResultStatus::Success) {
        status = secure_partition ? secure_partition->GetStatus() : Loader::ResultStatus::ErrorXCIMissingPartition;
        return;
    }

    status = Loader::ResultStatus::Success;
}

XCI::~XCI() = default;

Loader::ResultStatus XCI::GetStatus() const {
    return status;
}

Loader::ResultStatus XCI::GetProgramNCAStatus() const {
    return program_nca_status;
}

VirtualDir XCI::GetPartition(XCIPartition partition) {
    const auto id = static_cast<std::size_t>(partition);
    if (partitions[id] == nullptr && partitions_raw[id] != nullptr) {
        partitions[id] = std::make_shared<PartitionFilesystem>(partitions_raw[id]);
    }

    return partitions[static_cast<std::size_t>(partition)];
}

std::vector<VirtualDir> XCI::GetPartitions() {
    std::vector<VirtualDir> out;
    for (const auto& id :
         {XCIPartition::Update, XCIPartition::Normal, XCIPartition::Secure, XCIPartition::Logo}) {
        const auto part = GetPartition(id);
        if (part != nullptr) {
            out.push_back(part);
        }
    }
    return out;
}

std::shared_ptr<NSP> XCI::GetSecurePartitionNSP() const {
    return secure_partition;
}

VirtualDir XCI::GetSecurePartition() {
    return GetPartition(XCIPartition::Secure);
}

VirtualDir XCI::GetNormalPartition() {
    return GetPartition(XCIPartition::Normal);
}

VirtualDir XCI::GetUpdatePartition() {
    return GetPartition(XCIPartition::Update);
}

VirtualDir XCI::GetLogoPartition() {
    return GetPartition(XCIPartition::Logo);
}

VirtualFile XCI::GetPartitionRaw(XCIPartition partition) const {
    return partitions_raw[static_cast<std::size_t>(partition)];
}

VirtualFile XCI::GetSecurePartitionRaw() const {
    return GetPartitionRaw(XCIPartition::Secure);
}

VirtualFile XCI::GetStoragePartition0() const {
    return std::make_shared<OffsetVfsFile>(file, update_normal_partition_end, 0, "partition0");
}

VirtualFile XCI::GetStoragePartition1() const {
    return std::make_shared<OffsetVfsFile>(file, file->GetSize() - update_normal_partition_end,
                                           update_normal_partition_end, "partition1");
}

VirtualFile XCI::GetNormalPartitionRaw() const {
    return GetPartitionRaw(XCIPartition::Normal);
}

VirtualFile XCI::GetUpdatePartitionRaw() const {
    return GetPartitionRaw(XCIPartition::Update);
}

VirtualFile XCI::GetLogoPartitionRaw() const {
    return GetPartitionRaw(XCIPartition::Logo);
}

u64 XCI::GetProgramTitleID() const {
    return secure_partition->GetProgramTitleID();
}

std::vector<u64> XCI::GetProgramTitleIDs() const {
    return secure_partition->GetProgramTitleIDs();
}

u32 XCI::GetSystemUpdateVersion() {
    const auto update = GetPartition(XCIPartition::Update);
    if (update == nullptr) {
        return 0;
    }

    for (const auto& update_file : update->GetFiles()) {
        NCA nca{update_file};

        if (nca.GetStatus() != Loader::ResultStatus::Success || nca.GetSubdirectories().empty()) {
            continue;
        }

        if (nca.GetType() == NCAContentType::Meta && nca.GetTitleId() == 0x0100000000000816) {
            const auto dir = nca.GetSubdirectories()[0];
            const auto cnmt = dir->GetFile("SystemUpdate_0100000000000816.cnmt");
            if (cnmt == nullptr) {
                continue;
            }

            CNMT cnmt_data{cnmt};

            const auto metas = cnmt_data.GetMetaRecords();
            if (metas.empty()) {
                continue;
            }

            return metas[0].title_version;
        }
    }

    return 0;
}

u64 XCI::GetSystemUpdateTitleID() const {
    return 0x0100000000000816;
}

bool XCI::HasProgramNCA() const {
    return program != nullptr;
}

VirtualFile XCI::GetProgramNCAFile() const {
    if (!HasProgramNCA()) {
        return nullptr;
    }

    return program->GetBaseFile();
}

const std::vector<std::shared_ptr<NCA>>& XCI::GetNCAs() const {
    return ncas;
}

std::shared_ptr<NCA> XCI::GetNCAByType(NCAContentType type) const {
    const auto program_id = secure_partition->GetProgramTitleID();
    const auto iter =
        std::find_if(ncas.begin(), ncas.end(), [type, program_id](const std::shared_ptr<NCA>& nca) {
            return nca->GetType() == type && nca->GetTitleId() == program_id;
        });
    return iter == ncas.end() ? nullptr : *iter;
}

VirtualFile XCI::GetNCAFileByType(NCAContentType type) const {
    auto nca = GetNCAByType(type);
    if (nca != nullptr) {
        return nca->GetBaseFile();
    }
    return nullptr;
}

std::vector<VirtualFile> XCI::GetFiles() const {
    return {};
}

std::vector<VirtualDir> XCI::GetSubdirectories() const {
    return {};
}

std::string XCI::GetName() const {
    return file->GetName();
}

VirtualDir XCI::GetParentDirectory() const {
    return file->GetContainingDirectory();
}

VirtualDir XCI::ConcatenatedPseudoDirectory() {
    const auto out = std::make_shared<VectorVfsDirectory>();
    for (const auto& part_id : {XCIPartition::Normal, XCIPartition::Logo, XCIPartition::Secure}) {
        const auto& part = GetPartition(part_id);
        if (part == nullptr)
            continue;

        for (const auto& part_file : part->GetFiles())
            out->AddFile(part_file);
    }

    return out;
}

std::array<u8, 0x200> XCI::GetCertificate() const {
    std::array<u8, 0x200> out;
    file->Read(out.data(), out.size(), GAMECARD_CERTIFICATE_OFFSET);
    return out;
}

Loader::ResultStatus XCI::AddNCAFromPartition(XCIPartition part) {
    const auto partition_index = static_cast<std::size_t>(part);
    const auto partition = GetPartition(part);

    if (partition == nullptr) {
        return Loader::ResultStatus::Success;
    }

    for (const VirtualFile& partition_file : partition->GetFiles()) {
        if (partition_file == nullptr) {
            continue;
        }
        if (partition_file->GetExtension() != "nca" && partition_file->GetExtension() != "ncz") {
            continue;
        }

        VirtualFile file_to_use = partition_file;
        bool is_xcz = file->GetName().ends_with(".xcz") || file->GetName().ends_with(".XCZ");
        if (partition_file->GetExtension() == "ncz" || is_xcz) {
             file_to_use = std::make_shared<NCZVirtualFile>(partition_file);
        }

        auto nca = std::make_shared<NCA>(file_to_use);
        if (nca->IsUpdate()) {
            continue;
        }
        if (nca->GetType() == NCAContentType::Program) {
            program_nca_status = nca->GetStatus();
        }
        if (nca->GetStatus() == Loader::ResultStatus::Success) {
            ncas.push_back(std::move(nca));
        } else {
            const u16 error_id = static_cast<u16>(nca->GetStatus());
            LOG_DEBUG(Loader, "Could not load NCA {}/{}, code {:04X} ({})",
                      partition_names[partition_index], nca->GetName(), error_id,
                      nca->GetStatus());
        }
    }

    return Loader::ResultStatus::Success;
}

Loader::ResultStatus XCI::TryReadHeader() {
    const size_t card_image_size = file->GetSize();
    if (card_image_size < sizeof(GamecardHeader)) {
        return Loader::ResultStatus::ErrorBadXCIHeader;
    }

    const auto ReadCardHeaderAt = [&](size_t offset) -> bool {
        if (card_image_size < offset + sizeof(GamecardHeader)) {
            return false;
        }
        GamecardHeader temp_header{};
        if (file->Read(reinterpret_cast<u8*>(&temp_header), sizeof(GamecardHeader), offset) != sizeof(GamecardHeader)) {
            return false;
        }
        if (temp_header.magic == Common::MakeMagic('H', 'E', 'A', 'D')) {
            header = temp_header;
            if (offset > 0) {
                file = std::make_shared<OffsetVfsFile>(file, card_image_size - offset, offset);
            }
            return true;
        }
        // Also check if magic is at offset 0 of the buffer (signature-stripped dump)
        u32 direct_magic = 0;
        std::memcpy(&direct_magic, &temp_header, sizeof(u32));
        if (direct_magic == Common::MakeMagic('H', 'E', 'A', 'D')) {
            std::memset(&header, 0, sizeof(GamecardHeader));
            std::memcpy(reinterpret_cast<u8*>(&header) + 0x100, &temp_header, sizeof(GamecardHeader) - 0x100);
            if (offset > 0) {
                file = std::make_shared<OffsetVfsFile>(file, card_image_size - offset, offset);
            }
            return true;
        }
        return false;
    };

    // Try common offsets where XCI headers or cart dumps begin
    for (size_t candidate_offset : {0x0ULL, 0x1000ULL, 0x200ULL, 0x2000ULL, 0x10000ULL, 0x8000ULL}) {
        if (ReadCardHeaderAt(candidate_offset)) {
            return Loader::ResultStatus::Success;
        }
    }

    return Loader::ResultStatus::ErrorBadXCIHeader;
}

u8 XCI::GetFormatVersion() {
    return GetLogoPartition() == nullptr ? 0x1 : 0x2;
}
} // namespace FileSys
