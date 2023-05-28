// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "impeller/core/buffer.h"
#include "impeller/core/buffer_view.h"
#include "impeller/core/device_buffer.h"

#include "flutter/fml/macros.h"

namespace impeller {

class DevicePrivateBuffer final
    : public std::enable_shared_from_this<DevicePrivateBuffer>,
      public Buffer {
 public:
  static std::shared_ptr<DevicePrivateBuffer> Create();

  // |Buffer|
  virtual ~DevicePrivateBuffer();

  void SetLabel(std::string label);

  [[nodiscard]] BufferView AsBufferView();

  BufferView Reserve(size_t length);

  [[nodiscard]] BufferView AsBufferViewWithSize(size_t size);

 private:
  mutable std::shared_ptr<DeviceBuffer> device_buffer_;
  size_t size_ = 0u;
  mutable size_t device_buffer_generation_ = 0u;
  size_t generation_ = 1u;
  std::string label_;

  // |Buffer|
  std::shared_ptr<const DeviceBuffer> GetDeviceBuffer(
      Allocator& allocator) const override;

  DevicePrivateBuffer();

  FML_DISALLOW_COPY_AND_ASSIGN(DevicePrivateBuffer);
};

class BufferSwapper {
 public:
  static std::shared_ptr<BufferSwapper> Create(size_t count);

  std::shared_ptr<DevicePrivateBuffer> GetBuffer() const;

  BufferSwapper(std::shared_ptr<DevicePrivateBuffer> a,
                std::shared_ptr<DevicePrivateBuffer> b);

  ~BufferSwapper() = default;

 private:

  std::shared_ptr<DevicePrivateBuffer> a_;
  std::shared_ptr<DevicePrivateBuffer> b_;
  mutable bool switch_ = false;

  FML_DISALLOW_COPY_AND_ASSIGN(BufferSwapper);
};

}  // namespace impeller
