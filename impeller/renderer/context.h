// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <memory>
#include <string>

#include "flutter/fml/macros.h"
#include "impeller/core/formats.h"
#include "impeller/renderer/capabilities.h"

namespace fml {
class ConcurrentTaskRunner;
}

namespace impeller {

class ShaderLibrary;
class SamplerLibrary;
class CommandBuffer;
class PipelineLibrary;
class Allocator;

//------------------------------------------------------------------------------
/// @brief      To do anything rendering related with Impeller, you need a
///             context.
///
///             Contexts are expensive to construct and typically you only need
///             one in the process. The context represents a connection to a
///             graphics or compute accelerator on the device.
///
///             If there are multiple context in a process, it would typically
///             be for separation of concerns (say, use with multiple engines in
///             Flutter), talking to multiple accelerators, or talking to the
///             same accelerator using different client APIs (Metal, Vulkan,
///             OpenGL ES, etc..).
///
///             Contexts are thread-safe. They may be created, used, and
///             collected (though not from a thread used by an internal pool) on
///             any thread. They may also be accessed simultaneously from
///             multiple threads.
///
///             Contexts are abstract and a concrete instance must be created
///             using one of the subclasses of `Context` in
///             `//impeller/renderer/backend`.
class Context {
 public:
  //----------------------------------------------------------------------------
  /// @brief      Destroys an Impeller context.
  ///
  virtual ~Context();

  // TODO(129920): Refactor and move to capabilities.
  virtual std::string DescribeGpuModel() const = 0;

  //----------------------------------------------------------------------------
  /// @brief      Determines if a context is valid. If the caller ever receives
  ///             an invalid context, they must discard it and construct a new
  ///             context. There is no recovery mechanism to repair a bad
  ///             context.
  ///
  ///             It is convention in Impeller to never return an invalid
  ///             context from a call that returns an pointer to a context. The
  ///             call implementation performs validity checks itself and return
  ///             a null context instead of a pointer to an invalid context.
  ///
  ///             How a context goes invalid is backend specific. It could
  ///             happen due to device loss, or any other unrecoverable error.
  ///
  /// @return     If the context is valid.
  ///
  virtual bool IsValid() const = 0;

  //----------------------------------------------------------------------------
  /// @brief      Get the capabilities of Impeller context. All optionally
  ///             supported feature of the platform, client-rendering API, and
  ///             device can be queried using the `Capabilities`.
  ///
  /// @return     The capabilities. Can never be `nullptr` for a valid context.
  ///
  virtual const std::shared_ptr<const Capabilities>& GetCapabilities()
      const = 0;

  // TODO(129920): Refactor and move to capabilities.
  virtual bool UpdateOffscreenLayerPixelFormat(PixelFormat format);

  //----------------------------------------------------------------------------
  /// @brief      Returns the allocator used to create textures and buffers on
  ///             the device.
  ///
  /// @return     The resource allocator. Can never be `nullptr` for a valid
  ///             context.
  ///
  virtual std::shared_ptr<Allocator> GetResourceAllocator() const = 0;

  //----------------------------------------------------------------------------
  /// @brief      Returns the library of shaders used to specify the
  ///             programmable stages of a pipeline.
  ///
  /// @return     The shader library. Can never be `nullptr` for a valid
  ///             context.
  ///
  virtual std::shared_ptr<ShaderLibrary> GetShaderLibrary() const = 0;

  //----------------------------------------------------------------------------
  /// @brief      Returns the library of combined image samplers used in
  ///             shaders.
  ///
  /// @return     The sampler library. Can never be `nullptr` for a valid
  ///             context.
  ///
  virtual std::shared_ptr<SamplerLibrary> GetSamplerLibrary() const = 0;

  //----------------------------------------------------------------------------
  /// @brief      Returns the library of pipelines used by render or compute
  ///             commands.
  ///
  /// @return     The pipeline library. Can never be `nullptr` for a valid
  ///             context.
  ///
  virtual std::shared_ptr<PipelineLibrary> GetPipelineLibrary() const = 0;

  //----------------------------------------------------------------------------
  /// @brief      Create a new command buffer. Command buffers can be used to
  ///             encode graphics, blit, or compute commands to be submitted to
  ///             the device.
  ///
  ///             A command buffer can only be used on a single thread.
  ///             Multi-threaded render, blit, or compute passes must create a
  ///             new command buffer on each thread.
  ///
  /// @return     A new command buffer.
  ///
  virtual std::shared_ptr<CommandBuffer> CreateCommandBuffer() const = 0;

  virtual const std::shared_ptr<fml::ConcurrentTaskRunner>
  GetConcurrentWorkerTaskRunner() const {
    return nullptr;
  }

  //----------------------------------------------------------------------------
  /// @brief      Force all pending asynchronous work to finish. This is
  ///             achieved by deleting all owned concurrent message loops.
  ///
  virtual void Shutdown() = 0;

 protected:
  Context();

 private:
  FML_DISALLOW_COPY_AND_ASSIGN(Context);
};

}  // namespace impeller
