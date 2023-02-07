// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/entity/contents/tiled_texture_contents.h"
#include <iostream>

#include "impeller/entity/contents/clip_contents.h"
#include "impeller/entity/contents/content_context.h"
#include "impeller/entity/geometry.h"
#include "impeller/entity/tiled_texture_fill.frag.h"
#include "impeller/entity/tiled_texture_fill.vert.h"
#include "impeller/geometry/path_builder.h"
#include "impeller/renderer/render_pass.h"
#include "impeller/renderer/sampler_library.h"

namespace impeller {

TiledTextureContents::TiledTextureContents() = default;

TiledTextureContents::~TiledTextureContents() = default;

// static
SamplerAddressMode TiledTextureContents::ToSupportedAddressMode(Entity::TileMode mode) {
  switch (mode) {
    case Entity::TileMode::kClamp:
      return SamplerAddressMode::kClampToEdge;
    case Entity::TileMode::kRepeat:
      return SamplerAddressMode::kRepeat;
    case Entity::TileMode::kMirror:
      return SamplerAddressMode::kMirror;
    case Entity::TileMode::kDecal:
      return SamplerAddressMode::kMirror;
  }
}

// static
Entity::TileMode TiledTextureContents::FromAddressMode(SamplerAddressMode mode) {
  switch (mode) {
    case SamplerAddressMode::kClampToEdge:
      return Entity::TileMode::kClamp;
    case SamplerAddressMode::kRepeat:
      return Entity::TileMode::kMirror;
    case SamplerAddressMode::kMirror:
      return Entity::TileMode::kMirror;
  }
}

void TiledTextureContents::SetTexture(std::shared_ptr<Texture> texture) {
  texture_ = std::move(texture);
}

void TiledTextureContents::SetTileModes(Entity::TileMode x_tile_mode,
                                        Entity::TileMode y_tile_mode) {
  x_tile_mode_ = x_tile_mode;
  y_tile_mode_ = y_tile_mode;
}

void TiledTextureContents::SetSamplerDescriptor(SamplerDescriptor desc) {
  sampler_descriptor_ = std::move(desc);
}

void TiledTextureContents::SetDeferApplyingOpacity(bool value) {
  defer_applying_opacity_ = value;
}

std::optional<Snapshot> TiledTextureContents::RenderToSnapshot(
    const ContentContext& renderer,
    const Entity& entity) const {
  // Only natively supported tile modes can be passed through.
  auto rect = GetGeometry()->AsRect();
  if (x_tile_mode_ != Entity::TileMode::kDecal &&
      y_tile_mode_ != Entity::TileMode::kDecal && rect.has_value() &&
      (GetAlpha() >= 1 - kEhCloseEnough || defer_applying_opacity_)) {
    auto bounds = GetGeometry()->GetCoverage(entity.GetTransformation());
    SamplerDescriptor descriptor = sampler_descriptor_;
    descriptor.width_address_mode = ToSupportedAddressMode(x_tile_mode_);
    descriptor.height_address_mode = ToSupportedAddressMode(y_tile_mode_);
    return Snapshot{
        .texture = texture_,
        .transform = Matrix::MakeTranslation(bounds->origin),
        .sampler_descriptor = descriptor,
        .opacity = GetAlpha(),
        .dest_rect = bounds,
        .coverage_replacement = Rect::MakeSize(texture_->GetSize()),
    };
  }
  return Contents::RenderToSnapshot(renderer, entity);
}

bool TiledTextureContents::Render(const ContentContext& renderer,
                                  const Entity& entity,
                                  RenderPass& pass) const {
  if (texture_ == nullptr) {
    return true;
  }

  using VS = TiledTextureFillVertexShader;
  using FS = TiledTextureFillFragmentShader;

  const auto texture_size = texture_->GetSize();
  if (texture_size.IsEmpty()) {
    return true;
  }

  auto& host_buffer = pass.GetTransientsBuffer();

  auto geometry = GetGeometry();
  auto geometry_result =
      GetGeometry()->GetPositionBuffer(renderer, entity, pass);

  // TODO(bdero): The geometry should be fetched from GetPositionUVBuffer and
  //              contain coverage-mapped UVs, and this should use
  //              position_uv.vert.
  //              https://github.com/flutter/flutter/issues/118553

  VS::VertInfo vert_info;
  vert_info.mvp = geometry_result.transform;
  vert_info.effect_transform = GetInverseMatrix();
  vert_info.bounds_origin = geometry->GetCoverage(Matrix())->origin;
  vert_info.texture_size = Vector2(static_cast<Scalar>(texture_size.width),
                                   static_cast<Scalar>(texture_size.height));

  FS::FragInfo frag_info;
  frag_info.texture_sampler_y_coord_scale = texture_->GetYCoordScale();
  frag_info.x_tile_mode = static_cast<Scalar>(x_tile_mode_);
  frag_info.y_tile_mode = static_cast<Scalar>(y_tile_mode_);
  frag_info.alpha = GetAlpha();

  Command cmd;
  cmd.label = "TiledTextureFill";
  cmd.stencil_reference = entity.GetStencilDepth();

  auto options = OptionsFromPassAndEntity(pass, entity);
  if (geometry_result.prevent_overdraw) {
    options.stencil_compare = CompareFunction::kEqual;
    options.stencil_operation = StencilOperation::kIncrementClamp;
  }
  options.primitive_type = geometry_result.type;
  cmd.pipeline = renderer.GetTiledTexturePipeline(options);

  cmd.BindVertices(geometry_result.vertex_buffer);
  VS::BindVertInfo(cmd, host_buffer.EmplaceUniform(vert_info));
  FS::BindFragInfo(cmd, host_buffer.EmplaceUniform(frag_info));
  FS::BindTextureSampler(cmd, texture_,
                         renderer.GetContext()->GetSamplerLibrary()->GetSampler(
                             sampler_descriptor_));

  if (!pass.AddCommand(std::move(cmd))) {
    return false;
  }

  if (geometry_result.prevent_overdraw) {
    auto restore = ClipRestoreContents();
    restore.SetRestoreCoverage(GetCoverage(entity));
    return restore.Render(renderer, entity, pass);
  }
  return true;
}

}  // namespace impeller
