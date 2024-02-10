// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_RENDERER_BACKEND_VULKAN_COMMAND_QUEUE_VK_H_
#define FLUTTER_IMPELLER_RENDERER_BACKEND_VULKAN_COMMAND_QUEUE_VK_H_

#include <unordered_map>

#include "impeller/renderer/backend/vulkan/command_buffer_vk.h"
#include "impeller/renderer/command_queue.h"
#include "impeller/renderer/backend/vulkan/vk.h"  // IWYU pragma: keep.

namespace impeller {

class ContextVK;

class CommandQueueVK : public CommandQueue {
 public:
  explicit CommandQueueVK(const std::weak_ptr<ContextVK>& context);

  ~CommandQueueVK() override;

  fml::Status Submit(
      const std::vector<std::shared_ptr<CommandBuffer>>& buffers,
      const CompletionCallback& completion_callback = {}) override;

 private:
  std::weak_ptr<ContextVK> context_;
  std::unordered_map<vk::Image, CommandBufferVK::UsageStruct> image_states_;

  void DetermineFixupState(CommandBufferVK& buffer, CommandBufferVK& prev_buffer);

  CommandQueueVK(const CommandQueueVK&) = delete;

  CommandQueueVK& operator=(const CommandQueueVK&) = delete;
};

}  // namespace impeller

#endif  // FLUTTER_IMPELLER_RENDERER_BACKEND_VULKAN_COMMAND_QUEUE_VK_H_
