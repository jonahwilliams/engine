// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/renderer/backend/vulkan/render_pass_vk.h"

#include <array>
#include <cstdint>
#include <vector>

#include "impeller/base/validation.h"
#include "impeller/core/formats.h"
#include "impeller/renderer/backend/vulkan/barrier_vk.h"
#include "impeller/renderer/backend/vulkan/command_buffer_vk.h"
#include "impeller/renderer/backend/vulkan/command_encoder_vk.h"
#include "impeller/renderer/backend/vulkan/context_vk.h"
#include "impeller/renderer/backend/vulkan/device_buffer_vk.h"
#include "impeller/renderer/backend/vulkan/formats_vk.h"
#include "impeller/renderer/backend/vulkan/pipeline_vk.h"
#include "impeller/renderer/backend/vulkan/sampler_vk.h"
#include "impeller/renderer/backend/vulkan/shared_object_vk.h"
#include "impeller/renderer/backend/vulkan/texture_vk.h"
#include "impeller/renderer/command.h"
#include "vulkan/vulkan_enums.hpp"
#include "vulkan/vulkan_handles.hpp"
#include "vulkan/vulkan_to_string.hpp"

namespace impeller {

static void SetViewportAndScissor(const vk::CommandBuffer& cmd_buffer,
                                  PassBindingsCache& cmd_buffer_cache,
                                  const ISize& target_size) {
  // Set the viewport.
  const auto& vp = Viewport{.rect = Rect::MakeSize(target_size)};
  vk::Viewport viewport = vk::Viewport()
                              .setWidth(vp.rect.GetWidth())
                              .setHeight(-vp.rect.GetHeight())
                              .setY(vp.rect.GetHeight())
                              .setMinDepth(0.0f)
                              .setMaxDepth(1.0f);
  cmd_buffer_cache.SetViewport(cmd_buffer, 0, 1, &viewport);

  // Set the scissor rect.
  const auto& sc = IRect::MakeSize(target_size);
  vk::Rect2D scissor =
      vk::Rect2D()
          .setOffset(vk::Offset2D(sc.GetX(), sc.GetY()))
          .setExtent(vk::Extent2D(sc.GetWidth(), sc.GetHeight()));
  cmd_buffer_cache.SetScissor(cmd_buffer, 0, 1, &scissor);
}

static vk::ClearDepthStencilValue VKClearValueFromDepthStencil(uint32_t stencil,
                                                               Scalar depth) {
  vk::ClearDepthStencilValue value;
  value.depth = depth;
  value.stencil = stencil;
  return value;
}

static vk::ClearColorValue VKClearValueFromColor(Color color) {
  vk::ClearColorValue value;
  value.setFloat32(
      std::array<float, 4>{color.red, color.green, color.blue, color.alpha});
  return value;
}

static std::vector<vk::ClearValue> GetVKClearValues(
    const RenderTarget& target) {
  std::vector<vk::ClearValue> clears;

  for (const auto& [_, color] : target.GetColorAttachments()) {
    clears.emplace_back(VKClearValueFromColor(color.clear_color));
    if (color.resolve_texture) {
      clears.emplace_back(VKClearValueFromColor(color.clear_color));
    }
  }

  const auto& depth = target.GetDepthAttachment();
  const auto& stencil = target.GetStencilAttachment();

  if (depth.has_value()) {
    clears.emplace_back(VKClearValueFromDepthStencil(
        stencil ? stencil->clear_stencil : 0u, depth->clear_depth));
  }

  if (stencil.has_value()) {
    clears.emplace_back(VKClearValueFromDepthStencil(
        stencil->clear_stencil, depth ? depth->clear_depth : 0.0f));
  }

  return clears;
}

static vk::AttachmentDescription CreateAttachmentDescription(
    const Attachment& attachment,
    const std::shared_ptr<Texture> Attachment::*texture_ptr,
    bool supports_framebuffer_fetch) {
  const auto& texture = attachment.*texture_ptr;
  if (!texture) {
    return {};
  }
  const auto& texture_vk = TextureVK::Cast(*texture);
  const auto& desc = texture->GetTextureDescriptor();
  auto current_layout = texture_vk.GetLayout();

  auto load_action = attachment.load_action;
  auto store_action = attachment.store_action;

  if (current_layout == vk::ImageLayout::eUndefined) {
    load_action = LoadAction::kClear;
  }

  if (desc.storage_mode == StorageMode::kDeviceTransient) {
    store_action = StoreAction::kDontCare;
  } else if (texture_ptr == &Attachment::resolve_texture) {
    store_action = StoreAction::kStore;
  }

  // Always insert a barrier to transition to color attachment optimal.
  if (current_layout != vk::ImageLayout::ePresentSrcKHR &&
      current_layout != vk::ImageLayout::eUndefined) {
    // Note: This should incur a barrier.
    current_layout = vk::ImageLayout::eGeneral;
  }

  return CreateAttachmentDescription(desc.format,        //
                                     desc.sample_count,  //
                                     load_action,        //
                                     store_action,       //
                                     current_layout,
                                     supports_framebuffer_fetch  //
  );
}

static void SetTextureLayout(
    const Attachment& attachment,
    const vk::AttachmentDescription& attachment_desc,
    const std::shared_ptr<CommandBufferVK>& command_buffer,
    const std::shared_ptr<Texture> Attachment::*texture_ptr) {
  const auto& texture = attachment.*texture_ptr;
  if (!texture) {
    return;
  }
  const auto& texture_vk = TextureVK::Cast(*texture);

  if (attachment_desc.initialLayout == vk::ImageLayout::eGeneral) {
    BarrierVK barrier;
    barrier.new_layout = vk::ImageLayout::eGeneral;
    barrier.cmd_buffer = command_buffer->GetEncoder()->GetCommandBuffer();
    barrier.src_access = vk::AccessFlagBits::eShaderRead;
    barrier.src_stage = vk::PipelineStageFlagBits::eFragmentShader;
    barrier.dst_access = vk::AccessFlagBits::eColorAttachmentWrite |
                         vk::AccessFlagBits::eTransferWrite;
    barrier.dst_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                        vk::PipelineStageFlagBits::eTransfer;

    texture_vk.SetLayout(barrier);
  }

  // Instead of transitioning layouts manually using barriers, we are going to
  // make the subpass perform our transitions.
  texture_vk.SetLayoutWithoutEncoding(attachment_desc.finalLayout);
}

SharedHandleVK<vk::RenderPass> RenderPassVK::CreateVKRenderPass(
    const ContextVK& context,
    const std::shared_ptr<CommandBufferVK>& command_buffer,
    bool supports_framebuffer_fetch) const {
  std::vector<vk::AttachmentDescription> attachments;

  std::vector<vk::AttachmentReference> color_refs;
  std::vector<vk::AttachmentReference> resolve_refs;
  vk::AttachmentReference depth_stencil_ref = kUnusedAttachmentReference;

  // Spec says: "Each element of the pColorAttachments array corresponds to an
  // output location in the shader, i.e. if the shader declares an output
  // variable decorated with a Location value of X, then it uses the attachment
  // provided in pColorAttachments[X]. If the attachment member of any element
  // of pColorAttachments is VK_ATTACHMENT_UNUSED."
  //
  // Just initialize all the elements as unused and fill in the valid bind
  // points in the loop below.
  color_refs.resize(render_target_.GetMaxColorAttacmentBindIndex() + 1u,
                    kUnusedAttachmentReference);
  resolve_refs.resize(render_target_.GetMaxColorAttacmentBindIndex() + 1u,
                      kUnusedAttachmentReference);

  for (const auto& [bind_point, color] : render_target_.GetColorAttachments()) {
    color_refs[bind_point] = vk::AttachmentReference{
        static_cast<uint32_t>(attachments.size()),
        supports_framebuffer_fetch ? vk::ImageLayout::eGeneral
                                   : vk::ImageLayout::eColorAttachmentOptimal};
    attachments.emplace_back(CreateAttachmentDescription(
        color, &Attachment::texture, supports_framebuffer_fetch));
    SetTextureLayout(color, attachments.back(), command_buffer,
                     &Attachment::texture);
    if (color.resolve_texture) {
      resolve_refs[bind_point] = vk::AttachmentReference{
          static_cast<uint32_t>(attachments.size()),
          supports_framebuffer_fetch
              ? vk::ImageLayout::eGeneral
              : vk::ImageLayout::eColorAttachmentOptimal};
      attachments.emplace_back(CreateAttachmentDescription(
          color, &Attachment::resolve_texture, supports_framebuffer_fetch));
      SetTextureLayout(color, attachments.back(), command_buffer,
                       &Attachment::resolve_texture);
    }
  }

  if (auto depth = render_target_.GetDepthAttachment(); depth.has_value()) {
    depth_stencil_ref = vk::AttachmentReference{
        static_cast<uint32_t>(attachments.size()),
        vk::ImageLayout::eDepthStencilAttachmentOptimal};
    attachments.emplace_back(CreateAttachmentDescription(
        depth.value(), &Attachment::texture, supports_framebuffer_fetch));
    SetTextureLayout(depth.value(), attachments.back(), command_buffer,
                     &Attachment::texture);
  }

  if (auto stencil = render_target_.GetStencilAttachment();
      stencil.has_value()) {
    depth_stencil_ref = vk::AttachmentReference{
        static_cast<uint32_t>(attachments.size()),
        vk::ImageLayout::eDepthStencilAttachmentOptimal};
    attachments.emplace_back(CreateAttachmentDescription(
        stencil.value(), &Attachment::texture, supports_framebuffer_fetch));
    SetTextureLayout(stencil.value(), attachments.back(), command_buffer,
                     &Attachment::texture);
  }

  vk::SubpassDescription subpass_desc;
  subpass_desc.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
  subpass_desc.setColorAttachments(color_refs);
  subpass_desc.setResolveAttachments(resolve_refs);
  subpass_desc.setPDepthStencilAttachment(&depth_stencil_ref);

  std::vector<vk::SubpassDependency> subpass_dependencies;
  std::vector<vk::AttachmentReference> subpass_color_ref;
  subpass_color_ref.push_back(vk::AttachmentReference{
      static_cast<uint32_t>(0), vk::ImageLayout::eColorAttachmentOptimal});
  if (supports_framebuffer_fetch) {
    subpass_desc.setFlags(vk::SubpassDescriptionFlagBits::
                              eRasterizationOrderAttachmentColorAccessARM);
    subpass_desc.setInputAttachments(subpass_color_ref);
  }

  vk::RenderPassCreateInfo render_pass_desc;
  render_pass_desc.setAttachments(attachments);
  render_pass_desc.setPSubpasses(&subpass_desc);
  render_pass_desc.setSubpassCount(1u);

  auto [result, pass] =
      context.GetDevice().createRenderPassUnique(render_pass_desc);
  if (result != vk::Result::eSuccess) {
    VALIDATION_LOG << "Failed to create render pass: " << vk::to_string(result);
    return {};
  }
  context.SetDebugName(pass.get(), debug_label_.c_str());
  return MakeSharedVK(std::move(pass));
}

static bool AllocateAndBindDescriptorSets(const ContextVK& context,
                                          CommandEncoderVK& encoder,
                                          const PipelineVK& pipeline,
                                          TextureAndSampler bound_textures[],
                                          size_t bound_texture_count,
                                          BufferAndUniformSlot bound_buffers[],
                                          size_t bound_buffer_count,
                                          vk::DescriptorBufferInfo buffers[],
                                          vk::DescriptorImageInfo images[],
                                          vk::WriteDescriptorSet writes[]) {
  auto& desc_set =
      pipeline.GetDescriptor().GetVertexDescriptor()->GetDescriptorSetLayouts();
  auto vk_desc_set =
      encoder.AllocateDescriptorSets(pipeline.GetDescriptorSetLayout());
  if (!vk_desc_set.ok()) {
    return false;
  }

  auto& allocator = *context.GetResourceAllocator();

  size_t buffer_offset = 0;
  size_t image_offset = 0;
  size_t write_offset = 0;

  for (auto i = 0u; i < bound_texture_count; i++) {
    const auto& sampled_image = bound_textures[i];
    const auto& texture_vk = TextureVK::Cast(*sampled_image.texture.resource);
    const SamplerVK& sampler = SamplerVK::Cast(*sampled_image.sampler);

    if (!encoder.Track(sampled_image.texture.resource) ||
        !encoder.Track(sampler.GetSharedSampler())) {
      return false;
    }
    vk::DescriptorImageInfo image_info;
    image_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    image_info.sampler = sampler.GetSampler();
    image_info.imageView = texture_vk.GetImageView();
    images[image_offset++] = image_info;

    vk::WriteDescriptorSet write_set;
    write_set.dstSet = vk_desc_set.value();
    write_set.dstBinding = sampled_image.slot.binding;
    write_set.descriptorCount = 1u;
    write_set.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    write_set.pImageInfo = &images[image_offset - 1];

    writes[write_offset++] = write_set;
  }

  for (auto i = 0u; i < bound_buffer_count; i++) {
    const auto& bound_buffer = bound_buffers[i];
    const auto& buffer_view = bound_buffer.view.resource.buffer;

    auto device_buffer = buffer_view->GetDeviceBuffer(allocator);
    if (!device_buffer) {
      VALIDATION_LOG << "Failed to get device buffer for vertex binding";
      return false;
    }

    auto buffer = DeviceBufferVK::Cast(*device_buffer).GetBuffer();
    if (!buffer) {
      return false;
    }

    if (!encoder.Track(device_buffer)) {
      return false;
    }

    uint32_t offset = bound_buffer.view.resource.range.offset;

    vk::DescriptorBufferInfo buffer_info;
    buffer_info.buffer = buffer;
    buffer_info.offset = offset;
    buffer_info.range = bound_buffer.view.resource.range.length;

    auto slot = bound_buffer.slot;
    auto layout_it = std::find_if(desc_set.begin(), desc_set.end(),
                                  [&slot](const DescriptorSetLayout& layout) {
                                    return layout.binding == slot.binding;
                                  });
    if (layout_it == desc_set.end()) {
      VALIDATION_LOG << "Failed to get descriptor set layout for binding "
                     << slot.binding;
      return false;
    }
    auto layout = *layout_it;

    buffers[buffer_offset++] = buffer_info;

    vk::WriteDescriptorSet write_set;
    write_set.dstSet = vk_desc_set.value();
    write_set.dstBinding = slot.binding;
    write_set.descriptorCount = 1u;
    write_set.descriptorType = ToVKDescriptorType(layout.descriptor_type);
    write_set.pBufferInfo = &(buffers[buffer_offset - 1]);

    writes[write_offset++] = write_set;
  }

  context.GetDevice().updateDescriptorSets(write_offset, writes, {}, 0u);

  auto set = vk_desc_set.value();
  encoder.GetCommandBuffer().bindDescriptorSets(
      vk::PipelineBindPoint::eGraphics,          // bind point
      pipeline.GetPipelineLayout(),              // layout
      0,                                         // first set
      set,  // sets
      nullptr                                    // offsets
  );
  return true;
}

RenderPassVK::RenderPassVK(const std::shared_ptr<const Context>& context,
                           const RenderTarget& target,
                           std::shared_ptr<CommandBufferVK> command_buffer)
    : RenderPass(context, target), command_buffer_(std::move(command_buffer)) {
  is_valid_ = true;

  // SETUP
  const auto& vk_context = ContextVK::Cast(*GetContext());
  auto encoder = command_buffer_->GetEncoder();
  if (!encoder) {
    return;
  }

  fml::ScopedCleanupClosure pop_marker(
      [&encoder]() { encoder->PopDebugGroup(); });
  if (!debug_label_.empty()) {
    encoder->PushDebugGroup(debug_label_.c_str());
  } else {
    pop_marker.Release();
  }

  auto cmd_buffer = encoder->GetCommandBuffer();

  render_target_.IterateAllAttachments(
      [&encoder](const auto& attachment) -> bool {
        encoder->Track(attachment.texture);
        encoder->Track(attachment.resolve_texture);
        return true;
      });

  const auto& target_size = render_target_.GetRenderTargetSize();

  auto render_pass = CreateVKRenderPass(
      vk_context, command_buffer_,
      vk_context.GetCapabilities()->SupportsFramebufferFetch());
  if (!render_pass) {
    VALIDATION_LOG << "Could not create renderpass.";
    return;
  }

  auto framebuffer = CreateVKFramebuffer(vk_context, *render_pass);
  if (!framebuffer) {
    VALIDATION_LOG << "Could not create framebuffer.";
    return;
  }

  if (!encoder->Track(framebuffer) || !encoder->Track(render_pass)) {
    return;
  }

  auto clear_values = GetVKClearValues(render_target_);

  vk::RenderPassBeginInfo pass_info;
  pass_info.renderPass = *render_pass;
  pass_info.framebuffer = *framebuffer;
  pass_info.renderArea.extent.width = static_cast<uint32_t>(target_size.width);
  pass_info.renderArea.extent.height =
      static_cast<uint32_t>(target_size.height);
  pass_info.setClearValues(clear_values);

  cmd_buffer.beginRenderPass(pass_info, vk::SubpassContents::eInline);

  SetViewportAndScissor(cmd_buffer, pass_bindings_cache_, render_target_size_);
}

RenderPassVK::~RenderPassVK() = default;

bool RenderPassVK::IsValid() const {
  return is_valid_;
}

void RenderPassVK::OnSetLabel(std::string label) {
  debug_label_ = std::move(label);
}

SharedHandleVK<vk::Framebuffer> RenderPassVK::CreateVKFramebuffer(
    const ContextVK& context,
    const vk::RenderPass& pass) const {
  vk::FramebufferCreateInfo fb_info;

  fb_info.renderPass = pass;

  const auto target_size = render_target_.GetRenderTargetSize();
  fb_info.width = target_size.width;
  fb_info.height = target_size.height;
  fb_info.layers = 1u;

  std::vector<vk::ImageView> attachments;

  // This bit must be consistent to ensure compatibility with the pass created
  // earlier. Follow this order: Color attachments, then depth, then stencil.
  for (const auto& [_, color] : render_target_.GetColorAttachments()) {
    // The bind point doesn't matter here since that information is present in
    // the render pass.
    attachments.emplace_back(TextureVK::Cast(*color.texture).GetImageView());
    if (color.resolve_texture) {
      attachments.emplace_back(
          TextureVK::Cast(*color.resolve_texture).GetImageView());
    }
  }
  if (auto depth = render_target_.GetDepthAttachment(); depth.has_value()) {
    attachments.emplace_back(TextureVK::Cast(*depth->texture).GetImageView());
  }
  if (auto stencil = render_target_.GetStencilAttachment();
      stencil.has_value()) {
    attachments.emplace_back(TextureVK::Cast(*stencil->texture).GetImageView());
  }

  fb_info.setAttachments(attachments);

  auto [result, framebuffer] =
      context.GetDevice().createFramebufferUnique(fb_info);

  if (result != vk::Result::eSuccess) {
    VALIDATION_LOG << "Could not create framebuffer: " << vk::to_string(result);
    return {};
  }

  return MakeSharedVK(std::move(framebuffer));
}

void RenderPassVK::SetCommandLabel(const std::string& label) {
#ifdef IMPELLER_DEBUG
  has_label_ = true;
  command_buffer_->GetEncoder()->PushDebugGroup(label.c_str());
#endif  // IMPELLER_DEBUG
}

void RenderPassVK::SetPipeline(
    const std::shared_ptr<Pipeline<PipelineDescriptor>>& pipeline) {
  pipeline_ = pipeline;
  const auto& cmd_buffer = command_buffer_->GetEncoder()->GetCommandBuffer();
  pass_bindings_cache_.BindPipeline(cmd_buffer,
                                    vk::PipelineBindPoint::eGraphics,
                                    PipelineVK::Cast(*pipeline).GetPipeline());
}

void RenderPassVK::SetScissor(IRect value) {
  const auto& cmd_buffer = command_buffer_->GetEncoder()->GetCommandBuffer();
  vk::Rect2D vk_scissor =
      vk::Rect2D()
          .setOffset(vk::Offset2D(value.GetX(), value.GetY()))
          .setExtent(vk::Extent2D(value.GetWidth(), value.GetHeight()));
  pass_bindings_cache_.SetScissor(cmd_buffer, 0, 1, &vk_scissor);
}

void RenderPassVK::SetViewport(Viewport value) {
  const auto& cmd_buffer = command_buffer_->GetEncoder()->GetCommandBuffer();
  vk::Viewport vk_viewport = vk::Viewport()
                                 .setWidth(value.rect.GetWidth())
                                 .setHeight(-value.rect.GetHeight())
                                 .setY(value.rect.GetHeight())
                                 .setMinDepth(0.0f)
                                 .setMaxDepth(1.0f);
  pass_bindings_cache_.SetViewport(cmd_buffer, 0, 1, &vk_viewport);
}

void RenderPassVK::SetStencilReference(uint32_t stencil_reference) {
  const auto& cmd_buffer = command_buffer_->GetEncoder()->GetCommandBuffer();
  pass_bindings_cache_.SetStencilReference(
      cmd_buffer, vk::StencilFaceFlagBits::eVkStencilFrontAndBack,
      stencil_reference);
}

bool RenderPassVK::OnRecordCommand(
    uint64_t base_vertex,
    size_t instance_count,
    const VertexBuffer& vertex_buffer,
    TextureAndSampler bound_textures[],
    size_t bound_texture_count,
    BufferAndUniformSlot bound_buffers[],
    size_t bound_buffer_count) {
  if (!pipeline_) {
    return false;
  }
  auto& encoder = *command_buffer_->GetEncoder();
  auto& context_vk = ContextVK::Cast(*GetContext());
  const auto& cmd_buffer = encoder.GetCommandBuffer();
  const auto& pipeline_vk = PipelineVK::Cast(*pipeline_);

  // All previous writes via a render or blit pass must be done before another
  // shader attempts to read the resource.
  BarrierVK barrier;
  barrier.cmd_buffer = cmd_buffer;
  barrier.src_access = vk::AccessFlagBits::eColorAttachmentWrite |
                       vk::AccessFlagBits::eTransferWrite;
  barrier.src_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                      vk::PipelineStageFlagBits::eTransfer;
  barrier.dst_access = vk::AccessFlagBits::eShaderRead;
  barrier.dst_stage = vk::PipelineStageFlagBits::eFragmentShader;

  barrier.new_layout = vk::ImageLayout::eShaderReadOnlyOptimal;

  for (auto i = 0u; i < bound_texture_count; i++) {
    if (!TextureVK::Cast(*bound_textures[i].texture.resource)
             .SetLayout(barrier)) {
      return false;
    }
  }

  AllocateAndBindDescriptorSets(
      context_vk, encoder, pipeline_vk, bound_textures, bound_texture_count,
      bound_buffers, bound_buffer_count, buffers_, images_, writes_);

  // Configure vertex and index and buffers for binding.
  auto& vertex_buffer_view = vertex_buffer.vertex_buffer;

  if (!vertex_buffer_view) {
    return false;
  }

  auto& allocator = *context_vk.GetResourceAllocator();
  auto vertex_device_buffer =
      vertex_buffer_view.buffer->GetDeviceBuffer(allocator);

  if (!vertex_buffer) {
    VALIDATION_LOG << "Failed to acquire device buffer"
                   << " for vertex buffer view";
    return false;
  }

  if (!encoder.Track(vertex_device_buffer)) {
    return false;
  }

  // Bind the vertex buffer.
  auto vertex_buffer_handle =
      DeviceBufferVK::Cast(*vertex_device_buffer).GetBuffer();
  vk::Buffer vertex_buffers[] = {vertex_buffer_handle};
  vk::DeviceSize vertex_buffer_offsets[] = {vertex_buffer_view.range.offset};
  cmd_buffer.bindVertexBuffers(0u, 1u, vertex_buffers, vertex_buffer_offsets);

  if (vertex_buffer.index_type != IndexType::kNone) {
    // Bind the index buffer.
    auto index_buffer_view = vertex_buffer.index_buffer;
    if (!index_buffer_view) {
      return false;
    }

    auto index_buffer = index_buffer_view.buffer->GetDeviceBuffer(allocator);
    if (!index_buffer) {
      VALIDATION_LOG << "Failed to acquire device buffer"
                     << " for index buffer view";
      return false;
    }

    if (!encoder.Track(index_buffer)) {
      return false;
    }

    auto index_buffer_handle = DeviceBufferVK::Cast(*index_buffer).GetBuffer();
    cmd_buffer.bindIndexBuffer(index_buffer_handle,
                               index_buffer_view.range.offset,
                               ToVKIndexType(vertex_buffer.index_type));

    // Engage!
    cmd_buffer.drawIndexed(vertex_buffer.vertex_count,  // index count
                           instance_count,              // instance count
                           0u,                          // first index
                           base_vertex,                 // vertex offset
                           0u                           // first instance
    );
  } else {
    cmd_buffer.draw(vertex_buffer.vertex_count,  // vertex count
                    instance_count,              // instance count
                    base_vertex,                 // vertex offset
                    0u                           // first instance
    );
  }

#ifdef IMPELLER_DEBUG
  if (has_label_) {
    encoder.PopDebugGroup();
  }
  has_label_ = false;
#endif  // IMPELLER_DEBUG

  return true;
}

bool RenderPassVK::OnEncodeCommands(const Context& context) const {
  command_buffer_->GetEncoder()->GetCommandBuffer().endRenderPass();
  return true;
}

}  // namespace impeller
