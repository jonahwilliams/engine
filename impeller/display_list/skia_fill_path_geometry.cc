// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/display_list/skia_fill_path_geometry.h"
#include <vector>

#include "fml/logging.h"
#include "impeller/core/formats.h"
#include "impeller/core/vertex_buffer.h"
#include "impeller/display_list/skia_conversions.h"
#include "impeller/entity/contents/content_context.h"
#include "impeller/entity/geometry/geometry.h"
#include "impeller/geometry/path_component.h"

namespace impeller {

SkiaFillPathGeometry::SkiaFillPathGeometry(const SkPath& path) : path_(path) {}

SkiaFillPathGeometry::~SkiaFillPathGeometry() {}

GeometryResult SkiaFillPathGeometry::GetPositionBuffer(
    const ContentContext& renderer,
    const Entity& entity,
    RenderPass& pass) const {
  auto& host_buffer = renderer.GetTransientsBuffer();

  const auto& bounding_box = path_.getBounds();
  if (bounding_box.isEmpty()) {
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
  auto scale = entity.GetTransform().GetMaxBasisLengthXY();

  std::vector<Point>& points = renderer.GetTessellator()->UnsafeGetPointBuffer();
  std::vector<uint16_t>& indices = renderer.GetTessellator()->UnsafeGetIndexBuffer();
  points.clear();
  indices.clear();
  VertexWriter writer(points, indices);

  SkPath::Verb verb = SkPath::Verb::kDone_Verb;
  bool started_contour = false;
  bool first_point = true;

  struct PathData {
    union {
      SkPoint points[4];
    };
  };
  PathData data;

  SkPath::Iter iterator = SkPath::Iter(path_, false);
  do {
    verb = iterator.next(data.points);
    switch (verb) {
      case SkPath::kLine_Verb: {
        started_contour = true;
        LinearPathComponent* linear =
            reinterpret_cast<LinearPathComponent*>(data.points);

        if (first_point) {
          writer.Write(linear->p1);
          first_point = false;
        }
        writer.Write(linear->p2);
        break;
      }
      case SkPath::kQuad_Verb: {
        started_contour = true;
        QuadraticPathComponent* quad =
            reinterpret_cast<QuadraticPathComponent*>(data.points);
        if (first_point) {
          writer.Write(quad->p1);
          first_point = false;
        }
        quad->ToLinearPathComponents(scale, writer);
        break;
      }
      case SkPath::kConic_Verb: {
        started_contour = true;
        constexpr auto kPow2 = 1;  // Only works for sweeps up to 90 degrees.
        constexpr auto kQuadCount = 1 + (2 * (1 << kPow2));
        SkPoint points[kQuadCount];
        const auto curve_count =
            SkPath::ConvertConicToQuads(data.points[0],          //
                                        data.points[1],          //
                                        data.points[2],          //
                                        iterator.conicWeight(),  //
                                        points,                  //
                                        kPow2                    //
            );

        for (int curve_index = 0, point_index = 0;  //
             curve_index < curve_count;             //
             curve_index++, point_index += 2        //
        ) {
          QuadraticPathComponent* quad =
              reinterpret_cast<QuadraticPathComponent*>(points + point_index);

          if (first_point) {
            writer.Write(quad->p1);
            first_point = false;
          }
          quad->ToLinearPathComponents(scale, writer);
        }
      } break;
      case SkPath::kCubic_Verb: {
        started_contour = true;
        CubicPathComponent* cubic =
            reinterpret_cast<CubicPathComponent*>(data.points);
        if (first_point) {
          writer.Write(cubic->p1);
          first_point = false;
        }
        cubic->ToLinearPathComponents(scale, writer);
        break;
      }
      case SkPath::kMove_Verb:
      case SkPath::kClose_Verb:
      case SkPath::kDone_Verb:
        if (started_contour) {
          writer.EndContour();
        }
        started_contour = false;
        first_point = true;
        break;
    }
  } while (verb != SkPath::Verb::kDone_Verb);

  if (started_contour) {
    writer.EndContour();
  }

  if (points.empty()) {
    return GeometryResult{
        .type = PrimitiveType::kTriangleStrip,
        .vertex_buffer =
            VertexBuffer{
                .vertex_buffer = {},
                .index_buffer = {},
                .vertex_count = 0u,
                .index_type = IndexType::k16bit,
            },
        .transform = entity.GetShaderTransform(pass),
        .mode = GetResultMode(),
    };
  }

  BufferView vertex_buffer = host_buffer.Emplace(
      points.data(), sizeof(Point) * points.size(), alignof(Point));

  BufferView index_buffer = host_buffer.Emplace(
      indices.data(), sizeof(uint16_t) * points.size(), alignof(uint16_t));

  return GeometryResult{
      .type = PrimitiveType::kTriangleStrip,
      .vertex_buffer =
          VertexBuffer{
              .vertex_buffer = std::move(vertex_buffer),
              .index_buffer = std::move(index_buffer),
              .vertex_count = indices.size(),
              .index_type = IndexType::k16bit,
          },
      .transform = entity.GetShaderTransform(pass),
      .mode = GetResultMode(),
  };
}

GeometryResult::Mode SkiaFillPathGeometry::GetResultMode() const {
  const auto& bounding_box = path_.getBounds();
  if (path_.isConvex() || bounding_box.isEmpty()) {
    return GeometryResult::Mode::kNormal;
  }

  switch (path_.getFillType()) {
    case SkPathFillType::kWinding:
      return GeometryResult::Mode::kNonZero;
    case SkPathFillType::kEvenOdd:
      return GeometryResult::Mode::kEvenOdd;
    case SkPathFillType::kInverseWinding:
    case SkPathFillType::kInverseEvenOdd:
      // Unsupported.
      break;
  }

  FML_UNREACHABLE();
}

std::optional<Rect> SkiaFillPathGeometry::GetCoverage(
    const Matrix& transform) const {
  return skia_conversions::ToRect(path_.getBounds()).TransformBounds(transform);
}

bool SkiaFillPathGeometry::CoversArea(const Matrix& transform,
                                      const Rect& rect) const {
  if (!transform.IsTranslationScaleOnly()) {
    return false;
  }
  Rect coverage =
      skia_conversions::ToRect(path_.getBounds()).TransformBounds(transform);
  return coverage.Contains(rect);
}

}  // namespace impeller
