// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_ENTITY_GEOMETRY_RECT_GEOMETRY_H_
#define FLUTTER_IMPELLER_ENTITY_GEOMETRY_RECT_GEOMETRY_H_

#include "impeller/entity/geometry/geometry.h"

namespace impeller {

bool RectDataCoversArea(const RectData& data,
                        const Matrix& transform,
                        const Rect& rect);

bool RectDataIsAxisAlignedRect(const RectData& data);

GeometryResult RectDataGetPositionBuffer(const RectData& data,
                                         const ContentContext& renderer,
                                         const Entity& entity,
                                         RenderPass& pass);

GeometryVertexType RectDataGetVertexType(const RectData& data);

std::optional<Rect> RectDataGetCoverage(const RectData& data,
                                        const Matrix& transform);

GeometryResult RectDataGetPositionUVBuffer(const RectData& data,
                                           Rect texture_coverage,
                                           Matrix effect_transform,
                                           const ContentContext& renderer,
                                           const Entity& entity,
                                           RenderPass& pass);

}  // namespace impeller

#endif  // FLUTTER_IMPELLER_ENTITY_GEOMETRY_RECT_GEOMETRY_H_
