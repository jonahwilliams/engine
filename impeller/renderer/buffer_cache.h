// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <optional>
#include <unordered_map>

#include "flutter/fml/macros.h"
#include "impeller/renderer/buffer.h"
#include "impeller/renderer/range.h"
#include "impeller/renderer/vertex_buffer.h"

namespace impeller {

class BufferCache {
 public:
  BufferCache();

  ~BufferCache();

  void StoreBuffer(uint64_t key, VertexBuffer vertex_buffer);

  std::optional<VertexBuffer> GetBuffer(uint64_t key) const;

  private:
    std::unordered_map<uint64_t, VertexBuffer> cache_;
};

}
