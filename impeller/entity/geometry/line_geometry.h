// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_ENTITY_GEOMETRY_LINE_GEOMETRY_H_
#define FLUTTER_IMPELLER_ENTITY_GEOMETRY_LINE_GEOMETRY_H_

#include "impeller/entity/geometry/geometry.h"

namespace impeller {

bool LineDataCoversArea(const LineData& data,
                        const Matrix& transform,
                        const Rect& rect);

bool LineDataIsAxisAlignedRect(const LineData& data);

GeometryResult LineDataGetPositionBuffer(const LineData& data,
                                         const ContentContext& renderer,
                                         const Entity& entity,
                                         RenderPass& pass);

GeometryVertexType LineDataGetVertexType(const LineData& data);

std::optional<Rect> LineDataGetCoverage(const LineData& data,
                                        const Matrix& transform);

GeometryResult LineDataGetPositionUVBuffer(const LineData& data,
                                           Rect texture_coverage,
                                           Matrix effect_transform,
                                           const ContentContext& renderer,
                                           const Entity& entity,
                                           RenderPass& pass);

}  // namespace impeller

#endif  // FLUTTER_IMPELLER_ENTITY_GEOMETRY_LINE_GEOMETRY_H_
