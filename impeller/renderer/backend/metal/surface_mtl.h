// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <QuartzCore/CAMetalLayer.h>
#include <memory>

#include "flutter/fml/macros.h"
#include "impeller/geometry/rect.h"
#include "impeller/renderer/context.h"
#include "impeller/renderer/surface.h"

namespace impeller {

class SurfaceMTL final : public Surface {
 public:
#pragma GCC diagnostic push
  // Disable the diagnostic for iOS Simulators. Metal without emulation isn't
  // available prior to iOS 13 and that's what the simulator headers say when
  // support for CAMetalLayer begins. CAMetalLayer is available on iOS 8.0 and
  // above which is well below Flutters support level.
#pragma GCC diagnostic ignored "-Wunguarded-availability-new"
  //----------------------------------------------------------------------------
  /// @brief      Wraps the current drawable of the given Metal layer to create
  ///             a surface Impeller can render to. The surface must be created
  ///             as late as possible and discarded immediately after rendering
  ///             to it.
  ///
  /// @param[in]  context  The context
  /// @param[in]    layer  The layer whose current drawable to wrap to create a
  ///                      surface.
  ///
  /// @return     A pointer to the wrapped surface or null.
  ///
  // static id<CAMetalDrawable> GetMetalDrawableAndValidate(
  //     const std::shared_ptr<Context>& context,
  //     CAMetalLayer* layer);

  // static std::unique_ptr<SurfaceMTL> MakeFromMetalLayerDrawable(
  //     const std::shared_ptr<Context>& context,
  //     id<CAMetalDrawable> drawable,
  //     std::optional<IRect> clip_rect = std::nullopt);

  static std::unique_ptr<SurfaceMTL> MakeFromMetalLayer(
      const std::shared_ptr<Context>& context,
      CAMetalLayer* layer);

  // static std::unique_ptr<SurfaceMTL> MakeFromTexture(
  //     const std::shared_ptr<Context>& context,
  //     id<MTLTexture> texture,
  //     std::optional<IRect> clip_rect,
  //     id<CAMetalDrawable> drawable = nil);
#pragma GCC diagnostic pop

  // |Surface|
  ~SurfaceMTL() override;

  // Returns a Rect defining the area of the surface in device pixels
  IRect coverage() const;

  // |Surface|
  bool Present() const override;

 private:
  std::weak_ptr<Context> context_;
  std::shared_ptr<Texture> resolve_texture_;

  SurfaceMTL(const std::weak_ptr<Context>& context,
             const RenderTarget& target,
             std::shared_ptr<Texture> resolve_texture);

  SurfaceMTL(const SurfaceMTL&) = delete;

  SurfaceMTL& operator=(const SurfaceMTL&) = delete;
};

}  // namespace impeller
