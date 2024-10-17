// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/renderer/context.h"

#include <utility>

namespace impeller {

Context::~Context() = default;

Context::Context() = default;

bool Context::UpdateOffscreenLayerPixelFormat(PixelFormat format) {
  return false;
}

void Context::SubmitCommandBuffer(
    std::shared_ptr<CommandBuffer> command_buffer) {
  GetCommandQueue()->Submit({std::move(command_buffer)});
}

void Context::SubmitCommandBuffer(
    std::vector<std::shared_ptr<CommandBuffer>> command_buffers) {
  GetCommandQueue()->Submit(command_buffers);
}

void Context::FlushCommandBuffers() {}

}  // namespace impeller
