// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_ENTITY_ARENA_ALLOCATOR_H_
#define FLUTTER_IMPELLER_ENTITY_ARENA_ALLOCATOR_H_

#include <cstddef>
#include <cstdlib>
#include <memory>
#include "fml/logging.h"

static constexpr size_t kSize = 409600;

class Lifetime;

class ArenaAllocator {
 public:
  ArenaAllocator() {
    total_size_ = kSize;
    data_ = static_cast<uint8_t*>(::malloc(kSize));
    FML_CHECK(data_);
  }

  ~ArenaAllocator() { free(data_); }

  template <typename T, class... Types>
  T* Allocate(Types... args) {
    size_t size = sizeof(T);
    if (offset_ + size >= total_size_) {
      return nullptr;
    }
    T* result = new (data_ + offset_) T(args...);
    FML_CHECK(result);
    offset_ += size;
    return result;
  }

  void CollectLifetime() {
    active_lifetimes_--;
    if (active_lifetimes_ == 0) {
      offset_ = 0;
    }
  }

  std::unique_ptr<Lifetime> ExtendLifetime() {
    active_lifetimes_++;
    return std::make_unique<Lifetime>(this);
  }

 private:
  size_t offset_ = 0u;
  size_t total_size_ = 0u;
  size_t active_lifetimes_ = 0u;
  uint8_t* data_ = nullptr;
};

class Lifetime {
  public:
    explicit Lifetime(ArenaAllocator* arena) : arena_(arena) {}

    ~Lifetime() {
      arena_->CollectLifetime();
    }

  private:
  ArenaAllocator* arena_;
};

#endif  // #FLUTTER_IMPELLER_ENTITY_ARENA_ALLOCATOR_H_