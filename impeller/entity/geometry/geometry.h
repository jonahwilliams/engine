// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_ENTITY_GEOMETRY_GEOMETRY_H_
#define FLUTTER_IMPELLER_ENTITY_GEOMETRY_GEOMETRY_H_

#include <optional>
#include <variant>

#include "impeller/core/formats.h"
#include "impeller/core/vertex_buffer.h"
#include "impeller/entity/contents/content_context.h"
#include "impeller/entity/entity.h"
#include "impeller/entity/texture_fill.vert.h"
#include "impeller/geometry/scalar.h"
#include "impeller/renderer/render_pass.h"
#include "impeller/renderer/vertex_buffer_builder.h"

namespace impeller {

class Tessellator;

struct GeometryResult {
  PrimitiveType type;
  VertexBuffer vertex_buffer;
  Matrix transform;
  bool prevent_overdraw;
};

static const GeometryResult kEmptyResult = {
    .vertex_buffer =
        {
            .index_type = IndexType::kNone,
        },
};

enum GeometryVertexType {
  kPosition,
  kColor,
  kUV,
};

/// @brief Compute UV geometry for a VBB that contains only position geometry.
///
/// texture_origin should be set to 0, 0 for stroke and stroke based geometry,
/// like the point field.
VertexBufferBuilder<TextureFillVertexShader::PerVertexData>
ComputeUVGeometryCPU(
    VertexBufferBuilder<SolidFillVertexShader::PerVertexData>& input,
    Point texture_origin,
    Size texture_coverage,
    Matrix effect_transform);

GeometryResult ComputeUVGeometryForRect(Rect source_rect,
                                        Rect texture_coverage,
                                        Matrix effect_transform,
                                        const ContentContext& renderer,
                                        const Entity& entity,
                                        RenderPass& pass);

struct FillPathData {
  Path path;
  std::optional<Rect> inner_rect;
};

struct StrokePathData {
  Path path;
  Scalar stroke_width;
  Scalar miter_limit;
  Cap stroke_cap;
  Join stroke_join;
};

struct CoverData {};

struct RectData {
  Rect rect;
};

// Geometry class that can generate vertices (with or without texture
// coordinates) for filled ellipses. Generating vertices for a stroked
// ellipse would require a lot more work since the line width must be
// applied perpendicular to the distorted ellipse shape.
struct EllipseData {
  Rect rect;
};

struct LineData {
  Point p0;
  Point p1;
  Scalar width;
  Cap cap;
};

struct CircleData {
  Point center;
  Scalar radius;
  Scalar stroke_width;
};

struct RoundRectData {
  Rect rect;
  Size size;
};

struct PointFieldData {
  std::vector<Point> points;
  Scalar radius;
  bool round;
};

class VerticesGeometry;

using GeometryData = std::variant<FillPathData,
                                  StrokePathData,
                                  CoverData,
                                  RectData,
                                  EllipseData,
                                  LineData,
                                  CircleData,
                                  RoundRectData,
                                  PointFieldData>;

class Geometry final {
 public:
  Geometry() : data_(RectData{.rect = Rect::MakeLTRB(0, 0, 0, 0)}) {};

  static Scalar ComputePixelHalfWidth(const Matrix& transform, Scalar width);

  static Geometry MakeFillPath(Path path,
                               std::optional<Rect> inner_rect = std::nullopt);

  static Geometry MakeStrokePath(Path path,
                                 Scalar stroke_width = 0.0,
                                 Scalar miter_limit = 4.0,
                                 Cap stroke_cap = Cap::kButt,
                                 Join stroke_join = Join::kMiter);

  static Geometry MakeCover();

  static Geometry MakeRect(const Rect& rect);

  static Geometry MakeOval(const Rect& rect);

  static Geometry MakeLine(const Point& p0,
                           const Point& p1,
                           Scalar width,
                           Cap cap);

  static Geometry MakeCircle(const Point& center, Scalar radius);

  static Geometry MakeStrokedCircle(const Point& center,
                                    Scalar radius,
                                    Scalar stroke_width);

  static Geometry MakeRoundRect(const Rect& rect, const Size& radii);

  static Geometry MakePointField(std::vector<Point> points,
                                 Scalar radius,
                                 bool round);

  GeometryResult GetPositionBuffer(const ContentContext& renderer,
                                   const Entity& entity,
                                   RenderPass& pass) const;

  GeometryResult GetPositionUVBuffer(Rect texture_coverage,
                                     Matrix effect_transform,
                                     const ContentContext& renderer,
                                     const Entity& entity,
                                     RenderPass& pass) const;

  GeometryVertexType GetVertexType() const;

  std::optional<Rect> GetCoverage(const Matrix& transform) const;

  /// @brief    Determines if this geometry, transformed by the given
  ///           `transform`, will completely cover all surface area of the given
  ///           `rect`.
  ///
  ///           This is a conservative estimate useful for certain
  ///           optimizations.
  ///
  /// @returns  `true` if the transformed geometry is guaranteed to cover the
  ///           given `rect`. May return `false` in many undetected cases where
  ///           the transformed geometry does in fact cover the `rect`.
  bool CoversArea(const Matrix& transform, const Rect& rect) const;

  bool IsAxisAlignedRect() const;

  static GeometryResult ComputePositionGeometry(
      const Tessellator::VertexGenerator& generator,
      const Entity& entity,
      RenderPass& pass);

  static GeometryResult ComputePositionUVGeometry(
      const Tessellator::VertexGenerator& generator,
      const Matrix& uv_transform,
      const Entity& entity,
      RenderPass& pass);

 private:
  explicit Geometry(GeometryData data);

  GeometryData data_;
};

}  // namespace impeller

#endif  // FLUTTER_IMPELLER_ENTITY_GEOMETRY_GEOMETRY_H_
