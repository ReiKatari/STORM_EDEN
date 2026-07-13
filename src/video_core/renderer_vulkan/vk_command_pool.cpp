// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstddef>
#include <stdexcept>

#include "common/logging.h"
#include "video_core/renderer_vulkan/vk_command_pool.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

constexpr size_t COMMAND_BUFFER_POOL_SIZE = 4;

struct CommandPool::Pool {
    vk::CommandPool handle;
    vk::CommandBuffers cmdbufs;
};

CommandPool::CommandPool(MasterSemaphore& master_semaphore_, const Device& device_)
    : ResourcePool(master_semaphore_, COMMAND_BUFFER_POOL_SIZE), device{device_} {}

CommandPool::~CommandPool() = default;

void CommandPool::Allocate(size_t begin, size_t end) {
    // Command buffers are going to be committed, recorded, executed every single usage cycle.
    // They are also going to be reset when committed.
    Pool& pool = pools.emplace_back();
    pool.handle = device.GetLogical().CreateCommandPool({
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags =
            VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = device.GetGraphicsFamily(),
    });
    pool.cmdbufs = pool.handle.Allocate(COMMAND_BUFFER_POOL_SIZE);
}

VkCommandBuffer CommandPool::Commit() {
    const size_t index = CommitResource();
    const auto pool_index = index / COMMAND_BUFFER_POOL_SIZE;
    const auto sub_index = index % COMMAND_BUFFER_POOL_SIZE;

    if (pool_index >= pools.size()) {
        LOG_CRITICAL(Render_Vulkan, "CommandPool::Commit: pool_index ({}) out of bounds (pools size: {})", pool_index, pools.size());
        throw std::runtime_error("CommandPool::Commit: pool_index out of bounds");
    }

    const auto& pool = pools[pool_index];
    if (pool.cmdbufs.IsOutOfPoolMemory()) {
        LOG_CRITICAL(Render_Vulkan, "CommandPool::Commit: CommandBuffers allocation failed (out of pool memory or allocation returned empty)");
        throw std::runtime_error("CommandPool::Commit: CommandBuffers allocation failed");
    }

    VkCommandBuffer cmdbuf = pool.cmdbufs[sub_index];
    if (cmdbuf == nullptr) {
        LOG_CRITICAL(Render_Vulkan, "CommandPool::Commit: Allocated VkCommandBuffer at pool_index {}, sub_index {} is NULL", pool_index, sub_index);
        throw std::runtime_error("CommandPool::Commit: Allocated VkCommandBuffer is NULL");
    }

    return cmdbuf;
}


} // namespace Vulkan
