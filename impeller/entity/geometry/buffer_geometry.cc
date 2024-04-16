// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/entity/geometry/buffer_geometry.h"
#include "impeller/core/formats.h"
#include "impeller/core/vertex_buffer.h"
#include "impeller/entity/geometry/geometry.h"

namespace impeller {

BufferGeometry::BufferGeometry(VertexBuffer vertex_buffer,
                               Rect coverage,
                               GeometryResult::Mode mode)
    : vertex_buffer_(std::move(vertex_buffer)),
      coverage_(coverage),
      mode_(mode) {}

bool BufferGeometry::CoversArea(const Matrix& transform,
                                const Rect& rect) const {
  return false;
}

GeometryResult BufferGeometry::GetPositionBuffer(const ContentContext& renderer,
                                                 const Entity& entity,
                                                 RenderPass& pass) const {
  return GeometryResult{
      .type = PrimitiveType::kTriangleStrip,
      .vertex_buffer = vertex_buffer_,
      .transform = entity.GetShaderTransform(pass),
      .mode = mode_,
  };
}

// |Geometry|
GeometryVertexType BufferGeometry::GetVertexType() const {
  return GeometryVertexType::kPosition;
}

// |Geometry|
std::optional<Rect> BufferGeometry::GetCoverage(const Matrix& transform) const {
  return coverage_.TransformBounds(transform);
}

// |Geometry|
GeometryResult BufferGeometry::GetPositionUVBuffer(
    Rect texture_coverage,
    Matrix effect_transform,
    const ContentContext& renderer,
    const Entity& entity,
    RenderPass& pass) const {
  FML_CHECK(false);
  return {};
}

// |Geometry|
GeometryResult::Mode BufferGeometry::GetResultMode() const {
  return mode_;
}

}  // namespace impeller
