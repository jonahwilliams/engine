// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <utility>

#include "impeller/core/formats.h"
#include "impeller/entity/contents/atlas_contents.h"
#include "impeller/entity/contents/content_context.h"
#include "impeller/entity/contents/filters/blend_filter_contents.h"
#include "impeller/entity/contents/texture_contents.h"
#include "impeller/entity/entity.h"
#include "impeller/entity/texture_fill.vert.h"
#include "impeller/geometry/color.h"
#include "impeller/renderer/render_pass.h"
#include "impeller/renderer/vertex_buffer_builder.h"

namespace impeller {

AtlasContents::AtlasContents() = default;

AtlasContents::~AtlasContents() = default;

void AtlasContents::SetTexture(std::shared_ptr<Texture> texture) {
  texture_ = std::move(texture);
}

std::shared_ptr<Texture> AtlasContents::GetTexture() const {
  return texture_;
}

void AtlasContents::SetTransforms(std::vector<Matrix> transforms) {
  transforms_ = std::move(transforms);
  bounding_box_cache_.reset();
}

void AtlasContents::SetTextureCoordinates(std::vector<Rect> texture_coords) {
  texture_coords_ = std::move(texture_coords);
  bounding_box_cache_.reset();
}

void AtlasContents::SetColors(std::vector<Color> colors) {
  colors_ = std::move(colors);
}

void AtlasContents::SetAlpha(Scalar alpha) {
  alpha_ = alpha;
}

void AtlasContents::SetBlendMode(BlendMode blend_mode) {
  blend_mode_ = blend_mode;
}

void AtlasContents::SetCullRect(std::optional<Rect> cull_rect) {
  cull_rect_ = cull_rect;
}

std::optional<Rect> AtlasContents::GetCoverage(const Entity& entity) const {
  if (cull_rect_.has_value()) {
    return cull_rect_.value().TransformBounds(entity.GetTransform());
  }
  return ComputeBoundingBox().TransformBounds(entity.GetTransform());
}

Rect AtlasContents::ComputeBoundingBox() const {
  if (!bounding_box_cache_.has_value()) {
    Rect bounding_box = {};
    for (size_t i = 0; i < texture_coords_.size(); i++) {
      auto matrix = transforms_[i];
      auto sample_rect = texture_coords_[i];
      auto bounds =
          Rect::MakeSize(sample_rect.GetSize()).TransformBounds(matrix);
      bounding_box = bounds.Union(bounding_box);
    }
    bounding_box_cache_ = bounding_box;
  }
  return bounding_box_cache_.value();
}

void AtlasContents::SetSamplerDescriptor(SamplerDescriptor desc) {
  sampler_descriptor_ = std::move(desc);
}

const SamplerDescriptor& AtlasContents::GetSamplerDescriptor() const {
  return sampler_descriptor_;
}

const std::vector<Matrix>& AtlasContents::GetTransforms() const {
  return transforms_;
}

const std::vector<Rect>& AtlasContents::GetTextureCoordinates() const {
  return texture_coords_;
}

const std::vector<Color>& AtlasContents::GetColors() const {
  return colors_;
}

bool AtlasContents::Render(const ContentContext& renderer,
                           const Entity& entity,
                           RenderPass& pass) const {
  if (texture_ == nullptr || blend_mode_ == BlendMode::kClear ||
      alpha_ <= 0.0) {
    return true;
  }

  constexpr size_t indices[6] = {0, 1, 2, 1, 2, 3};

  if (colors_.empty() || blend_mode_ == BlendMode::kSource) {
    using VS = TexturePipeline::VertexShader;
    using FS = TexturePipeline::FragmentShader;

    VertexBufferBuilder<VS::PerVertexData> vtx_builder;
    vtx_builder.Reserve(texture_coords_.size() * 6);
    const auto texture_size = texture_->GetSize();
    auto& host_buffer = renderer.GetTransientsBuffer();

    for (size_t i = 0; i < texture_coords_.size(); i++) {
      auto sample_rect = texture_coords_[i];
      auto matrix = transforms_[i];
      auto points = sample_rect.GetPoints();
      auto transformed_points =
          Rect::MakeSize(sample_rect.GetSize()).GetTransformedPoints(matrix);
      for (size_t j = 0; j < 6; j++) {
        VS::PerVertexData data;
        data.position = transformed_points[indices[j]];
        data.texture_coords = points[indices[j]] / texture_size;
        vtx_builder.AppendVertex(data);
      }
    }

    pass.SetCommandLabel("DrawAtlas");
    pass.SetVertexBuffer(vtx_builder.CreateVertexBuffer(host_buffer));
    pass.SetPipeline(renderer.GetTexturePipeline(OptionsFromPass(pass)));

    VS::FrameInfo frame_info;
    frame_info.texture_sampler_y_coord_scale = texture_->GetYCoordScale();
    frame_info.mvp = entity.GetShaderTransform(pass);
    frame_info.alpha = alpha_;

    VS::BindFrameInfo(pass, host_buffer.EmplaceUniform(frame_info));

    auto dst_sampler_descriptor = sampler_descriptor_;
    if (renderer.GetDeviceCapabilities().SupportsDecalSamplerAddressMode()) {
      dst_sampler_descriptor.width_address_mode = SamplerAddressMode::kDecal;
      dst_sampler_descriptor.height_address_mode = SamplerAddressMode::kDecal;
    }
    const std::unique_ptr<const Sampler>& dst_sampler =
        renderer.GetContext()->GetSamplerLibrary()->GetSampler(
            dst_sampler_descriptor);

    FS::BindTextureSampler(pass, texture_, dst_sampler);
    return pass.Draw().ok();
  }

  using VS = PorterDuffBlendPipeline::VertexShader;
  using FS = PorterDuffBlendPipeline::FragmentShader;
  using FSAdvanced = VerticesUberShader::FragmentShader;

  VertexBufferBuilder<VS::PerVertexData> vtx_builder;
  vtx_builder.Reserve(texture_coords_.size() * 6);
  const auto texture_size = texture_->GetSize();
  auto& host_buffer = renderer.GetTransientsBuffer();

  for (size_t i = 0; i < texture_coords_.size(); i++) {
    auto sample_rect = texture_coords_[i];
    auto matrix = transforms_[i];
    auto points = sample_rect.GetPoints();
    auto transformed_points =
        Rect::MakeSize(sample_rect.GetSize()).GetTransformedPoints(matrix);
    auto color = colors_[i].Premultiply();
    for (size_t j = 0; j < 6; j++) {
      VS::PerVertexData data;
      data.vertices = transformed_points[indices[j]];
      data.texture_coords = points[indices[j]] / texture_size;
      data.color = color;
      vtx_builder.AppendVertex(data);
    }
  }

#ifdef IMPELLER_DEBUG
  pass.SetCommandLabel(
      SPrintF("DrawAtlas Blend (%s)", BlendModeToString(blend_mode_)));
#endif  // IMPELLER_DEBUG
  pass.SetVertexBuffer(vtx_builder.CreateVertexBuffer(host_buffer));
  if (blend_mode_ <= BlendMode::kModulate) {
    pass.SetPipeline(
        renderer.GetPorterDuffBlendPipeline(OptionsFromPass(pass)));
  } else {
    pass.SetPipeline(renderer.GetVerticesUberShader(OptionsFromPass(pass)));
  }

  VS::FrameInfo frame_info;
  frame_info.texture_sampler_y_coord_scale = texture_->GetYCoordScale();
  frame_info.mvp = entity.GetShaderTransform(pass);

  VS::BindFrameInfo(pass, host_buffer.EmplaceUniform(frame_info));

  auto dst_sampler_descriptor = sampler_descriptor_;
  if (renderer.GetDeviceCapabilities().SupportsDecalSamplerAddressMode()) {
    dst_sampler_descriptor.width_address_mode = SamplerAddressMode::kDecal;
    dst_sampler_descriptor.height_address_mode = SamplerAddressMode::kDecal;
  }
  const std::unique_ptr<const Sampler>& dst_sampler =
      renderer.GetContext()->GetSamplerLibrary()->GetSampler(
          dst_sampler_descriptor);

  if (blend_mode_ <= BlendMode::kModulate) {
    FS::FragInfo frag_info;
    frag_info.output_alpha = alpha_;
    frag_info.input_alpha = 1.0;

    auto inverted_blend_mode =
        InvertPorterDuffBlend(blend_mode_).value_or(BlendMode::kSource);
    auto blend_coefficients =
        kPorterDuffCoefficients[static_cast<int>(inverted_blend_mode)];
    frag_info.src_coeff = blend_coefficients[0];
    frag_info.src_coeff_dst_alpha = blend_coefficients[1];
    frag_info.dst_coeff = blend_coefficients[2];
    frag_info.dst_coeff_src_alpha = blend_coefficients[3];
    frag_info.dst_coeff_src_color = blend_coefficients[4];

    FS::BindFragInfo(pass, host_buffer.EmplaceUniform(frag_info));
    FS::BindTextureSamplerDst(pass, texture_, dst_sampler);
  } else {
    FSAdvanced::FragInfo frag_info;
    frag_info.alpha = alpha_;
    frag_info.blend_mode = static_cast<int>(blend_mode_);

    FSAdvanced::BindFragInfo(pass, host_buffer.EmplaceUniform(frag_info));
    FSAdvanced::BindTextureSampler(pass, texture_, dst_sampler);
  }

  return pass.Draw().ok();
}

}  // namespace impeller
