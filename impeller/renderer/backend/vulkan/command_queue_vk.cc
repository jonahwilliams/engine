// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fml/status.h"

#include "impeller/renderer/backend/vulkan/command_queue_vk.h"

#include "impeller/base/validation.h"
#include "impeller/renderer/backend/vulkan/command_buffer_vk.h"
#include "impeller/renderer/backend/vulkan/command_encoder_vk.h"
#include "impeller/renderer/backend/vulkan/context_vk.h"
#include "impeller/renderer/backend/vulkan/fence_waiter_vk.h"
#include "impeller/renderer/backend/vulkan/texture_source_vk.h"
#include "impeller/renderer/backend/vulkan/tracked_objects_vk.h"
#include "impeller/renderer/command_buffer.h"

namespace impeller {

CommandQueueVK::CommandQueueVK(const std::weak_ptr<ContextVK>& context)
    : context_(context) {}

CommandQueueVK::~CommandQueueVK() = default;

void SetTextureLayout(const TextureSourceVK& texture,
                      const BarrierVK& barrier,
                      vk::ImageLayout old_layout) {
  const TextureDescriptor& desc = texture.GetTextureDescriptor();
  vk::ImageMemoryBarrier image_barrier;
  image_barrier.srcAccessMask = barrier.src_access;
  image_barrier.dstAccessMask = barrier.dst_access;
  image_barrier.oldLayout = old_layout;
  image_barrier.newLayout = barrier.new_layout;
  image_barrier.image = texture.GetImage();
  image_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  image_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  image_barrier.subresourceRange.aspectMask = ToImageAspectFlags(desc.format);
  image_barrier.subresourceRange.baseMipLevel = 0u;
  image_barrier.subresourceRange.levelCount = desc.mip_count;
  image_barrier.subresourceRange.baseArrayLayer = 0u;
  image_barrier.subresourceRange.layerCount = ToArrayLayerCount(desc.type);

  barrier.cmd_buffer.pipelineBarrier(barrier.src_stage,  // src stage
                                     barrier.dst_stage,  // dst stage
                                     {},                 // dependency flags
                                     nullptr,            // memory barriers
                                     nullptr,            // buffer barriers
                                     image_barrier       // image barriers
  );
}

void CommandQueueVK::DetermineFixupState(CommandBufferVK& buffer,
                                         CommandBufferVK& prev_buffer) {
  const auto& texture_usage = buffer.GetUsage();
  for (const auto& [texture, usage] : texture_usage) {
    const TextureVK& texture_vk = TextureVK::Cast(*texture);
    CommandBufferVK::UsageStruct old_usage = {.layout =
                                                  vk::ImageLayout::eUndefined};

    auto maybe_existing_state = image_states_.find(texture_vk.GetImage());
    if (maybe_existing_state != image_states_.end()) {
      old_usage = maybe_existing_state->second;
    }
    // No barrier needed, states match. Not sure what to do for stage mismatch.
    if (old_usage.layout == usage.layout) {
      continue;
    }

    // barrier mismatch, add pipeline barrier based on current and past
    // submitted usage.
    BarrierVK barrier;
    barrier.new_layout = usage.layout;
    barrier.cmd_buffer = prev_buffer.GetEncoder()->GetCommandBuffer();
    barrier.src_stage = old_usage.stage;
    barrier.src_access = old_usage.access;
    barrier.dst_stage = usage.stage;
    barrier.dst_access = usage.access;

    SetTextureLayout(texture_vk, barrier, old_usage.layout);

    // Updated recorded image state.
    image_states_[texture_vk.GetImage()] = usage;
  }
}

fml::Status CommandQueueVK::Submit(
    const std::vector<std::shared_ptr<CommandBuffer>>& buffers,
    const CompletionCallback& completion_callback) {
  if (buffers.empty()) {
    return fml::Status(fml::StatusCode::kInvalidArgument,
                       "No command buffers provided.");
  }
  // Success or failure, you only get to submit once.
  fml::ScopedCleanupClosure reset([&]() {
    if (completion_callback) {
      completion_callback(CommandBuffer::Status::kError);
    }
  });

  std::vector<vk::CommandBuffer> vk_buffers;
  std::vector<std::shared_ptr<TrackedObjectsVK>> tracked_objects;
  vk_buffers.reserve(buffers.size());
  tracked_objects.reserve(buffers.size());
  for (const std::shared_ptr<CommandBuffer>& buffer : buffers) {
    auto encoder = CommandBufferVK::Cast(*buffer).GetEncoder();
    if (!encoder->EndCommandBuffer()) {
      return fml::Status(fml::StatusCode::kCancelled,
                         "Failed to end command buffer.");
    }
    tracked_objects.push_back(encoder->tracked_objects_);
    vk_buffers.push_back(encoder->GetCommandBuffer());
    encoder->Reset();
  }

  auto context = context_.lock();
  if (!context) {
    VALIDATION_LOG << "Device lost.";
    return fml::Status(fml::StatusCode::kCancelled, "Device lost.");
  }
  auto [fence_result, fence] = context->GetDevice().createFenceUnique({});
  if (fence_result != vk::Result::eSuccess) {
    VALIDATION_LOG << "Failed to create fence: " << vk::to_string(fence_result);
    return fml::Status(fml::StatusCode::kCancelled, "Failed to create fence.");
  }

  vk::SubmitInfo submit_info;
  submit_info.setCommandBuffers(vk_buffers);
  auto status = context->GetGraphicsQueue()->Submit(submit_info, *fence);
  if (status != vk::Result::eSuccess) {
    VALIDATION_LOG << "Failed to submit queue: " << vk::to_string(status);
    return fml::Status(fml::StatusCode::kCancelled, "Failed to submit queue: ");
  }

  // Submit will proceed, call callback with true when it is done and do not
  // call when `reset` is collected.
  auto added_fence = context->GetFenceWaiter()->AddFence(
      std::move(fence), [completion_callback, tracked_objects = std::move(
                                                  tracked_objects)]() mutable {
        // Ensure tracked objects are destructed before calling any final
        // callbacks.
        tracked_objects.clear();
        if (completion_callback) {
          completion_callback(CommandBuffer::Status::kCompleted);
        }
      });
  if (!added_fence) {
    return fml::Status(fml::StatusCode::kCancelled, "Failed to add fence.");
  }
  reset.Release();
  return fml::Status();
}

}  // namespace impeller
