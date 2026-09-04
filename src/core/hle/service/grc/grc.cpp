// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>

#include "common/logging.h"
#include "core/hle/service/grc/grc.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/service.h"

namespace Service::GRC {

class IContinuousRecorder final : public ServiceFramework<IContinuousRecorder> {
public:
    explicit IContinuousRecorder(Core::System& system_) : ServiceFramework{system_, "IContinuousRecorder"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "Unknown0"},
            {1, nullptr, "Unknown1"},
            {2, nullptr, "Unknown2"},
            {3, nullptr, "Unknown3"},
        };
        // clang-format on
        RegisterHandlers(functions);
    }
};

class IGameMovieTrimmer final : public ServiceFramework<IGameMovieTrimmer> {
public:
    explicit IGameMovieTrimmer(Core::System& system_) : ServiceFramework{system_, "IGameMovieTrimmer"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "Unknown0"},
            {1, nullptr, "Unknown1"},
        };
        // clang-format on
        RegisterHandlers(functions);
    }
};

class IOffscreenRecorder final : public ServiceFramework<IOffscreenRecorder> {
public:
    explicit IOffscreenRecorder(Core::System& system_) : ServiceFramework{system_, "IOffscreenRecorder"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "Unknown0"},
            {1, nullptr, "Unknown1"},
        };
        // clang-format on
        RegisterHandlers(functions);
    }
};

class IMovieMaker final : public ServiceFramework<IMovieMaker> {
public:
    explicit IMovieMaker(Core::System& system_) : ServiceFramework{system_, "IMovieMaker"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "Unknown0"},
            {1, nullptr, "Unknown1"},
        };
        // clang-format on
        RegisterHandlers(functions);
    }
};

class GRC final : public ServiceFramework<GRC> {
public:
    explicit GRC(Core::System& system_) : ServiceFramework{system_, "grc:c"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {1, &GRC::OpenContinuousRecorder, "OpenContinuousRecorder"},
            {2, &GRC::OpenGameMovieTrimmer, "OpenGameMovieTrimmer"},
            {3, &GRC::OpenOffscreenRecorder, "OpenOffscreenRecorder"},
            {101, &GRC::CreateMovieMaker, "CreateMovieMaker"},
            {9903, &GRC::SetOffscreenRecordingMarker, "SetOffscreenRecordingMarker"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void OpenContinuousRecorder(HLERequestContext& ctx) {
        LOG_WARNING(Service_GRC, "(STUBBED) called");
        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<IContinuousRecorder>(ctx, system);
    }

    void OpenGameMovieTrimmer(HLERequestContext& ctx) {
        LOG_WARNING(Service_GRC, "(STUBBED) called");
        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<IGameMovieTrimmer>(ctx, system);
    }

    void OpenOffscreenRecorder(HLERequestContext& ctx) {
        LOG_WARNING(Service_GRC, "(STUBBED) called");
        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<IOffscreenRecorder>(ctx, system);
    }

    void CreateMovieMaker(HLERequestContext& ctx) {
        LOG_WARNING(Service_GRC, "(STUBBED) called");
        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<IMovieMaker>(ctx, system);
    }

    void SetOffscreenRecordingMarker(HLERequestContext& ctx) {
        LOG_WARNING(Service_GRC, "(STUBBED) called");
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }
};

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);

    server_manager->RegisterNamedService("grc:c", std::make_shared<GRC>(system));
    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::GRC
