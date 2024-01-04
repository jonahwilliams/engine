// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_CORE_HOST_BUFFER_POOL_H_
#define FLUTTER_IMPELLER_CORE_HOST_BUFFER_POOL_H_

#include "impeller/core/device_buffer.h"

namespace impeller {

constexpr size_t kCacheLimit = 16;
constexpr size_t kInitialBufferSize = 4e+6; // 4 MB.

class HostBufferPool {
  public:
    HostBufferPool() = default;

    ~HostBufferPool() = default;


  private:
    std::vector<std::shared_ptr<DeviceBuffer>> buffers_;
};

}

#endif // FLUTTER_IMPELLER_CORE_HOST_BUFFER_POOL_H_
