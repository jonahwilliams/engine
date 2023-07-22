// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "impeller/entity/geometry/geometry.h"

namespace impeller {

/// @brief A geometry that is created from a filled path object.
class FillPathGeometry : public Geometry {
 public:
  FillPathGeometry(const Path& path, std::optional<Rect> inner_rect = std::nullopt);

  ~FillPathGeometry();

 private:
  // |Geometry|
  GeometryResult GetPositionBuffer(const ContentContext& renderer,
                                   const Entity& entity,
                                   RenderPass& pass) override;

  // |Geometry|
  GeometryVertexType GetVertexType() const override;

  // |Geometry|
  std::optional<Rect> GetCoverage(const Matrix& transform) const override;

  // |Geometry|
  GeometryResult GetPositionUVBuffer(Rect texture_coverage,
                                     Matrix effect_transform,
                                     const ContentContext& renderer,
                                     const Entity& entity,
                                     RenderPass& pass) override;

  bool CoversArea(const Matrix& transform, const Rect& rect) const override;

  Path path_;
  std::optional<Rect> inner_rect_;

  FML_DISALLOW_COPY_AND_ASSIGN(FillPathGeometry);
};

}  // namespace impeller
