// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/entity/geometry/rect_geometry.h"

namespace impeller {

GeometryResult RectDataGetPositionBuffer(const RectData& data,
                                         const ContentContext& renderer,
                                         const Entity& entity,
                                         RenderPass& pass) {
  auto& host_buffer = pass.GetTransientsBuffer();
  return GeometryResult{
      .type = PrimitiveType::kTriangleStrip,
      .vertex_buffer =
          {
              .vertex_buffer =
                  host_buffer.Emplace(data.rect.GetPoints().data(),
                                      8 * sizeof(float), alignof(float)),
              .vertex_count = 4,
              .index_type = IndexType::kNone,
          },
      .transform = Matrix::MakeOrthographic(pass.GetRenderTargetSize()) *
                   entity.GetTransform(),
      .prevent_overdraw = false,
  };
}

GeometryResult RectDataGetPositionUVBuffer(const RectData& data,
                                           Rect texture_coverage,
                                           Matrix effect_transform,
                                           const ContentContext& renderer,
                                           const Entity& entity,
                                           RenderPass& pass) {
  return ComputeUVGeometryForRect(data.rect, texture_coverage, effect_transform,
                                  renderer, entity, pass);
}

GeometryVertexType RectDataGetVertexType(const RectData& data) {
  return GeometryVertexType::kPosition;
}

std::optional<Rect> RectDataGetCoverage(const RectData& data,
                                        const Matrix& transform) {
  return data.rect.TransformBounds(transform);
}

bool RectDataCoversArea(const RectData& data,
                        const Matrix& transform,
                        const Rect& rect) {
  if (!transform.IsTranslationScaleOnly()) {
    return false;
  }
  Rect coverage = data.rect.TransformBounds(transform);
  return coverage.Contains(rect);
}

bool RectDataIsAxisAlignedRect(const RectData& data) {
  return true;
}

}  // namespace impeller
