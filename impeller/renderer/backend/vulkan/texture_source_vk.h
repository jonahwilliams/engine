// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_RENDERER_BACKEND_VULKAN_TEXTURE_SOURCE_VK_H_
#define FLUTTER_IMPELLER_RENDERER_BACKEND_VULKAN_TEXTURE_SOURCE_VK_H_

#include "flutter/fml/status.h"
#include "impeller/base/thread.h"
#include "impeller/core/texture_descriptor.h"
#include "impeller/renderer/backend/vulkan/barrier_vk.h"
#include "impeller/renderer/backend/vulkan/formats_vk.h"
#include "impeller/renderer/backend/vulkan/shared_object_vk.h"
#include "impeller/renderer/backend/vulkan/vk.h"
#include "vulkan/vulkan_handles.hpp"

namespace impeller {

/// Abstract base class that represents a vkImage and an vkImageView.
///
/// This is intended to be used with an impeller::TextureVK. Example
/// implementations represent swapchain images or uploaded textures.
class TextureSourceVK {
 public:
  virtual ~TextureSourceVK();

  const TextureDescriptor& GetTextureDescriptor() const;

  virtual vk::Image GetImage() const = 0;

  /// @brief Retrieve the image view used for sampling/blitting/compute with
  ///        this texture source.
  virtual vk::ImageView GetImageView() const = 0;

  /// @brief Retrieve the image view used for render target attachments
  ///        with this texture source.
  ///
  /// ImageViews used as render target attachments cannot have any mip levels.
  /// In cases where we want to generate mipmaps with the result of this
  /// texture, we need to create multiple image views.
  virtual vk::ImageView GetRenderTargetView() const = 0;

  /// Whether or not this is a swapchain image.
  virtual bool IsSwapchainImage() const = 0;

 protected:
  const TextureDescriptor desc_;

  explicit TextureSourceVK(TextureDescriptor desc);
};

}  // namespace impeller

#endif  // FLUTTER_IMPELLER_RENDERER_BACKEND_VULKAN_TEXTURE_SOURCE_VK_H_
