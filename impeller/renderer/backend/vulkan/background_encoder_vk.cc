// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/renderer/backend/vulkan/background_encoder_vk.h"

#include <algorithm>
#include <chrono>


#ifdef FML_OS_ANDROID
#include <pthread.h>
#include <sys/resource.h>
#include <sys/time.h>
#endif  // FML_OS_ANDROID


#include "flutter/fml/synchronization/count_down_latch.h"
#include "flutter/fml/thread.h"
#include "flutter/fml/trace_event.h"
#include "impeller/base/validation.h"
#include "impeller/renderer/context.h"

namespace impeller {

BackgroundEncoderVK::BackgroundEncoderVK(std::weak_ptr<Context> context)
    : context_(std::move(context)) {
  waiter_thread_ = std::make_unique<std::thread>([&]() { Main(); });
  is_valid_ = true;
}

BackgroundEncoderVK::~BackgroundEncoderVK() {
  Terminate();
  if (waiter_thread_) {
    waiter_thread_->join();
  }
}

bool BackgroundEncoderVK::IsValid() const {
  return is_valid_;
}

bool BackgroundEncoderVK::HasPendingTasks() const {
  return pending_ > 0u;
}

void BackgroundEncoderVK::Flush() {
  if (!HasPendingTasks()) {
    return;
  }
  auto cdl = std::make_shared<fml::CountDownLatch>(1u);
  AddTask([cdl]() {
    cdl->CountDown();
    return true;
  });
  cdl->Wait();
}

bool BackgroundEncoderVK::AddTask(const fml::closure& task) {
  TRACE_EVENT0("flutter", "BackgroundEncoderVK::AddTask");
  if (!IsValid() || !task) {
    return false;
  }
  {
    std::scoped_lock lock(wait_set_mutex_);
    encode_tasks_.emplace_back(task);
  }
  pending_++;
  wait_set_cv_.notify_one();
  return true;
}

void BackgroundEncoderVK::Main() {
  fml::Thread::SetCurrentThreadName(
      fml::Thread::ThreadConfig{"io.flutter.impeller.background_encoder"});

#ifdef FML_OS_ANDROID
  if (::setpriority(PRIO_PROCESS, gettid(), -5) != 0) {
    FML_LOG(ERROR) << "Failed to set Workers task runner priority";
  }
#endif  // FML_OS_ANDROID

  using namespace std::literals::chrono_literals;

  while (true) {
    std::unique_lock lock(wait_set_mutex_);

    // If there are no fences to wait on, wait on the condition variable.
    wait_set_cv_.wait(lock,
                      [&]() { return !encode_tasks_.empty() || terminate_; });

    // Avoid executing task logic while holdig the lock.
    std::vector<const fml::closure> encode_tasks = encode_tasks_;
    encode_tasks_.clear();

    const auto terminate = terminate_;

    lock.unlock();

    if (terminate) {
      break;
    }

    // Check if the context had died in the meantime.
    auto context = context_.lock();
    if (!context) {
      break;
    }
    auto allocator = context->GetResourceAllocator();

    // Perform encode tasks in order.
    for (const auto& task : encode_tasks) {
      task();
      pending_--;
    }
  }
}

void BackgroundEncoderVK::Terminate() {
  {
    std::scoped_lock lock(wait_set_mutex_);
    terminate_ = true;
  }
  wait_set_cv_.notify_one();
}

}  // namespace impeller
