// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/entity/geometry/geometry.h"

#include <optional>
#include <variant>

#include "fml/logging.h"
#include "impeller/entity/geometry/circle_geometry.h"
#include "impeller/entity/geometry/cover_geometry.h"
#include "impeller/entity/geometry/ellipse_geometry.h"
#include "impeller/entity/geometry/fill_path_geometry.h"
#include "impeller/entity/geometry/line_geometry.h"
#include "impeller/entity/geometry/point_field_geometry.h"
#include "impeller/entity/geometry/rect_geometry.h"
#include "impeller/entity/geometry/round_rect_geometry.h"
#include "impeller/entity/geometry/stroke_path_geometry.h"
#include "impeller/geometry/rect.h"

namespace impeller {

Geometry::Geometry(GeometryData data) : data_(std::move(data)) {}

// using GeometryData = std::variant<FillPathData,
//                                   StrokePathData,
//                                   CoverData,
//                                   RectData,
//                                   EllipseData,
//                                   LineData,
//                                   CircleData,
//                                   RoundRectData,
//                                   PointFieldData>;

Scalar Geometry::ComputePixelHalfWidth(const Matrix& transform, Scalar width) {
  auto determinant = transform.GetDeterminant();
  if (determinant == 0) {
    return 0.0f;
  }

  Scalar min_size = 1.0f / sqrt(std::abs(determinant));
  return std::max(width, min_size) * 0.5f;
}

bool Geometry::IsAxisAlignedRect() const {
  if (auto* data = std::get_if<FillPathData>(&data_)) {
    return false;
  }
  if (auto* data = std::get_if<StrokePathData>(&data_)) {
    return false;
  }
  if (auto* data = std::get_if<CoverData>(&data_)) {
    return false;
  }
  if (auto* data = std::get_if<RectData>(&data_)) {
    return RectDataIsAxisAlignedRect(*data);
  }
  if (auto* data = std::get_if<EllipseData>(&data_)) {
    return EllipseDataIsAxisAlignedRect(*data);
  }
  if (auto* data = std::get_if<LineData>(&data_)) {
    return LineDataIsAxisAlignedRect(*data);
  }
  if (auto* data = std::get_if<CircleData>(&data_)) {
    return CircleDataIsAxisAlignedRect(*data);
  }
  if (auto* data = std::get_if<RoundRectData>(&data_)) {
    return RoundRectDataIsAxisAlignedRect(*data);
  }
  if (auto* data = std::get_if<PointFieldData>(&data_)) {
    return false;
  }
  FML_UNREACHABLE();
}

bool Geometry::CoversArea(const Matrix& transform, const Rect& rect) const {
  if (auto* data = std::get_if<FillPathData>(&data_)) {
    return FillPathDataCoversArea(*data, transform, rect);
  }
  if (auto* data = std::get_if<StrokePathData>(&data_)) {
    return false;
  }
  if (auto* data = std::get_if<CoverData>(&data_)) {
    return true;
  }
  if (auto* data = std::get_if<RectData>(&data_)) {
    return RectDataCoversArea(*data, transform, rect);
  }
  if (auto* data = std::get_if<EllipseData>(&data_)) {
    return EllipseDataCoversArea(*data, transform, rect);
  }
  if (auto* data = std::get_if<LineData>(&data_)) {
    return LineDataCoversArea(*data, transform, rect);
  }
  if (auto* data = std::get_if<CircleData>(&data_)) {
    return CircleDataCoversArea(*data, transform, rect);
  }
  if (auto* data = std::get_if<RoundRectData>(&data_)) {
    return RoundRectDataCoversArea(*data, transform, rect);
  }
  if (auto* data = std::get_if<PointFieldData>(&data_)) {
    return false;
  }
  FML_UNREACHABLE();
}

GeometryResult Geometry::GetPositionBuffer(const ContentContext& renderer,
                                           const Entity& entity,
                                           RenderPass& pass) const {
  if (auto* data = std::get_if<FillPathData>(&data_)) {
    return FillPathDataGetPositionBuffer(*data, renderer, entity, pass);
  }
  if (auto* data = std::get_if<StrokePathData>(&data_)) {
    return StrokePathDataGetPositionBuffer(*data, renderer, entity, pass);
  }
  if (auto* data = std::get_if<CoverData>(&data_)) {
    return CoverDataGetPositionBuffer(*data, renderer, entity, pass);
  }
  if (auto* data = std::get_if<RectData>(&data_)) {
    return RectDataGetPositionBuffer(*data, renderer, entity, pass);
  }
  if (auto* data = std::get_if<EllipseData>(&data_)) {
    return EllipseDataGetPositionBuffer(*data, renderer, entity, pass);
  }
  if (auto* data = std::get_if<LineData>(&data_)) {
    return LineDataGetPositionBuffer(*data, renderer, entity, pass);
  }
  if (auto* data = std::get_if<CircleData>(&data_)) {
    return CircleDataGetPositionBuffer(*data, renderer, entity, pass);
  }
  if (auto* data = std::get_if<RoundRectData>(&data_)) {
    return RoundRectDataGetPositionBuffer(*data, renderer, entity, pass);
  }
  if (auto* data = std::get_if<PointFieldData>(&data_)) {
    return PointFieldDataGetPositionBuffer(*data, renderer, entity, pass);
  }
  FML_UNREACHABLE();
}

GeometryResult Geometry::GetPositionUVBuffer(Rect texture_coverage,
                                             Matrix effect_transform,
                                             const ContentContext& renderer,
                                             const Entity& entity,
                                             RenderPass& pass) const {
  if (auto* data = std::get_if<FillPathData>(&data_)) {
    return FillPathDataGetPositionUVBuffer(
        *data, texture_coverage, effect_transform, renderer, entity, pass);
  }
  if (auto* data = std::get_if<StrokePathData>(&data_)) {
    return StrokePathDataGetPositionUVBuffer(
        *data, texture_coverage, effect_transform, renderer, entity, pass);
  }
  if (auto* data = std::get_if<CoverData>(&data_)) {
    return CoverDataGetPositionUVBuffer(
        *data, texture_coverage, effect_transform, renderer, entity, pass);
  }
  if (auto* data = std::get_if<RectData>(&data_)) {
    return RectDataGetPositionUVBuffer(
        *data, texture_coverage, effect_transform, renderer, entity, pass);
  }
  if (auto* data = std::get_if<EllipseData>(&data_)) {
    return EllipseDataGetPositionUVBuffer(
        *data, texture_coverage, effect_transform, renderer, entity, pass);
  }
  if (auto* data = std::get_if<LineData>(&data_)) {
    return LineDataGetPositionUVBuffer(
        *data, texture_coverage, effect_transform, renderer, entity, pass);
  }
  if (auto* data = std::get_if<CircleData>(&data_)) {
    return CircleDataGetPositionUVBuffer(
        *data, texture_coverage, effect_transform, renderer, entity, pass);
  }
  if (auto* data = std::get_if<RoundRectData>(&data_)) {
    return RoundRectDataGetPositionUVBuffer(
        *data, texture_coverage, effect_transform, renderer, entity, pass);
  }
  if (auto* data = std::get_if<PointFieldData>(&data_)) {
    return PointFieldDataGetPositionUVBuffer(
        *data, texture_coverage, effect_transform, renderer, entity, pass);
  }
  FML_UNREACHABLE();
}

std::optional<Rect> Geometry::GetCoverage(const Matrix& transform) const {
  if (auto* data = std::get_if<FillPathData>(&data_)) {
    return FillPathDataGetCoverage(*data, transform);
  }
  if (auto* data = std::get_if<StrokePathData>(&data_)) {
    return StrokePathDataGetCoverage(*data, transform);
  }
  if (auto* data = std::get_if<CoverData>(&data_)) {
    return CoverDataGetCoverage(*data, transform);
  }
  if (auto* data = std::get_if<RectData>(&data_)) {
    return RectDataGetCoverage(*data, transform);
  }
  if (auto* data = std::get_if<EllipseData>(&data_)) {
    return EllipseDataGetCoverage(*data, transform);
  }
  if (auto* data = std::get_if<LineData>(&data_)) {
    return LineDataGetCoverage(*data, transform);
  }
  if (auto* data = std::get_if<CircleData>(&data_)) {
    return CircleDataGetCoverage(*data, transform);
  }
  if (auto* data = std::get_if<RoundRectData>(&data_)) {
    return RoundRectDataGetCoverage(*data, transform);
  }
  if (auto* data = std::get_if<PointFieldData>(&data_)) {
    return PointFieldDataGetCoverage(*data, transform);
  }
  FML_UNREACHABLE();
}

GeometryResult Geometry::ComputePositionGeometry(
    const Tessellator::VertexGenerator& generator,
    const Entity& entity,
    RenderPass& pass) {
  using VT = SolidFillVertexShader::PerVertexData;

  size_t count = generator.GetVertexCount();

  return GeometryResult{
      .type = generator.GetTriangleType(),
      .vertex_buffer =
          {
              .vertex_buffer = pass.GetTransientsBuffer().Emplace(
                  count * sizeof(VT), alignof(VT),
                  [&generator](uint8_t* buffer) {
                    auto vertices = reinterpret_cast<VT*>(buffer);
                    generator.GenerateVertices([&vertices](const Point& p) {
                      *vertices++ = {
                          .position = p,
                      };
                    });
                    FML_DCHECK(vertices == reinterpret_cast<VT*>(buffer) +
                                               generator.GetVertexCount());
                  }),
              .vertex_count = count,
              .index_type = IndexType::kNone,
          },
      .transform = Matrix::MakeOrthographic(pass.GetRenderTargetSize()) *
                   entity.GetTransform(),
      .prevent_overdraw = false,
  };
}

GeometryResult Geometry::ComputePositionUVGeometry(
    const Tessellator::VertexGenerator& generator,
    const Matrix& uv_transform,
    const Entity& entity,
    RenderPass& pass) {
  using VT = TextureFillVertexShader::PerVertexData;

  size_t count = generator.GetVertexCount();

  return GeometryResult{
      .type = generator.GetTriangleType(),
      .vertex_buffer =
          {
              .vertex_buffer = pass.GetTransientsBuffer().Emplace(
                  count * sizeof(VT), alignof(VT),
                  [&generator, &uv_transform](uint8_t* buffer) {
                    auto vertices = reinterpret_cast<VT*>(buffer);
                    generator.GenerateVertices(
                        [&vertices, &uv_transform](const Point& p) {  //
                          *vertices++ = {
                              .position = p,
                              .texture_coords = uv_transform * p,
                          };
                        });
                    FML_DCHECK(vertices == reinterpret_cast<VT*>(buffer) +
                                               generator.GetVertexCount());
                  }),
              .vertex_count = count,
              .index_type = IndexType::kNone,
          },
      .transform = Matrix::MakeOrthographic(pass.GetRenderTargetSize()) *
                   entity.GetTransform(),
      .prevent_overdraw = false,
  };
}

VertexBufferBuilder<TextureFillVertexShader::PerVertexData>
ComputeUVGeometryCPU(
    VertexBufferBuilder<SolidFillVertexShader::PerVertexData>& input,
    Point texture_origin,
    Size texture_coverage,
    Matrix effect_transform) {
  VertexBufferBuilder<TextureFillVertexShader::PerVertexData> vertex_builder;
  vertex_builder.Reserve(input.GetVertexCount());
  input.IterateVertices(
      [&vertex_builder, &texture_coverage, &effect_transform,
       &texture_origin](SolidFillVertexShader::PerVertexData old_vtx) {
        TextureFillVertexShader::PerVertexData data;
        data.position = old_vtx.position;
        data.texture_coords = effect_transform *
                              (old_vtx.position - texture_origin) /
                              texture_coverage;
        vertex_builder.AppendVertex(data);
      });
  return vertex_builder;
}

GeometryResult ComputeUVGeometryForRect(Rect source_rect,
                                        Rect texture_coverage,
                                        Matrix effect_transform,
                                        const ContentContext& renderer,
                                        const Entity& entity,
                                        RenderPass& pass) {
  auto& host_buffer = pass.GetTransientsBuffer();

  auto uv_transform =
      texture_coverage.GetNormalizingTransform() * effect_transform;
  std::vector<Point> data(8);
  auto points = source_rect.GetPoints();
  for (auto i = 0u, j = 0u; i < 8; i += 2, j++) {
    data[i] = points[j];
    data[i + 1] = uv_transform * points[j];
  }

  return GeometryResult{
      .type = PrimitiveType::kTriangleStrip,
      .vertex_buffer =
          {
              .vertex_buffer = host_buffer.Emplace(
                  data.data(), 16 * sizeof(float), alignof(float)),
              .vertex_count = 4,
              .index_type = IndexType::kNone,
          },
      .transform = Matrix::MakeOrthographic(pass.GetRenderTargetSize()) *
                   entity.GetTransform(),
      .prevent_overdraw = false,
  };
}

Geometry Geometry::MakeFillPath(Path path, std::optional<Rect> inner_rect) {
  return Geometry(
      FillPathData{.path = std::move(path), .inner_rect = inner_rect});
}

Geometry Geometry::MakePointField(std::vector<Point> points,
                                  Scalar radius,
                                  bool round) {
  return Geometry(PointFieldData{
      .points = std::move(points), .radius = radius, .round = round});
}

Geometry Geometry::MakeStrokePath(Path path,
                                  Scalar stroke_width,
                                  Scalar miter_limit,
                                  Cap stroke_cap,
                                  Join stroke_join) {
  // Skia behaves like this.
  if (miter_limit < 0) {
    miter_limit = 4.0;
  }
  return Geometry(StrokePathData{.path = std::move(path),
                                 .stroke_width = stroke_width,
                                 .miter_limit = miter_limit,
                                 .stroke_cap = stroke_cap,
                                 .stroke_join = stroke_join});
}

Geometry Geometry::MakeCover() {
  return Geometry(CoverData{});
}

Geometry Geometry::MakeRect(const Rect& rect) {
  return Geometry(RectData{.rect = rect});
}

Geometry Geometry::MakeOval(const Rect& rect) {
  return Geometry(EllipseData{.rect = rect});
}

Geometry Geometry::MakeLine(const Point& p0,
                            const Point& p1,
                            Scalar width,
                            Cap cap) {
  return Geometry(LineData{.p0 = p0, .p1 = p1, .width = width, .cap = cap});
}

Geometry Geometry::MakeCircle(const Point& center, Scalar radius) {
  return Geometry(CircleData{.center = center, .radius = radius});
}

Geometry Geometry::MakeStrokedCircle(const Point& center,
                                     Scalar radius,
                                     Scalar stroke_width) {
  return Geometry(CircleData{
      .center = center, .radius = radius, .stroke_width = stroke_width});
}

Geometry Geometry::MakeRoundRect(const Rect& rect, const Size& radii) {
  return Geometry(RoundRectData{.rect = rect, .size = radii});
}

}  // namespace impeller
