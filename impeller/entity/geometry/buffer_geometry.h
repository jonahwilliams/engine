// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_ENTITY_GEOMETRY_BUFFER_GEOMETRY_H_
#define FLUTTER_IMPELLER_ENTITY_GEOMETRY_BUFFER_GEOMETRY_H_

#include <optional>

#include "impeller/core/vertex_buffer.h"
#include "impeller/entity/geometry/geometry.h"
#include "impeller/geometry/rect.h"

namespace impeller {

/// @brief A geometry that is created directly from vertex data.
///
/// This class is meant to be a temporary adaptor while we move from aiks to dl.
/// This allows conversion to an impeller supported type for paths without
/// converting from SkPath to impeller::Path.
class BufferGeometry final : public Geometry {
 public:
  BufferGeometry(VertexBuffer vertex_buffer,
                 Rect coverage,
                 GeometryResult::Mode mode);

  ~BufferGeometry() = default;

  // |Geometry|
  bool CoversArea(const Matrix& transform, const Rect& rect) const override;

 private:
  // |Geometry|
  GeometryResult GetPositionBuffer(const ContentContext& renderer,
                                   const Entity& entity,
                                   RenderPass& pass) const override;

  // |Geometry|
  GeometryVertexType GetVertexType() const override;

  // |Geometry|
  std::optional<Rect> GetCoverage(const Matrix& transform) const override;

  // |Geometry|
  GeometryResult GetPositionUVBuffer(Rect texture_coverage,
                                     Matrix effect_transform,
                                     const ContentContext& renderer,
                                     const Entity& entity,
                                     RenderPass& pass) const override;

  // |Geometry|
  GeometryResult::Mode GetResultMode() const override;

  VertexBuffer vertex_buffer_;
  Rect coverage_;
  GeometryResult::Mode mode_;

  BufferGeometry(const BufferGeometry&) = delete;

  BufferGeometry& operator=(const BufferGeometry&) = delete;
};

}  // namespace impeller

#endif  // FLUTTER_IMPELLER_ENTITY_GEOMETRY_BUFFER_GEOMETRY_H_
