// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/flow/layers/offset_layer.h"

namespace flutter {

OffsetLayer::OffsetLayer(float dx, float dy)
    : dx_(dx),
      dy_(dy) { }

void OffsetLayer::Diff(DiffContext* context, const Layer* old_layer) {
  DiffContext::AutoSubtreeRestore subtree(context);
  auto* prev = static_cast<const OffsetLayer*>(old_layer);
  if (!context->IsSubtreeDirty()) {
    FML_DCHECK(prev);
    if (dx_ != prev->dx_ || dy_ != prev->dy_) {
      context->MarkSubtreeDirty(context->GetOldLayerPaintRegion(old_layer));
    }
  }

  context->PushOffset(dx_, dy_);
  DiffChildren(context, prev);
  context->SetLayerPaintRegion(this, context->CurrentSubtreeRegion());
}

void OffsetLayer::Preroll(PrerollContext* context, const SkMatrix& matrix) {
  TRACE_EVENT0("flutter", "TransformLayer::Preroll");

  SkMatrix child_matrix = matrix;
  child_matrix.preTranslate(dx_, dy_);
  context->mutators_stack.PushOffset(dx_, dy_);
  SkRect previous_cull_rect = context->cull_rect;

  context->cull_rect.offset(dx_, dy_);

  SkRect child_paint_bounds = SkRect::MakeEmpty();
  PrerollChildren(context, child_matrix, &child_paint_bounds);

  child_paint_bounds.offset(dx_, dy_);
  set_paint_bounds(child_paint_bounds);

  context->cull_rect = previous_cull_rect;
  context->mutators_stack.Pop();
}

void OffsetLayer::Paint(PaintContext& context) const {
  TRACE_EVENT0("flutter", "OffsetLayer::Paint");
  FML_DCHECK(needs_painting(context));

  SkAutoCanvasRestore save(context.internal_nodes_canvas, true);
  context.internal_nodes_canvas->translate(dx_, dy_);

  PaintChildren(context);
}

}  // namespace flutter
