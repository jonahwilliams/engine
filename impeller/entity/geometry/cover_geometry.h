// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_ENTITY_GEOMETRY_COVER_GEOMETRY_H_
#define FLUTTER_IMPELLER_ENTITY_GEOMETRY_COVER_GEOMETRY_H_

#include "impeller/entity/geometry/geometry.h"

namespace impeller {

GeometryResult CoverDataGetPositionBuffer(const CoverData& data,
                                          const ContentContext& renderer,
                                          const Entity& entity,
                                          RenderPass& pass);

GeometryVertexType CoverDataGetVertexType(const CoverData& data);

std::optional<Rect> CoverDataGetCoverage(const CoverData& data,
                                         const Matrix& transform);

GeometryResult CoverDataGetPositionUVBuffer(const CoverData& data,
                                            Rect texture_coverage,
                                            Matrix effect_transform,
                                            const ContentContext& renderer,
                                            const Entity& entity,
                                            RenderPass& pass);

}  // namespace impeller

#endif  // FLUTTER_IMPELLER_ENTITY_GEOMETRY_COVER_GEOMETRY_H_
