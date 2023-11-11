// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/entity/contents/frame_allocator.h"
#include "fml/logging.h"

namespace impeller {

PerFrameAllocator::PerFrameAllocator() {
  data_ = static_cast<uint8_t*>(::malloc(1.6e+7));
}

PerFrameAllocator::~PerFrameAllocator() {
  free(data_);
}

void* PerFrameAllocator::AllocateOrDie(size_t bytes) {
  if (offset_ >= 1.6e+7) {
    FML_CHECK(false);
  }
  auto* ptr = static_cast<void*>(data_ + offset_);
  offset_ += bytes;
  return ptr;
}

std::unique_ptr<FrameLifetime> PerFrameAllocator::ExtendLifetime() {
  lifetimes_++;
  return std::make_unique<FrameLifetime>(shared_from_this());
}

void PerFrameAllocator::Decrement() {
  lifetimes_--;
  if (lifetimes_ <= 0) {
    lifetimes_ = 0;
    offset_ = 0;
  }
}

}  // namespace impeller
