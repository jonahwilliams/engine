// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/impeller/entity/geometry/ellipse_geometry.h"

namespace impeller {

GeometryResult EllipseDataGetPositionBuffer(const EllipseData& data,
                                            const ContentContext& renderer,
                                            const Entity& entity,
                                            RenderPass& pass) {
  return Geometry::ComputePositionGeometry(
      renderer.GetTessellator()->FilledEllipse(entity.GetTransform(),
                                               data.rect),
      entity, pass);
}

GeometryResult EllipseDataGetPositionUVBuffer(const EllipseData& data,
                                              Rect texture_coverage,
                                              Matrix effect_transform,
                                              const ContentContext& renderer,
                                              const Entity& entity,
                                              RenderPass& pass) {
  return Geometry::ComputePositionUVGeometry(
      renderer.GetTessellator()->FilledEllipse(entity.GetTransform(),
                                               data.rect),
      texture_coverage.GetNormalizingTransform() * effect_transform, entity,
      pass);
}

GeometryVertexType EllipseDataGetVertexType(const EllipseData& data) {
  return GeometryVertexType::kPosition;
}

std::optional<Rect> EllipseDataGetCoverage(const EllipseData& data,
                                           const Matrix& transform) {
  return data.rect.TransformBounds(transform);
}

bool EllipseDataCoversArea(const EllipseData& data,
                           const Matrix& transform,
                           const Rect& rect) {
  return false;
}

bool EllipseDataIsAxisAlignedRect(const EllipseData& data) {
  return false;
}

}  // namespace impeller
