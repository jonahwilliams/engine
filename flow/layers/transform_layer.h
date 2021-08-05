// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_FLOW_LAYERS_TRANSFORM_LAYER_H_
#define FLUTTER_FLOW_LAYERS_TRANSFORM_LAYER_H_

#include "flutter/flow/layers/container_layer.h"

namespace flutter {

// Be careful that SkMatrix's default constructor doesn't initialize the matrix
// at all. Hence |set_transform| must be called with an initialized SkMatrix.
class TransformLayer : public ContainerLayer {
 public:
  TransformLayer(const SkMatrix& transform);

#ifdef FLUTTER_ENABLE_DIFF_CONTEXT

  void Diff(DiffContext* context, const Layer* old_layer) override;

#endif  // FLUTTER_ENABLE_DIFF_CONTEXT

  void Preroll(PrerollContext* context, const SkMatrix& matrix) override;

  void Paint(PaintContext& context) const override;

  void SetScrollTransform(bool value) {
    is_scroll_transform_ = value;
  }

  bool GetScrollTransform() const {
    return is_scroll_transform_;
  }

 private:
  SkMatrix transform_;

  // Indicates that this offset layer is used to position child layers for a scrollable
  // container. If the child layers are complex enough (i.e. not a single picture layer), then
  // this indicates that it is worth raster caching.
  bool is_scroll_transform_;

  bool ConsiderRasterCache() const;

  FML_DISALLOW_COPY_AND_ASSIGN(TransformLayer);
};

}  // namespace flutter

#endif  // FLUTTER_FLOW_LAYERS_TRANSFORM_LAYER_H_
