// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/lib/gpu/render_pass.h"

#include "flutter/lib/gpu/formats.h"
#include "flutter/lib/gpu/render_pipeline.h"
#include "fml/memory/ref_ptr.h"
#include "impeller/core/buffer_view.h"
#include "impeller/core/formats.h"
#include "impeller/core/sampler_descriptor.h"
#include "impeller/core/shader_types.h"
#include "impeller/core/vertex_buffer.h"
#include "impeller/geometry/color.h"
#include "impeller/renderer/pipeline_library.h"
#include "impeller/renderer/sampler_library.h"
#include "tonic/converter/dart_converter.h"

namespace flutter {
namespace gpu {

IMPLEMENT_WRAPPERTYPEINFO(flutter_gpu, RenderPass);

RenderPass::RenderPass()
    : vertex_buffer_(
          impeller::VertexBuffer{.index_type = impeller::IndexType::kNone}){};

RenderPass::~RenderPass() = default;

const std::shared_ptr<const impeller::Context>& RenderPass::GetContext() const {
  return render_pass_->GetContext();
}

impeller::Command& RenderPass::GetCommand() {
  return command_;
}

const impeller::Command& RenderPass::GetCommand() const {
  return command_;
}

impeller::RenderTarget& RenderPass::GetRenderTarget() {
  return render_target_;
}

const impeller::RenderTarget& RenderPass::GetRenderTarget() const {
  return render_target_;
}

impeller::ColorAttachmentDescriptor& RenderPass::GetColorAttachmentDescriptor(
    size_t color_attachment_index) {
  auto color = color_descriptors_.find(color_attachment_index);
  if (color == color_descriptors_.end()) {
    return color_descriptors_[color_attachment_index] = {};
  }
  return color->second;
}

impeller::DepthAttachmentDescriptor&
RenderPass::GetDepthAttachmentDescriptor() {
  return depth_desc_;
}

impeller::VertexBuffer& RenderPass::GetVertexBuffer() {
  return vertex_buffer_;
}

bool RenderPass::Begin(flutter::gpu::CommandBuffer& command_buffer) {
  render_pass_ =
      command_buffer.GetCommandBuffer()->CreateRenderPass(render_target_);
  if (!render_pass_) {
    return false;
  }
  command_buffer.AddRenderPass(render_pass_);
  return true;
}

void RenderPass::SetPipeline(fml::RefPtr<RenderPipeline> pipeline) {
  render_pipeline_ = std::move(pipeline);
}

std::shared_ptr<impeller::Pipeline<impeller::PipelineDescriptor>>
RenderPass::GetOrCreatePipeline() {
  // Infer the pipeline layout based on the shape of the RenderTarget.
  auto pipeline_desc = pipeline_descriptor_;
  for (const auto& it : render_target_.GetColorAttachments()) {
    auto& color = GetColorAttachmentDescriptor(it.first);
    color.format = render_target_.GetRenderTargetPixelFormat();
  }
  pipeline_desc.SetColorAttachmentDescriptors(color_descriptors_);

  {
    auto stencil = render_target_.GetStencilAttachment();
    if (stencil && impeller::IsStencilWritable(
                       stencil->texture->GetTextureDescriptor().format)) {
      pipeline_desc.SetStencilPixelFormat(
          stencil->texture->GetTextureDescriptor().format);
      pipeline_desc.SetStencilAttachmentDescriptors(stencil_front_desc_,
                                                    stencil_back_desc_);
    } else {
      pipeline_desc.ClearStencilAttachments();
    }
  }

  {
    auto depth = render_target_.GetDepthAttachment();
    if (depth && impeller::IsDepthWritable(
                     depth->texture->GetTextureDescriptor().format)) {
      pipeline_desc.SetDepthPixelFormat(
          depth->texture->GetTextureDescriptor().format);
      pipeline_desc.SetDepthStencilAttachmentDescriptor(depth_desc_);
    } else {
      pipeline_desc.ClearDepthAttachment();
    }
  }

  auto& context = *GetContext();

  render_pipeline_->BindToPipelineDescriptor(*context.GetShaderLibrary(),
                                             pipeline_desc);

  auto pipeline =
      context.GetPipelineLibrary()->GetPipeline(pipeline_desc).Get();
  FML_DCHECK(pipeline) << "Couldn't resolve render pipeline";
  return pipeline;
}

impeller::Command RenderPass::ProvisionRasterCommand() {
  impeller::Command result = command_;

  result.pipeline = GetOrCreatePipeline();
  result.BindVertices(vertex_buffer_);

  return result;
}

bool RenderPass::Draw() {
  impeller::Command result = ProvisionRasterCommand();
  return render_pass_->AddCommand(std::move(result));
}

}  // namespace gpu
}  // namespace flutter

static impeller::Color ToImpellerColor(uint32_t argb) {
  return impeller::Color::MakeRGBA8((argb >> 16) & 0xFF,  // R
                                    (argb >> 8) & 0xFF,   // G
                                    argb & 0xFF,          // B
                                    argb >> 24);          // A
}

//----------------------------------------------------------------------------
/// Exports
///

void InternalFlutterGpu_RenderPass_Initialize(Dart_Handle wrapper) {
  auto res = fml::MakeRefCounted<flutter::gpu::RenderPass>();
  res->AssociateWithDartWrapper(wrapper);
}

Dart_Handle InternalFlutterGpu_RenderPass_SetColorAttachment(
    flutter::gpu::RenderPass* wrapper,
    int color_attachment_index,
    int load_action,
    int store_action,
    int clear_color,
    flutter::gpu::Texture* texture,
    Dart_Handle resolve_texture_wrapper) {
  impeller::ColorAttachment desc;
  desc.load_action = flutter::gpu::ToImpellerLoadAction(load_action);
  desc.store_action = flutter::gpu::ToImpellerStoreAction(store_action);
  desc.clear_color = ToImpellerColor(static_cast<uint32_t>(clear_color));
  desc.texture = texture->GetTexture();
  if (!Dart_IsNull(resolve_texture_wrapper)) {
    flutter::gpu::Texture* resolve_texture =
        tonic::DartConverter<flutter::gpu::Texture*>::FromDart(
            resolve_texture_wrapper);
    desc.resolve_texture = resolve_texture->GetTexture();
  }
  wrapper->GetRenderTarget().SetColorAttachment(desc, color_attachment_index);
  return Dart_Null();
}

Dart_Handle InternalFlutterGpu_RenderPass_SetDepthStencilAttachment(
    flutter::gpu::RenderPass* wrapper,
    int depth_load_action,
    int depth_store_action,
    float depth_clear_value,
    int stencil_load_action,
    int stencil_store_action,
    int stencil_clear_value,
    flutter::gpu::Texture* texture) {
  {
    impeller::DepthAttachment desc;
    desc.load_action = flutter::gpu::ToImpellerLoadAction(depth_load_action);
    desc.store_action = flutter::gpu::ToImpellerStoreAction(depth_store_action);
    desc.clear_depth = depth_clear_value;
    desc.texture = texture->GetTexture();
    wrapper->GetRenderTarget().SetDepthAttachment(desc);
  }
  {
    impeller::StencilAttachment desc;
    desc.load_action = flutter::gpu::ToImpellerLoadAction(stencil_load_action);
    desc.store_action =
        flutter::gpu::ToImpellerStoreAction(stencil_store_action);
    desc.clear_stencil = stencil_clear_value;
    desc.texture = texture->GetTexture();
    wrapper->GetRenderTarget().SetStencilAttachment(desc);
  }

  return Dart_Null();
}

Dart_Handle InternalFlutterGpu_RenderPass_Begin(
    flutter::gpu::RenderPass* wrapper,
    flutter::gpu::CommandBuffer* command_buffer) {
  if (!wrapper->Begin(*command_buffer)) {
    return tonic::ToDart("Failed to begin RenderPass");
  }
  return Dart_Null();
}

void InternalFlutterGpu_RenderPass_BindPipeline(
    flutter::gpu::RenderPass* wrapper,
    flutter::gpu::RenderPipeline* pipeline) {
  auto ref = fml::RefPtr<flutter::gpu::RenderPipeline>(pipeline);
  wrapper->SetPipeline(std::move(ref));
}

template <typename TBuffer>
static void BindVertexBuffer(flutter::gpu::RenderPass* wrapper,
                             TBuffer* buffer,
                             int offset_in_bytes,
                             int length_in_bytes,
                             int vertex_count) {
  auto& vertex_buffer = wrapper->GetVertexBuffer();
  vertex_buffer.vertex_buffer = impeller::BufferView{
      .buffer = buffer->GetBuffer(),
      .range = impeller::Range(offset_in_bytes, length_in_bytes),
  };
  // If the index type is set, then the `vertex_count` becomes the index
  // count... So don't overwrite the count if it's already been set when binding
  // the index buffer.
  // TODO(bdero): Consider just doing a more traditional API with
  //              draw(vertexCount) and drawIndexed(indexCount). This is fine,
  //              but overall it would be a bit more explicit and we wouldn't
  //              have to document this behavior where the presence of the index
  //              buffer always takes precedent.
  if (vertex_buffer.index_type == impeller::IndexType::kNone) {
    vertex_buffer.vertex_count = vertex_count;
  }
}

void InternalFlutterGpu_RenderPass_BindVertexBufferDevice(
    flutter::gpu::RenderPass* wrapper,
    flutter::gpu::DeviceBuffer* device_buffer,
    int offset_in_bytes,
    int length_in_bytes,
    int vertex_count) {
  BindVertexBuffer(wrapper, device_buffer, offset_in_bytes, length_in_bytes,
                   vertex_count);
}

void InternalFlutterGpu_RenderPass_BindVertexBufferHost(
    flutter::gpu::RenderPass* wrapper,
    flutter::gpu::HostBuffer* host_buffer,
    int offset_in_bytes,
    int length_in_bytes,
    int vertex_count) {
  BindVertexBuffer(wrapper, host_buffer, offset_in_bytes, length_in_bytes,
                   vertex_count);
}

template <typename TBuffer>
static void BindIndexBuffer(flutter::gpu::RenderPass* wrapper,
                            TBuffer* buffer,
                            int offset_in_bytes,
                            int length_in_bytes,
                            int index_type,
                            int index_count) {
  auto& vertex_buffer = wrapper->GetVertexBuffer();
  vertex_buffer.index_buffer = impeller::BufferView{
      .buffer = buffer->GetBuffer(),
      .range = impeller::Range(offset_in_bytes, length_in_bytes),
  };
  vertex_buffer.index_type = flutter::gpu::ToImpellerIndexType(index_type);
  vertex_buffer.vertex_count = index_count;
}

void InternalFlutterGpu_RenderPass_BindIndexBufferDevice(
    flutter::gpu::RenderPass* wrapper,
    flutter::gpu::DeviceBuffer* device_buffer,
    int offset_in_bytes,
    int length_in_bytes,
    int index_type,
    int index_count) {
  BindIndexBuffer(wrapper, device_buffer, offset_in_bytes, length_in_bytes,
                  index_type, index_count);
}

void InternalFlutterGpu_RenderPass_BindIndexBufferHost(
    flutter::gpu::RenderPass* wrapper,
    flutter::gpu::HostBuffer* host_buffer,
    int offset_in_bytes,
    int length_in_bytes,
    int index_type,
    int index_count) {
  BindIndexBuffer(wrapper, host_buffer, offset_in_bytes, length_in_bytes,
                  index_type, index_count);
}

template <typename TBuffer>
static bool BindUniform(flutter::gpu::RenderPass* wrapper,
                        int stage,
                        int slot_id,
                        TBuffer* buffer,
                        int offset_in_bytes,
                        int length_in_bytes) {
  // TODO(113715): Populate this metadata once GLES is able to handle
  //               non-struct uniform names.
  std::shared_ptr<impeller::ShaderMetadata> metadata =
      std::make_shared<impeller::ShaderMetadata>();

  auto& command = wrapper->GetCommand();
  impeller::ShaderUniformSlot slot;
  // Don't populate the slot name... we don't have it here and Impeller doesn't
  // even use it for anything.
  slot.ext_res_0 = slot_id;
  return command.BindResource(
      flutter::gpu::ToImpellerShaderStage(stage), slot, metadata,
      impeller::BufferView{
          .buffer = buffer->GetBuffer(),
          .range = impeller::Range(offset_in_bytes, length_in_bytes),
      });
}

bool InternalFlutterGpu_RenderPass_BindUniformDevice(
    flutter::gpu::RenderPass* wrapper,
    int stage,
    int slot_id,
    flutter::gpu::DeviceBuffer* device_buffer,
    int offset_in_bytes,
    int length_in_bytes) {
  return BindUniform(wrapper, stage, slot_id, device_buffer, offset_in_bytes,
                     length_in_bytes);
}

bool InternalFlutterGpu_RenderPass_BindUniformHost(
    flutter::gpu::RenderPass* wrapper,
    int stage,
    int slot_id,
    flutter::gpu::HostBuffer* host_buffer,
    int offset_in_bytes,
    int length_in_bytes) {
  return BindUniform(wrapper, stage, slot_id, host_buffer, offset_in_bytes,
                     length_in_bytes);
}

bool InternalFlutterGpu_RenderPass_BindTexture(
    flutter::gpu::RenderPass* wrapper,
    int stage,
    int slot_id,
    flutter::gpu::Texture* texture,
    int min_filter,
    int mag_filter,
    int mip_filter,
    int width_address_mode,
    int height_address_mode) {
  auto& command = wrapper->GetCommand();

  // TODO(113715): Populate this metadata once GLES is able to handle
  //               non-struct uniform names.
  impeller::ShaderMetadata metadata;

  impeller::SamplerDescriptor sampler_desc;
  sampler_desc.min_filter = flutter::gpu::ToImpellerMinMagFilter(min_filter);
  sampler_desc.mag_filter = flutter::gpu::ToImpellerMinMagFilter(mag_filter);
  sampler_desc.mip_filter = flutter::gpu::ToImpellerMipFilter(mip_filter);
  sampler_desc.width_address_mode =
      flutter::gpu::ToImpellerSamplerAddressMode(width_address_mode);
  sampler_desc.height_address_mode =
      flutter::gpu::ToImpellerSamplerAddressMode(height_address_mode);
  auto sampler = wrapper->GetContext()->GetSamplerLibrary()->GetSampler(
      sampler_desc);

  impeller::SampledImageSlot image_slot;
  image_slot.texture_index = slot_id;
  return command.BindResource(flutter::gpu::ToImpellerShaderStage(stage),
                              image_slot, metadata, texture->GetTexture(),
                              sampler);
}

void InternalFlutterGpu_RenderPass_ClearBindings(
    flutter::gpu::RenderPass* wrapper) {
  auto& command = wrapper->GetCommand();
  command.vertex_buffer = {};
  command.vertex_bindings = {};
  command.fragment_bindings = {};
}

void InternalFlutterGpu_RenderPass_SetColorBlendEnable(
    flutter::gpu::RenderPass* wrapper,
    int color_attachment_index,
    bool enable) {
  auto& color = wrapper->GetColorAttachmentDescriptor(color_attachment_index);
  color.blending_enabled = enable;
}

void InternalFlutterGpu_RenderPass_SetColorBlendEquation(
    flutter::gpu::RenderPass* wrapper,
    int color_attachment_index,
    int color_blend_operation,
    int source_color_blend_factor,
    int destination_color_blend_factor,
    int alpha_blend_operation,
    int source_alpha_blend_factor,
    int destination_alpha_blend_factor) {
  auto& color = wrapper->GetColorAttachmentDescriptor(color_attachment_index);
  color.color_blend_op =
      flutter::gpu::ToImpellerBlendOperation(color_blend_operation);
  color.src_color_blend_factor =
      flutter::gpu::ToImpellerBlendFactor(source_color_blend_factor);
  color.dst_color_blend_factor =
      flutter::gpu::ToImpellerBlendFactor(destination_color_blend_factor);
  color.alpha_blend_op =
      flutter::gpu::ToImpellerBlendOperation(alpha_blend_operation);
  color.src_alpha_blend_factor =
      flutter::gpu::ToImpellerBlendFactor(source_alpha_blend_factor);
  color.dst_alpha_blend_factor =
      flutter::gpu::ToImpellerBlendFactor(destination_alpha_blend_factor);
}

void InternalFlutterGpu_RenderPass_SetDepthWriteEnable(
    flutter::gpu::RenderPass* wrapper,
    bool enable) {
  auto& depth = wrapper->GetDepthAttachmentDescriptor();
  depth.depth_write_enabled = true;
}

void InternalFlutterGpu_RenderPass_SetDepthCompareOperation(
    flutter::gpu::RenderPass* wrapper,
    int compare_operation) {
  auto& depth = wrapper->GetDepthAttachmentDescriptor();
  depth.depth_compare =
      flutter::gpu::ToImpellerCompareFunction(compare_operation);
}

bool InternalFlutterGpu_RenderPass_Draw(flutter::gpu::RenderPass* wrapper) {
  return wrapper->Draw();
}
