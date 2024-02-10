// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_RENDERER_BACKEND_VULKAN_COMMAND_BUFFER_VK_H_
#define FLUTTER_IMPELLER_RENDERER_BACKEND_VULKAN_COMMAND_BUFFER_VK_H_

#include <unordered_map>
#include "impeller/base/backend_cast.h"
#include "impeller/core/shader_types.h"
#include "impeller/renderer/backend/vulkan/texture_vk.h"
#include "impeller/renderer/backend/vulkan/vk.h"
#include "impeller/renderer/command_buffer.h"

namespace impeller {

class ContextVK;
class CommandEncoderFactoryVK;
class CommandEncoderVK;

class CommandBufferVK final
    : public CommandBuffer,
      public BackendCast<CommandBufferVK, CommandBuffer>,
      public std::enable_shared_from_this<CommandBufferVK> {
 public:
  // |CommandBuffer|
  ~CommandBufferVK() override;

  const std::shared_ptr<CommandEncoderVK>& GetEncoder();

  struct UsageStruct {
    vk::ImageLayout layout;
    vk::Flags<vk::PipelineStageFlagBits> stage;
    vk::Flags<vk::AccessFlagBits> access;
  };

  const std::unordered_map<std::shared_ptr<const Texture>, UsageStruct>& GetUsage() const {
    return image_dependencies_;
  }

  void RecordUsage(const std::shared_ptr<const Texture>& texture,
                   vk::ImageLayout layout,
                   vk::PipelineStageFlagBits stage,
                   vk::AccessFlagBits access) {
    auto res = image_dependencies_.find(texture);
    // Only the first image layout is recorded. If we need more than one
    // image layout, then we'd need to handle inserting barriers here.
    if (res == image_dependencies_.end()) {
      image_dependencies_[texture] =
          UsageStruct{.layout = layout, .stage = stage, .access = access};
      return;
    }
    FML_DCHECK(res->second.stage == stage);
    res->second.access |= access;
    res->second.stage |= stage;
  }

 private:
  friend class ContextVK;

  std::shared_ptr<CommandEncoderVK> encoder_;
  std::shared_ptr<CommandEncoderFactoryVK> encoder_factory_;
  std::unordered_map<std::shared_ptr<const Texture>, UsageStruct> image_dependencies_;

  CommandBufferVK(std::weak_ptr<const Context> context,
                  std::shared_ptr<CommandEncoderFactoryVK> encoder_factory);

  // |CommandBuffer|
  void SetLabel(const std::string& label) const override;

  // |CommandBuffer|
  bool IsValid() const override;

  // |CommandBuffer|
  bool OnSubmitCommands(CompletionCallback callback) override;

  // |CommandBuffer|
  void OnWaitUntilScheduled() override;

  // |CommandBuffer|
  std::shared_ptr<RenderPass> OnCreateRenderPass(RenderTarget target) override;

  // |CommandBuffer|
  std::shared_ptr<BlitPass> OnCreateBlitPass() override;

  // |CommandBuffer|
  std::shared_ptr<ComputePass> OnCreateComputePass() override;

  CommandBufferVK(const CommandBufferVK&) = delete;

  CommandBufferVK& operator=(const CommandBufferVK&) = delete;
};

}  // namespace impeller

#endif  // FLUTTER_IMPELLER_RENDERER_BACKEND_VULKAN_COMMAND_BUFFER_VK_H_
