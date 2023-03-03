// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/aiks/paint_pass_delegate.h"
#include <iostream>

#include "impeller/entity/contents/contents.h"
#include "impeller/entity/contents/texture_contents.h"
#include "impeller/entity/entity_pass.h"
#include "impeller/geometry/path_builder.h"

namespace impeller {

PaintPassDelegate::PaintPassDelegate(Paint paint, std::optional<Rect> coverage)
    : paint_(std::move(paint)), coverage_(coverage) {}

// |EntityPassDelgate|
PaintPassDelegate::~PaintPassDelegate() = default;

// |EntityPassDelgate|
std::optional<Rect> PaintPassDelegate::GetCoverageRect() {
  return coverage_;
}

// |EntityPassDelgate|
bool PaintPassDelegate::CanElide() {
  return paint_.blend_mode == BlendMode::kDestination;
}

// |EntityPassDelgate|
bool PaintPassDelegate::CanCollapseIntoParentPass(EntityPass* pass) {
  if (paint_.blend_mode != BlendMode::kSourceOver) {
    return false;
  }
  if (paint_.color.alpha >= 1.0) {
    return true;
  }

  bool can_collapse = true;
  std::vector<Rect> all_bounds;
  pass->IterateAllEntities([&](Entity& entity) -> bool {
    auto contents = entity.GetContents();
    if (!contents->CanApplyOpacity()) {
      can_collapse = false;
      return false;
    }
    auto bounds = entity.GetCoverage();
    if (!bounds.has_value()) {
      return true;
    }
    auto bds_value = bounds.value();
    for (const auto& bd : all_bounds) {
      if (bd.Intersection(bds_value).has_value()) {
        can_collapse = false;
        return false;
      }
    }
    all_bounds.push_back(bds_value);
    return true;
  });
  if (!can_collapse) {
    return false;
  }
  pass->IterateAllEntities([&](Entity& entity) -> bool {
    entity.GetContents()->ApplyOpacity(paint_.color.alpha);
    return true;
  });
  return true;
}

// |EntityPassDelgate|
std::shared_ptr<Contents> PaintPassDelegate::CreateContentsForSubpassTarget(
    std::shared_ptr<Texture> target,
    const Matrix& effect_transform) {
  auto contents = TextureContents::MakeRect(Rect::MakeSize(target->GetSize()));
  contents->SetTexture(target);
  contents->SetSourceRect(Rect::MakeSize(target->GetSize()));
  contents->SetOpacity(paint_.color.alpha);
  contents->SetDeferApplyingOpacity(true);
  return paint_.WithFiltersForSubpassTarget(std::move(contents),
                                            effect_transform);
}

}  // namespace impeller
