#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "core/file_sys/vfs/vfs.h"

namespace FileSys {

class BufferedVfsFile : public VfsFile {
public:
    explicit BufferedVfsFile(VirtualFile file, std::size_t buffer_size = 1024 * 1024);
    ~BufferedVfsFile() override;

    std::string GetName() const override;
    std::size_t GetSize() const override;
    bool Resize(std::size_t new_size) override;
    VirtualDir GetContainingDirectory() const override;
    bool IsWritable() const override;
    bool IsReadable() const override;
    bool IsNczFile() const override;

    std::size_t Read(u8* data, std::size_t length, std::size_t offset) const override;
    std::size_t Write(const u8* data, std::size_t length, std::size_t offset) override;
    bool Rename(std::string_view new_name) override;

private:
    VirtualFile file;
    std::size_t buffer_size;

    mutable std::mutex buffer_mutex;
    mutable std::vector<u8> buffer;
    mutable std::size_t buffer_offset = 0;
    mutable std::size_t buffer_valid_size = 0;
};

} // namespace FileSys
