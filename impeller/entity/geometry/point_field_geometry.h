// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_ENTITY_GEOMETRY_POINT_FIELD_GEOMETRY_H_
#define FLUTTER_IMPELLER_ENTITY_GEOMETRY_POINT_FIELD_GEOMETRY_H_

#include "impeller/entity/geometry/geometry.h"

namespace impeller {

GeometryResult PointFieldDataGetPositionBuffer(const PointFieldData& data,
                                               const ContentContext& renderer,
                                               const Entity& entity,
                                               RenderPass& pass);

GeometryResult PointFieldDataGetPositionUVBuffer(const PointFieldData& data,
                                                 Rect texture_coverage,
                                                 Matrix effect_transform,
                                                 const ContentContext& renderer,
                                                 const Entity& entity,
                                                 RenderPass& pass);

GeometryVertexType PointFieldDataGetVertexType(const PointFieldData& data);

std::optional<Rect> PointFieldDataGetCoverage(const PointFieldData& data,
                                              const Matrix& transform);

GeometryResult PointFieldDataGetPositionBufferGPU(
    const PointFieldData& data,
    const ContentContext& renderer,
    const Entity& entity,
    RenderPass& pass,
    std::optional<Rect> texture_coverage = std::nullopt,
    std::optional<Matrix> effect_transform = std::nullopt);

}  // namespace impeller

#endif  // FLUTTER_IMPELLER_ENTITY_GEOMETRY_POINT_FIELD_GEOMETRY_H_
