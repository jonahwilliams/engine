// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <string>
#include <variant>

#include "impeller/core/device_private_buffer.h"
#include "impeller/core/texture.h"
#include "impeller/renderer/compute_command.h"
#include "impeller/renderer/compute_pass.h"

namespace impeller {

class RenderPass;
class ContentContext;

struct GeometryPassResult {
  BufferView indirect_command_arguments;
  BufferView output_geometry;
};

struct IndirectCommandArguments {
  uint32_t vertex_count;
  uint32_t instance_count;
  uint32_t vertex_start;
  uint32_t base_instance;
};

class GeometryPass {
 public:
  GeometryPass();

  GeometryPassResult AddPath(Path path, const ContentContext& renderer);

  /// Add a polyline to the current geometry pass.
  ///
  /// This will return a BufferView that contains an IndirectCommandArguments
  /// for indirect command encoding. The compute pass will be processed before
  /// the current render pass is encoded.
  GeometryPassResult AddPolyline(Path::Polyline polyline,
                                 const ContentContext& renderer);

  bool Encode(ComputePass& pass);

 private:
  struct AccumulatedConvexCommand {
    uint32_t count;
    uint32_t size;
    std::shared_ptr<HostBuffer> input_buffer;
    std::shared_ptr<HostBuffer> indirect_buffer;
    std::shared_ptr<HostBuffer> index_buffer;
    std::shared_ptr<DevicePrivateBuffer> output_buffer;
  };

  struct AccumulatedPolylineCommand {
    uint32_t count;
    uint32_t size;
    uint32_t line_offset;
    uint32_t quad_offset;
    std::shared_ptr<HostBuffer> line_buffer;
    std::shared_ptr<HostBuffer> quad_buffer;
    std::shared_ptr<HostBuffer> index_buffer;
    std::shared_ptr<HostBuffer> component_buffer;
    std::shared_ptr<DevicePrivateBuffer> output_buffer;
    std::shared_ptr<DevicePrivateBuffer> output_index_buffer;
    std::shared_ptr<DevicePrivateBuffer> indirect_command_buffer;
    std::shared_ptr<DevicePrivateBuffer> geometry_buffer;
  };

  AccumulatedConvexCommand& GetOrCreateConvex(size_t count);

  AccumulatedPolylineCommand& GetOrCreatePolyline(size_t count);

  std::vector<AccumulatedConvexCommand> convex_commands_;
  std::vector<AccumulatedPolylineCommand> polyline_commands_;

  std::shared_ptr<Pipeline<ComputePipelineDescriptor>> convex_pipeline_;
  std::shared_ptr<Pipeline<ComputePipelineDescriptor>> polyline_pipeline_;

  FML_DISALLOW_COPY_AND_ASSIGN(GeometryPass);
};

}  // namespace impeller
