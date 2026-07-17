// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <mutex>
#include <string>

#include "core/file_sys/vfs/vfs.h"

namespace FileSys {

/**
 * A lazy-loading VFS file wrapper. Defers the expensive creation of the
 * underlying VirtualFile until the first actual I/O operation (Read, GetSize, etc.).
 *
 * This is used to speed up game boot by deferring DLC RomFS patching
 * until the game actually requests DLC data.
 */
class LazyVfsFile : public VfsFile {
public:
    using FileFactory = std::function<VirtualFile()>;

    explicit LazyVfsFile(FileFactory factory, std::size_t estimated_size,
                         std::string name)
        : factory_{std::move(factory)}, estimated_size_{estimated_size},
          name_{std::move(name)} {}

    ~LazyVfsFile() override = default;

    std::string GetName() const override {
        return name_;
    }

    std::size_t GetSize() const override {
        const auto& file = EnsureResolved();
        if (!file) {
            return estimated_size_;
        }
        return file->GetSize();
    }

    bool Resize(std::size_t new_size) override {
        auto& file = EnsureResolved();
        if (!file) {
            return false;
        }
        return file->Resize(new_size);
    }

    VirtualDir GetContainingDirectory() const override {
        const auto& file = EnsureResolved();
        if (!file) {
            return nullptr;
        }
        return file->GetContainingDirectory();
    }

    bool IsWritable() const override {
        return false;
    }

    bool IsReadable() const override {
        return true;
    }

    bool Rename(std::string_view new_name) override {
        name_ = new_name;
        return true;
    }

    bool IsNczFile() const override {
        const auto& file = EnsureResolved();
        return file ? file->IsNczFile() : false;
    }

    std::shared_ptr<VfsFile> GetUnderlyingFile() const override {
        return EnsureResolved();
    }

    NCZVirtualFile* GetNczFilePointer() override {
        const auto& file = EnsureResolved();
        return file ? file->GetNczFilePointer() : nullptr;
    }



    std::size_t Read(u8* data, std::size_t length, std::size_t offset = 0) const override {
        const auto& file = EnsureResolved();
        if (!file) {
            return 0;
        }
        return file->Read(data, length, offset);
    }

    std::size_t Write(const u8* data, std::size_t length, std::size_t offset = 0) override {
        auto& file = EnsureResolved();
        if (!file) {
            return 0;
        }
        return file->Write(data, length, offset);
    }

private:
    const VirtualFile& EnsureResolved() const {
        std::call_once(resolve_flag_, [this]() {
            resolved_file_ = factory_();
            factory_ = nullptr; // Release the factory and its captures
        });
        return resolved_file_;
    }

    VirtualFile& EnsureResolved() {
        std::call_once(resolve_flag_, [this]() {
            resolved_file_ = factory_();
            factory_ = nullptr;
        });
        return resolved_file_;
    }

    mutable FileFactory factory_;
    mutable VirtualFile resolved_file_;
    mutable std::once_flag resolve_flag_;
    std::size_t estimated_size_;
    std::string name_;
};

} // namespace FileSys
