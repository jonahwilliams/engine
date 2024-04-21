// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/display_list/skia_conversions.h"
#include "display_list/dl_color.h"
#include "impeller/core/vertex_buffer.h"
#include "impeller/geometry/path_component.h"
#include "third_party/skia/modules/skparagraph/include/Paragraph.h"

namespace impeller {
namespace skia_conversions {

SkiaFillPathGeometry::SkiaFillPathGeometry(const SkPath& path,
                                           std::optional<Rect> inner_rect)
    : path_(path), inner_rect_(inner_rect) {}

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
  std::vector<Point>& point_data =
      renderer.GetTessellator()->GetTemporaryPointArena();
  std::vector<uint16_t>& index_data =
      renderer.GetTessellator()->GetTemporaryIndexArena();
  point_data.clear();
  index_data.clear();
  VertexWriter writer(point_data, index_data);

  auto iterator = SkPath::Iter(path_, false);

  struct PathData {
    union {
      SkPoint points[4];
    };
  };

  PathData data;
  auto verb = SkPath::Verb::kDone_Verb;
  Point current = Point(0, 0);
  std::optional<Point> curve_start = std::nullopt;
  do {
    verb = iterator.next(data.points);
    switch (verb) {
      case SkPath::kMove_Verb: {
        // TODO: does a move close behave the same as a regular close?
        if (curve_start.has_value()) {
          writer.EndContour();
          curve_start = std::nullopt;
        }
        current = ToPoint(data.points[0]);
        break;
      }
      case SkPath::kLine_Verb: {
        if (!curve_start.has_value()) {
          curve_start = ToPoint(data.points[0]);
        }
        writer.Write(ToPoint(data.points[1]));
        current = ToPoint(data.points[1]);
        break;
      }
      case SkPath::kQuad_Verb: {
        QuadraticPathComponent quad{current, ToPoint(data.points[1]),
                                    ToPoint(data.points[2])};
        quad.ToLinearPathComponents(scale, writer);
        current = ToPoint(data.points[2]);
        if (!curve_start.has_value()) {
          curve_start = ToPoint(data.points[0]);
        }
        break;
      }
      case SkPath::kConic_Verb: {
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

        if (!curve_start.has_value()) {
          curve_start = ToPoint(data.points[0]);
        }

        for (int curve_index = 0, point_index = 0;  //
             curve_index < curve_count;             //
             curve_index++, point_index += 2        //
        ) {
          QuadraticPathComponent quad{current, ToPoint(points[point_index + 1]),
                                      ToPoint(points[point_index + 2])};
          quad.ToLinearPathComponents(scale, writer);
          current = ToPoint(points[point_index + 2]);
        }
      } break;
      case SkPath::kCubic_Verb: {
        if (!curve_start.has_value()) {
          curve_start = ToPoint(data.points[0]);
        }
        CubicPathComponent cubic{current, ToPoint(data.points[1]),
                                 ToPoint(data.points[2]),
                                 ToPoint(data.points[3])};
        cubic.ToLinearPathComponents(scale, writer);
        current = ToPoint(data.points[3]);
      } break;
      case SkPath::kClose_Verb:
        if (curve_start.has_value()) {
          writer.Write(curve_start.value());
          writer.EndContour();
        }
        curve_start = std::nullopt;
        break;
      case SkPath::kDone_Verb:
        if (curve_start.has_value()) {
          writer.Write(curve_start.value());
          writer.EndContour();
        }
        break;
    }
  } while (verb != SkPath::Verb::kDone_Verb);

  if (point_data.empty()) {
    VertexBuffer vertex_buffer{
        .vertex_buffer = {},
        .index_buffer = {},
        .vertex_count = 0u,
        .index_type = IndexType::k16bit,
    };
    return GeometryResult{
        .type = PrimitiveType::kTriangleStrip,
        .vertex_buffer = vertex_buffer,
        .transform = entity.GetShaderTransform(pass),
        .mode = GetResultMode(),
    };
  }

  BufferView vertex_buffer = host_buffer.Emplace(
      point_data.data(), sizeof(Point) * point_data.size(), alignof(Point));

  BufferView index_buffer = host_buffer.Emplace(
      index_data.data(), sizeof(uint16_t) * index_data.size(),
      alignof(uint16_t));

  return GeometryResult{
      .type = PrimitiveType::kTriangleStrip,
      .vertex_buffer =
          VertexBuffer{
              .vertex_buffer = std::move(vertex_buffer),
              .index_buffer = std::move(index_buffer),
              .vertex_count = index_data.size(),
              .index_type = IndexType::k16bit,
          },
      .transform = entity.GetShaderTransform(pass),
      .mode = GetResultMode(),
  };
}

GeometryResult::Mode SkiaFillPathGeometry::GetResultMode() const {
  const auto& bounding_box = path_.getBounds();
  if (path_.isConvex() || (bounding_box.isEmpty())) {
    return GeometryResult::Mode::kNormal;
  }

  FillType fill_type;
  switch (path_.getFillType()) {
    case SkPathFillType::kWinding:
      fill_type = FillType::kNonZero;
      break;
    case SkPathFillType::kEvenOdd:
      fill_type = FillType::kOdd;
      break;
    case SkPathFillType::kInverseWinding:
    case SkPathFillType::kInverseEvenOdd:
      // Flutter doesn't expose these path fill types. These are only visible
      // via the receiver interface. We should never get here.
      fill_type = FillType::kNonZero;
      break;
  }

  switch (fill_type) {
    case FillType::kNonZero:
      return GeometryResult::Mode::kNonZero;
    case FillType::kOdd:
      return GeometryResult::Mode::kEvenOdd;
  }

  FML_UNREACHABLE();
}

GeometryVertexType SkiaFillPathGeometry::GetVertexType() const {
  return GeometryVertexType::kPosition;
}

std::optional<Rect> SkiaFillPathGeometry::GetCoverage(
    const Matrix& transform) const {
  return ToRect(path_.getBounds()).TransformBounds(transform);
}

bool SkiaFillPathGeometry::CoversArea(const Matrix& transform,
                                      const Rect& rect) const {
  return false;
}

Rect ToRect(const SkRect& rect) {
  return Rect::MakeLTRB(rect.fLeft, rect.fTop, rect.fRight, rect.fBottom);
}

std::optional<Rect> ToRect(const SkRect* rect) {
  if (rect == nullptr) {
    return std::nullopt;
  }
  return Rect::MakeLTRB(rect->fLeft, rect->fTop, rect->fRight, rect->fBottom);
}

std::vector<Rect> ToRects(const SkRect tex[], int count) {
  auto result = std::vector<Rect>();
  for (int i = 0; i < count; i++) {
    result.push_back(ToRect(tex[i]));
  }
  return result;
}

std::vector<Point> ToPoints(const SkPoint points[], int count) {
  std::vector<Point> result(count);
  for (auto i = 0; i < count; i++) {
    result[i] = ToPoint(points[i]);
  }
  return result;
}

PathBuilder::RoundingRadii ToRoundingRadii(const SkRRect& rrect) {
  using Corner = SkRRect::Corner;
  PathBuilder::RoundingRadii radii;
  radii.bottom_left = ToPoint(rrect.radii(Corner::kLowerLeft_Corner));
  radii.bottom_right = ToPoint(rrect.radii(Corner::kLowerRight_Corner));
  radii.top_left = ToPoint(rrect.radii(Corner::kUpperLeft_Corner));
  radii.top_right = ToPoint(rrect.radii(Corner::kUpperRight_Corner));
  return radii;
}

Path ToPath(const SkPath& path, Point shift) {
  auto iterator = SkPath::Iter(path, false);

  struct PathData {
    union {
      SkPoint points[4];
    };
  };

  PathBuilder builder;
  PathData data;
  // Reserve a path size with some arbitrarily additional padding.
  builder.Reserve(path.countPoints() + 8, path.countVerbs() + 8);
  auto verb = SkPath::Verb::kDone_Verb;
  do {
    verb = iterator.next(data.points);
    switch (verb) {
      case SkPath::kMove_Verb:
        builder.MoveTo(ToPoint(data.points[0]));
        break;
      case SkPath::kLine_Verb:
        builder.LineTo(ToPoint(data.points[1]));
        break;
      case SkPath::kQuad_Verb:
        builder.QuadraticCurveTo(ToPoint(data.points[1]),
                                 ToPoint(data.points[2]));
        break;
      case SkPath::kConic_Verb: {
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
          builder.QuadraticCurveTo(ToPoint(points[point_index + 1]),
                                   ToPoint(points[point_index + 2]));
        }
      } break;
      case SkPath::kCubic_Verb:
        builder.CubicCurveTo(ToPoint(data.points[1]), ToPoint(data.points[2]),
                             ToPoint(data.points[3]));
        break;
      case SkPath::kClose_Verb:
        builder.Close();
        break;
      case SkPath::kDone_Verb:
        break;
    }
  } while (verb != SkPath::Verb::kDone_Verb);

  FillType fill_type;
  switch (path.getFillType()) {
    case SkPathFillType::kWinding:
      fill_type = FillType::kNonZero;
      break;
    case SkPathFillType::kEvenOdd:
      fill_type = FillType::kOdd;
      break;
    case SkPathFillType::kInverseWinding:
    case SkPathFillType::kInverseEvenOdd:
      // Flutter doesn't expose these path fill types. These are only visible
      // via the receiver interface. We should never get here.
      fill_type = FillType::kNonZero;
      break;
  }
  builder.SetConvexity(path.isConvex() ? Convexity::kConvex
                                       : Convexity::kUnknown);
  builder.Shift(shift);
  auto sk_bounds = path.getBounds().makeOutset(shift.x, shift.y);
  builder.SetBounds(ToRect(sk_bounds));
  return builder.TakePath(fill_type);
}

Path ToPath(const SkRRect& rrect) {
  return PathBuilder{}
      .AddRoundedRect(ToRect(rrect.getBounds()), ToRoundingRadii(rrect))
      .SetConvexity(Convexity::kConvex)
      .SetBounds(ToRect(rrect.getBounds()))
      .TakePath();
}

Point ToPoint(const SkPoint& point) {
  return Point::MakeXY(point.fX, point.fY);
}

Size ToSize(const SkPoint& point) {
  return Size(point.fX, point.fY);
}

Color ToColor(const flutter::DlColor& color) {
  return {
      static_cast<Scalar>(color.getRedF()),    //
      static_cast<Scalar>(color.getGreenF()),  //
      static_cast<Scalar>(color.getBlueF()),   //
      static_cast<Scalar>(color.getAlphaF())   //
  };
}

std::vector<Matrix> ToRSXForms(const SkRSXform xform[], int count) {
  auto result = std::vector<Matrix>();
  for (int i = 0; i < count; i++) {
    auto form = xform[i];
    // clang-format off
    auto matrix = Matrix{
      form.fSCos, form.fSSin, 0, 0,
     -form.fSSin, form.fSCos, 0, 0,
      0,          0,          1, 0,
      form.fTx,   form.fTy,   0, 1
    };
    // clang-format on
    result.push_back(matrix);
  }
  return result;
}

Path PathDataFromTextBlob(const sk_sp<SkTextBlob>& blob, Point shift) {
  if (!blob) {
    return {};
  }

  return ToPath(skia::textlayout::Paragraph::GetPath(blob.get()), shift);
}

std::optional<impeller::PixelFormat> ToPixelFormat(SkColorType type) {
  switch (type) {
    case kRGBA_8888_SkColorType:
      return impeller::PixelFormat::kR8G8B8A8UNormInt;
    case kBGRA_8888_SkColorType:
      return impeller::PixelFormat::kB8G8R8A8UNormInt;
    case kRGBA_F16_SkColorType:
      return impeller::PixelFormat::kR16G16B16A16Float;
    case kBGR_101010x_XR_SkColorType:
      return impeller::PixelFormat::kB10G10R10XR;
    default:
      return std::nullopt;
  }
  return std::nullopt;
}

void ConvertStops(const flutter::DlGradientColorSourceBase* gradient,
                  std::vector<Color>& colors,
                  std::vector<float>& stops) {
  FML_DCHECK(gradient->stop_count() >= 2);

  auto* dl_colors = gradient->colors();
  auto* dl_stops = gradient->stops();
  if (dl_stops[0] != 0.0) {
    colors.emplace_back(skia_conversions::ToColor(dl_colors[0]));
    stops.emplace_back(0);
  }
  for (auto i = 0; i < gradient->stop_count(); i++) {
    colors.emplace_back(skia_conversions::ToColor(dl_colors[i]));
    stops.emplace_back(std::clamp(dl_stops[i], 0.0f, 1.0f));
  }
  if (dl_stops[gradient->stop_count() - 1] != 1.0) {
    colors.emplace_back(colors.back());
    stops.emplace_back(1.0);
  }
  for (auto i = 1; i < gradient->stop_count(); i++) {
    stops[i] = std::clamp(stops[i], stops[i - 1], stops[i]);
  }
}

}  // namespace skia_conversions
}  // namespace impeller
