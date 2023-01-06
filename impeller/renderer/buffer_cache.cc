// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/renderer/buffer_cache.h"

namespace impeller {

BufferCache::BufferCache() {}

BufferCache::~BufferCache() {}

void BufferCache::StoreBuffer(uint64_t key, VertexBuffer buffer) {
  cache_[key] = buffer;
}

std::optional<VertexBuffer> BufferCache::GetBuffer(uint64_t key) const {
  if (auto result = cache_.find(key); result != cache_.end()) {
    return result->second;
  }
  return std::nullopt;
}

}  // namespace impeller
