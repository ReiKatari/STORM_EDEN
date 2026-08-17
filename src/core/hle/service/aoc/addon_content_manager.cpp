// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <numeric>
#include <vector>

#include "common/logging.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/file_sys/common_funcs.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/control_metadata.h"
#include "core/file_sys/nca_metadata.h"
#include "core/file_sys/patch_manager.h"
#include "core/file_sys/registered_cache.h"
#include "core/hle/kernel/k_event.h"
#include "core/hle/service/aoc/addon_content_manager.h"
#include "core/hle/service/aoc/purchase_event_manager.h"
#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/server_manager.h"
#include "core/loader/loader.h"

namespace Service::AOC {

static bool CheckAOCTitleIDMatchesBase(u64 title_id, u64 base) {
    return FileSys::GetBaseTitleID(title_id) == base;
}

static std::vector<u64> AccumulateAOCTitleIDs(Core::System& system) {
    std::vector<u64> add_on_content;
    const auto& rcu = system.GetContentProvider();
    const auto list =
        rcu.ListEntriesFilter(FileSys::TitleType::AOC, FileSys::ContentRecordType::Data);
    std::transform(list.begin(), list.end(), std::back_inserter(add_on_content),
                   [](const FileSys::ContentProviderEntry& rce) { return rce.title_id; });
    add_on_content.erase(
        std::remove_if(
            add_on_content.begin(), add_on_content.end(),
            [&rcu](u64 tid) {
                const auto entry = rcu.GetEntry(tid, FileSys::ContentRecordType::Data);
                if (entry == nullptr) {
                    LOG_WARNING(Service_AOC, "DLC title_id={:016X} has no Data entry in ContentProvider, ignoring", tid);
                    return true;
                }
                if (entry->GetStatus() != Loader::ResultStatus::Success) {
                    LOG_WARNING(Service_AOC, "DLC title_id={:016X} has invalid NCA status ({}), ignoring",
                                tid, static_cast<int>(entry->GetStatus()));
                    return true;
                }
                const auto romfs = entry->GetRomFS();
                if (romfs == nullptr || romfs->GetSize() == 0) {
                    LOG_WARNING(Service_AOC, "DLC title_id={:016X} has empty/missing RomFS payload, ignoring to prevent crash", tid);
                    return true;
                }
                LOG_INFO(Service_AOC, "DLC title_id={:016X} verified and loaded successfully (RomFS size: {} bytes)",
                         tid, romfs->GetSize());
                return false;
            }),
        add_on_content.end());
    return add_on_content;
}

IAddOnContentManager::IAddOnContentManager(Core::System& system_)
    : ServiceFramework{system_, "aoc:u"}, add_on_content{AccumulateAOCTitleIDs(system)},
      service_context{system_, "aoc:u"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, D<&IAddOnContentManager::CountAddOnContentByApplicationId>, "CountAddOnContentByApplicationId"},
        {1, D<&IAddOnContentManager::ListAddOnContentByApplicationId>, "ListAddOnContentByApplicationId"},
        {2, D<&IAddOnContentManager::CountAddOnContent>, "CountAddOnContent"},
        {3, D<&IAddOnContentManager::ListAddOnContent>, "ListAddOnContent"},
        {4, D<&IAddOnContentManager::GetAddOnContentBaseIdByApplicationId>, "GetAddOnContentBaseIdByApplicationId"},
        {5, D<&IAddOnContentManager::GetAddOnContentBaseId>, "GetAddOnContentBaseId"},
        {6, D<&IAddOnContentManager::PrepareAddOnContentByApplicationId>, "PrepareAddOnContentByApplicationId"},
        {7, D<&IAddOnContentManager::PrepareAddOnContent>, "PrepareAddOnContent"},
        {8, D<&IAddOnContentManager::GetAddOnContentListChangedEvent>, "GetAddOnContentListChangedEvent"},
        {9, D<&IAddOnContentManager::GetAddOnContentLostErrorCode>, "GetAddOnContentLostErrorCode"},
        {10, D<&IAddOnContentManager::GetAddOnContentListChangedEventWithProcessId>, "GetAddOnContentListChangedEventWithProcessId"},
        {11, D<&IAddOnContentManager::NotifyMountAddOnContent>, "NotifyMountAddOnContent"},
        {12, D<&IAddOnContentManager::NotifyUnmountAddOnContent>, "NotifyUnmountAddOnContent"},
        {13, nullptr, "IsAddOnContentMountedForDebug"},
        {50, D<&IAddOnContentManager::CheckAddOnContentMountStatus>, "CheckAddOnContentMountStatus"},
        {100, D<&IAddOnContentManager::CreateEcPurchasedEventManager>, "CreateEcPurchasedEventManager"},
        {101, D<&IAddOnContentManager::CreatePermanentEcPurchasedEventManager>, "CreatePermanentEcPurchasedEventManager"},
        {110, nullptr, "CreateContentsServiceManager"},
        {200, nullptr, "SetRequiredAddOnContentsOnContentsAvailabilityTransition"},
        {300, nullptr, "SetupHostAddOnContent"},
        {301, nullptr, "GetRegisteredAddOnContentPath"},
        {302, nullptr, "UpdateCachedList"},
    };
    // clang-format on

    RegisterHandlers(functions);

    aoc_change_event = service_context.CreateEvent("GetAddOnContentListChanged:Event");
}

IAddOnContentManager::~IAddOnContentManager() {
    service_context.CloseEvent(aoc_change_event);
}

Result IAddOnContentManager::CountAddOnContentByApplicationId(Out<u32> out_count,
                                                              u64 application_id) {
    const auto& disabled = Settings::values.disabled_addons[application_id];
    if (std::find(disabled.begin(), disabled.end(), "DLC") != disabled.end()) {
        *out_count = 0;
        LOG_INFO(Service_AOC, "CountAddOnContentByApplicationId: DLC disabled in settings for {:016X}", application_id);
        R_SUCCEED();
    }

    *out_count = static_cast<u32>(
        std::count_if(add_on_content.begin(), add_on_content.end(),
                      [application_id](u64 tid) { return CheckAOCTitleIDMatchesBase(tid, application_id); }));

    LOG_INFO(Service_AOC, "CountAddOnContentByApplicationId: application_id={:016X} returned count={}",
             application_id, *out_count);

    R_SUCCEED();
}

Result IAddOnContentManager::ListAddOnContentByApplicationId(
    Out<u32> out_count, OutBuffer<BufferAttr_HipcMapAlias> out_addons, u32 offset, u32 count,
    u64 application_id) {
    const auto base_id = FileSys::GetBaseTitleID(application_id);

    std::vector<u32> out;
    const auto& disabled = Settings::values.disabled_addons[base_id];
    if (std::find(disabled.begin(), disabled.end(), "DLC") == disabled.end()) {
        for (u64 content_id : add_on_content) {
            if (FileSys::GetBaseTitleID(content_id) != base_id) {
                continue;
            }

            out.push_back(static_cast<u32>(FileSys::GetAOCID(content_id)));
        }
    }

    R_UNLESS(out.size() >= offset, ResultUnknown);

    *out_count = static_cast<u32>(std::min<size_t>(out.size() - offset, count));
    std::rotate(out.begin(), out.begin() + offset, out.end());

    if (*out_count > 0 && out_addons.data() != nullptr) {
        std::memcpy(out_addons.data(), out.data(), *out_count * sizeof(u32));
    }

    LOG_INFO(Service_AOC, "ListAddOnContentByApplicationId: app_id={:016X} offset={} count={} returned out_count={} addons=[{}]",
             application_id, offset, count, *out_count, fmt::join(out, ", "));

    R_SUCCEED();
}

Result IAddOnContentManager::GetAddOnContentBaseIdByApplicationId(Out<u64> out_title_id,
                                                                  u64 application_id) {
    const FileSys::PatchManager pm{application_id, system.GetFileSystemController(),
                                   system.GetContentProvider()};

    const auto res = pm.GetControlMetadata();
    if (res.first == nullptr) {
        *out_title_id = FileSys::GetAOCBaseTitleID(application_id);
    } else {
        *out_title_id = res.first->GetDLCBaseTitleId();
        if (*out_title_id == 0) {
            *out_title_id = FileSys::GetAOCBaseTitleID(application_id);
        }
    }

    LOG_INFO(Service_AOC, "GetAddOnContentBaseIdByApplicationId: app_id={:016X} returned out_title_id={:016X}",
             application_id, *out_title_id);

    R_SUCCEED();
}

Result IAddOnContentManager::PrepareAddOnContentByApplicationId(s32 addon_index,
                                                                u64 application_id) {
    LOG_INFO(Service_AOC, "PrepareAddOnContentByApplicationId: addon_index={}, app_id={:016X}",
             addon_index, application_id);

    R_SUCCEED();
}

Result IAddOnContentManager::GetAddOnContentLostErrorCode(Out<u32> out_error_code, u32 index,
                                                          u64 application_id) {
    LOG_INFO(Service_AOC, "GetAddOnContentLostErrorCode: index={}, app_id={:016X}", index,
             application_id);

    *out_error_code = 0;
    R_SUCCEED();
}

Result IAddOnContentManager::CountAddOnContent(Out<u32> out_count, ClientProcessId process_id) {
    const auto current = system.GetApplicationProcessProgramID();

    const auto& disabled = Settings::values.disabled_addons[current];
    if (std::find(disabled.begin(), disabled.end(), "DLC") != disabled.end()) {
        *out_count = 0;
        LOG_INFO(Service_AOC, "CountAddOnContent: DLC disabled in settings, returned count=0 (process_id={})", process_id.pid);
        R_SUCCEED();
    }

    *out_count = static_cast<u32>(
        std::count_if(add_on_content.begin(), add_on_content.end(),
                      [current](u64 tid) { return CheckAOCTitleIDMatchesBase(tid, current); }));

    LOG_INFO(Service_AOC, "CountAddOnContent: title_id={:016X} returned count={} (process_id={})",
             current, *out_count, process_id.pid);

    R_SUCCEED();
}

Result IAddOnContentManager::ListAddOnContent(Out<u32> out_count,
                                              OutBuffer<BufferAttr_HipcMapAlias> out_addons,
                                              u32 offset, u32 count, ClientProcessId process_id) {
    const auto current = FileSys::GetBaseTitleID(system.GetApplicationProcessProgramID());

    std::vector<u32> out;
    const auto& disabled = Settings::values.disabled_addons[current];
    if (std::find(disabled.begin(), disabled.end(), "DLC") == disabled.end()) {
        for (u64 content_id : add_on_content) {
            if (FileSys::GetBaseTitleID(content_id) != current) {
                continue;
            }

            out.push_back(static_cast<u32>(FileSys::GetAOCID(content_id)));
        }
    }

    R_UNLESS(out.size() >= offset, ResultUnknown);

    *out_count = static_cast<u32>(std::min<size_t>(out.size() - offset, count));
    std::rotate(out.begin(), out.begin() + offset, out.end());

    if (*out_count > 0 && out_addons.data() != nullptr) {
        std::memcpy(out_addons.data(), out.data(), *out_count * sizeof(u32));
    }

    LOG_INFO(Service_AOC, "ListAddOnContent: offset={} count={} returned out_count={} addons=[{}] (process_id={})",
             offset, count, *out_count, fmt::join(out, ", "), process_id.pid);

    R_SUCCEED();
}

Result IAddOnContentManager::GetAddOnContentBaseId(Out<u64> out_title_id,
                                                   ClientProcessId process_id) {
    const auto title_id = system.GetApplicationProcessProgramID();
    const FileSys::PatchManager pm{title_id, system.GetFileSystemController(),
                                   system.GetContentProvider()};

    const auto res = pm.GetControlMetadata();
    if (res.first == nullptr) {
        *out_title_id = FileSys::GetAOCBaseTitleID(title_id);
    } else {
        *out_title_id = res.first->GetDLCBaseTitleId();
        if (*out_title_id == 0) {
            *out_title_id = FileSys::GetAOCBaseTitleID(title_id);
        }
    }

    LOG_INFO(Service_AOC, "GetAddOnContentBaseId: title_id={:016X} returned out_title_id={:016X} (process_id={})",
             title_id, *out_title_id, process_id.pid);

    R_SUCCEED();
}

Result IAddOnContentManager::PrepareAddOnContent(s32 addon_index, ClientProcessId process_id) {
    LOG_INFO(Service_AOC, "PrepareAddOnContent: addon_index={}, process_id={}", addon_index,
             process_id.pid);

    R_SUCCEED();
}

Result IAddOnContentManager::GetAddOnContentListChangedEvent(
    OutCopyHandle<Kernel::KReadableEvent> out_event) {
    LOG_INFO(Service_AOC, "GetAddOnContentListChangedEvent called");

    *out_event = &aoc_change_event->GetReadableEvent();

    R_SUCCEED();
}

Result IAddOnContentManager::GetAddOnContentListChangedEventWithProcessId(
    OutCopyHandle<Kernel::KReadableEvent> out_event, ClientProcessId process_id) {
    LOG_INFO(Service_AOC, "GetAddOnContentListChangedEventWithProcessId called (process_id={})", process_id.pid);

    *out_event = &aoc_change_event->GetReadableEvent();

    R_SUCCEED();
}

Result IAddOnContentManager::NotifyMountAddOnContent() {
    LOG_INFO(Service_AOC, "NotifyMountAddOnContent called");

    R_SUCCEED();
}

Result IAddOnContentManager::NotifyUnmountAddOnContent() {
    LOG_INFO(Service_AOC, "NotifyUnmountAddOnContent called");

    R_SUCCEED();
}

Result IAddOnContentManager::CheckAddOnContentMountStatus() {
    LOG_INFO(Service_AOC, "CheckAddOnContentMountStatus called");

    R_SUCCEED();
}

Result IAddOnContentManager::CreateEcPurchasedEventManager(
    OutInterface<IPurchaseEventManager> out_interface) {
    LOG_WARNING(Service_AOC, "(STUBBED) called");

    *out_interface = std::make_shared<IPurchaseEventManager>(system);

    R_SUCCEED();
}

Result IAddOnContentManager::CreatePermanentEcPurchasedEventManager(
    OutInterface<IPurchaseEventManager> out_interface) {
    LOG_WARNING(Service_AOC, "(STUBBED) called");

    *out_interface = std::make_shared<IPurchaseEventManager>(system);

    R_SUCCEED();
}

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);
    server_manager->RegisterNamedService("aoc:u", std::make_shared<IAddOnContentManager>(system));
    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::AOC
