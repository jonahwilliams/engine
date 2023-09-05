// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/geometry/path.h"

#include <optional>
#include <variant>

#include "impeller/geometry/path_component.h"
#include "impeller/geometry/point.h"

namespace impeller {

Path::Path() {
  AddContourComponent({});
};

Path::~Path() = default;

std::tuple<size_t, size_t> Path::Polyline::GetContourPointBounds(
    size_t contour_index) const {
  if (contour_index >= contours.size()) {
    return {points.size(), points.size()};
  }
  const size_t start_index = contours.at(contour_index).start_index;
  const size_t end_index = (contour_index >= contours.size() - 1)
                               ? points.size()
                               : contours.at(contour_index + 1).start_index;
  return std::make_tuple(start_index, end_index);
}

size_t Path::GetComponentCount(std::optional<ComponentType> type) const {
  if (type.has_value()) {
    switch (type.value()) {
      case ComponentType::kLinear:
        return linears_.size();
      case ComponentType::kQuadratic:
        return quads_.size();
      case ComponentType::kCubic:
        return cubics_.size();
      case ComponentType::kContour:
        return contours_.size();
    }
  }
  return components_.size();
}

void Path::SetFillType(FillType fill) {
  fill_ = fill;
}

FillType Path::GetFillType() const {
  return fill_;
}

bool Path::IsConvex() const {
  return convexity_ == Convexity::kConvex;
}

void Path::SetConvexity(Convexity value) {
  convexity_ = value;
}

void Path::Shift(Point shift) {
  size_t currentIndex = 0;
  for (const auto& component : components_) {
    switch (component.type) {
      case ComponentType::kLinear:
        linears_[component.index].p1 += shift;
        linears_[component.index].p2 += shift;
        break;
      case ComponentType::kQuadratic:
        quads_[component.index].cp += shift;
        quads_[component.index].p1 += shift;
        quads_[component.index].p2 += shift;
        break;
      case ComponentType::kCubic:
        cubics_[component.index].cp1 += shift;
        cubics_[component.index].cp2 += shift;
        cubics_[component.index].p1 += shift;
        cubics_[component.index].p2 += shift;
        break;
      case ComponentType::kContour:
        contours_[component.index].destination += shift;
        break;
    }
    currentIndex++;
  }
}

Path& Path::AddLinearComponent(Point p1, Point p2) {
  linears_.emplace_back(p1, p2);
  components_.emplace_back(ComponentType::kLinear, linears_.size() - 1);
  return *this;
}

Path& Path::AddQuadraticComponent(Point p1, Point cp, Point p2) {
  quads_.emplace_back(p1, cp, p2);
  components_.emplace_back(ComponentType::kQuadratic, quads_.size() - 1);
  return *this;
}

Path& Path::AddCubicComponent(Point p1, Point cp1, Point cp2, Point p2) {
  cubics_.emplace_back(p1, cp1, cp2, p2);
  components_.emplace_back(ComponentType::kCubic, cubics_.size() - 1);
  return *this;
}

Path& Path::AddContourComponent(Point destination, bool is_closed) {
  if (components_.size() > 0 &&
      components_.back().type == ComponentType::kContour) {
    // Never insert contiguous contours.
    contours_.back() = ContourComponent(destination, is_closed);
  } else {
    contours_.emplace_back(ContourComponent(destination, is_closed));
    components_.emplace_back(ComponentType::kContour, contours_.size() - 1);
  }
  return *this;
}

void Path::SetContourClosed(bool is_closed) {
  contours_.back().is_closed = is_closed;
}

void Path::EnumerateComponents(
    const Applier<LinearPathComponent>& linear_applier,
    const Applier<QuadraticPathComponent>& quad_applier,
    const Applier<CubicPathComponent>& cubic_applier,
    const Applier<ContourComponent>& contour_applier) const {
  size_t currentIndex = 0;
  for (const auto& component : components_) {
    switch (component.type) {
      case ComponentType::kLinear:
        if (linear_applier) {
          linear_applier(currentIndex, linears_[component.index]);
        }
        break;
      case ComponentType::kQuadratic:
        if (quad_applier) {
          quad_applier(currentIndex, quads_[component.index]);
        }
        break;
      case ComponentType::kCubic:
        if (cubic_applier) {
          cubic_applier(currentIndex, cubics_[component.index]);
        }
        break;
      case ComponentType::kContour:
        if (contour_applier) {
          contour_applier(currentIndex, contours_[component.index]);
        }
        break;
    }
    currentIndex++;
  }
}

bool Path::GetLinearComponentAtIndex(size_t index,
                                     LinearPathComponent& linear) const {
  if (index >= components_.size()) {
    return false;
  }

  if (components_[index].type != ComponentType::kLinear) {
    return false;
  }

  linear = linears_[components_[index].index];
  return true;
}

bool Path::GetQuadraticComponentAtIndex(
    size_t index,
    QuadraticPathComponent& quadratic) const {
  if (index >= components_.size()) {
    return false;
  }

  if (components_[index].type != ComponentType::kQuadratic) {
    return false;
  }

  quadratic = quads_[components_[index].index];
  return true;
}

bool Path::GetCubicComponentAtIndex(size_t index,
                                    CubicPathComponent& cubic) const {
  if (index >= components_.size()) {
    return false;
  }

  if (components_[index].type != ComponentType::kCubic) {
    return false;
  }

  cubic = cubics_[components_[index].index];
  return true;
}

bool Path::GetContourComponentAtIndex(size_t index,
                                      ContourComponent& move) const {
  if (index >= components_.size()) {
    return false;
  }

  if (components_[index].type != ComponentType::kContour) {
    return false;
  }

  move = contours_[components_[index].index];
  return true;
}

bool Path::UpdateLinearComponentAtIndex(size_t index,
                                        const LinearPathComponent& linear) {
  if (index >= components_.size()) {
    return false;
  }

  if (components_[index].type != ComponentType::kLinear) {
    return false;
  }

  linears_[components_[index].index] = linear;
  return true;
}

bool Path::UpdateQuadraticComponentAtIndex(
    size_t index,
    const QuadraticPathComponent& quadratic) {
  if (index >= components_.size()) {
    return false;
  }

  if (components_[index].type != ComponentType::kQuadratic) {
    return false;
  }

  quads_[components_[index].index] = quadratic;
  return true;
}

bool Path::UpdateCubicComponentAtIndex(size_t index,
                                       CubicPathComponent& cubic) {
  if (index >= components_.size()) {
    return false;
  }

  if (components_[index].type != ComponentType::kCubic) {
    return false;
  }

  cubics_[components_[index].index] = cubic;
  return true;
}

bool Path::UpdateContourComponentAtIndex(size_t index,
                                         const ContourComponent& move) {
  if (index >= components_.size()) {
    return false;
  }

  if (components_[index].type != ComponentType::kContour) {
    return false;
  }

  contours_[components_[index].index] = move;
  return true;
}

void PathListener::AddPoint(Point point) {
  if (last_point_.has_value() && last_point_ == point) {
    return;
  }
  last_point_ = point;
  storage_.emplace_back(point);
}

void PathListener::StartCountour(
    const ContourComponent& contour,
    std::optional<PathComponentVariant>& next_variant) {
  last_point_ = std::nullopt;
  Vector2 start_direction = Vector2(0, -1);
  if (next_variant.has_value()) {
    auto maybe_vector =
        std::visit(PathComponentStartDirectionVisitor(), next_variant.value());
    if (maybe_vector.has_value()) {
      start_direction = maybe_vector.value();
    }
  }

  OnContourStart(contour.is_closed, start_direction);
}

void PathListener::EndContour(std::optional<PathComponentVariant>& variant) {
  // Whenever a contour has ended, extract the exact end direction from the
  // last component.
  if (!variant.has_value()) {
    return;
  }

  auto maybe_vector =
      std::visit(PathComponentEndDirectionVisitor(), variant.value());
  UpdateLastContourEndDirection(maybe_vector.value_or(Vector2{0, 1}));

  if (storage_.size() > 0u) {
    OnContour(storage_.data(), storage_.size());
    storage_.clear();
  }
}

PathComponentVariant Path::GetPathComponent(size_t index) const {
  if (index >= components_.size()) {
    return std::monostate{};
  }
  const auto& component = components_[index];
  switch (component.type) {
    case ComponentType::kLinear:
      return &linears_[component.index];
    case ComponentType::kQuadratic:
      return &quads_[component.index];
    case ComponentType::kCubic:
      return &cubics_[component.index];
    case ComponentType::kContour:
      return std::monostate{};
  }
};

class PolylineBuilder : public PathListener {
 public:
  PolylineBuilder() = default;

  ~PolylineBuilder() = default;

  Path::Polyline& GetPolyline() { return polyline_; }

 private:
  void OnContourStart(bool is_closed, Vector2 start_direction) override {
    polyline_.contours.push_back({.start_index = polyline_.points.size(),
                                  .is_closed = is_closed,
                                  .start_direction = start_direction});
  }

  void OnContour(const Point data[], size_t countour_size) override {
    for (auto i = 0u; i < countour_size; i++) {
      polyline_.points.push_back(data[i]);
    }
  }

  void UpdateLastContourEndDirection(Vector2 end_direction) override {
    if (polyline_.contours.size() == 0u) {
      return;
    }
    polyline_.contours.back().end_direction = end_direction;
  }

  Path::Polyline polyline_;
};

Path::Polyline Path::CreatePolyline(Scalar scale) const {
  PolylineBuilder builder;
  CreatePolyline(scale, builder);
  return builder.GetPolyline();
}

void Path::CreatePolyline(Scalar scale, PathListener& listener) const {
  std::optional<PathComponentVariant> variant;

  for (size_t component_i = 0; component_i < components_.size();
       component_i++) {
    const auto& component = components_[component_i];
    switch (component.type) {
      case ComponentType::kLinear:
        linears_[component.index].CreatePolyline(listener);
        variant = &linears_[component.index];
        break;
      case ComponentType::kQuadratic:
        quads_[component.index].CreatePolyline(scale, listener);
        variant = &quads_[component.index];
        break;
      case ComponentType::kCubic:
        cubics_[component.index].CreatePolyline(scale, listener);
        variant = &cubics_[component.index];
        break;
      case ComponentType::kContour:
        if (component_i == components_.size() - 1) {
          // If the last component is a contour, that means it's an empty
          // contour, so skip it.
          continue;
        }
        listener.EndContour(variant);
        // Find the next non-contour component, if any. this is used to
        // compute the starting direction of the contour.
        std::optional<PathComponentVariant> next_variant = std::nullopt;
        for (auto i = component_i + 1; i < components_.size(); i++) {
          auto component_variant = GetPathComponent(i);
          if (!std::holds_alternative<std::monostate>(component_variant)) {
            next_variant = component_variant;
            break;
          }
        }
        const auto& contour = contours_[component.index];
        listener.StartCountour(contour, next_variant);
        listener.AddPoint(contour.destination);
        break;
    }
  }
  listener.EndContour(variant);
}

std::optional<Rect> Path::GetBoundingBox() const {
  auto min_max = GetMinMaxCoveragePoints();
  if (!min_max.has_value()) {
    return std::nullopt;
  }
  auto min = min_max->first;
  auto max = min_max->second;
  const auto difference = max - min;
  return Rect{min.x, min.y, difference.x, difference.y};
}

std::optional<Rect> Path::GetTransformedBoundingBox(
    const Matrix& transform) const {
  auto bounds = GetBoundingBox();
  if (!bounds.has_value()) {
    return std::nullopt;
  }
  return bounds->TransformBounds(transform);
}

std::optional<std::pair<Point, Point>> Path::GetMinMaxCoveragePoints() const {
  if (linears_.empty() && quads_.empty() && cubics_.empty()) {
    return std::nullopt;
  }

  std::optional<Point> min, max;

  auto clamp = [&min, &max](const Point& point) {
    if (min.has_value()) {
      min = min->Min(point);
    } else {
      min = point;
    }

    if (max.has_value()) {
      max = max->Max(point);
    } else {
      max = point;
    }
  };

  for (const auto& linear : linears_) {
    clamp(linear.p1);
    clamp(linear.p2);
  }

  for (const auto& quad : quads_) {
    for (const Point& point : quad.Extrema()) {
      clamp(point);
    }
  }

  for (const auto& cubic : cubics_) {
    for (const Point& point : cubic.Extrema()) {
      clamp(point);
    }
  }

  if (!min.has_value() || !max.has_value()) {
    return std::nullopt;
  }

  return std::make_pair(min.value(), max.value());
}

}  // namespace impeller
