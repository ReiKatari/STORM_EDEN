// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/alignment.h"
#include "core/file_sys/fssystem/fssystem_integrity_verification_storage.h"

namespace FileSys {

constexpr inline u32 ILog2(u32 val) {
    ASSERT(val > 0);
    return static_cast<u32>((sizeof(u32) * 8) - 1 - std::countl_zero<u32>(val));
}

void IntegrityVerificationStorage::Initialize(VirtualFile hs, VirtualFile ds, s64 verif_block_size,
                                              s64 upper_layer_verif_block_size, bool is_real_data) {
    // Validate preconditions.
    ASSERT(verif_block_size >= HashSize);

    // Set storages.
    m_hash_storage = hs;
    m_data_storage = ds;

    // Set verification block sizes.
    m_verification_block_size = verif_block_size;
    m_verification_block_order = ILog2(static_cast<u32>(verif_block_size));
    ASSERT(m_verification_block_size == 1ll << m_verification_block_order);

    // Set upper layer block sizes.
    upper_layer_verif_block_size = (std::max)(upper_layer_verif_block_size, HashSize);
    m_upper_layer_verification_block_size = upper_layer_verif_block_size;
    m_upper_layer_verification_block_order = ILog2(static_cast<u32>(upper_layer_verif_block_size));
    ASSERT(m_upper_layer_verification_block_size == 1ll << m_upper_layer_verification_block_order);

    // Validate sizes.
    {
        s64 hash_size = m_hash_storage->GetSize();
        s64 data_size = m_data_storage->GetSize();
        ASSERT(((hash_size / HashSize) * m_verification_block_size) >= data_size);
    }

    // Set data.
    m_is_real_data = is_real_data;
}

void IntegrityVerificationStorage::Finalize() {
    m_hash_storage = VirtualFile();
    m_data_storage = VirtualFile();
}

size_t IntegrityVerificationStorage::Read(u8* buffer, size_t size, size_t offset) const {
    // Succeed if zero size.
    if (size == 0) {
        return size;
    }

    // Validate arguments.
    ASSERT(buffer != nullptr);

    // Validate the offset gracefully without crashing the emulator.
    s64 data_size = m_data_storage->GetSize();
    if (offset > static_cast<size_t>(data_size)) {
        LOG_WARNING(Service_FS, "Out of bounds read offset in IntegrityVerificationStorage: offset={:X}, data_size={:X}", offset, data_size);
        return 0; // EOF
    }

    // Validate the access range gracefully without crashing the emulator.
    size_t aligned_data_size = Common::AlignUp(data_size, static_cast<size_t>(m_verification_block_size));
    if (R_FAILED(IStorage::CheckAccessRange(offset, size, aligned_data_size))) {
        // Log as warning instead of error so the console isn't flooded with red text
        LOG_WARNING(Service_FS, "Out of bounds read in IntegrityVerificationStorage: offset={:X}, size={:X}, aligned_data_size={:X}", offset, size, aligned_data_size);
        if (offset >= aligned_data_size) {
            return 0; // EOF
        }
        size = aligned_data_size - offset; // Clamp to available size
    }

    // Determine the read extents.
    size_t read_size = size;
    if (static_cast<s64>(offset + read_size) > data_size) {
        // The valid data size from this offset to the end of the real data.
        s64 valid_data_size = std::max<s64>(0, data_size - offset);
        
        // The rest of the requested size must be padded with 0s.
        size_t padding_size = size - static_cast<size_t>(valid_data_size);

        // Clear the padding within the bounds of the requested buffer size.
        std::memset(static_cast<u8*>(buffer) + valid_data_size, 0, padding_size);

        // Set the new in-bounds size for the underlying read.
        read_size = static_cast<size_t>(valid_data_size);
    }

    // Perform the read.
    return m_data_storage->Read(buffer, read_size, offset);
}

size_t IntegrityVerificationStorage::GetSize() const {
    return m_data_storage->GetSize();
}

} // namespace FileSys
