[1mdiff --git a/flow/diff_context.cc b/flow/diff_context.cc[m
[1mindex c9782594af..a661026ec6 100644[m
[1m--- a/flow/diff_context.cc[m
[1m+++ b/flow/diff_context.cc[m
[36m@@ -48,6 +48,10 @@[m [mvoid DiffContext::PushTransform(const SkMatrix& transform) {[m
   state_.transform.preConcat(transform);[m
 }[m
 [m
[32m+[m[32mvoid DiffContext::PushOffset(float dx, float dy) {[m
[32m+[m[32m  state_.transform.preTranslate(dx, dy);[m
[32m+[m[32m}[m
[32m+[m
 void DiffContext::SetTransform(const SkMatrix& transform) {[m
   state_.transform_override = transform;[m
 }[m
[1mdiff --git a/flow/diff_context.h b/flow/diff_context.h[m
[1mindex b57df7a946..033cdb9e2f 100644[m
[1m--- a/flow/diff_context.h[m
[1m+++ b/flow/diff_context.h[m
[36m@@ -71,6 +71,9 @@[m [mclass DiffContext {[m
   // Pushes additional transform for current subtree[m
   void PushTransform(const SkMatrix& transform);[m
 [m
[32m+[m[32m  // Push offset transform for current subtree[m
[32m+[m[32m  void PushOffset(float dx, float dy);[m
[32m+[m
   // Pushes cull rect for current subtree[m
   bool PushCullRect(const SkRect& clip);[m
 [m
[1mdiff --git a/flow/embedded_views.h b/flow/embedded_views.h[m
[1mindex f62e4e86fe..a239e080b1 100644[m
[1m--- a/flow/embedded_views.h[m
[1m+++ b/flow/embedded_views.h[m
[36m@@ -133,6 +133,7 @@[m [mclass MutatorsStack {[m
   void PushClipPath(const SkPath& path);[m
   void PushTransform(const SkMatrix& matrix);[m
   void PushOpacity(const int& alpha);[m
[32m+[m[32m  void PushOffset(float dx, float dy);[m
 [m
   // Removes the `Mutator` on the top of the stack[m
   // and destroys it.[m
[1mdiff --git a/flow/layers/transform_layer_unittests.cc b/flow/layers/transform_layer_unittests.cc[m
[1mdeleted file mode 100644[m
[1mindex 345a1051f3..0000000000[m
[1m--- a/flow/layers/transform_layer_unittests.cc[m
[1m+++ /dev/null[m
[36m@@ -1,335 +0,0 @@[m
[31m-// Copyright 2013 The Flutter Authors. All rights reserved.[m
[31m-// Use of this source code is governed by a BSD-style license that can be[m
[31m-// found in the LICENSE file.[m
[31m-[m
[31m-#include "flutter/flow/layers/transform_layer.h"[m
[31m-[m
[31m-#include "flutter/flow/testing/diff_context_test.h"[m
[31m-#include "flutter/flow/testing/layer_test.h"[m
[31m-#include "flutter/flow/testing/mock_layer.h"[m
[31m-#include "flutter/fml/macros.h"[m
[31m-#include "flutter/testing/mock_canvas.h"[m
[31m-[m
[31m-namespace flutter {[m
[31m-namespace testing {[m
[31m-[m
[31m-using TransformLayerTest = LayerTest;[m
[31m-[m
[31m-#ifndef NDEBUG[m
[31m-TEST_F(TransformLayerTest, PaintingEmptyLayerDies) {[m
[31m-  auto layer = std::make_shared<TransformLayer>(SkMatrix());  // identity[m
[31m-[m
[31m-  layer->Preroll(preroll_context(), SkMatrix());[m
[31m-  EXPECT_EQ(layer->paint_bounds(), SkRect::MakeEmpty());[m
[31m-  EXPECT_FALSE(layer->needs_painting(paint_context()));[m
[31m-[m
[31m-  EXPECT_DEATH_IF_SUPPORTED(layer->Paint(paint_context()),[m
[31m-                            "needs_painting\\(context\\)");[m
[31m-}[m
[31m-[m
[31m-TEST_F(TransformLayerTest, PaintBeforePrerollDies) {[m
[31m-  SkPath child_path;[m
[31m-  child_path.addRect(5.0f, 6.0f, 20.5f, 21.5f);[m
[31m-  auto mock_layer = std::make_shared<MockLayer>(child_path, SkPaint());[m
[31m-  auto layer = std::make_shared<TransformLayer>(SkMatrix());  // identity[m
[31m-  layer->Add(mock_layer);[m
[31m-[m
[31m-  EXPECT_DEATH_IF_SUPPORTED(layer->Paint(paint_context()),[m
[31m-                            "needs_painting\\(context\\)");[m
[31m-}[m
[31m-#endif[m
[31m-[m
[31m-TEST_F(TransformLayerTest, Identity) {[m
[31m-  SkPath child_path;[m
[31m-  child_path.addRect(5.0f, 6.0f, 20.5f, 21.5f);[m
[31m-  SkRect cull_rect = SkRect::MakeXYWH(2.0f, 2.0f, 14.0f, 14.0f);[m
[31m-  auto mock_layer = std::make_shared<MockLayer>(child_path, SkPaint());[m
[31m-  auto layer = std::make_shared<TransformLayer>(SkMatrix());  // identity[m
[31m-  layer->Add(mock_layer);[m
[31m-[m
[31m-  preroll_context()->cull_rect = cull_rect;[m
[31m-  layer->Preroll(preroll_context(), SkMatrix());[m
[31m-  EXPECT_EQ(mock_layer->paint_bounds(), child_path.getBounds());[m
[31m-  EXPECT_EQ(layer->paint_bounds(), mock_layer->paint_bounds());[m
[31m-  EXPECT_TRUE(mock_layer->needs_painting(paint_context()));[m
[31m-  EXPECT_TRUE(layer->needs_painting(paint_context()));[m
[31m-  EXPECT_EQ(mock_layer->parent_matrix(), SkMatrix());  // identity[m
[31m-  EXPECT_EQ(mock_layer->parent_cull_rect(), cull_rect);[m
[31m-  EXPECT_EQ(mock_layer->parent_mutators(), std::vector({Mutator(SkMatrix())}));[m
[31m-[m
[31m-  layer->Paint(paint_context());[m
[31m-  EXPECT_EQ(mock_canvas().draw_calls(),[m
[31m-            std::vector({MockCanvas::DrawCall{[m
[31m-                0, MockCanvas::DrawPathData{child_path, SkPaint()}}}));[m
[31m-}[m
[31m-[m
[31m-TEST_F(TransformLayerTest, Simple) {[m
[31m-  SkPath child_path;[m
[31m-  child_path.addRect(5.0f, 6.0f, 20.5f, 21.5f);[m
[31m-  SkRect cull_rect = SkRect::MakeXYWH(2.0f, 2.0f, 14.0f, 14.0f);[m
[31m-  SkMatrix initial_transform = SkMatrix::Translate(-0.5f, -0.5f);[m
[31m-  SkMatrix layer_transform = SkMatrix::Translate(2.5f, 2.5f);[m
[31m-  SkMatrix inverse_layer_transform;[m
[31m-  EXPECT_TRUE(layer_transform.invert(&inverse_layer_transform));[m
[31m-[m
[31m-  auto mock_layer = std::make_shared<MockLayer>(child_path, SkPaint());[m
[31m-  auto layer = std::make_shared<TransformLayer>(layer_transform);[m
[31m-  layer->Add(mock_layer);[m
[31m-[m
[31m-  preroll_context()->cull_rect = cull_rect;[m
[31m-  layer->Preroll(preroll_context(), initial_transform);[m
[31m-  EXPECT_EQ(mock_layer->paint_bounds(), child_path.getBounds());[m
[31m-  EXPECT_EQ(layer->paint_bounds(),[m
[31m-            layer_transform.mapRect(mock_layer->paint_bounds()));[m
[31m-  EXPECT_TRUE(mock_layer->needs_painting(paint_context()));[m
[31m-  EXPECT_TRUE(layer->needs_painting(paint_context()));[m
[31m-  EXPECT_EQ(mock_layer->parent_matrix(),[m
[31m-            SkMatrix::Concat(initial_transform, layer_transform));[m
[31m-  EXPECT_EQ(mock_layer->parent_cull_rect(),[m
[31m-            inverse_layer_transform.mapRect(cull_rect));[m
[31m-  EXPECT_EQ(mock_layer->parent_mutators(),[m
[31m-            std::vector({Mutator(layer_transform)}));[m
[31m-[m
[31m-  layer->Paint(paint_context());[m
[31m-  EXPECT_EQ([m
[31m-      mock_canvas().draw_calls(),[m
[31m-      std::vector({MockCanvas::DrawCall{0, MockCanvas::SaveData{1}},[m
[31m-                   MockCanvas::DrawCall{[m
[31m-                       1, MockCanvas::ConcatMatrixData{SkM44(layer_transform)}},[m
[31m-                   MockCanvas::DrawCall{[m
[31m-                       1, MockCanvas::DrawPathData{child_path, SkPaint()}},[m
[31m-                   MockCanvas::DrawCall{1, MockCanvas::RestoreData{0}}}));[m
[31m-}[m
[31m-[m
[31m-TEST_F(TransformLayerTest, Nested) {[m
[31m-  SkPath child_path;[m
[31m-  child_path.addRect(5.0f, 6.0f, 20.5f, 21.5f);[m
[31m-  SkRect cull_rect = SkRect::MakeXYWH(2.0f, 2.0f, 14.0f, 14.0f);[m
[31m-  SkMatrix initial_transform = SkMatrix::Translate(-0.5f, -0.5f);[m
[31m-  SkMatrix layer1_transform = SkMatrix::Translate(2.5f, 2.5f);[m
[31m-  SkMatrix layer2_transform = SkMatrix::Translate(2.5f, 2.5f);[m
[31m-  SkMatrix inverse_layer1_transform, inverse_layer2_transform;[m
[31m-  EXPECT_TRUE(layer1_transform.invert(&inverse_layer1_transform));[m
[31m-  EXPECT_TRUE(layer2_transform.invert(&inverse_layer2_transform));[m
[31m-[m
[31m-  auto mock_layer = std::make_shared<MockLayer>(child_path, SkPaint());[m
[31m-  auto layer1 = std::make_shared<TransformLayer>(layer1_transform);[m
[31m-  auto layer2 = std::make_shared<TransformLayer>(layer2_transform);[m
[31m-  layer1->Add(layer2);[m
[31m-  layer2->Add(mock_layer);[m
[31m-[m
[31m-  preroll_context()->cull_rect = cull_rect;[m
[31m-  layer1->Preroll(preroll_context(), initial_transform);[m
[31m-  EXPECT_EQ(mock_layer->paint_bounds(), child_path.getBounds());[m
[31m-  EXPECT_EQ(layer2->paint_bounds(),[m
[31m-            layer2_transform.mapRect(mock_layer->paint_bounds()));[m
[31m-  EXPECT_EQ(layer1->paint_bounds(),[m
[31m-            layer1_transform.mapRect(layer2->paint_bounds()));[m
[31m-  EXPECT_TRUE(mock_layer->needs_painting(paint_context()));[m
[31m-  EXPECT_TRUE(layer2->needs_painting(paint_context()));[m
[31m-  EXPECT_TRUE(layer1->needs_painting(paint_context()));[m
[31m-  EXPECT_EQ([m
[31m-      mock_layer->parent_matrix(),[m
[31m-      SkMatrix::Concat(SkMatrix::Concat(initial_transform, layer1_transform),[m
[31m-                       layer2_transform));[m
[31m-  EXPECT_EQ(mock_layer->parent_cull_rect(),[m
[31m-            inverse_layer2_transform.mapRect([m
[31m-                inverse_layer1_transform.mapRect(cull_rect)));[m
[31m-  EXPECT_EQ([m
[31m-      mock_layer->parent_mutators(),[m
[31m-      std::vector({Mutator(layer2_transform), Mutator(layer1_transform)}));[m
[31m-[m
[31m-  layer1->Paint(paint_context());[m
[31m-  EXPECT_EQ(mock_canvas().draw_calls(),[m
[31m-            std::vector([m
[31m-                {MockCanvas::DrawCall{0, MockCanvas::SaveData{1}},[m
[31m-                 MockCanvas::DrawCall{[m
[31m-                     1, MockCanvas::ConcatMatrixData{SkM44(layer1_transform)}},[m
[31m-                 MockCanvas::DrawCall{1, MockCanvas::SaveData{2}},[m
[31m-                 MockCanvas::DrawCall{[m
[31m-                     2, MockCanvas::ConcatMatrixData{SkM44(layer2_transform)}},[m
[31m-                 MockCanvas::DrawCall{[m
[31m-                     2, MockCanvas::DrawPathData{child_path, SkPaint()}},[m
[31m-                 MockCanvas::DrawCall{2, MockCanvas::RestoreData{1}},[m
[31m-                 MockCanvas::DrawCall{1, MockCanvas::RestoreData{0}}}));[m
[31m-}[m
[31m-[m
[31m-TEST_F(TransformLayerTest, NestedSeparated) {[m
[31m-  SkPath child_path;[m
[31m-  child_path.addRect(5.0f, 6.0f, 20.5f, 21.5f);[m
[31m-  SkRect cull_rect = SkRect::MakeXYWH(2.0f, 2.0f, 14.0f, 14.0f);[m
[31m-  SkMatrix initial_transform = SkMatrix::Translate(-0.5f, -0.5f);[m
[31m-  SkMatrix layer1_transform = SkMatrix::Translate(2.5f, 2.5f);[m
[31m-  SkMatrix layer2_transform = SkMatrix::Translate(2.5f, 2.5f);[m
[31m-  SkMatrix inverse_layer1_transform, inverse_layer2_transform;[m
[31m-  EXPECT_TRUE(layer1_transform.invert(&inverse_layer1_transform));[m
[31m-  EXPECT_TRUE(layer2_transform.invert(&inverse_layer2_transform));[m
[31m-[m
[31m-  auto mock_layer1 =[m
[31m-      std::make_shared<MockLayer>(child_path, SkPaint(SkColors::kBlue));[m
[31m-  auto mock_layer2 =[m
[31m-      std::make_shared<MockLayer>(child_path, SkPaint(SkColors::kGreen));[m
[31m-  auto layer1 = std::make_shared<TransformLayer>(layer1_transform);[m
[31m-  auto layer2 = std::make_shared<TransformLayer>(layer2_transform);[m
[31m-  layer1->Add(mock_layer1);[m
[31m-  layer1->Add(layer2);[m
[31m-  layer2->Add(mock_layer2);[m
[31m-[m
[31m-  preroll_context()->cull_rect = cull_rect;[m
[31m-  layer1->Preroll(preroll_context(), initial_transform);[m
[31m-  SkRect expected_layer1_bounds = layer2->paint_bounds();[m
[31m-  expected_layer1_bounds.join(mock_layer1->paint_bounds());[m
[31m-  layer1_transform.mapRect(&expected_layer1_bounds);[m
[31m-  EXPECT_EQ(mock_layer2->paint_bounds(), child_path.getBounds());[m
[31m-  EXPECT_EQ(layer2->paint_bounds(),[m
[31m-            layer2_transform.mapRect(mock_layer2->paint_bounds()));[m
[31m-  EXPECT_EQ(mock_layer1->paint_bounds(), child_path.getBounds());[m
[31m-  EXPECT_EQ(layer1->paint_bounds(), expected_layer1_bounds);[m
[31m-  EXPECT_TRUE(mock_layer2->needs_painting(paint_context()));[m
[31m-  EXPECT_TRUE(layer2->needs_painting(paint_context()));[m
[31m-  EXPECT_TRUE(mock_layer1->needs_painting(paint_context()));[m
[31m-  EXPECT_TRUE(layer1->needs_painting(paint_context()));[m
[31m-  EXPECT_EQ(mock_layer1->parent_matrix(),[m
[31m-            SkMatrix::Concat(initial_transform, layer1_transform));[m
[31m-  EXPECT_EQ([m
[31m-      mock_layer2->parent_matrix(),[m
[31m-      SkMatrix::Concat(SkMatrix::Concat(initial_transform, layer1_transform),[m
[31m-                       layer2_transform));[m
[31m-  EXPECT_EQ(mock_layer1->parent_cull_rect(),[m
[31m-            inverse_layer1_transform.mapRect(cull_rect));[m
[31m-  EXPECT_EQ(mock_layer2->parent_cull_rect(),[m
[31m-            inverse_layer2_transform.mapRect([m
[31m-                inverse_layer1_transform.mapRect(cull_rect)));[m
[31m-  EXPECT_EQ(mock_layer1->parent_mutators(),[m
[31m-            std::vector({Mutator(layer1_transform)}));[m
[31m-  EXPECT_EQ([m
[31m-      mock_layer2->parent_mutators(),[m
[31m-      std::vector({Mutator(layer2_transform), Mutator(layer1_transform)}));[m
[31m-[m
[31m-  layer1->Paint(paint_context());[m
[31m-  EXPECT_EQ(mock_canvas().draw_calls(),[m
[31m-            std::vector([m
[31m-                {MockCanvas::DrawCall{0, MockCanvas::SaveData{1}},[m
[31m-                 MockCanvas::DrawCall{[m
[31m-                     1, MockCanvas::ConcatMatrixData{SkM44(layer1_transform)}},[m
[31m-                 MockCanvas::DrawCall{[m
[31m-                     1, MockCanvas::DrawPathData{child_path,[m
[31m-                                                 SkPaint(SkColors::kBlue)}},[m
[31m-                 MockCanvas::DrawCall{1, MockCanvas::SaveData{2}},[m
[31m-                 MockCanvas::DrawCall{[m
[31m-                     2, MockCanvas::ConcatMatrixData{SkM44(layer2_transform)}},[m
[31m-                 MockCanvas::DrawCall{[m
[31m-                     2, MockCanvas::DrawPathData{child_path,[m
[31m-                                                 SkPaint(SkColors::kGreen)}},[m
[31m-                 MockCanvas::DrawCall{2, MockCanvas::RestoreData{1}},[m
[31m-                 MockCanvas::DrawCall{1, MockCanvas::RestoreData{0}}}));[m
[31m-}[m
[31m-[m
[31m-using TransformLayerLayerDiffTest = DiffContextTest;[m
[31m-[m
[31m-TEST_F(TransformLayerLayerDiffTest, Transform) {[m
[31m-  auto path1 = SkPath().addRect(SkRect::MakeLTRB(0, 0, 50, 50));[m
[31m-  auto m1 = std::make_shared<MockLayer>(path1);[m
[31m-[m
[31m-  auto transform1 =[m
[31m-      std::make_shared<TransformLayer>(SkMatrix::Translate(10, 10));[m
[31m-  transform1->Add(m1);[m
[31m-[m
[31m-  MockLayerTree t1;[m
[31m-  t1.root()->Add(transform1);[m
[31m-[m
[31m-  auto damage = DiffLayerTree(t1, MockLayerTree());[m
[31m-  EXPECT_EQ(damage.frame_damage, SkIRect::MakeLTRB(10, 10, 60, 60));[m
[31m-[m
[31m-  auto transform2 =[m
[31m-      std::make_shared<TransformLayer>(SkMatrix::Translate(20, 20));[m
[31m-  transform2->Add(m1);[m
[31m-  transform2->AssignOldLayer(transform1.get());[m
[31m-[m
[31m-  MockLayerTree t2;[m
[31m-  t2.root()->Add(transform2);[m
[31m-[m
[31m-  damage = DiffLayerTree(t2, t1);[m
[31m-  EXPECT_EQ(damage.frame_damage, SkIRect::MakeLTRB(10, 10, 70, 70));[m
[31m-[m
[31m-  auto transform3 =[m
[31m-      std::make_shared<TransformLayer>(SkMatrix::Translate(20, 20));[m
[31m-  transform3->Add(m1);[m
[31m-  transform3->AssignOldLayer(transform2.get());[m
[31m-[m
[31m-  MockLayerTree t3;[m
[31m-  t3.root()->Add(transform3);[m
[31m-[m
[31m-  damage = DiffLayerTree(t3, t2);[m
[31m-  EXPECT_EQ(damage.frame_damage, SkIRect::MakeEmpty());[m
[31m-}[m
[31m-[m
[31m-TEST_F(TransformLayerLayerDiffTest, TransformNested) {[m
[31m-  auto path1 = SkPath().addRect(SkRect::MakeLTRB(0, 0, 50, 50));[m
[31m-  auto m1 = CreateContainerLayer(std::make_shared<MockLayer>(path1));[m
[31m-  auto m2 = CreateContainerLayer(std::make_shared<MockLayer>(path1));[m
[31m-  auto m3 = CreateContainerLayer(std::make_shared<MockLayer>(path1));[m
[31m-[m
[31m-  auto transform1 = std::make_shared<TransformLayer>(SkMatrix::Scale(2.0, 2.0));[m
[31m-[m
[31m-  auto transform1_1 =[m
[31m-      std::make_shared<TransformLayer>(SkMatrix::Translate(10, 10));[m
[31m-  transform1_1->Add(m1);[m
[31m-  transform1->Add(transform1_1);[m
[31m-[m
[31m-  auto transform1_2 =[m
[31m-      std::make_shared<TransformLayer>(SkMatrix::Translate(100, 100));[m
[31m-  transform1_2->Add(m2);[m
[31m-  transform1->Add(transform1_2);[m
[31m-[m
[31m-  auto transform1_3 =[m
[31m-      std::make_shared<TransformLayer>(SkMatrix::Translate(200, 200));[m
[31m-  transform1_3->Add(m3);[m
[31m-  transform1->Add(transform1_3);[m
[31m-[m
[31m-  MockLayerTree l1;[m
[31m-  l1.root()->Add(transform1);[m
[31m-[m
[31m-  auto damage = DiffLayerTree(l1, MockLayerTree());[m
[31m-  EXPECT_EQ(damage.frame_damage, SkIRect::MakeLTRB(20, 20, 500, 500));[m
[31m-[m
[31m-  auto transform2 = std::make_shared<TransformLayer>(SkMatrix::Scale(2.0, 2.0));[m
[31m-[m
[31m-  auto transform2_1 =[m
[31m-      std::make_shared<TransformLayer>(SkMatrix::Translate(10, 10));[m
[31m-  transform2_1->Add(m1);[m
[31m-  transform2_1->AssignOldLayer(transform1_1.get());[m
[31m-  transform2->Add(transform2_1);[m
[31m-[m
[31m-  // Offset 1px from transform1_2 so that they're not the same[m
[31m-  auto transform2_2 =[m
[31m-      std::make_shared<TransformLayer>(SkMatrix::Translate(100, 101));[m
[31m-  transform2_2->Add(m2);[m
[31m-  transform2_2->AssignOldLayer(transform1_2.get());[m
[31m-  transform2->Add(transform2_2);[m
[31m-[m
[31m-  auto transform2_3 =[m
[31m-      std::make_shared<TransformLayer>(SkMatrix::Translate(200, 200));[m
[31m-  transform2_3->Add(m3);[m
[31m-  transform2_3->AssignOldLayer(transform1_3.get());[m
[31m-  transform2->Add(transform2_3);[m
[31m-[m
[31m-  MockLayerTree l2;[m
[31m-  l2.root()->Add(transform2);[m
[31m-[m
[31m-  damage = DiffLayerTree(l2, l1);[m
[31m-[m
[31m-  // transform2 has not transform1 assigned as old layer, so it should be[m
[31m-  // invalidated completely[m
[31m-  EXPECT_EQ(damage.frame_damage, SkIRect::MakeLTRB(20, 20, 500, 500));[m
[31m-[m
[31m-  // now diff the tree properly, the only difference being transform2_2 and[m
[31m-  // transform_2_1[m
[31m-  transform2->AssignOldLayer(transform1.get());[m
[31m-  damage = DiffLayerTree(l2, l1);[m
[31m-[m
[31m-  EXPECT_EQ(damage.frame_damage, SkIRect::MakeLTRB(200, 200, 300, 302));[m
[31m-}[m
[31m-[m
[31m-}  // namespace testing[m
[31m-}  // namespace flutter[m
