// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <locale>
#include "common/hex_util.h"
#include "common/swap.h"
#include "core/arm/debug.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/k_page_table.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_process_page_table.h"
#include "core/hle/kernel/svc_types.h"
#include "core/hle/service/hid/hid_server.h"
#include "core/hle/service/sm/sm.h"
#include "core/memory.h"
#include "core/memory/cheat_engine.h"
#include "hid_core/resource_manager.h"
#include "hid_core/resources/npad/npad.h"

namespace Core::Memory {
namespace {
constexpr auto CHEAT_ENGINE_NS = std::chrono::nanoseconds{1000000000 / 60};
} // namespace

StandardVmCallbacks::StandardVmCallbacks(System& system_, const CheatProcessMetadata& metadata_)
    : metadata{metadata_}, system{system_} {}

StandardVmCallbacks::~StandardVmCallbacks() = default;

void StandardVmCallbacks::MemoryReadUnsafe(VAddr address, void* data, u64 size) {
    if (address == 0 || size == 0 || data == nullptr) {
        if (data != nullptr && size > 0) {
            std::memset(data, 0, size);
        }
        return;
    }

    if (!system.ApplicationMemory().IsValidVirtualAddressRange(address, size)) {
        std::memset(data, 0, size);
        return;
    }

    system.ApplicationMemory().ReadBlockUnsafe(address, data, size);
}

void StandardVmCallbacks::MemoryWriteUnsafe(VAddr address, const void* data, u64 size) {
    if (address == 0 || size == 0 || data == nullptr) {
        return;
    }

    if (!system.ApplicationMemory().IsValidVirtualAddressRange(address, size)) {
        return;
    }

    if (system.ApplicationMemory().WriteBlockUnsafe(address, data, size)) {
        auto* proc = system.ApplicationProcess();
        if (proc) {
            Core::InvalidateInstructionCacheRange(proc, address, size);
        }
    }
}

u64 StandardVmCallbacks::HidKeysDown() {
    const auto hid = system.ServiceManager().GetService<Service::HID::IHidServer>("hid");
    if (hid == nullptr) {
        LOG_WARNING(CheatEngine, "Attempted to read input state, but hid is not initialized!");
        return 0;
    }

    const auto applet_resource = hid->GetResourceManager();
    if (applet_resource == nullptr || applet_resource->GetNpad() == nullptr) {
        LOG_WARNING(CheatEngine,
                    "Attempted to read input state, but applet resource is not initialized!");
        return 0;
    }

    const auto press_state = applet_resource->GetNpad()->GetAndResetPressState();
    return static_cast<u64>(press_state & HID::NpadButton::All);
}

void StandardVmCallbacks::PauseProcess() {
    if (!system.ApplicationProcess()->IsSuspended()) {
        system.ApplicationProcess()->SetActivity(system.Kernel(), Kernel::Svc::ProcessActivity::Paused);
    }
}

void StandardVmCallbacks::ResumeProcess() {
    if (system.ApplicationProcess()->IsSuspended()) {
        system.ApplicationProcess()->SetActivity(system.Kernel(), Kernel::Svc::ProcessActivity::Runnable);
    }
}

void StandardVmCallbacks::DebugLog(u8 id, u64 value) {
    LOG_INFO(CheatEngine, "Cheat triggered DebugLog: ID '{:01X}' Value '{:016X}'", id, value);
}

void StandardVmCallbacks::CommandLog(std::string_view data) {
    LOG_DEBUG(CheatEngine, "[DmntCheatVm]: {}",
              data.back() == '\n' ? data.substr(0, data.size() - 1) : data);
}

bool StandardVmCallbacks::IsAddressInRange(VAddr in) const {
    if (in == 0) {
        return false;
    }
    if (system.ApplicationMemory().IsValidVirtualAddress(in)) {
        return true;
    }
    if ((in >= metadata.main_nso_extents.base &&
         in < metadata.main_nso_extents.base + metadata.main_nso_extents.size) ||
        (in >= metadata.heap_extents.base &&
         in < metadata.heap_extents.base + metadata.heap_extents.size) ||
        (in >= metadata.alias_extents.base &&
         in < metadata.alias_extents.base + metadata.alias_extents.size) ||
        (in >= metadata.aslr_extents.base &&
         in < metadata.aslr_extents.base + metadata.aslr_extents.size)) {
        return true;
    }
    return false;
}

CheatParser::~CheatParser() = default;

TextCheatParser::~TextCheatParser() = default;

std::vector<CheatEntry> TextCheatParser::Parse(std::string_view data) const {
    if (data.empty()) {
        return {};
    }

    // Skip UTF-8 BOM if present
    if (data.size() >= 3 && static_cast<u8>(data[0]) == 0xEF &&
        static_cast<u8>(data[1]) == 0xBB && static_cast<u8>(data[2]) == 0xBF) {
        data.remove_prefix(3);
    }

    std::vector<CheatEntry> result;
    CheatEntry current{};
    bool in_entry = false;

    std::size_t pos = 0;
    while (pos < data.size()) {
        // Read one line
        std::size_t end_line = data.find('\n', pos);
        if (end_line == std::string_view::npos) {
            end_line = data.size();
        }

        std::string_view line = data.substr(pos, end_line - pos);
        pos = (end_line < data.size()) ? end_line + 1 : data.size();

        // Trim leading and trailing whitespace / carriage return
        while (!line.empty() && (line.front() == ' ' || line.front() == '\t' || line.front() == '\r')) {
            line.remove_prefix(1);
        }
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t' || line.back() == '\r')) {
            line.remove_suffix(1);
        }

        if (line.empty() || line.starts_with('#') || line.starts_with("//") || line.starts_with(';')) {
            continue;
        }

        // Section header: [Cheat Name] or {Cheat Name}
        if ((line.front() == '[' && line.find(']') != std::string_view::npos) ||
            (line.front() == '{' && line.find('}') != std::string_view::npos)) {
            if (in_entry && current.definition.num_opcodes > 0) {
                current.enabled = true;
                current.cheat_id = static_cast<u32>(result.size());
                result.push_back(current);
            }

            current = CheatEntry{};
            char close_char = (line.front() == '[') ? ']' : '}';
            std::size_t close_idx = line.find(close_char);
            std::string_view name = line.substr(1, close_idx - 1);

            // Trim name
            while (!name.empty() && (name.front() == ' ' || name.front() == '\t')) name.remove_prefix(1);
            while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) name.remove_suffix(1);

            std::size_t copy_len = std::min<std::size_t>(name.size(), current.definition.readable_name.size() - 1);
            std::memcpy(current.definition.readable_name.data(), name.data(), copy_len);
            current.definition.readable_name[copy_len] = '\0';
            in_entry = true;
            continue;
        }

        // Parse opcode line (e.g. "04000000 01234567 00000001" or "580F0000 01234568 // comment")
        if (!in_entry) {
            // Unnamed first cheat fallback
            current = CheatEntry{};
            const std::string default_name = "Cheat " + std::to_string(result.size() + 1);
            std::memcpy(current.definition.readable_name.data(), default_name.data(),
                        std::min(default_name.size(), current.definition.readable_name.size() - 1));
            in_entry = true;
        }

        // Strip inline comments
        auto comment_pos = line.find("//");
        if (comment_pos != std::string_view::npos) {
            line = line.substr(0, comment_pos);
        }
        comment_pos = line.find('#');
        if (comment_pos != std::string_view::npos) {
            line = line.substr(0, comment_pos);
        }
        comment_pos = line.find(';');
        if (comment_pos != std::string_view::npos) {
            line = line.substr(0, comment_pos);
        }

        // Tokenize line by whitespace
        std::size_t token_start = 0;
        while (token_start < line.size()) {
            while (token_start < line.size() && (line[token_start] == ' ' || line[token_start] == '\t')) {
                token_start++;
            }
            if (token_start >= line.size()) break;

            std::size_t token_end = token_start;
            while (token_end < line.size() && line[token_end] != ' ' && line[token_end] != '\t') {
                token_end++;
            }

            std::string_view token = line.substr(token_start, token_end - token_start);
            token_start = token_end;

            // Check if token is valid hex string (typically 8 hex chars)
            if (token.size() <= 8 && std::all_of(token.begin(), token.end(), ::isxdigit)) {
                if (current.definition.num_opcodes < current.definition.opcodes.size()) {
                    std::string hex_str(token);
                    u32 val = static_cast<u32>(std::strtoul(hex_str.c_str(), nullptr, 16));
                    current.definition.opcodes[current.definition.num_opcodes++] = val;
                }
            }
        }
    }

    if (in_entry && current.definition.num_opcodes > 0) {
        current.enabled = true;
        current.cheat_id = static_cast<u32>(result.size());
        result.push_back(current);
    }

    return result;
}

CheatEngine::CheatEngine(System& system_, std::vector<CheatEntry> cheats_,
                         const std::array<u8, 0x20>& build_id_)
    : vm{std::make_unique<StandardVmCallbacks>(system_, metadata)},
      cheats(std::move(cheats_)), core_timing{system_.CoreTiming()}, system{system_} {
    metadata.main_nso_build_id = build_id_;
    vm.LoadProgram(cheats);
}

CheatEngine::~CheatEngine() {
    if (event)
        core_timing.UnscheduleEvent(event);
    else
        LOG_ERROR(CheatEngine, "~CheatEngine before event was registered");
}

void CheatEngine::Initialize() {
    if (event) {
        core_timing.UnscheduleEvent(event);
        event.reset();
    }
    event = Core::Timing::CreateEvent(
        "CheatEngine::FrameCallback",
        [this](s64 time, std::chrono::nanoseconds ns_late)
            -> std::optional<std::chrono::nanoseconds> {
            FrameCallback(ns_late);
            return CHEAT_ENGINE_NS;
        });
    core_timing.ScheduleLoopingEvent(CHEAT_ENGINE_NS, CHEAT_ENGINE_NS, event);
    LOG_INFO(CheatEngine, "CheatEngine initialized and scheduled at 60Hz frame timing");
}

void CheatEngine::SetMainMemoryParameters(VAddr main_region_begin, u64 main_region_size) {
    if (main_region_begin != 0) {
        metadata.main_nso_extents = {
            .base = main_region_begin,
            .size = main_region_size,
        };
    }
}

void CheatEngine::Reload(std::vector<CheatEntry> reload_cheats) {
    cheats = std::move(reload_cheats);
    vm.LoadProgram(cheats);
    is_pending_reload.exchange(false);
}

void CheatEngine::FrameCallback(std::chrono::nanoseconds ns_late) {
    if (cheats.empty() || is_pending_reload.load()) {
        return;
    }

    if (!system.IsPoweredOn()) {
        return;
    }

    auto* proc = system.ApplicationProcess();
    if (proc == nullptr || proc->IsSuspended()) {
        return;
    }

    if (metadata.main_nso_extents.base == 0) {
        metadata.main_nso_extents = {
            .base = GetInteger(proc->GetPageTable().GetCodeRegionStart()),
            .size = proc->GetPageTable().GetCodeRegionSize(),
        };
        if (metadata.main_nso_extents.base == 0) {
            return;
        }
    }

    if (metadata.heap_extents.base == 0) {
        metadata.heap_extents = {
            .base = GetInteger(proc->GetPageTable().GetHeapRegionStart()),
            .size = proc->GetPageTable().GetHeapRegionSize(),
        };
    }

    if (metadata.alias_extents.base == 0) {
        metadata.alias_extents = {
            .base = GetInteger(proc->GetPageTable().GetAliasRegionStart()),
            .size = proc->GetPageTable().GetAliasRegionSize(),
        };
    }

    if (metadata.aslr_extents.base == 0) {
        metadata.aslr_extents = {
            .base = GetInteger(proc->GetPageTable().GetAddressSpaceStart()),
            .size = proc->GetPageTable().GetAddressSpaceSize(),
        };
    }

    vm.Execute(metadata);
}

} // namespace Core::Memory
