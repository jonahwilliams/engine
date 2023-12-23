// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_ENTITY_GEOMETRY_ROUND_RECT_GEOMETRY_H_
#define FLUTTER_IMPELLER_ENTITY_GEOMETRY_ROUND_RECT_GEOMETRY_H_

#include "impeller/entity/geometry/geometry.h"

namespace impeller {

// Geometry class that can generate vertices (with or without texture
// coordinates) for filled ellipses. Generating vertices for a stroked
// ellipse would require a lot more work since the line width must be
// applied perpendicular to the distorted ellipse shape.

bool RoundRectDataCoversArea(const RoundRectData& data,
                             const Matrix& transform,
                             const Rect& rect);

bool RoundRectDataIsAxisAlignedRect(const RoundRectData& data);

GeometryResult RoundRectDataGetPositionBuffer(const RoundRectData& data,
                                              const ContentContext& renderer,
                                              const Entity& entity,
                                              RenderPass& pass);

GeometryVertexType RoundRectDataGetVertexType(const RoundRectData& data);

std::optional<Rect> RoundRectDataGetCoverage(const RoundRectData& data,
                                             const Matrix& transform);

GeometryResult RoundRectDataGetPositionUVBuffer(const RoundRectData& data,
                                                Rect texture_coverage,
                                                Matrix effect_transform,
                                                const ContentContext& renderer,
                                                const Entity& entity,
                                                RenderPass& pass);
}  // namespace impeller

#endif  // FLUTTER_IMPELLER_ENTITY_GEOMETRY_ROUND_RECT_GEOMETRY_H_
