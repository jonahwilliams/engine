// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/renderer/backend/vulkan/texture_vk.h"

#include "impeller/core/texture_descriptor.h"
#include "impeller/renderer/backend/vulkan/command_buffer_vk.h"
#include "impeller/renderer/backend/vulkan/command_encoder_vk.h"
#include "impeller/renderer/backend/vulkan/formats_vk.h"

namespace impeller {

TextureVK::TextureVK(std::weak_ptr<Context> context,
                     std::shared_ptr<TextureSourceVK> source)
    : Texture(source->GetTextureDescriptor()),
      context_(std::move(context)),
      source_(std::move(source)) {}

TextureVK::~TextureVK() = default;

void TextureVK::SetLabel(std::string_view label) {
  auto context = context_.lock();
  if (!context) {
    // The context may have died.
    return;
  }
  ContextVK::Cast(*context).SetDebugName(GetImage(), label);
  ContextVK::Cast(*context).SetDebugName(GetImageView(), label);
}

bool TextureVK::OnSetContents(const uint8_t* contents,
                              size_t length,
                              size_t slice) {
  if (!IsValid() || !contents) {
    return false;
  }

  const auto& desc = GetTextureDescriptor();

  // Out of bounds access.
  if (length != desc.GetByteSizeOfBaseMipLevel()) {
    VALIDATION_LOG << "Illegal to set contents for invalid size.";
    return false;
  }

  auto context = context_.lock();
  if (!context) {
    VALIDATION_LOG << "Context died before setting contents on texture.";
    return false;
  }

  auto staging_buffer =
      context->GetResourceAllocator()->CreateBufferWithCopy(contents, length);

  if (!staging_buffer) {
    VALIDATION_LOG << "Could not create staging buffer.";
    return false;
  }

  auto cmd_buffer = context->CreateCommandBuffer();

  if (!cmd_buffer) {
    return false;
  }

  const auto encoder = CommandBufferVK::Cast(*cmd_buffer).GetEncoder();

  if (!encoder->Track(staging_buffer) || !encoder->Track(source_)) {
    return false;
  }

  const auto& vk_cmd_buffer = encoder->GetCommandBuffer();

  BarrierVK barrier;
  barrier.cmd_buffer = vk_cmd_buffer;
  barrier.new_layout = vk::ImageLayout::eTransferDstOptimal;
  barrier.src_access = {};
  barrier.src_stage = vk::PipelineStageFlagBits::eTopOfPipe;
  barrier.dst_access = vk::AccessFlagBits::eTransferWrite;
  barrier.dst_stage = vk::PipelineStageFlagBits::eTransfer;

  // The contract of OnSetContents replaces the entire texture, so it is safe
  // to specify the previous layout as undefined to allow the driver to
  // discard the contents. If setContents is updated to do partial replacements
  // then the old layout will need to be correctly specified.
  SetTextureLayout(*this, barrier);

  vk::BufferImageCopy copy;
  copy.bufferOffset = 0u;
  copy.bufferRowLength = 0u;    // 0u means tightly packed per spec.
  copy.bufferImageHeight = 0u;  // 0u means tightly packed per spec.
  copy.imageOffset.x = 0u;
  copy.imageOffset.y = 0u;
  copy.imageOffset.z = 0u;
  copy.imageExtent.width = desc.size.width;
  copy.imageExtent.height = desc.size.height;
  copy.imageExtent.depth = 1u;
  copy.imageSubresource.aspectMask =
      ToImageAspectFlags(GetTextureDescriptor().format);
  copy.imageSubresource.mipLevel = 0u;
  copy.imageSubresource.baseArrayLayer = slice;
  copy.imageSubresource.layerCount = 1u;

  vk_cmd_buffer.copyBufferToImage(
      DeviceBufferVK::Cast(*staging_buffer).GetBuffer(),  // src buffer
      GetImage(),                                         // dst image
      barrier.new_layout,                                 // dst image layout
      1u,                                                 // region count
      &copy                                               // regions
  );

  // Transition to shader-read.
  {
    BarrierVK barrier;
    barrier.cmd_buffer = vk_cmd_buffer;
    barrier.src_access = vk::AccessFlagBits::eColorAttachmentWrite |
                         vk::AccessFlagBits::eTransferWrite;
    barrier.src_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                        vk::PipelineStageFlagBits::eTransfer;
    barrier.dst_access = vk::AccessFlagBits::eShaderRead;
    barrier.dst_stage = vk::PipelineStageFlagBits::eFragmentShader;

    barrier.new_layout = vk::ImageLayout::eShaderReadOnlyOptimal;

    SetTextureLayout(*this, barrier, vk::ImageLayout::eTransferDstOptimal);
  }

  return context->GetCommandQueue()->Submit({cmd_buffer}).ok();
}

bool TextureVK::OnSetContents(std::shared_ptr<const fml::Mapping> mapping,
                              size_t slice) {
  // Vulkan has no threading restrictions. So we can pass this data along to the
  // client rendering API immediately.
  return OnSetContents(mapping->GetMapping(), mapping->GetSize(), slice);
}

bool TextureVK::IsValid() const {
  return !!source_;
}

ISize TextureVK::GetSize() const {
  return GetTextureDescriptor().size;
}

vk::Image TextureVK::GetImage() const {
  return source_->GetImage();
}

vk::ImageView TextureVK::GetImageView() const {
  return source_->GetImageView();
}

std::shared_ptr<const TextureSourceVK> TextureVK::GetTextureSource() const {
  return source_;
}

vk::ImageView TextureVK::GetRenderTargetView() const {
  return source_->GetRenderTargetView();
}

void TextureVK::SetFramebuffer(
    const SharedHandleVK<vk::Framebuffer>& framebuffer) {
  framebuffer_ = framebuffer;
}

void TextureVK::SetRenderPass(
    const SharedHandleVK<vk::RenderPass>& render_pass) {
  render_pass_ = render_pass;
}

SharedHandleVK<vk::Framebuffer> TextureVK::GetFramebuffer() const {
  return framebuffer_;
}

SharedHandleVK<vk::RenderPass> TextureVK::GetRenderPass() const {
  return render_pass_;
}

void SetTextureLayout(const TextureVK& texture,
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

}  // namespace impeller
