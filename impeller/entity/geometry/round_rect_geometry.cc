// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/impeller/entity/geometry/round_rect_geometry.h"

namespace impeller {

GeometryResult RoundRectDataGetPositionBuffer(const RoundRectData& data,
                                              const ContentContext& renderer,
                                              const Entity& entity,
                                              RenderPass& pass) {
  return Geometry::ComputePositionGeometry(
      renderer.GetTessellator()->FilledRoundRect(entity.GetTransform(),
                                                 data.rect, data.size),
      entity, pass);
}

GeometryResult RoundRectDataGetPositionUVBuffer(const RoundRectData& data,
                                                Rect texture_coverage,
                                                Matrix effect_transform,
                                                const ContentContext& renderer,
                                                const Entity& entity,
                                                RenderPass& pass) {
  return Geometry::ComputePositionUVGeometry(
      renderer.GetTessellator()->FilledRoundRect(entity.GetTransform(),
                                                 data.rect, data.size),
      texture_coverage.GetNormalizingTransform() * effect_transform, entity,
      pass);
}

GeometryVertexType RoundRectDataGetVertexType(const RoundRectData& data) {
  return GeometryVertexType::kPosition;
}

std::optional<Rect> RoundRectDataGetCoverage(const RoundRectData& data,
                                             const Matrix& transform) {
  return data.rect.TransformBounds(transform);
}

bool RoundRectDataCoversArea(const RoundRectData& data,
                             const Matrix& transform,
                             const Rect& rect) {
  if (!transform.IsTranslationScaleOnly()) {
    return false;
  }
  bool flat_on_tb = data.rect.GetSize().width > data.size.width * 2;
  bool flat_on_lr = data.rect.GetSize().height > data.size.height * 2;
  if (!flat_on_tb && !flat_on_lr) {
    return false;
  }
  // We either transform the bounds and delta-transform the radii,
  // or we compute the vertical and horizontal bounds and then
  // transform each. Either way there are 2 transform operations.
  // We could also get a weaker answer by computing just the
  // "inner rect" and only doing a coverage analysis on that,
  // but this process will produce more culling results.
  if (flat_on_tb) {
    Rect vertical_bounds = data.rect.Expand(Size{-data.size.width, 0});
    Rect coverage = vertical_bounds.TransformBounds(transform);
    if (coverage.Contains(rect)) {
      return true;
    }
  }
  if (flat_on_lr) {
    Rect horizontal_bounds = data.rect.Expand(Size{0, -data.size.height});
    Rect coverage = horizontal_bounds.TransformBounds(transform);
    if (coverage.Contains(rect)) {
      return true;
    }
  }
  return false;
}

bool RoundRectDataIsAxisAlignedRect(const RoundRectData& data) {
  return false;
}

}  // namespace impeller
