// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Foundation/Foundation.h>
#include <QuartzCore/QuartzCore.h>

#include "flutter/shell/gpu/gpu_surface_metal_impeller.h"
#include "gtest/gtest.h"
#include "impeller/entity/mtl/entity_shaders.h"
#include "impeller/entity/mtl/framebuffer_blend_shaders.h"
#include "impeller/entity/mtl/modern_shaders.h"
#include "impeller/renderer/backend/metal/context_mtl.h"

namespace flutter {
namespace testing {

class TestGPUSurfaceMetalDelegate : public GPUSurfaceMetalDelegate {
 public:
  TestGPUSurfaceMetalDelegate() : GPUSurfaceMetalDelegate(MTLRenderTargetType::kCAMetalLayer) {
    layer_ = [[CAMetalLayer alloc] init];
  }

  ~TestGPUSurfaceMetalDelegate() = default;

  GPUCAMetalLayerHandle GetCAMetalLayer(const SkISize& frame_info) const override {
    layer_.drawableSize = CGSizeMake(frame_info.width(), frame_info.height());
    return (__bridge GPUCAMetalLayerHandle)(layer_);
  }

  bool PresentDrawable(GrMTLHandle drawable) const override { return true; }

  GPUMTLTextureInfo GetMTLTexture(const SkISize& frame_info) const override { return {}; }

  bool PresentTexture(GPUMTLTextureInfo texture) const override { return true; }

  bool AllowsDrawingWhenGpuDisabled() const override { return true; }

  void SetDevice() { layer_.device = ::MTLCreateSystemDefaultDevice(); }

 private:
  CAMetalLayer* layer_ = nil;
};

static std::shared_ptr<impeller::ContextMTL> CreateImpellerContext() {
  std::vector<std::shared_ptr<fml::Mapping>> shader_mappings = {
      std::make_shared<fml::NonOwnedMapping>(impeller_entity_shaders_data,
                                             impeller_entity_shaders_length),
      std::make_shared<fml::NonOwnedMapping>(impeller_modern_shaders_data,
                                             impeller_modern_shaders_length),
      std::make_shared<fml::NonOwnedMapping>(impeller_framebuffer_blend_shaders_data,
                                             impeller_framebuffer_blend_shaders_length),
  };
  auto sync_switch = std::make_shared<fml::SyncSwitch>(false);
  return impeller::ContextMTL::Create(shader_mappings, sync_switch, "Impeller Library");
}

TEST(GPUSurfaceMetalImpeller, InvalidImpellerContextCreatesCausesSurfaceToBeInvalid) {
  auto delegate = std::make_shared<TestGPUSurfaceMetalDelegate>();
  auto surface = std::make_shared<GPUSurfaceMetalImpeller>(delegate.get(), nullptr);

  ASSERT_FALSE(surface->IsValid());
}

TEST(GPUSurfaceMetalImpeller, CanCreateValidSurface) {
  auto delegate = std::make_shared<TestGPUSurfaceMetalDelegate>();
  auto surface = std::make_shared<GPUSurfaceMetalImpeller>(delegate.get(), CreateImpellerContext());

  ASSERT_TRUE(surface->IsValid());
}

TEST(GPUSurfaceMetalImpeller, AcquireFrameFromCAMetalLayerNullChecksDrawable) {
  auto delegate = std::make_shared<TestGPUSurfaceMetalDelegate>();
  std::shared_ptr<Surface> surface =
      std::make_shared<GPUSurfaceMetalImpeller>(delegate.get(), CreateImpellerContext());

  ASSERT_TRUE(surface->IsValid());

  auto frame = surface->AcquireFrame(SkISize::Make(100, 100));
  ASSERT_EQ(frame, nullptr);
}

TEST(GPUSurfaceMetalImpeller, AcquireFrameFromCAMetalLayerDoesNotRetainThis) {
  auto delegate = std::make_shared<TestGPUSurfaceMetalDelegate>();
  delegate->SetDevice();
  std::unique_ptr<Surface> surface =
      std::make_unique<GPUSurfaceMetalImpeller>(delegate.get(), CreateImpellerContext());

  ASSERT_TRUE(surface->IsValid());

  auto frame = surface->AcquireFrame(SkISize::Make(100, 100));
  ASSERT_TRUE(frame);

  // Simulate a rasterizer teardown, e.g. due to going to the background.
  surface.reset();

  ASSERT_TRUE(frame->Submit());
}

TEST(GPUSurfaceMetalImpeller, ResetHostBufferBasedOnFrameBoundary) {
  auto delegate = std::make_shared<TestGPUSurfaceMetalDelegate>();
  delegate->SetDevice();

  auto context = CreateImpellerContext();
  std::unique_ptr<Surface> surface =
      std::make_unique<GPUSurfaceMetalImpeller>(delegate.get(), CreateImpellerContext());

  ASSERT_TRUE(surface->IsValid());

  auto& host_buffer = surface->GetAiksContext()->GetContentContext().GetTransientsBuffer();

#if EXPERIMENTAL_CANVAS
  EXPECT_EQ(host_buffer.GetStateForTest().current_frame, 1u);
#else
  EXPECT_EQ(host_buffer.GetStateForTest().current_frame, 0u);
#endif  // EXPERIMENTAL_CANVAS

  auto frame = surface->AcquireFrame(SkISize::Make(100, 100));
  frame->set_submit_info({.frame_boundary = false});

  ASSERT_TRUE(frame->Submit());
#if EXPERIMENTAL_CANVAS
  EXPECT_EQ(host_buffer.GetStateForTest().current_frame, 1u);
#else
  EXPECT_EQ(host_buffer.GetStateForTest().current_frame, 0u);
#endif  // EXPERIMENTAL_CANVAS

  frame = surface->AcquireFrame(SkISize::Make(100, 100));
  frame->set_submit_info({.frame_boundary = true});

  ASSERT_TRUE(frame->Submit());
#if EXPERIMENTAL_CANVAS
  EXPECT_EQ(host_buffer.GetStateForTest().current_frame, 2u);
#else
  EXPECT_EQ(host_buffer.GetStateForTest().current_frame, 1u);
#endif  // EXPERIMENTAL_CANVAS
}

}  // namespace testing
}  // namespace flutter
