// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cstring>

#include <fmt/ostream.h>

#include "common/hex_util.h"
#include "common/logging.h"
#include "core/crypto/key_manager.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/nca_metadata.h"
#include "core/file_sys/partition_filesystem.h"
#include "core/file_sys/program_metadata.h"
#include "core/file_sys/submission_package.h"
#include "core/file_sys/ncz_virtual_file.h"
#include "core/loader/loader.h"

namespace FileSys {

NSP::NSP(VirtualFile file_, u64 title_id_, std::size_t program_index_)
    : file(std::move(file_)), expected_program_id(title_id_),
      program_index(program_index_), status{Loader::ResultStatus::Success},
      pfs(std::make_shared<PartitionFilesystem>(file)), keys{Core::Crypto::KeyManager::Instance()} {
    if (pfs->GetStatus() != Loader::ResultStatus::Success) {
        status = pfs->GetStatus();
        return;
    }

    const auto files = pfs->GetFiles();

    if (IsDirectoryExeFS(pfs)) {
        extracted = true;
        InitializeExeFSAndRomFS(files);
        return;
    }

    SetTicketKeys(files);
    ReadNCAs(files);
}

NSP::~NSP() = default;

Loader::ResultStatus NSP::GetStatus() const {
    return status;
}

Loader::ResultStatus NSP::GetProgramStatus() const {
    if (IsExtractedType() && GetExeFS() != nullptr && FileSys::IsDirectoryExeFS(GetExeFS())) {
        return Loader::ResultStatus::Success;
    }

    const auto iter = program_status.find(GetProgramTitleID());
    if (iter == program_status.end())
        return Loader::ResultStatus::ErrorNSPMissingProgramNCA;
    return iter->second;
}

u64 NSP::GetProgramTitleID() const {
    if (IsExtractedType()) {
        return GetExtractedTitleID() + program_index;
    }

    auto program_id = expected_program_id;
    if (program_id == 0) {
        if (!program_status.empty()) {
            program_id = program_status.begin()->first;
        }
    }

    program_id = program_id + program_index;
    if (program_status.find(program_id) != program_status.end()) {
        return program_id;
    }

    const auto ids = GetProgramTitleIDs();
    const auto iter =
        std::find_if(ids.begin(), ids.end(), [](u64 tid) { return (tid & 0x800) == 0; });
    return iter == ids.end() ? 0 : *iter;
}

u64 NSP::GetExtractedTitleID() const {
    if (GetExeFS() == nullptr || !IsDirectoryExeFS(GetExeFS())) {
        return 0;
    }

    ProgramMetadata meta;
    if (meta.Load(GetExeFS()->GetFile("main.npdm")) == Loader::ResultStatus::Success) {
        return meta.GetTitleID();
    } else {
        return 0;
    }
}

std::vector<u64> NSP::GetProgramTitleIDs() const {
    if (IsExtractedType()) {
        return {GetExtractedTitleID()};
    }

    std::vector<u64> out{program_ids.cbegin(), program_ids.cend()};
    return out;
}

bool NSP::IsExtractedType() const {
    return extracted;
}

VirtualFile NSP::GetRomFS() const {
    return romfs;
}

VirtualDir NSP::GetExeFS() const {
    return exefs;
}

std::vector<std::shared_ptr<NCA>> NSP::GetNCAsCollapsed() const {
    if (extracted)
        LOG_WARNING(Service_FS, "called on an NSP that is of type extracted.");
    std::vector<std::shared_ptr<NCA>> out;
    for (const auto& map : ncas) {
        for (const auto& inner_map : map.second)
            out.push_back(inner_map.second);
    }
    return out;
}

std::multimap<u64, std::shared_ptr<NCA>> NSP::GetNCAsByTitleID() const {
    if (extracted)
        LOG_WARNING(Service_FS, "called on an NSP that is of type extracted.");
    std::multimap<u64, std::shared_ptr<NCA>> out;
    for (const auto& map : ncas) {
        for (const auto& inner_map : map.second)
            out.emplace(map.first, inner_map.second);
    }
    return out;
}

std::map<u64, std::map<std::pair<TitleType, ContentRecordType>, std::shared_ptr<NCA>>>
NSP::GetNCAs() const {
    return ncas;
}

std::shared_ptr<NCA> NSP::GetNCA(u64 title_id, ContentRecordType type, TitleType title_type) const {
    if (extracted)
        LOG_WARNING(Service_FS, "called on an NSP that is of type extracted.");

    auto title_id_iter = ncas.find(title_id);
    if (title_id_iter == ncas.end()) {
        if (ncas.size() == 1) {
            title_id_iter = ncas.begin();
        } else {
            title_id_iter = ncas.find(0x0100000000000000ULL);
        }
    }

    if (title_id_iter == ncas.end())
        return nullptr;

    const auto type_iter = title_id_iter->second.find({title_type, type});
    if (type_iter == title_id_iter->second.end())
        return nullptr;

    return type_iter->second;
}

VirtualFile NSP::GetNCAFile(u64 title_id, ContentRecordType type, TitleType title_type) const {
    if (extracted)
        LOG_WARNING(Service_FS, "called on an NSP that is of type extracted.");
    const auto nca = GetNCA(title_id, type, title_type);
    if (nca != nullptr)
        return nca->GetBaseFile();
    return nullptr;
}

std::vector<VirtualFile> NSP::GetFiles() const {
    return pfs->GetFiles();
}

std::vector<VirtualDir> NSP::GetSubdirectories() const {
    return pfs->GetSubdirectories();
}

std::string NSP::GetName() const {
    return file->GetName();
}

VirtualDir NSP::GetParentDirectory() const {
    return file->GetContainingDirectory();
}

void NSP::SetTicketKeys(const std::vector<VirtualFile>& files) {
    for (const auto& ticket_file : files) {
        if (ticket_file == nullptr) {
            continue;
        }

        if (ticket_file->GetExtension() != "tik") {
            continue;
        }

        auto ticket = Core::Crypto::Ticket::Read(ticket_file);
        if (!keys.AddTicket(ticket)) {
            LOG_WARNING(Common_Filesystem, "Could not load NSP ticket {}", ticket_file->GetName());
            continue;
        }
    }
}

void NSP::InitializeExeFSAndRomFS(const std::vector<VirtualFile>& files) {
    exefs = pfs;

    const auto iter = std::find_if(files.begin(), files.end(), [](const VirtualFile& entry) {
        return entry->GetName().rfind(".romfs") != std::string::npos;
    });

    if (iter == files.end()) {
        return;
    }

    romfs = *iter;
}

static bool IsNczFile(const VirtualFile& file) {
    if (!file || file->GetSize() < 8) return false;
    u64 magic = 0;
    if (file->ReadObject(&magic, 0) != sizeof(magic)) return false;
    constexpr u64 MAGIC_NCZBLOCK = 0x4B434F4C425A434E;
    constexpr u64 MAGIC_NCZSECTN = 0x4E544345535A434E;
    LOG_DEBUG(Loader, "NSP DEBUG: IsNczFile '{}' magic@0 = {:016X}", file->GetName(), magic);
    if (magic == MAGIC_NCZBLOCK || magic == MAGIC_NCZSECTN) return true;
    if (file->GetSize() >= 0x4008) {
        if (file->ReadObject(&magic, 0x4000) != sizeof(magic)) return false;
        LOG_DEBUG(Loader, "NSP DEBUG: IsNczFile '{}' magic@0x4000 = {:016X}", file->GetName(), magic);
        if (magic == MAGIC_NCZBLOCK || magic == MAGIC_NCZSECTN) return true;
    }
    return false;
}

void NSP::ReadNCAs(const std::vector<VirtualFile>& files) {
    if (std::FILE* dbg = nullptr) {
        std::fprintf(dbg, "[NSP DEBUG] ReadNCAs called. Container: %s, file count: %zu\n", file->GetName().c_str(), files.size());
        std::fflush(dbg); std::fclose(dbg);
    }
    for (const auto& outer_file : files) {
        if (outer_file == nullptr) {
            continue;
        }
        bool is_cnmt_ncz = outer_file->GetName().ends_with(".cnmt.ncz");
        if (!is_cnmt_ncz && (outer_file->GetName().size() < 9 ||
            outer_file->GetName().substr(outer_file->GetName().size() - 9) != ".cnmt.nca")) {
            continue;
        }

        if (std::FILE* dbg = nullptr) {
            std::fprintf(dbg, "[NSP DEBUG] Processing CNMT: %s (size=%zu)\n", outer_file->GetName().c_str(), outer_file->GetSize());
            std::fflush(dbg); std::fclose(dbg);
        }

        VirtualFile file_to_use = outer_file;
        bool is_nsz = file->GetName().ends_with(".nsz") || file->GetName().ends_with(".xcz") || 
                      file->GetName().ends_with(".NSZ") || file->GetName().ends_with(".XCZ");
                      
        if (is_cnmt_ncz || is_nsz || IsNczFile(outer_file)) {
            if (std::FILE* dbg = nullptr) {
                std::fprintf(dbg, "[NSP DEBUG] Wrapping CNMT in NCZVirtualFile (is_nsz=%d, is_cnmt_ncz=%d)\n", is_nsz, is_cnmt_ncz);
                std::fflush(dbg); std::fclose(dbg);
            }
            file_to_use = std::make_shared<NCZVirtualFile>(outer_file);
        }


        LOG_DEBUG(Loader, "NSP DEBUG: Creating NCA from CNMT file (size={}, IsNcz={})", file_to_use->GetSize(), file_to_use->IsNczFile());
        const auto nca = std::make_shared<NCA>(file_to_use);
        LOG_DEBUG(Loader, "NSP DEBUG: NCA status: {}, subdirs: {}", static_cast<int>(nca->GetStatus()), nca->GetSubdirectories().size());
        if (nca->GetStatus() != Loader::ResultStatus::Success || nca->GetSubdirectories().empty()) {
            program_status[nca->GetTitleId()] = nca->GetStatus();
            continue;
        }

        const auto section0 = nca->GetSubdirectories()[0];

        for (const auto& inner_file : section0->GetFiles()) {
            if (inner_file->GetExtension() != "cnmt") {
                continue;
            }

            const CNMT cnmt(inner_file);

            ncas[cnmt.GetTitleID()][{cnmt.GetType(), ContentRecordType::Meta}] = nca;

            for (const auto& rec : cnmt.GetContentRecords()) {
                const auto id_string = Common::HexToString(rec.nca_id, false);
                LOG_DEBUG(Loader, "NSP DEBUG: Looking for content NCA: {}.nca (type={})", id_string, static_cast<int>(rec.type));
                auto next_file = pfs->GetFile(fmt::format("{}.nca", id_string));

                if (next_file == nullptr) {
                    next_file = pfs->GetFile(fmt::format("{}.ncz", id_string));
                    if (next_file != nullptr) {
                        LOG_DEBUG(Loader, "NSP DEBUG: Found as .ncz, wrapping in NCZVirtualFile (size={})", next_file->GetSize());
                        next_file = std::make_shared<NCZVirtualFile>(next_file);
                    }
                } else if (is_nsz || IsNczFile(next_file)) {
                    LOG_DEBUG(Loader, "NSP DEBUG: Found as .nca in NSZ or detected NCZ magic, wrapping in NCZVirtualFile (size={})", next_file->GetSize());
                    next_file = std::make_shared<NCZVirtualFile>(next_file);
                }

                if (next_file == nullptr) {
                    LOG_INFO(Service_FS, "NCA with ID {}.nca not found by name. Performing content-based fallback matching...", id_string);
                    for (const auto& potential_file : files) {
                        if (potential_file == nullptr) continue;
                        
                        std::string name = potential_file->GetName();
                        bool is_ncz = name.ends_with(".ncz") || IsNczFile(potential_file);
                        bool is_nca = name.ends_with(".nca");
                        if (!is_ncz && !is_nca) continue;
                        
                        if (name.find(".cnmt.nca") != std::string::npos || name.find(".cnmt.ncz") != std::string::npos) {
                            continue;
                        }
                        
                        VirtualFile temp_file = potential_file;
                        if (is_ncz || is_nsz) {
                            temp_file = std::make_shared<NCZVirtualFile>(temp_file);
                        }
                        
                        auto temp_nca = std::make_shared<NCA>(temp_file);
                        if (temp_nca->GetStatus() == Loader::ResultStatus::Success) {
                            NCAContentType expected_nca_type;
                            switch (rec.type) {
                                case ContentRecordType::Program:
                                    expected_nca_type = NCAContentType::Program;
                                    break;
                                case ContentRecordType::Meta:
                                    expected_nca_type = NCAContentType::Meta;
                                    break;
                                case ContentRecordType::Control:
                                    expected_nca_type = NCAContentType::Control;
                                    break;
                                case ContentRecordType::HtmlDocument:
                                case ContentRecordType::LegalInformation:
                                    expected_nca_type = NCAContentType::Manual;
                                    break;
                                case ContentRecordType::Data:
                                    expected_nca_type = NCAContentType::Data;
                                    break;
                                default:
                                    expected_nca_type = NCAContentType::PublicData;
                                    break;
                            }
                            
                            bool type_matches = (temp_nca->GetType() == expected_nca_type);
                            bool title_id_matches = ((temp_nca->GetTitleId() & 0xFFFFFFFFFFFFF000) == (cnmt.GetTitleID() & 0xFFFFFFFFFFFFF000));
                            
                            if (type_matches && title_id_matches) {
                                LOG_INFO(Service_FS, "Successfully matched unmatched record type {} to file {}", static_cast<int>(rec.type), name);
                                next_file = temp_file;
                                break;
                            }
                        }
                    }
                }

                if (next_file == nullptr) {
                    if (rec.type != ContentRecordType::DeltaFragment) {
                        LOG_WARNING(Service_FS,
                                    "NCA with ID {}.nca is listed in content metadata, but cannot "
                                    "be found in PFS. NSP appears to be corrupted.",
                                    id_string);
                        if (std::FILE* dbg = nullptr) {
                            std::fprintf(dbg, "[NSP DEBUG] WARNING: NCA %s not found in PFS!\n", id_string.c_str());
                            std::fflush(dbg); std::fclose(dbg);
                        }
                    }

                    continue;
                }

                if (std::FILE* dbg = nullptr) {
                    std::fprintf(dbg, "[NSP DEBUG] Creating NCA object from content file (IsNcz=%d, size=%zu)\n", next_file->IsNczFile(), next_file->GetSize());
                    std::fflush(dbg); std::fclose(dbg);
                }
                auto next_nca = std::make_shared<NCA>(std::move(next_file));
                if (std::FILE* dbg = nullptr) {
                    std::fprintf(dbg, "[NSP DEBUG] Content NCA created. Type=%d, TitleID=%016llX, Status=%d\n",
                                 static_cast<int>(next_nca->GetType()),
                                 static_cast<unsigned long long>(next_nca->GetTitleId()),
                                 static_cast<int>(next_nca->GetStatus()));
                    std::fflush(dbg); std::fclose(dbg);
                }

                if (next_nca->GetType() == NCAContentType::Program) {
                    program_status[next_nca->GetTitleId()] = next_nca->GetStatus();
                    program_ids.insert(next_nca->GetTitleId() & 0xFFFFFFFFFFFFF000);
                }

                if (next_nca->GetStatus() != Loader::ResultStatus::Success &&
                    next_nca->GetStatus() != Loader::ResultStatus::ErrorMissingBKTRBaseRomFS) {
                    continue;
                }

                // If the last 3 hexadecimal digits of the CNMT TitleID is 0x800 or is missing the
                // BKTRBaseRomFS, this is an update NCA. Otherwise, this is a base NCA.
                if ((cnmt.GetTitleID() & 0x800) != 0 ||
                    next_nca->GetStatus() == Loader::ResultStatus::ErrorMissingBKTRBaseRomFS) {
                    // If the last 3 hexadecimal digits of the NCA's TitleID is between 0x1 and
                    // 0x7FF, this is a multi-program update NCA. Otherwise, this is a regular
                    // update NCA.
                    if ((next_nca->GetTitleId() & 0x7FF) != 0 &&
                        (next_nca->GetTitleId() & 0x800) == 0) {
                        ncas[next_nca->GetTitleId()][{cnmt.GetType(), rec.type}] =
                            std::move(next_nca);
                    } else {
                        // fix for Bayonetta Origins in Bayonetta 3 and external content
                        // where multiple update NCAs exist for the same title and type.
                        auto& target_map = ncas[cnmt.GetTitleID()];
                        auto existing = target_map.find({cnmt.GetType(), rec.type});

                        if (existing != target_map.end() && rec.type == ContentRecordType::Program) {
                            continue;
                        }
                        ncas[cnmt.GetTitleID()][{cnmt.GetType(), rec.type}] = std::move(next_nca);
                    }
                } else {
                    ncas[next_nca->GetTitleId()][{cnmt.GetType(), rec.type}] = std::move(next_nca);
                }
            }

            break;
        }
    }
}

} // namespace FileSys
