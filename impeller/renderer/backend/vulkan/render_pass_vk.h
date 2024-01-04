// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_RENDERER_BACKEND_VULKAN_RENDER_PASS_VK_H_
#define FLUTTER_IMPELLER_RENDERER_BACKEND_VULKAN_RENDER_PASS_VK_H_

#include "flutter/fml/macros.h"
#include "impeller/core/formats.h"
#include "impeller/renderer/backend/vulkan/context_vk.h"
#include "impeller/renderer/backend/vulkan/pass_bindings_cache.h"
#include "impeller/renderer/backend/vulkan/shared_object_vk.h"
#include "impeller/renderer/render_pass.h"
#include "impeller/renderer/render_target.h"

namespace impeller {

class CommandBufferVK;

class RenderPassVK final : public RenderPass {
 public:
  // |RenderPass|
  ~RenderPassVK() override;

 private:
  friend class CommandBufferVK;

  std::shared_ptr<CommandBufferVK> command_buffer_;
  std::string debug_label_;
  std::shared_ptr<Pipeline<PipelineDescriptor>> pipeline_;
  vk::DescriptorBufferInfo buffers_[16];
  vk::DescriptorImageInfo images_[16];
  vk::WriteDescriptorSet writes_[32];
  bool is_valid_ = false;
  bool has_label_ = false;
  PassBindingsCache pass_bindings_cache_;

  RenderPassVK(const std::shared_ptr<const Context>& context,
               const RenderTarget& target,
               std::shared_ptr<CommandBufferVK> command_buffer);

  // |RenderPass|
  bool IsValid() const override;

  // |RenderPass|
  void OnSetLabel(std::string label) override;

  // |RenderPass|
  bool OnEncodeCommands(const Context& context) const override;

  void SetCommandLabel(const std::string& label) override;

  void SetPipeline(
      const std::shared_ptr<Pipeline<PipelineDescriptor>>& pipeline) override;

  void SetScissor(IRect value) override;

  void SetViewport(Viewport value) override;

  void SetStencilReference(uint32_t stencil_reference) override;

  bool OnRecordCommand(
      uint64_t base_vertex,
      size_t instance_count,
      const VertexBuffer& vertex_buffer,
      TextureAndSampler bound_textures[],
      size_t bound_texture_count,
      BufferAndUniformSlot bound_buffers[],
      size_t bound_buffer_count) override;

  SharedHandleVK<vk::RenderPass> CreateVKRenderPass(
      const ContextVK& context,
      const std::shared_ptr<CommandBufferVK>& command_buffer,
      bool has_subpass_dependency) const;

  SharedHandleVK<vk::Framebuffer> CreateVKFramebuffer(
      const ContextVK& context,
      const vk::RenderPass& pass) const;

  RenderPassVK(const RenderPassVK&) = delete;

  RenderPassVK& operator=(const RenderPassVK&) = delete;
};

}  // namespace impeller

#endif  // FLUTTER_IMPELLER_RENDERER_BACKEND_VULKAN_RENDER_PASS_VK_H_
