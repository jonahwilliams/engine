// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/entity/geometry/point_field_geometry.h"

#include "impeller/renderer/command_buffer.h"

namespace impeller {

PointFieldGeometry::PointFieldGeometry(std::vector<Point> points,
                                       Scalar radius,
                                       bool round)
    : points_(std::move(points)), radius_(radius), round_(round) {}

GeometryResult PointFieldGeometry::GetPositionBuffer(
    const ContentContext& renderer,
    const Entity& entity,
    RenderPass& pass) const {
  auto vtx_builder = GetPositionBufferCPU(renderer, entity, pass);
  if (!vtx_builder.has_value()) {
    return {};
  }

  auto& host_buffer = renderer.GetTransientsBuffer();
  return {
      .type = PrimitiveType::kTriangleStrip,
      .vertex_buffer = vtx_builder->CreateVertexBuffer(host_buffer),
      .transform = entity.GetShaderTransform(pass),
  };
}

GeometryResult PointFieldGeometry::GetPositionUVBuffer(
    Rect texture_coverage,
    Matrix effect_transform,
    const ContentContext& renderer,
    const Entity& entity,
    RenderPass& pass) const {
  auto vtx_builder = GetPositionBufferCPU(renderer, entity, pass);
  if (!vtx_builder.has_value()) {
    return {};
  }
  auto uv_vtx_builder =
      ComputeUVGeometryCPU(vtx_builder.value(), {0, 0},
                           texture_coverage.GetSize(), effect_transform);

  auto& host_buffer = renderer.GetTransientsBuffer();
  return {
      .type = PrimitiveType::kTriangleStrip,
      .vertex_buffer = uv_vtx_builder.CreateVertexBuffer(host_buffer),
      .transform = entity.GetShaderTransform(pass),
  };
}

std::optional<VertexBufferBuilder<SolidFillVertexShader::PerVertexData>>
PointFieldGeometry::GetPositionBufferCPU(const ContentContext& renderer,
                                         const Entity& entity,
                                         RenderPass& pass) const {
  if (radius_ < 0.0) {
    return std::nullopt;
  }
  auto transform = entity.GetTransform();
  auto determinant = transform.GetDeterminant();
  if (determinant == 0) {
    return std::nullopt;
  }

  Scalar min_size = 1.0f / sqrt(std::abs(determinant));
  Scalar radius = std::max(radius_, min_size);

  VertexBufferBuilder<SolidFillVertexShader::PerVertexData> vtx_builder;

  if (round_) {
    // Get triangulation relative to {0, 0} so we can translate it to each
    // point in turn.
    auto generator =
        renderer.GetTessellator()->FilledCircle(transform, {}, radius);
    FML_DCHECK(generator.GetTriangleType() == PrimitiveType::kTriangleStrip);
    std::vector<Point> circle_vertices;
    circle_vertices.reserve(generator.GetVertexCount());
    generator.GenerateVertices([&circle_vertices](const Point& p) {  //
      circle_vertices.push_back(p);
    });
    FML_DCHECK(circle_vertices.size() == generator.GetVertexCount());

    vtx_builder.Reserve((circle_vertices.size() + 2) * points_.size() - 2);
    for (auto& center : points_) {
      if (vtx_builder.HasVertices()) {
        vtx_builder.AppendVertex(vtx_builder.Last());
        vtx_builder.AppendVertex({center + circle_vertices[0]});
      }

      for (auto& vertex : circle_vertices) {
        vtx_builder.AppendVertex({center + vertex});
      }
    }
  } else {
    vtx_builder.Reserve(6 * points_.size() - 2);
    for (auto& point : points_) {
      auto first = Point(point.x - radius, point.y - radius);

      if (vtx_builder.HasVertices()) {
        vtx_builder.AppendVertex(vtx_builder.Last());
        vtx_builder.AppendVertex({first});
      }

      // Z pattern from UL -> UR -> LL -> LR
      vtx_builder.AppendVertex({first});
      vtx_builder.AppendVertex({{point.x + radius, point.y - radius}});
      vtx_builder.AppendVertex({{point.x - radius, point.y + radius}});
      vtx_builder.AppendVertex({{point.x + radius, point.y + radius}});
    }
  }

  return vtx_builder;
}

// |Geometry|
GeometryVertexType PointFieldGeometry::GetVertexType() const {
  return GeometryVertexType::kPosition;
}

// |Geometry|
std::optional<Rect> PointFieldGeometry::GetCoverage(
    const Matrix& transform) const {
  if (points_.size() > 0) {
    // Doesn't use MakePointBounds as this isn't resilient to points that
    // all lie along the same axis.
    auto first = points_.begin();
    auto last = points_.end();
    auto left = first->x;
    auto top = first->y;
    auto right = first->x;
    auto bottom = first->y;
    for (auto it = first + 1; it < last; ++it) {
      left = std::min(left, it->x);
      top = std::min(top, it->y);
      right = std::max(right, it->x);
      bottom = std::max(bottom, it->y);
    }
    auto coverage = Rect::MakeLTRB(left - radius_, top - radius_,
                                   right + radius_, bottom + radius_);
    return coverage.TransformBounds(transform);
  }
  return std::nullopt;
}

}  // namespace impeller
