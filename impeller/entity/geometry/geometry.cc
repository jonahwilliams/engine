// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/entity/geometry/geometry.h"

#include <optional>

#include "impeller/entity/geometry/cover_geometry.h"
#include "impeller/entity/geometry/fill_path_geometry.h"
#include "impeller/entity/geometry/point_field_geometry.h"
#include "impeller/entity/geometry/rect_geometry.h"
#include "impeller/entity/geometry/stroke_path_geometry.h"
#include "impeller/geometry/rect.h"

namespace impeller {

/// Given a convex polyline, create a triangle fan structure.
std::pair<std::vector<Point>, std::vector<uint16_t>> TessellateConvex(
    Path::Polyline polyline) {
  std::vector<Point> output;
  std::vector<uint16_t> indices;

  for (auto j = 0u; j < polyline.contours.size(); j++) {
    auto [start, end] = polyline.GetContourPointBounds(j);
    auto center = polyline.points[start];

    // Some polygons will not self close and an additional triangle
    // must be inserted, others will self close and we need to avoid
    // inserting an extra triangle.
    if (polyline.points[end - 1] == polyline.points[start]) {
      end--;
    }
    output.emplace_back(center);
    output.emplace_back(polyline.points[start + 1]);

    for (auto i = start + 2; i < end; i++) {
      const auto& point_b = polyline.points[i];
      output.emplace_back(point_b);

      indices.emplace_back(0);
      indices.emplace_back(i - 1);
      indices.emplace_back(i);
    }
  }
  return std::make_pair(output, indices);
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
                   entity.GetTransformation(),
      .prevent_overdraw = false,
  };
}

Geometry::Geometry() = default;

Geometry::~Geometry() = default;

GeometryResult Geometry::GetPositionUVBuffer(Rect texture_coverage,
                                             Matrix effect_transform,
                                             const ContentContext& renderer,
                                             const Entity& entity,
                                             RenderPass& pass) {
  return {};
}

GeometryRef Geometry::MakeFillPath(
    const std::shared_ptr<PerFrameAllocator>& allocator,
    const Path& path,
    std::optional<Rect> inner_rect) {
  auto* geom = allocator->AllocateObjectOrDie<FillPathGeometry>();
  geom->SetPath(path);
  geom->SetInnerRect(inner_rect);
  return geom;
}

GeometryRef Geometry::MakePointField(
    const std::shared_ptr<PerFrameAllocator>& allocator,
    const std::vector<Point>& points,
    Scalar radius,
    bool round) {
  auto* geom = allocator->AllocateObjectOrDie<PointFieldGeometry>();
  geom->SetPoints(points);
  geom->SetRadius(radius);
  geom->SetRound(round);

  return geom;
}

GeometryRef Geometry::MakeStrokePath(
    const std::shared_ptr<PerFrameAllocator>& allocator,
    const Path& path,
    Scalar stroke_width,
    Scalar miter_limit,
    Cap stroke_cap,
    Join stroke_join) {
  // Skia behaves like this.
  if (miter_limit < 0) {
    miter_limit = 4.0;
  }

  auto* geom = allocator->AllocateObjectOrDie<StrokePathGeometry>();
  geom->SetPath(path);
  geom->SetStrokeWidth(stroke_width);
  geom->SetMiterLimit(miter_limit);
  geom->SetStrokeCap(stroke_cap);
  geom->SetStrokeJoin(stroke_join);
  return geom;
}

GeometryRef Geometry::MakeCover(
    const std::shared_ptr<PerFrameAllocator>& allocator) {
  auto* geom = allocator->AllocateObjectOrDie<CoverGeometry>();
  return geom;
}

GeometryRef Geometry::MakeRect(
    const std::shared_ptr<PerFrameAllocator>& allocator,
    Rect rect) {
  auto* geom = allocator->AllocateObjectOrDie<RectGeometry>();
  geom->SetRect(rect);
  return geom;
}

bool Geometry::CoversArea(const Matrix& transform, const Rect& rect) const {
  return false;
}

bool Geometry::IsAxisAlignedRect() const {
  return false;
}

}  // namespace impeller
