// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/impeller/entity/geometry/circle_geometry.h"

#include "impeller/entity/geometry/geometry.h"

namespace impeller {

GeometryResult CircleDataGetPositionBuffer(const CircleData& data,
                                           const ContentContext& renderer,
                                           const Entity& entity,
                                           RenderPass& pass) {
  auto& transform = entity.GetTransform();

  Scalar half_width =
      data.stroke_width < 0
          ? 0.0
          : Geometry::ComputePixelHalfWidth(transform, data.stroke_width);

  std::shared_ptr<Tessellator> tessellator = renderer.GetTessellator();

  // We call the StrokedCircle method which will simplify to a
  // FilledCircleGenerator if the inner_radius is <= 0.
  auto generator = tessellator->StrokedCircle(transform, data.center,
                                              data.radius, half_width);

  return Geometry::ComputePositionGeometry(generator, entity, pass);
}

// |Geometry|
GeometryResult CircleDataGetPositionUVBuffer(const CircleData& data,
                                             Rect texture_coverage,
                                             Matrix effect_transform,
                                             const ContentContext& renderer,
                                             const Entity& entity,
                                             RenderPass& pass) {
  auto& transform = entity.GetTransform();
  auto uv_transform =
      texture_coverage.GetNormalizingTransform() * effect_transform;

  Scalar half_width = data.stroke_width < 0 ? 0.0
                                        : Geometry::ComputePixelHalfWidth(
                                              transform, data.stroke_width);
  std::shared_ptr<Tessellator> tessellator = renderer.GetTessellator();

  // We call the StrokedCircle method which will simplify to a
  // FilledCircleGenerator if the inner_radius is <= 0.
  auto generator =
      tessellator->StrokedCircle(transform, data.center, data.radius, half_width);

  return Geometry::ComputePositionUVGeometry(generator, uv_transform, entity, pass);
}

GeometryVertexType CircleDataGetVertexType(const CircleData& data) {
  return GeometryVertexType::kPosition;
}

std::optional<Rect> CircleDataGetCoverage(const CircleData& data, const Matrix& transform) {
  Point corners[4]{
      {data.center.x, data.center.y - data.radius},
      {data.center.x + data.radius, data.center.y},
      {data.center.x, data.center.y + data.radius},
      {data.center.x - data.radius, data.center.y},
  };

  for (int i = 0; i < 4; i++) {
    corners[i] = transform * corners[i];
  }
  return Rect::MakePointBounds(std::begin(corners), std::end(corners));
}

bool CircleDataCoversArea(const CircleData& data, const Matrix& transform, const Rect& rect) {
  return false;
}

bool CircleDataIsAxisAlignedRect(const CircleData& data) {
  return false;
}

}  // namespace impeller
