// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_ENTITY_GEOMETRY_STROKE_PATH_GEOMETRY_H_
#define FLUTTER_IMPELLER_ENTITY_GEOMETRY_STROKE_PATH_GEOMETRY_H_

#include "impeller/entity/geometry/geometry.h"

namespace impeller {

GeometryResult StrokePathDataGetPositionBuffer(const StrokePathData& data,
                                               const ContentContext& renderer,
                                               const Entity& entity,
                                               RenderPass& pass);

GeometryResult StrokePathDataGetPositionUVBuffer(const StrokePathData& data,
                                                 Rect texture_coverage,
                                                 Matrix effect_transform,
                                                 const ContentContext& renderer,
                                                 const Entity& entity,
                                                 RenderPass& pass);

GeometryVertexType StrokePathDataGetVertexType(const StrokePathData& data);


std::optional<Rect> StrokePathDataGetCoverage(const StrokePathData& data,
                                              const Matrix& transform);

}  // namespace impeller

#endif  // FLUTTER_IMPELLER_ENTITY_GEOMETRY_STROKE_PATH_GEOMETRY_H_
