// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/entity/geometry/point_field_geometry.h"

#include "impeller/entity/geometry/geometry.h"
#include "impeller/renderer/command_buffer.h"
#include "impeller/renderer/compute_command.h"

namespace impeller {

/// @brief Compute the number of vertices to divide each circle into.
///
/// @return the number of vertices.
static size_t ComputeCircleDivisions(Scalar scaled_radius, bool round) {
  if (!round) {
    return 4;
  }

  // Note: these values are approximated based on the values returned from
  // the decomposition of 4 cubics performed by Path::CreatePolyline.
  if (scaled_radius < 1.0) {
    return 4;
  }
  if (scaled_radius < 2.0) {
    return 8;
  }
  if (scaled_radius < 12.0) {
    return 24;
  }
  if (scaled_radius < 22.0) {
    return 34;
  }
  return std::min(scaled_radius, 140.0f);
}

std::optional<VertexBufferBuilder<SolidFillVertexShader::PerVertexData>>
GetPositionBufferCPU(const PointFieldData& data,
                     const ContentContext& renderer,
                     const Entity& entity,
                     RenderPass& pass) {
  if (data.radius < 0.0) {
    return std::nullopt;
  }
  auto transform = entity.GetTransform();
  auto determinant = transform.GetDeterminant();
  if (determinant == 0) {
    return std::nullopt;
  }

  Scalar min_size = 1.0f / sqrt(std::abs(determinant));
  Scalar radius = std::max(data.radius, min_size);

  VertexBufferBuilder<SolidFillVertexShader::PerVertexData> vtx_builder;

  if (data.round) {
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

    vtx_builder.Reserve((circle_vertices.size() + 2) * data.points.size() - 2);
    for (auto& center : data.points) {
      if (vtx_builder.HasVertices()) {
        vtx_builder.AppendVertex(vtx_builder.Last());
        vtx_builder.AppendVertex({center + circle_vertices[0]});
      }

      for (auto& vertex : circle_vertices) {
        vtx_builder.AppendVertex({center + vertex});
      }
    }
  } else {
    vtx_builder.Reserve(6 * data.points.size() - 2);
    for (auto& point : data.points) {
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

GeometryResult GetPositionBufferGPU(
    const PointFieldData& data,
    const ContentContext& renderer,
    const Entity& entity,
    RenderPass& pass,
    std::optional<Rect> texture_coverage = std::nullopt,
    std::optional<Matrix> effect_transform = std::nullopt) {
  FML_DCHECK(renderer.GetDeviceCapabilities().SupportsCompute());
  if (data.radius < 0.0) {
    return {};
  }
  auto determinant = entity.GetTransform().GetDeterminant();
  if (determinant == 0) {
    return {};
  }

  Scalar min_size = 1.0f / sqrt(std::abs(determinant));
  Scalar radius = std::max(data.radius, min_size);

  auto vertices_per_geom = ComputeCircleDivisions(
      entity.GetTransform().GetMaxBasisLength() * radius, data.round);

  auto points_per_circle = 3 + (vertices_per_geom - 3) * 3;
  auto total = points_per_circle * data.points.size();

  auto cmd_buffer = renderer.GetContext()->CreateCommandBuffer();
  auto compute_pass = cmd_buffer->CreateComputePass();
  auto& host_buffer = compute_pass->GetTransientsBuffer();

  auto points_data = host_buffer.Emplace(data.points.data(),
                                         data.points.size() * sizeof(Point),
                                         DefaultUniformAlignment());

  DeviceBufferDescriptor buffer_desc;
  buffer_desc.size = total * sizeof(Point);
  buffer_desc.storage_mode = StorageMode::kDevicePrivate;

  auto geometry_buffer = renderer.GetContext()
                             ->GetResourceAllocator()
                             ->CreateBuffer(buffer_desc)
                             ->AsBufferView();

  BufferView output;
  {
    using PS = PointsComputeShader;
    ComputeCommand cmd;
    DEBUG_COMMAND_INFO(cmd, "Points Geometry");
    cmd.pipeline = renderer.GetPointComputePipeline();

    PS::FrameInfo frame_info;
    frame_info.count = data.points.size();
    frame_info.radius = data.round ? radius : radius * kSqrt2;
    frame_info.radian_start = data.round ? 0.0f : kPiOver4;
    frame_info.radian_step = k2Pi / vertices_per_geom;
    frame_info.points_per_circle = points_per_circle;
    frame_info.divisions_per_circle = vertices_per_geom;

    PS::BindFrameInfo(cmd, host_buffer.EmplaceUniform(frame_info));
    PS::BindGeometryData(cmd, geometry_buffer);
    PS::BindPointData(cmd, points_data);

    if (!compute_pass->AddCommand(std::move(cmd))) {
      return {};
    }
    output = geometry_buffer;
  }

  if (texture_coverage.has_value() && effect_transform.has_value()) {
    DeviceBufferDescriptor buffer_desc;
    buffer_desc.size = total * sizeof(Vector4);
    buffer_desc.storage_mode = StorageMode::kDevicePrivate;

    auto geometry_uv_buffer = renderer.GetContext()
                                  ->GetResourceAllocator()
                                  ->CreateBuffer(buffer_desc)
                                  ->AsBufferView();

    using UV = UvComputeShader;

    ComputeCommand cmd;
    DEBUG_COMMAND_INFO(cmd, "UV Geometry");
    cmd.pipeline = renderer.GetUvComputePipeline();

    UV::FrameInfo frame_info;
    frame_info.count = total;
    frame_info.effect_transform = effect_transform.value();
    frame_info.texture_origin = {0, 0};
    frame_info.texture_size = Vector2(texture_coverage.value().size);

    UV::BindFrameInfo(cmd, host_buffer.EmplaceUniform(frame_info));
    UV::BindGeometryData(cmd, geometry_buffer);
    UV::BindGeometryUVData(cmd, geometry_uv_buffer);

    if (!compute_pass->AddCommand(std::move(cmd))) {
      return {};
    }
    output = geometry_uv_buffer;
  }

  compute_pass->SetGridSize(ISize(total, 1));
  compute_pass->SetThreadGroupSize(ISize(total, 1));

  if (!compute_pass->EncodeCommands() || !cmd_buffer->SubmitCommands()) {
    return {};
  }

  return {
      .type = PrimitiveType::kTriangle,
      .vertex_buffer = {.vertex_buffer = output,
                        .vertex_count = total,
                        .index_type = IndexType::kNone},
      .transform = Matrix::MakeOrthographic(pass.GetRenderTargetSize()) *
                   entity.GetTransform(),
      .prevent_overdraw = false,
  };
}

GeometryResult PointFieldDataGetPositionBuffer(const PointFieldData& data,
                                               const ContentContext& renderer,
                                               const Entity& entity,
                                               RenderPass& pass) {
  if (renderer.GetDeviceCapabilities().SupportsCompute()) {
    return GetPositionBufferGPU(data, renderer, entity, pass);
  }
  auto vtx_builder = GetPositionBufferCPU(data, renderer, entity, pass);
  if (!vtx_builder.has_value()) {
    return {};
  }

  auto& host_buffer = pass.GetTransientsBuffer();
  return {
      .type = PrimitiveType::kTriangleStrip,
      .vertex_buffer = vtx_builder->CreateVertexBuffer(host_buffer),
      .transform = Matrix::MakeOrthographic(pass.GetRenderTargetSize()) *
                   entity.GetTransform(),
      .prevent_overdraw = false,
  };
}

GeometryResult PointFieldDataGetPositionUVBuffer(const PointFieldData& data,
                                                 Rect texture_coverage,
                                                 Matrix effect_transform,
                                                 const ContentContext& renderer,
                                                 const Entity& entity,
                                                 RenderPass& pass) {
  if (renderer.GetDeviceCapabilities().SupportsCompute()) {
    return GetPositionBufferGPU(data, renderer, entity, pass, texture_coverage,
                                effect_transform);
  }

  auto vtx_builder = GetPositionBufferCPU(data, renderer, entity, pass);
  if (!vtx_builder.has_value()) {
    return {};
  }
  auto uv_vtx_builder = ComputeUVGeometryCPU(
      vtx_builder.value(), {0, 0}, texture_coverage.size, effect_transform);

  auto& host_buffer = pass.GetTransientsBuffer();
  return {
      .type = PrimitiveType::kTriangleStrip,
      .vertex_buffer = uv_vtx_builder.CreateVertexBuffer(host_buffer),
      .transform = Matrix::MakeOrthographic(pass.GetRenderTargetSize()) *
                   entity.GetTransform(),
      .prevent_overdraw = false,
  };
}

// |Geometry|
GeometryVertexType PointFieldDataGetVertexType(const PointFieldData& data) {
  return GeometryVertexType::kPosition;
}

// |Geometry|
std::optional<Rect> PointFieldDataGetCoverage(const PointFieldData& data,
                                              const Matrix& transform) {
  if (data.points.size() > 0) {
    // Doesn't use MakePointBounds as this isn't resilient to points that
    // all lie along the same axis.
    auto first = data.points.begin();
    auto last = data.points.end();
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
    auto coverage = Rect::MakeLTRB(left - data.radius, top - data.radius,
                                   right + data.radius, bottom + data.radius);
    return coverage.TransformBounds(transform);
  }
  return std::nullopt;
}

}  // namespace impeller
