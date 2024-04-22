// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/entity/geometry/fill_path_geometry.h"

#include "fml/logging.h"
#include "impeller/core/formats.h"
#include "impeller/core/vertex_buffer.h"
#include "impeller/entity/contents/content_context.h"
#include "impeller/entity/geometry/geometry.h"
#include "impeller/geometry/path.h"

namespace impeller {

FillPathGeometry::FillPathGeometry(const Path& path,
                                   std::optional<Rect> inner_rect)
    : path_(path), inner_rect_(inner_rect) {}

GeometryResult FillPathGeometry::GetPositionBuffer(
    const ContentContext& renderer,
    const Entity& entity,
    RenderPass& pass) const {
  auto& host_buffer = renderer.GetTransientsBuffer();

  const auto& bounding_box = path_.GetBoundingBox();
  if (bounding_box.has_value() && bounding_box->IsEmpty()) {
    return GeometryResult{
        .type = PrimitiveType::kTriangle,
        .vertex_buffer =
            VertexBuffer{
                .vertex_buffer = {},
                .vertex_count = 0,
                .index_type = IndexType::k16bit,
            },
        .transform = pass.GetOrthographicTransform() * entity.GetTransform(),
    };
  }

  if (renderer.GetDeviceCapabilities().SupportsCompute()) {
    // Vertex count is sum of subdivisions.
    // Index count is sum of subdivisions plus two for each contour end.

    auto tolerance = entity.GetTransform().GetMaxBasisLength();

    std::vector<PathComponent> data;
    data.reserve(path_.GetVerbCount());
    uint subdivision_count = path_.WriteComputeData(tolerance, data);
    if (subdivision_count == 0) {

    return GeometryResult{
        .type = PrimitiveType::kTriangleStrip,
        .vertex_buffer =
            VertexBuffer{
                .vertex_buffer = {},
                .index_buffer = {},
                .vertex_count = 0,
                .index_type = IndexType::kNone,
            },
        .transform = entity.GetShaderTransform(pass),
        .mode = GetResultMode(),
    };
    }

    size_t vertex_count = subdivision_count;

    BufferView vertex_buffer = host_buffer.Emplace(
        nullptr, sizeof(Point) * (vertex_count + 10), DefaultUniformAlignment());

    {
      using CS = SubdivisionComputeShaderPipeline::ComputeShader;

      ComputePass& compute_pass = renderer.GetOrCreateComputePass();

      compute_pass.SetPipeline(renderer.GetSubdivisionComputePipeline());
      compute_pass.SetCommandLabel("Convex Tessellation");

      CS::Config config;
      config.tolerance = tolerance;
      config.count = data.size();

      BufferView input_data =
          host_buffer.Emplace(data.data(), sizeof(PathComponent) * data.size(),
                              DefaultUniformAlignment());

      CS::BindConfig(compute_pass, host_buffer.EmplaceUniform(config));
      CS::BindPathData(compute_pass, input_data);
      CS::BindVertexData(compute_pass, vertex_buffer);

      if (!compute_pass.Compute(ISize(data.size(), 1)).ok()) {
        FML_LOG(ERROR) << "Computed with size: " << data.size();
        return {};
      }
    }

    return GeometryResult{
        .type = PrimitiveType::kTriangleStrip,
        .vertex_buffer =
            VertexBuffer{
                .vertex_buffer = std::move(vertex_buffer),
                .index_buffer = {},
                .vertex_count = vertex_count,
                .index_type = IndexType::kNone,
            },
        .transform = entity.GetShaderTransform(pass),
        .mode = GetResultMode(),
    };
  }

  VertexBuffer vertex_buffer = renderer.GetTessellator()->TessellateConvex(
      path_, host_buffer, entity.GetTransform().GetMaxBasisLength());

  return GeometryResult{
      .type = PrimitiveType::kTriangleStrip,
      .vertex_buffer = vertex_buffer,
      .transform = entity.GetShaderTransform(pass),
      .mode = GetResultMode(),
  };
}

GeometryResult::Mode FillPathGeometry::GetResultMode() const {
  const auto& bounding_box = path_.GetBoundingBox();
  if (path_.IsConvex() ||
      (bounding_box.has_value() && bounding_box->IsEmpty())) {
    return GeometryResult::Mode::kNormal;
  }

  switch (path_.GetFillType()) {
    case FillType::kNonZero:
      return GeometryResult::Mode::kNonZero;
    case FillType::kOdd:
      return GeometryResult::Mode::kEvenOdd;
  }

  FML_UNREACHABLE();
}

GeometryVertexType FillPathGeometry::GetVertexType() const {
  return GeometryVertexType::kPosition;
}

std::optional<Rect> FillPathGeometry::GetCoverage(
    const Matrix& transform) const {
  return path_.GetTransformedBoundingBox(transform);
}

bool FillPathGeometry::CoversArea(const Matrix& transform,
                                  const Rect& rect) const {
  if (!inner_rect_.has_value()) {
    return false;
  }
  if (!transform.IsTranslationScaleOnly()) {
    return false;
  }
  Rect coverage = inner_rect_->TransformBounds(transform);
  return coverage.Contains(rect);
}

}  // namespace impeller
