// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <condition_variable>
#include <memory>
#include <thread>
#include <vector>

#include "flutter/fml/closure.h"
#include "flutter/fml/macros.h"
#include "impeller/base/thread.h"
#include "impeller/core/allocator.h"
#include "impeller/renderer/backend/vulkan/device_holder.h"
#include "impeller/renderer/backend/vulkan/shared_object_vk.h"
#include "impeller/renderer/backend/vulkan/vk.h"

namespace impeller {

class ContextVK;

class BackgroundEncoderVK {
 public:
  ~BackgroundEncoderVK();

  bool IsValid() const;

  void Terminate();

  bool AddTask(const fml::closure& task);

  bool HasPendingTasks() const;

  void Flush();

 private:
  friend class ContextVK;

  std::weak_ptr<Context> context_;
  std::unique_ptr<std::thread> waiter_thread_;
  std::mutex wait_set_mutex_;
  std::condition_variable wait_set_cv_;
  std::vector<const fml::closure> encode_tasks_;
  std::atomic<size_t> pending_ = 0u;
  bool terminate_ = false;
  bool is_valid_ = false;

  explicit BackgroundEncoderVK(std::weak_ptr<Context> context);

  void Main();

  FML_DISALLOW_COPY_AND_ASSIGN(BackgroundEncoderVK);
};

}  // namespace impeller
