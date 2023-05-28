// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/core/device_private_buffer.h"

namespace impeller {

 std::shared_ptr<BufferSwapper> BufferSwapper::Create(size_t size) {
  auto a = DevicePrivateBuffer::Create();
  a->Reserve(size);
  auto b = DevicePrivateBuffer::Create();
  b->Reserve(size);
  return std::make_shared<BufferSwapper>(a, b);
}

BufferSwapper::BufferSwapper(std::shared_ptr<DevicePrivateBuffer> a,
                             std::shared_ptr<DevicePrivateBuffer> b)
    : a_(a), b_(b) {}

std::shared_ptr<DevicePrivateBuffer> BufferSwapper::GetBuffer() const {
  if (switch_) {
    switch_ = !switch_;
    return a_;
  } else {
    switch_ = !switch_;
    return b_;
  }
}

std::shared_ptr<DevicePrivateBuffer> DevicePrivateBuffer::Create() {
  return std::shared_ptr<DevicePrivateBuffer>(new DevicePrivateBuffer());
}

DevicePrivateBuffer::DevicePrivateBuffer() = default;

DevicePrivateBuffer::~DevicePrivateBuffer() = default;

void DevicePrivateBuffer::SetLabel(std::string label) {
  label_ = std::move(label);
}

BufferView DevicePrivateBuffer::AsBufferView() {
  return BufferView{shared_from_this(), nullptr, Range{0, size_}};
}

BufferView DevicePrivateBuffer::AsBufferViewWithSize(size_t size) {
  return BufferView{shared_from_this(), nullptr, Range{0, size}};
}

BufferView DevicePrivateBuffer::Reserve(size_t length) {
  auto old_length = size_;
  size_ += length;
  generation_++;
  return BufferView{shared_from_this(), nullptr, Range{old_length, length}};
}

std::shared_ptr<const DeviceBuffer> DevicePrivateBuffer::GetDeviceBuffer(
    Allocator& allocator) const {
  if (generation_ == device_buffer_generation_) {
    return device_buffer_;
  }
  DeviceBufferDescriptor desc;
  desc.storage_mode = StorageMode::kDevicePrivate;
  desc.size = size_;

  auto new_buffer = allocator.CreateBuffer(desc);
  if (!new_buffer) {
    return nullptr;
  }
  new_buffer->SetLabel(label_);
  device_buffer_generation_ = generation_;
  device_buffer_ = std::move(new_buffer);
  return device_buffer_;
}

}  // namespace impeller
