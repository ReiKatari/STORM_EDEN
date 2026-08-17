// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/file_sys/errors.h"
#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/filesystem/fsp/fs_i_storage.h"

namespace Service::FileSystem {

IStorage::IStorage(Core::System& system_, FileSys::VirtualFile backend_)
    : ServiceFramework{system_, "IStorage"}, backend(std::move(backend_)) {
    static const FunctionInfo functions[] = {
        {0, D<&IStorage::Read>, "Read"},
        {1, nullptr, "Write"},
        {2, nullptr, "Flush"},
        {3, nullptr, "SetSize"},
        {4, D<&IStorage::GetSize>, "GetSize"},
        {5, nullptr, "OperateRange"},
    };
    RegisterHandlers(functions);
}

Result IStorage::Read(
    OutBuffer<BufferAttr_HipcMapAlias | BufferAttr_HipcMapTransferAllowsNonSecure> out_bytes,
    s64 offset, s64 length) {
    if (!backend) {
        LOG_ERROR(Service_FS, "IStorage::Read: backend is null! Returning ResultTargetNotFound (offset={:#x}, length={})", offset, length);
        R_RETURN(FileSys::ResultTargetNotFound);
    }
    if (length < 0) {
        LOG_ERROR(Service_FS, "IStorage::Read: invalid length {}! Returning ResultInvalidSize", length);
        R_RETURN(FileSys::ResultInvalidSize);
    }
    if (offset < 0) {
        LOG_ERROR(Service_FS, "IStorage::Read: invalid offset {:#x}! Returning ResultInvalidOffset", offset);
        R_RETURN(FileSys::ResultInvalidOffset);
    }

    if (length == 0) {
        R_SUCCEED();
    }
    if (out_bytes.data() == nullptr) {
        LOG_ERROR(Service_FS, "IStorage::Read: out_bytes.data() is null for length={}! Returning ResultNullptrArgument (0x2F5E02)", length);
        R_RETURN(FileSys::ResultNullptrArgument);
    }

    const std::size_t read_bytes = backend->Read(out_bytes.data(), length, offset);
    LOG_INFO(Service_FS, "IStorage::Read: backend='{}' offset={:#x} length={} read_bytes={}",
             backend->GetName(), offset, length, read_bytes);
    if (read_bytes < static_cast<std::size_t>(length)) {
        LOG_WARNING(Service_FS, "IStorage::Read: short read for '{}'! Requested {}, read {}. Zero-filling remainder.",
                    backend->GetName(), length, read_bytes);
        std::memset(out_bytes.data() + read_bytes, 0, length - read_bytes);
    }

    R_SUCCEED();
}

Result IStorage::GetSize(Out<u64> out_size) {
    R_UNLESS(backend != nullptr, FileSys::ResultTargetNotFound);
    *out_size = backend->GetSize();

    LOG_INFO(Service_FS, "IStorage::GetSize: backend='{}' size={}", backend->GetName(), *out_size);

    R_SUCCEED();
}

} // namespace Service::FileSystem
