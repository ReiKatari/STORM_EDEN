#include <algorithm>
#include <cstring>
#include "core/file_sys/vfs/vfs_buffered.h"

namespace FileSys {

BufferedVfsFile::BufferedVfsFile(VirtualFile file_, std::size_t buffer_size_)
    : file(std::move(file_)), buffer_size(buffer_size_) {
    buffer.resize(buffer_size);
}

BufferedVfsFile::~BufferedVfsFile() = default;

std::string BufferedVfsFile::GetName() const {
    return file->GetName();
}

std::size_t BufferedVfsFile::GetSize() const {
    return file->GetSize();
}

bool BufferedVfsFile::Resize(std::size_t new_size) {
    return file->Resize(new_size);
}

VirtualDir BufferedVfsFile::GetContainingDirectory() const {
    return file->GetContainingDirectory();
}

bool BufferedVfsFile::IsWritable() const {
    return file->IsWritable();
}

bool BufferedVfsFile::IsReadable() const {
    return file->IsReadable();
}

bool BufferedVfsFile::IsNczFile() const {
    return file->IsNczFile();
}

VirtualFile BufferedVfsFile::GetUnderlyingFile() const {
    return file;
}

NCZVirtualFile* BufferedVfsFile::GetNczFilePointer() {
    return file ? file->GetNczFilePointer() : nullptr;
}



std::size_t BufferedVfsFile::Read(u8* data, std::size_t length, std::size_t offset) const {
    if (length == 0) {
        return 0;
    }

    // If the request is larger than the buffer, just read directly to avoid double-copying
    if (length >= buffer_size) {
        return file->Read(data, length, offset);
    }

    std::scoped_lock lock{buffer_mutex};

    // Check if the requested range is fully within the current buffer
    if (buffer_valid_size > 0 && offset >= buffer_offset && (offset + length) <= (buffer_offset + buffer_valid_size)) {
        std::memcpy(data, buffer.data() + (offset - buffer_offset), length);
        return length;
    }

    // Check if it's partially within the buffer
    if (buffer_valid_size > 0 && offset >= buffer_offset && offset < (buffer_offset + buffer_valid_size)) {
        std::size_t available = (buffer_offset + buffer_valid_size) - offset;
        std::memcpy(data, buffer.data() + (offset - buffer_offset), available);
        
        // Read the rest directly (or we could refill the buffer, but this is simple enough)
        return available + file->Read(data + available, length - available, offset + available);
    }

    // Otherwise, we need to read from the file to populate the buffer
    buffer_offset = offset;
    
    // Clamp the requested size to the remaining file size to avoid underlying storage OOB errors
    std::size_t remaining_file_size = 0;
    std::size_t total_size = file->GetSize();
    if (offset < total_size) {
        remaining_file_size = total_size - offset;
    }
    std::size_t read_request_size = std::min(buffer_size, remaining_file_size);
    
    buffer_valid_size = file->Read(buffer.data(), read_request_size, buffer_offset);

    // If we read nothing, return 0
    if (buffer_valid_size == 0) {
        return 0;
    }

    // Determine how much we can actually satisfy from what we just read
    std::size_t bytes_to_copy = std::min(length, buffer_valid_size);
    std::memcpy(data, buffer.data(), bytes_to_copy);

    return bytes_to_copy;
}

std::size_t BufferedVfsFile::Write(const u8* data, std::size_t length, std::size_t offset) {
    // Invalidate buffer on write to prevent stale reads
    {
        std::scoped_lock lock{buffer_mutex};
        buffer_valid_size = 0;
    }
    return file->Write(data, length, offset);
}

bool BufferedVfsFile::Rename(std::string_view new_name) {
    return file->Rename(new_name);
}

} // namespace FileSys
