#pragma once

#include "core/file_sys/fs_operate_range.h"
#include "core/file_sys/vfs/vfs.h"
#include "core/hle/service/cmif_types.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/hle/service/service.h"

namespace Service::FileSystem {

struct FsRangeInfo {
    s64 aes_ctr_key_type{0};
    s64 speed_emulation_type{0};
};

class IStorage final : public ServiceFramework<IStorage> {
public:
    explicit IStorage(Core::System& system_, FileSys::VirtualFile backend_);

private:
    FileSys::VirtualFile backend;

    Result Read(
        OutBuffer<BufferAttr_HipcMapAlias | BufferAttr_HipcMapTransferAllowsNonSecure | BufferAttr_HipcAutoSelect | BufferAttr_HipcPointer> out_bytes,
        s64 offset, s64 length);
    Result Write(
        InBuffer<BufferAttr_HipcMapAlias | BufferAttr_HipcMapTransferAllowsNonSecure | BufferAttr_HipcAutoSelect | BufferAttr_HipcPointer> buffer,
        s64 offset, s64 length);
    Result Flush();
    Result SetSize(s64 size);
    Result GetSize(Out<u64> out_size);
    Result OperateRange(Out<FsRangeInfo> out_info, u32 operation_type, s64 offset, s64 size);
};

} // namespace Service::FileSystem
