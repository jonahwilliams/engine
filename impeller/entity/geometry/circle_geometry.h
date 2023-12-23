// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_ENTITY_GEOMETRY_CIRCLE_GEOMETRY_H_
#define FLUTTER_IMPELLER_ENTITY_GEOMETRY_CIRCLE_GEOMETRY_H_

#include "impeller/entity/geometry/geometry.h"

namespace impeller {

bool CircleDataCoversArea(const CircleData& data,
                          const Matrix& transform,
                          const Rect& rect);

bool CircleDataIsAxisAlignedRect(const CircleData& data);

GeometryResult CircleDataGetPositionBuffer(const CircleData& data,
                                           const ContentContext& renderer,
                                           const Entity& entity,
                                           RenderPass& pass);

GeometryVertexType CircleDataGetVertexType(const CircleData& data);

std::optional<Rect> CircleDataGetCoverage(const CircleData& data,
                                          const Matrix& transform);

GeometryResult CircleDataGetPositionUVBuffer(const CircleData& data,
                                             Rect texture_coverage,
                                             Matrix effect_transform,
                                             const ContentContext& renderer,
                                             const Entity& entity,
                                             RenderPass& pass);

}  // namespace impeller

#endif  // FLUTTER_IMPELLER_ENTITY_GEOMETRY_CIRCLE_GEOMETRY_H_
