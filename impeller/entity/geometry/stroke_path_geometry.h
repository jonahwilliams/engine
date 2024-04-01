// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_ENTITY_GEOMETRY_STROKE_PATH_GEOMETRY_H_
#define FLUTTER_IMPELLER_ENTITY_GEOMETRY_STROKE_PATH_GEOMETRY_H_

#include "impeller/entity/geometry/geometry.h"

namespace impeller {

/// @brief A geometry that is created from a stroked path object.
class StrokePathGeometry final : public Geometry {
 public:
  StrokePathGeometry(const Path& path,
                     Scalar stroke_width,
                     Scalar miter_limit,
                     Cap stroke_cap,
                     Join stroke_join);

  ~StrokePathGeometry();

  Scalar GetStrokeWidth() const;

  Scalar GetMiterLimit() const;

  Cap GetStrokeCap() const;

  Join GetStrokeJoin() const;

 private:
  // |Geometry|
  GeometryResult GetPositionBuffer(const ContentContext& renderer,
                                   const Entity& entity,
                                   RenderPass& pass) const override;

  // |Geometry|
  GeometryResult GetPositionUVBuffer(Rect texture_coverage,
                                     Matrix effect_transform,
                                     const ContentContext& renderer,
                                     const Entity& entity,
                                     RenderPass& pass) const override;

  // |Geometry|
  GeometryResult::Mode GetResultMode() const override;

  // |Geometry|
  GeometryVertexType GetVertexType() const override;

  // |Geometry|
  std::optional<Rect> GetCoverage(const Matrix& transform) const override;

  // Private for benchmarking and debugging
  static std::vector<SolidFillVertexShader::PerVertexData>
  GenerateSolidStrokeVertices(const Path::Polyline& polyline,
                              Scalar stroke_width,
                              Scalar miter_limit,
                              Join stroke_join,
                              Cap stroke_cap,
                              Scalar scale);

  static std::vector<TextureFillVertexShader::PerVertexData>
  GenerateSolidStrokeVerticesUV(const Path::Polyline& polyline,
                                Scalar stroke_width,
                                Scalar miter_limit,
                                Join stroke_join,
                                Cap stroke_cap,
                                Scalar scale,
                                Point texture_origin,
                                Size texture_size,
                                const Matrix& effect_transform);

  friend class ImpellerBenchmarkAccessor;
  friend class ImpellerEntityUnitTestAccessor;

  bool SkipRendering() const;

  Path path_;
  Scalar stroke_width_;
  Scalar miter_limit_;
  Cap stroke_cap_;
  Join stroke_join_;

  StrokePathGeometry(const StrokePathGeometry&) = delete;

  StrokePathGeometry& operator=(const StrokePathGeometry&) = delete;
};

// static_assert(std::is_trivially_destructible<RoundRectGeometry>());


}  // namespace impeller

#endif  // FLUTTER_IMPELLER_ENTITY_GEOMETRY_STROKE_PATH_GEOMETRY_H_
