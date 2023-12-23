// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/entity/geometry/line_geometry.h"

namespace impeller {

Vector2 ComputeAlongVector(const LineData& data,
                           const Matrix& transform,
                           bool allow_zero_length) {
  Scalar stroke_half_width =
      Geometry::ComputePixelHalfWidth(transform, data.width);
  if (stroke_half_width < kEhCloseEnough) {
    return {};
  }

  auto along = data.p1 - data.p0;
  Scalar length = along.GetLength();
  if (length < kEhCloseEnough) {
    if (!allow_zero_length) {
      // We won't enclose any pixels unless the endpoints are extended
      return {};
    }
    return {stroke_half_width, 0};
  } else {
    return along * stroke_half_width / length;
  }
}

// Computes the 4 corners of a rectangle that defines the line and
// possibly extended endpoints which will be rendered under the given
// transform, and returns true if such a rectangle is defined.
//
// The coordinates will be generated in the original coordinate system
// of the line end points and the transform will only be used to determine
// the minimum line width.
//
// For kButt and kSquare end caps the ends should always be exteded as
// per that decoration, but for kRound caps the ends might be extended
// if the goal is to get a conservative bounds and might not be extended
// if the calling code is planning to draw the round caps on the ends.
//
// @return true if the transform and width were not degenerate
bool ComputeCorners(const LineData& data,
                    Point corners[4],
                    const Matrix& transform,
                    bool extend_endpoints) {
  auto along = ComputeAlongVector(data, transform, extend_endpoints);
  if (along.IsZero()) {
    return false;
  }

  auto across = Vector2(along.y, -along.x);
  corners[0] = data.p0 - across;
  corners[1] = data.p1 - across;
  corners[2] = data.p0 + across;
  corners[3] = data.p1 + across;
  if (extend_endpoints) {
    corners[0] -= along;
    corners[1] += along;
    corners[2] -= along;
    corners[3] += along;
  }
  return true;
}

GeometryResult LineDataGetPositionBuffer(const LineData& data,
                                         const ContentContext& renderer,
                                         const Entity& entity,
                                         RenderPass& pass) {
  using VT = SolidFillVertexShader::PerVertexData;

  auto& transform = entity.GetTransform();
  auto radius = Geometry::ComputePixelHalfWidth(transform, data.width);

  if (data.cap == Cap::kRound) {
    std::shared_ptr<Tessellator> tessellator = renderer.GetTessellator();
    auto generator =
        tessellator->RoundCapLine(transform, data.p0, data.p1, radius);
    return Geometry::ComputePositionGeometry(generator, entity, pass);
  }

  Point corners[4];
  if (!ComputeCorners(data, corners, transform, data.cap == Cap::kSquare)) {
    return kEmptyResult;
  }

  auto& host_buffer = pass.GetTransientsBuffer();

  size_t count = 4;
  BufferView vertex_buffer = host_buffer.Emplace(
      count * sizeof(VT), alignof(VT), [&corners](uint8_t* buffer) {
        auto vertices = reinterpret_cast<VT*>(buffer);
        for (auto& corner : corners) {
          *vertices++ = {
              .position = corner,
          };
        }
      });

  return GeometryResult{
      .type = PrimitiveType::kTriangleStrip,
      .vertex_buffer =
          {
              .vertex_buffer = vertex_buffer,
              .vertex_count = count,
              .index_type = IndexType::kNone,
          },
      .transform = Matrix::MakeOrthographic(pass.GetRenderTargetSize()) *
                   entity.GetTransform(),
      .prevent_overdraw = false,
  };
}

// |Geometry|
GeometryResult LineDataGetPositionUVBuffer(const LineData& data,
                                           Rect texture_coverage,
                                           Matrix effect_transform,
                                           const ContentContext& renderer,
                                           const Entity& entity,
                                           RenderPass& pass) {
  auto& host_buffer = pass.GetTransientsBuffer();
  using VT = TextureFillVertexShader::PerVertexData;

  auto& transform = entity.GetTransform();
  auto radius = Geometry::ComputePixelHalfWidth(transform, data.width);

  auto uv_transform =
      texture_coverage.GetNormalizingTransform() * effect_transform;

  if (data.cap == Cap::kRound) {
    std::shared_ptr<Tessellator> tessellator = renderer.GetTessellator();
    auto generator =
        tessellator->RoundCapLine(transform, data.p0, data.p1, radius);
    return Geometry::ComputePositionUVGeometry(generator, uv_transform, entity,
                                               pass);
  }

  Point corners[4];
  if (!ComputeCorners(data, corners, transform, data.cap == Cap::kSquare)) {
    return kEmptyResult;
  }

  size_t count = 4;
  BufferView vertex_buffer =
      host_buffer.Emplace(count * sizeof(VT), alignof(VT),
                          [&uv_transform, &corners](uint8_t* buffer) {
                            auto vertices = reinterpret_cast<VT*>(buffer);
                            for (auto& corner : corners) {
                              *vertices++ = {
                                  .position = corner,
                                  .texture_coords = uv_transform * corner,
                              };
                            }
                          });

  return GeometryResult{
      .type = PrimitiveType::kTriangleStrip,
      .vertex_buffer =
          {
              .vertex_buffer = vertex_buffer,
              .vertex_count = count,
              .index_type = IndexType::kNone,
          },
      .transform = Matrix::MakeOrthographic(pass.GetRenderTargetSize()) *
                   entity.GetTransform(),
      .prevent_overdraw = false,
  };
}

GeometryVertexType LineDataGetVertexType(const LineData& data) {
  return GeometryVertexType::kPosition;
}

std::optional<Rect> LineDataGetCoverage(const LineData& data,
                                        const Matrix& transform) {
  Point corners[4];
  if (!ComputeCorners(data, corners, transform, data.cap != Cap::kButt)) {
    return {};
  }

  for (int i = 0; i < 4; i++) {
    corners[i] = transform * corners[i];
  }
  return Rect::MakePointBounds(std::begin(corners), std::end(corners));
}

bool LineDataCoversArea(const LineData& data,
                        const Matrix& transform,
                        const Rect& rect) {
  if (!transform.IsTranslationScaleOnly() || !LineDataIsAxisAlignedRect(data)) {
    return false;
  }
  auto coverage = LineDataGetCoverage(data, transform);
  return coverage.has_value() ? coverage->Contains(rect) : false;
}

bool LineDataIsAxisAlignedRect(const LineData& data) {
  return data.cap != Cap::kRound &&
         (data.p0.x == data.p1.x || data.p0.y == data.p1.y);
}

}  // namespace impeller
