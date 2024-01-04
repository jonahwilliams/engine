// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/renderer/render_pass.h"
#include "impeller/core/host_buffer.h"
#include "impeller/renderer/command.h"

namespace impeller {

RenderPass::RenderPass(std::shared_ptr<const Context> context,
                       const RenderTarget& target)
    : context_(std::move(context)),
      sample_count_(target.GetSampleCount()),
      pixel_format_(target.GetRenderTargetPixelFormat()),
      has_stencil_attachment_(target.GetStencilAttachment().has_value()),
      render_target_size_(target.GetRenderTargetSize()),
      render_target_(target),
      transients_buffer_() {
  transients_buffer_ = HostBuffer::Create(context_->GetResourceAllocator());
}

RenderPass::~RenderPass() {
  // auto strong_context = context_.lock();
  // if (strong_context) {
  //   strong_context->GetHostBufferPool().Recycle(transients_buffer_);
  // }
}

SampleCount RenderPass::GetSampleCount() const {
  return sample_count_;
}

PixelFormat RenderPass::GetRenderTargetPixelFormat() const {
  return pixel_format_;
}

bool RenderPass::HasStencilAttachment() const {
  return has_stencil_attachment_;
}

const RenderTarget& RenderPass::GetRenderTarget() const {
  return render_target_;
}

ISize RenderPass::GetRenderTargetSize() const {
  return render_target_size_;
}

HostBuffer& RenderPass::GetTransientsBuffer() {
  return *transients_buffer_;
}

void RenderPass::SetLabel(std::string label) {
  if (label.empty()) {
    return;
  }
  transients_buffer_->SetLabel(SPrintF("%s Transients", label.c_str()));
  OnSetLabel(std::move(label));
}

bool RenderPass::AddCommand(Command&& command) {
  if (!command.IsValid()) {
    VALIDATION_LOG << "Attempted to add an invalid command to the render pass.";
    return false;
  }

  if (command.scissor.has_value()) {
    auto target_rect = IRect::MakeSize(render_target_.GetRenderTargetSize());
    if (!target_rect.Contains(command.scissor.value())) {
      VALIDATION_LOG << "Cannot apply a scissor that lies outside the bounds "
                        "of the render target.";
      return false;
    }
  }

  if (command.vertex_buffer.vertex_count == 0u ||
      command.instance_count == 0u) {
    // Essentially a no-op. Don't record the command but this is not necessary
    // an error either.
    return true;
  }

  pending_ = std::move(command);
  for (auto bound_texture : pending_.fragment_bindings.sampled_images) {
    bound_textures_[bound_texture_count_++] = std::move(bound_texture);
  }
  for (auto bound_texture : pending_.vertex_bindings.sampled_images) {
    bound_textures_[bound_texture_count_++] = std::move(bound_texture);
  }
  for (auto bound_buffer : pending_.fragment_bindings.buffers) {
    bound_buffers_[bound_buffer_count_++] = std::move(bound_buffer);
  }
  for (auto bound_buffer : pending_.vertex_bindings.buffers) {
    bound_buffers_[bound_buffer_count_++] = std::move(bound_buffer);
  }
  SetPipeline(pending_.pipeline);
  SetStencilReference(pending_.stencil_reference);
  return Dispatch();
}

bool RenderPass::EncodeCommands() const {
  return OnEncodeCommands(*GetContext());
}

const std::shared_ptr<const Context>& RenderPass::GetContext() const {
  return context_;
}

void RenderPass::SetPipeline(
    const std::shared_ptr<Pipeline<PipelineDescriptor>>& pipeline) {
  pending_.pipeline = pipeline;
}

void RenderPass::SetCommandLabel(const std::string& label) {
#ifdef IMPELLER_DEBUG
  pending_.label = label;
#endif  // IMPELLER_DEBUG
}

void RenderPass::SetStencilReference(uint32_t value) {
  pending_.stencil_reference = value;
}

void RenderPass::SetBaseVertex(uint64_t value) {
  pending_.base_vertex = value;
}

void RenderPass::SetViewport(Viewport viewport) {
  pending_.viewport = viewport;
}

void RenderPass::SetScissor(IRect scissor) {
  pending_.scissor = scissor;
}

void RenderPass::SetInstanceCount(size_t count) {
  pending_.instance_count = count;
}

bool RenderPass::SetVertexBuffer(VertexBuffer buffer) {
  return pending_.BindVertices(std::move(buffer));
}

bool RenderPass::Dispatch() {
  auto result = OnRecordCommand(
      pending_.base_vertex,
      pending_.instance_count, pending_.vertex_buffer, bound_textures_,
      bound_texture_count_, bound_buffers_, bound_buffer_count_);
  pending_ = Command{};
  bound_buffer_count_ = 0;
  bound_texture_count_ = 0;
  return result;
}

bool RenderPass::OnRecordCommand(
    uint64_t base_vertex,
    size_t instance_count,
    const VertexBuffer& vertex_buffer,
    TextureAndSampler bound_textures[],
    size_t bound_texture_count,
    BufferAndUniformSlot bound_buffers[],
    size_t bound_buffer_count) {
  return true;
}

// |ResourceBinder|
bool RenderPass::BindResource(ShaderStage stage,
                              const ShaderUniformSlot& slot,
                              const ShaderMetadata& metadata,
                              BufferView view) {
  bound_buffers_[bound_buffer_count_++] =
      BufferAndUniformSlot{.stage = stage,
                           .slot = slot,
                           .view = BufferResource(&metadata, std::move(view))};
  return true;
}

bool RenderPass::BindResource(ShaderStage stage,
                              const ShaderUniformSlot& slot,
                              std::shared_ptr<const ShaderMetadata>& metadata,
                              BufferView view) {
  bound_buffers_[bound_buffer_count_++] =
      std::move(BufferAndUniformSlot{.stage = stage,
                           .slot = slot,
                           .view = BufferResource(metadata, std::move(view))});
  return true;
}

// |ResourceBinder|
bool RenderPass::BindResource(ShaderStage stage,
                              const SampledImageSlot& slot,
                              const ShaderMetadata& metadata,
                              std::shared_ptr<const Texture> texture,
                              std::shared_ptr<const Sampler> sampler) {
  bound_textures_[bound_texture_count_++] = TextureAndSampler{
      .stage = stage,
      .slot = slot,
      .texture = {&metadata, std::move(texture)},
      .sampler = std::move(sampler),
  };
  return true;
}

}  // namespace impeller
