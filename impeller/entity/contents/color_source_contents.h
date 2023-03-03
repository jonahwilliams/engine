// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "flutter/fml/macros.h"
#include "impeller/entity/contents/contents.h"
#include "impeller/entity/geometry.h"
#include "impeller/geometry/matrix.h"
#include "impeller/geometry/path.h"

namespace impeller {

class ColorSourceContents : public Contents {
 public:
  ColorSourceContents();

  ~ColorSourceContents() override;

  void SetGeometry(std::shared_ptr<Geometry> geometry);

  void SetEffectTransform(Matrix matrix);

  void SetAlpha(Scalar alpha);

  // |Contents|
  std::optional<Rect> GetCoverage(const Entity& entity) const override;

  // |Contents|
  bool ShouldRender(const Entity& entity,
                    const std::optional<Rect>& stencil_coverage) const override;

  bool CanApplyOpacity() const override {
    return true;
  }

  void ApplyOpacity(Scalar opacity) override {
    SetAlpha(opacity * GetAlpha());
  }

 protected:
  const std::shared_ptr<Geometry>& GetGeometry() const;

  const Matrix& GetInverseMatrix() const;

  Scalar GetAlpha() const;

 private:
  std::shared_ptr<Geometry> geometry_;
  Matrix inverse_matrix_;
  Scalar alpha_ = 1.0;

  FML_DISALLOW_COPY_AND_ASSIGN(ColorSourceContents);
};

}  // namespace impeller
