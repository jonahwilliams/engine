// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_ENTITY_GEOMETRY_FILL_PATH_GEOMETRY_H_
#define FLUTTER_IMPELLER_ENTITY_GEOMETRY_FILL_PATH_GEOMETRY_H_

#include <optional>

#include "impeller/entity/geometry/geometry.h"
#include "impeller/geometry/rect.h"

namespace impeller {

/// A geometry that is created from a filled path object.

bool FillPathDataCoversArea(const FillPathData& data,
                            const Matrix& transform,
                            const Rect& rect);

GeometryResult FillPathDataGetPositionBuffer(const FillPathData& data,
                                             const ContentContext& renderer,
                                             const Entity& entity,
                                             RenderPass& pass);

GeometryVertexType FillPathDataGetVertexType(const FillPathData& data);

std::optional<Rect> FillPathDataGetCoverage(const FillPathData& data,
                                            const Matrix& transform);

GeometryResult FillPathDataGetPositionUVBuffer(const FillPathData& data,
                                               Rect texture_coverage,
                                               Matrix effect_transform,
                                               const ContentContext& renderer,
                                               const Entity& entity,
                                               RenderPass& pass);

}  // namespace impeller

#endif  // FLUTTER_IMPELLER_ENTITY_GEOMETRY_FILL_PATH_GEOMETRY_H_
