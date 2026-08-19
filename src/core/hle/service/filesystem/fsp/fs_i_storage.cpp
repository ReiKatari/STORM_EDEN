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
        {1, D<&IStorage::Write>, "Write"},
        {2, D<&IStorage::Flush>, "Flush"},
        {3, D<&IStorage::SetSize>, "SetSize"},
        {4, D<&IStorage::GetSize>, "GetSize"},
        {5, D<&IStorage::OperateRange>, "OperateRange"},
    };
    RegisterHandlers(functions);
}

Result IStorage::Read(
    OutBuffer<BufferAttr_HipcMapAlias | BufferAttr_HipcMapTransferAllowsNonSecure | BufferAttr_HipcAutoSelect | BufferAttr_HipcPointer> out_bytes,
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

    if (length == 0 || out_bytes.empty()) {
        R_SUCCEED();
    }
    if (out_bytes.data() == nullptr && length > 0) {
        LOG_ERROR(Service_FS, "IStorage::Read: out_bytes.data() is null for length={}! Returning ResultNullptrArgument (0x2F5E02)", length);
        R_RETURN(FileSys::ResultNullptrArgument);
    }

    const std::size_t to_read = std::min<std::size_t>(static_cast<std::size_t>(length), out_bytes.size());
    const std::size_t read_bytes = backend->Read(out_bytes.data(), to_read, offset);
    if (offset == 0 || length <= 128) {
        std::string hex_preview;
        const std::size_t preview_len = std::min<std::size_t>(read_bytes, 16);
        for (std::size_t i = 0; i < preview_len; ++i) {
            hex_preview += fmt::format("{:02X} ", out_bytes[i]);
        }
        LOG_TRACE(Service_FS, "IStorage::Read: backend='{}' offset={:#x} length={} read_bytes={} data=[{}]",
                  backend->GetName(), offset, length, read_bytes, hex_preview);
    } else {
        LOG_TRACE(Service_FS, "IStorage::Read: backend='{}' offset={:#x} length={} read_bytes={}",
                  backend->GetName(), offset, length, read_bytes);
    }
    if (read_bytes < static_cast<std::size_t>(length)) {
        LOG_WARNING(Service_FS, "IStorage::Read: short read for '{}'! Requested {}, read {}. Zero-filling remainder.",
                    backend->GetName(), length, read_bytes);
        if (out_bytes.size() >= static_cast<std::size_t>(length)) {
            std::memset(out_bytes.data() + read_bytes, 0, length - read_bytes);
        }
    }

    R_SUCCEED();
}

Result IStorage::GetSize(Out<u64> out_size) {
    R_UNLESS(backend != nullptr, FileSys::ResultTargetNotFound);
    *out_size = backend->GetSize();

    LOG_TRACE(Service_FS, "IStorage::GetSize: backend='{}' size={}", backend->GetName(), *out_size);

    R_SUCCEED();
}

Result IStorage::Write(
    InBuffer<BufferAttr_HipcMapAlias | BufferAttr_HipcMapTransferAllowsNonSecure | BufferAttr_HipcAutoSelect | BufferAttr_HipcPointer> buffer,
    s64 offset, s64 length) {
    LOG_WARNING(Service_FS, "IStorage::Write called on read-only storage!");
    R_RETURN(FileSys::ResultWriteNotPermitted);
}

Result IStorage::Flush() {
    R_SUCCEED();
}

Result IStorage::SetSize(s64 size) {
    R_RETURN(FileSys::ResultUnsupportedSetSizeForIndirectStorage);
}

Result IStorage::OperateRange(Out<FsRangeInfo> out_info, u32 operation_type, s64 offset, s64 size) {
    LOG_DEBUG(Service_FS, "IStorage::OperateRange: op={}, offset={:#x}, size={}", operation_type, offset, size);
    *out_info = FsRangeInfo{};
    R_SUCCEED();
}

} // namespace Service::FileSystem
