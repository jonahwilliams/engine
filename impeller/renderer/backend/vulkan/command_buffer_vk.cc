// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/renderer/backend/vulkan/command_buffer_vk.h"

#include <memory>
#include <utility>

#include "flutter/fml/logging.h"
#include "impeller/base/validation.h"
#include "impeller/renderer/backend/vulkan/blit_pass_vk.h"
#include "impeller/renderer/backend/vulkan/command_encoder_vk.h"
#include "impeller/renderer/backend/vulkan/compute_pass_vk.h"
#include "impeller/renderer/backend/vulkan/context_vk.h"
#include "impeller/renderer/backend/vulkan/formats_vk.h"
#include "impeller/renderer/backend/vulkan/render_pass_vk.h"
#include "impeller/renderer/command_buffer.h"
#include "impeller/renderer/render_target.h"

namespace impeller {

CommandBufferVK::CommandBufferVK(
    std::weak_ptr<const Context> context,
    std::shared_ptr<CommandEncoderFactoryVK> encoder_factory)
    : CommandBuffer(std::move(context)),
      encoder_factory_(std::move(encoder_factory)) {}

CommandBufferVK::~CommandBufferVK() = default;

void CommandBufferVK::SetLabel(const std::string& label) const {
  if (!encoder_) {
    encoder_factory_->SetLabel(label);
  } else {
    auto context = context_.lock();
    if (!context || !encoder_) {
      return;
    }
    ContextVK::Cast(*context).SetDebugName(encoder_->GetCommandBuffer(), label);
  }
}

bool CommandBufferVK::IsValid() const {
  return true;
}

const std::shared_ptr<CommandEncoderVK>& CommandBufferVK::GetEncoder() {
  if (!encoder_) {
    encoder_ = encoder_factory_->Create();
  }
  return encoder_;
}

bool CommandBufferVK::OnSubmitCommands(CompletionCallback callback) {
  if (!callback) {
    return encoder_->Submit();
  }
  return encoder_->Submit([callback](bool submitted) {
    callback(submitted ? CommandBuffer::Status::kCompleted
                       : CommandBuffer::Status::kError);
  });
}

bool CommandBufferVK::SubmitCommandsAsync(std::shared_ptr<BlitPass> blit_pass) {
  TRACE_EVENT0("impeller", "CommandBufferVK::SubmitCommandsAsync");
  if (!blit_pass->IsValid() || !IsValid()) {
    return false;
  }
  auto context = context_.lock();
  if (!context) {
    return false;
  }

  const auto& context_vk = ContextVK::Cast(*context);
  auto pending = std::make_shared<EnqueuedCommandBuffer>();
  context_vk.GetCommandBufferQueue()->Enqueue(pending);
  // context_vk.GetConcurrentWorkerTaskRunner()->PostTask(
  //     [cmd_buffer = shared_from_this(), pending, blit_pass,
  //      weak_context = context_]() {
  // auto context = weak_context.lock();
  // if (!context || !cmd_buffer) {
  //   return;
  // }
  auto encoder = GetEncoder();
  if (!blit_pass->EncodeCommands(context->GetResourceAllocator()) ||
      !encoder->Finish()) {
    VALIDATION_LOG << "Failed to encode render pass.";
  }
  pending->SetEncoder(std::move(encoder));
  // });

  return true;
}

bool CommandBufferVK::SubmitCommandsAsync(
    std::shared_ptr<RenderPass> render_pass) {
  TRACE_EVENT0("impeller", "CommandBufferVK::SubmitCommandsAsync");
  if (!render_pass->IsValid() || !IsValid()) {
    return false;
  }
  const auto context = context_.lock();
  if (!context) {
    return false;
  }
  const auto& context_vk = ContextVK::Cast(*context);
  auto pending = std::make_shared<EnqueuedCommandBuffer>();
  context_vk.GetCommandBufferQueue()->Enqueue(pending);
  // context_vk.GetConcurrentWorkerTaskRunner()->PostTask(
  //     [cmd_buffer = shared_from_this(), pending, render_pass,
  //      weak_context = context_]() {
  // auto context = weak_context.lock();
  // if (!context || !cmd_buffer) {
  //   return;
  // }
  auto encoder = GetEncoder();
  if (!render_pass->EncodeCommands() || !encoder->Finish()) {
    VALIDATION_LOG << "Failed to encode render pass.";
  }
  pending->SetEncoder(std::move(encoder));
  // });
  return true;
}

void CommandBufferVK::OnWaitUntilScheduled() {}

std::shared_ptr<RenderPass> CommandBufferVK::OnCreateRenderPass(
    RenderTarget target) {
  auto context = context_.lock();
  if (!context) {
    return nullptr;
  }
  auto pass =
      std::shared_ptr<RenderPassVK>(new RenderPassVK(context,          //
                                                     target,           //
                                                     weak_from_this()  //
                                                     ));
  if (!pass->IsValid()) {
    return nullptr;
  }
  return pass;
}

std::shared_ptr<BlitPass> CommandBufferVK::OnCreateBlitPass() {
  if (!IsValid()) {
    return nullptr;
  }
  auto pass = std::shared_ptr<BlitPassVK>(new BlitPassVK(weak_from_this()));
  if (!pass->IsValid()) {
    return nullptr;
  }
  return pass;
}

std::shared_ptr<ComputePass> CommandBufferVK::OnCreateComputePass() {
  if (!IsValid()) {
    return nullptr;
  }
  auto context = context_.lock();
  if (!context) {
    return nullptr;
  }
  auto pass =
      std::shared_ptr<ComputePassVK>(new ComputePassVK(context,          //
                                                       weak_from_this()  //
                                                       ));
  if (!pass->IsValid()) {
    return nullptr;
  }
  return pass;
}

}  // namespace impeller
