// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <functional>
#include <memory>
#include <variant>
#include <vector>

#include "flutter/fml/macros.h"
#include "impeller/entity/contents/contents.h"
#include "impeller/geometry/color.h"
#include "impeller/typographer/glyph_atlas.h"
#include "impeller/typographer/text_frame.h"
#include "impeller/entity/contents/text_contents.h"
#include "impeller/entity/contents/color_source_contents.h"

namespace impeller {

class TextColorContents final : public Contents {
 public:
  TextColorContents();

  ~TextColorContents();

  void SetTextContents(std::shared_ptr<TextContents> text_contents);

  void SetColorSourceProc(std::shared_ptr<ColorSourceContents> color_contents);

  void SetTransform(Matrix transform);

  // |Contents|
  std::optional<Rect> GetCoverage(const Entity& entity) const override;

  // |Contents|
  bool Render(const ContentContext& renderer,
              const Entity& entity,
              RenderPass& pass) const override;

 private:
  std::shared_ptr<TextContents> text_contents_;
  std::shared_ptr<ColorSourceContents> color_contents_;
  Matrix transform_;
  FML_DISALLOW_COPY_AND_ASSIGN(TextColorContents);
};

}  // namespace impeller
