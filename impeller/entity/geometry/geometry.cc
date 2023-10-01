// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/entity/geometry/geometry.h"

#include <cstdint>
#include <optional>

#include "impeller/core/formats.h"
#include "impeller/core/host_buffer.h"
#include "impeller/entity/geometry/cover_geometry.h"
#include "impeller/entity/geometry/fill_path_geometry.h"
#include "impeller/entity/geometry/point_field_geometry.h"
#include "impeller/entity/geometry/rect_geometry.h"
#include "impeller/entity/geometry/stroke_path_geometry.h"
#include "impeller/geometry/point.h"
#include "impeller/geometry/rect.h"

namespace impeller {

/// Given a convex polyline, create a triangle fan structure.
std::pair<std::vector<Point>, std::vector<uint16_t>> TessellateConvex(
    const Path::Polyline& polyline) {
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

VertexBuffer TessellateConvex(const Path::Polyline& polyline,
                              HostBuffer& host_buffer) {
  FML_DCHECK(polyline.contours.size() == 1u);
  if (polyline.contours.size() > 1) {
    FML_LOG(ERROR) << "Invalid contour with size: " << polyline.contours.size();
    return {};
  }

  auto [start, end] = polyline.GetContourPointBounds(0);
  // Some polygons will not self close and an additional triangle
  // must be inserted, others will self close and we need to avoid
  // inserting an extra triangle.
  if (polyline.points[end - 1] == polyline.points[start]) {
    end--;
  }
  size_t vertex_count = polyline.points.size();
  size_t index_count = (end - 2) * 3;

  auto vtx_data = host_buffer.Emplace(
      vertex_count * sizeof(Point), alignof(Point), [&](uint8_t* contents) {
        std::memcpy(contents, polyline.points.data(),
                    sizeof(Point) * polyline.points.size());
      });
  auto idx_data = host_buffer.Emplace(
      index_count * sizeof(uint16_t), alignof(uint16_t),
      [&, end = end](uint8_t* contents) {
        for (uint16_t i = 2u; i < end; i++) {
          uint16_t data[3] = {0, static_cast<uint16_t>(i - 1),
                              static_cast<uint16_t>(i)};
          std::memcpy(contents, data, sizeof(uint16_t[3]));
          contents += (sizeof(uint16_t[3]));
        }
      });
  return VertexBuffer{.vertex_buffer = vtx_data,
                      .index_buffer = idx_data,
                      .vertex_count = index_count,
                      .index_type = IndexType::k16bit};
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

  std::vector<Point> data(8);
  auto points = source_rect.GetPoints();
  for (auto i = 0u, j = 0u; i < 8; i += 2, j++) {
    data[i] = points[j];
    data[i + 1] = effect_transform * (points[j] - texture_coverage.origin) /
                  texture_coverage.size;
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

std::unique_ptr<Geometry> Geometry::MakeFillPath(
    const Path& path,
    std::optional<Rect> inner_rect) {
  return std::make_unique<FillPathGeometry>(path, inner_rect);
}

std::unique_ptr<Geometry> Geometry::MakePointField(std::vector<Point> points,
                                                   Scalar radius,
                                                   bool round) {
  return std::make_unique<PointFieldGeometry>(std::move(points), radius, round);
}

std::unique_ptr<Geometry> Geometry::MakeStrokePath(const Path& path,
                                                   Scalar stroke_width,
                                                   Scalar miter_limit,
                                                   Cap stroke_cap,
                                                   Join stroke_join) {
  // Skia behaves like this.
  if (miter_limit < 0) {
    miter_limit = 4.0;
  }
  return std::make_unique<StrokePathGeometry>(path, stroke_width, miter_limit,
                                              stroke_cap, stroke_join);
}

std::unique_ptr<Geometry> Geometry::MakeCover() {
  return std::make_unique<CoverGeometry>();
}

std::unique_ptr<Geometry> Geometry::MakeRect(Rect rect) {
  return std::make_unique<RectGeometry>(rect);
}

bool Geometry::CoversArea(const Matrix& transform, const Rect& rect) const {
  return false;
}

}  // namespace impeller
