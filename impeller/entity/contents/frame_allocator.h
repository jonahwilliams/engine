// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <memory>

namespace impeller {

class FrameLifetime;

class PerFrameAllocator
    : public std::enable_shared_from_this<PerFrameAllocator> {
 public:
  PerFrameAllocator();

  ~PerFrameAllocator();

  void* AllocateOrDie(size_t bytes);

  template <typename T>
  T* AllocateObjectOrDie() {
    auto* buf = AllocateOrDie(sizeof(T));
    return new (buf) T;
  }

  std::unique_ptr<FrameLifetime> ExtendLifetime();

  void Decrement();

 private:
  PerFrameAllocator(const PerFrameAllocator&) = default;
  PerFrameAllocator(PerFrameAllocator&&) = delete;

  PerFrameAllocator& operator=(const PerFrameAllocator&) = default;
  PerFrameAllocator& operator=(PerFrameAllocator&&) = delete;

  uint8_t* data_;
  size_t offset_ = 0;
  size_t lifetimes_ = 0;
};

class FrameLifetime {
 public:
  explicit FrameLifetime(const std::shared_ptr<PerFrameAllocator>& allocator) : allocator_(allocator) {}

  ~FrameLifetime() { allocator_->Decrement(); }

 private:
  FrameLifetime(const FrameLifetime&) = default;
  FrameLifetime(FrameLifetime&&) = delete;
  FrameLifetime& operator=(const FrameLifetime&) = default;
  FrameLifetime& operator=(FrameLifetime&&) = delete;

  std::shared_ptr<PerFrameAllocator> allocator_;
};

}  // namespace impeller
