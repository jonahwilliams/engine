// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_DISPLAY_LIST_SKIA_FILL_PATH_GEOMETRY_H_
#define FLUTTER_IMPELLER_DISPLAY_LIST_SKIA_FILL_PATH_GEOMETRY_H_

#include <optional>

#include "impeller/entity/geometry/geometry.h"
#include "impeller/geometry/rect.h"
#include "include/core/SkPath.h"

namespace impeller {

/// @brief A geometry that is created from a filled path object.
class SkiaFillPathGeometry final : public Geometry {
 public:
  explicit SkiaFillPathGeometry(const SkPath& path);

  ~SkiaFillPathGeometry() override;

  // |Geometry|
  bool CoversArea(const Matrix& transform, const Rect& rect) const override;

 private:
  // |Geometry|
  GeometryResult GetPositionBuffer(const ContentContext& renderer,
                                   const Entity& entity,
                                   RenderPass& pass) const override;

  // |Geometry|
  std::optional<Rect> GetCoverage(const Matrix& transform) const override;

  // |Geometry|
  GeometryResult::Mode GetResultMode() const override;

  SkPath path_;

  SkiaFillPathGeometry(const SkiaFillPathGeometry&) = delete;

  SkiaFillPathGeometry& operator=(const SkiaFillPathGeometry&) = delete;
};

}  // namespace impeller

#endif  // FLUTTER_IMPELLER_DISPLAY_LIST_SKIA_FILL_PATH_GEOMETRY_H_
