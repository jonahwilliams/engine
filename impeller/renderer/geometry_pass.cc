// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/renderer/geometry_pass.h"

#include <iostream>
#include <tuple>

#include "impeller/core/host_buffer.h"
#include "impeller/entity/contents/content_context.h"  // nogncheck
#include "impeller/entity/convex.comp.h"               // nogncheck
#include "impeller/entity/polyline.comp.h"             // nogncheck
#include "impeller/renderer/render_pass.h"

namespace impeller {

GeometryPass::GeometryPass() {}

constexpr size_t kMaxConvexSegments = 0xFFFFFFFF;  // max uint32_t.
constexpr size_t kMaxPolylineSegments = 1024;

GeometryPass::AccumulatedConvexCommand& GeometryPass::GetOrCreateConvex(
    size_t count) {
  FML_DCHECK(count <= kMaxConvexSegments);
  if (convex_commands_.size() == 0 ||
      (convex_commands_.size() + count > kMaxConvexSegments)) {
    convex_commands_.emplace_back(AccumulatedConvexCommand{
        .count = 0u,
        .size = 0u,
        .input_buffer = HostBuffer::Create(),
        .indirect_buffer = HostBuffer::Create(),
        .index_buffer = HostBuffer::Create(),
        .output_buffer = DevicePrivateBuffer::Create(),
    });
  }
  return convex_commands_.back();
}

GeometryPass::AccumulatedPolylineCommand& GeometryPass::GetOrCreatePolyline(
    size_t count) {
  FML_DCHECK(count <= kMaxPolylineSegments);
  if (polyline_commands_.size() == 0 ||
      (polyline_commands_.size() + count >= kMaxPolylineSegments)) {
    polyline_commands_.emplace_back(AccumulatedPolylineCommand{
        .count = 0u,
        .size = 0u,
        .line_offset = 0u,
        .quad_offset = 0u,
        .line_buffer = HostBuffer::Create(),
        .quad_buffer = HostBuffer::Create(),
        .index_buffer = HostBuffer::Create(),
        .component_buffer = HostBuffer::Create(),
        .indirect_command_buffer = DevicePrivateBuffer::Create(),
    });
  }
  return polyline_commands_.back();
}

GeometryPassResult GeometryPass::AddPath(Path path,
                                         const ContentContext& renderer) {
  polyline_pipeline_ = renderer.GetPolylineComputePipeline();
  convex_pipeline_ = renderer.GetConvexComputePipeline();

  using CS = PolylineComputeShader;
  const auto& linears = path.GetLinears();
  const auto& quads = path.GetQuads();
  const auto& components = path.GetComponents();

  size_t count = linears.size() + quads.size();
  auto& polyline_command = GetOrCreatePolyline(count);

  polyline_command.output_buffer = renderer.GetBufferA()->GetBuffer();
  polyline_command.output_index_buffer = renderer.GetBufferB()->GetBuffer();
  polyline_command.geometry_buffer = renderer.GetBufferC()->GetBuffer();

  std::vector<CS::IndexDataItem> index_data(count);
  std::vector<CS::PathComponent> component_data(count);

  uint32_t index_offset = 0u;
  uint32_t line_offset = 0u;
  uint32_t quad_offset = 0u;
  for (auto i = 0u; i < components.size(); i++) {
    if (components[i].type == Path::ComponentType::kLinear) {
      component_data[index_offset] = CS::PathComponent{
          .index = line_offset + polyline_command.line_offset,
          .count = 2u,
      };
      index_data[index_offset] = CS::IndexDataItem{
          .first_offset = polyline_command.size,
          .indirect_offset = polyline_command.count,
      };
      index_offset++;
      line_offset++;
    } else if (components[i].type == Path::ComponentType::kQuadratic) {
      component_data[index_offset] = CS::PathComponent{
          .index = quad_offset + polyline_command.quad_offset,
          .count = 3u,
      };
      index_data[index_offset] = CS::IndexDataItem{
          .first_offset = polyline_command.size,
          .indirect_offset = polyline_command.count,
      };
      quad_offset++;
      index_offset++;
    }
  }

  // Emplace with no padding as we're going to treat this as a single buffer.
  std::ignore = polyline_command.line_buffer->Emplace(
      linears.data(), linears.size() * sizeof(CS::LineData), 0);
  std::ignore = polyline_command.quad_buffer->Emplace(
      quads.data(), quads.size() * sizeof(CS::QuadData), 0);
  std::ignore = polyline_command.component_buffer->Emplace(
      component_data.data(), component_data.size() * sizeof(CS::PathComponent),
      0);
  std::ignore = polyline_command.index_buffer->Emplace(
      index_data.data(), index_data.size() * sizeof(CS::IndexDataItem), 0);

  auto indirect_command_buffer =
      polyline_command.indirect_command_buffer->Reserve(
          sizeof(IndirectCommandArguments));

  polyline_command.count += 1;
  polyline_command.size += count;
  polyline_command.quad_offset += quad_offset;
  polyline_command.line_offset += line_offset;

  return {
      .indirect_command_arguments = indirect_command_buffer,
      // We don't yet know what part of the buffer view will contain output data
      // for this command. Instead, we'll adjust the offset in the indirect
      // command arguments. Provide a buffer view with a range of the entire
      // eventual size of this buffer.
      .output_geometry =
          polyline_command.geometry_buffer->AsBufferViewWithSize(1024 * 200),
  };
}

GeometryPassResult GeometryPass::AddPolyline(Path::Polyline polyline,
                                             const ContentContext& renderer) {
  using CS = ConvexComputeShader;
  auto result_size = polyline.points.size() * 3;
  convex_pipeline_ = renderer.GetConvexComputePipeline();
  auto& convex_command = GetOrCreateConvex(polyline.points.size());

  // Emplace with no padding as we're going to treat this as a single buffer.
  std::ignore = convex_command.input_buffer->Emplace(
      polyline.points.data(), polyline.points.size() * sizeof(Point), 0);

  auto geometry_buffer =
      convex_command.output_buffer->Reserve(result_size * sizeof(Point));

  // TODO: find a smarter way to do this?
  std::vector<CS::IndexDataItem> index_data(polyline.points.size());
  for (auto i = 0u; i < polyline.points.size(); i++) {
    index_data[i] = CS::IndexDataItem{
        .first_offset = convex_command.size,
        .indirect_offset = convex_command.count,
    };
  }
  std::ignore = convex_command.index_buffer->Emplace(
      index_data.data(), polyline.points.size() * sizeof(CS::IndexDataItem), 0);

  convex_command.count += 1;
  convex_command.size += polyline.points.size();
  return {
      .indirect_command_arguments = {},
      .output_geometry = geometry_buffer,
  };
}

bool GeometryPass::Encode(ComputePass& pass) {
  for (const auto& polyline_command : polyline_commands_) {
    using PS = PolylineComputeShader;
    using CS = ConvexComputeShader;

    // Size estimate: 1 * linears + 20 * quads. Make this reliable by capping the
    // number of linear segments in the buffer? Right now, just a guess. Note
    // that this has implications for how we write output data in the shader, as
    // we can't assume that the correct offset is just computed from the prefix
    // sum. Instead, we must essential apply a ceil to the prefix sum from
    // previous components. Or maybe it doesn't matter? and we just track this
    // with a bump allocator
    // polyline_command.output_buffer->Reserve(1024 * 40);
    // polyline_command.output_index_buffer->Reserve(1024 * 40);
    // 3 points for each input vertex, so *3 the previous size.
    // polyline_command.geometry_buffer->Reserve(1024 * 40);

    // Output Config will contain count for convex stage.
    auto output_config_buffer = DevicePrivateBuffer::Create();
    output_config_buffer->SetLabel("ComputeOutputConfig");
    auto output_config = output_config_buffer->Reserve(sizeof(uint32_t));

    {
      ComputeCommand cmd;
      cmd.label = "Polyline Geometry";
      cmd.pipeline = polyline_pipeline_;
      cmd.grid_size = ISize(polyline_command.size, 1);

      PS::Config config;
      config.input_count = polyline_command.size;

      polyline_command.quad_buffer->SetLabel("ComputeQuadBuffer");
      polyline_command.index_buffer->SetLabel("ComputeIndexBuffer");
      polyline_command.line_buffer->SetLabel("ComputeLineBuffer");
      polyline_command.component_buffer->SetLabel("ComputeComponentBuffer");
      polyline_command.output_buffer->SetLabel("ComputeOutputBuffer");
      polyline_command.output_index_buffer->SetLabel(
          "ComputeOutputIndexBuffer");

      PS::BindConfig(cmd, pass.GetTransientsBuffer().EmplaceUniform(config));
      PS::BindQuads(cmd, polyline_command.quad_buffer->AsBufferView());
      PS::BindIndexData(cmd, polyline_command.index_buffer->AsBufferView());
      PS::BindLines(cmd, polyline_command.line_buffer->AsBufferView());
      PS::BindComponents(cmd,
                         polyline_command.component_buffer->AsBufferView());
      PS::BindPolyline(cmd, polyline_command.output_buffer->AsBufferView());
      PS::BindOutputIndexData(
          cmd, polyline_command.output_index_buffer->AsBufferView());
      PS::BindOutputConfig(cmd, output_config);

      if (!pass.AddCommand(std::move(cmd))) {
        return false;
      }
    }

    {
      ComputeCommand cmd;
      cmd.label = "Convex Geometry";
      cmd.pipeline = convex_pipeline_;
      // TODO, use dispatchThreadgroupsWithIndirectBuffer with value computed in
      // 1st stage.
      cmd.grid_size = ISize(1024 * 200, 1);  // TODO: more accurate grid size.
      // This is related to the above with the estimate for the output buffer
      // size. We need to err on the side of caution to ensure everything gets
      // evaluated.

      CS::BindFrameData(cmd, output_config);
      CS::BindGeometry(cmd, polyline_command.geometry_buffer->AsBufferView());
      CS::BindPolyline(cmd, polyline_command.output_buffer->AsBufferView());
      CS::BindIndexData(cmd,
                        polyline_command.output_index_buffer->AsBufferView());
      CS::BindIndirectCommandData(
          cmd, polyline_command.indirect_command_buffer->AsBufferView());

      if (!pass.AddCommand(std::move(cmd))) {
        return false;
      }
    }

    // Encode second stage of convex computation.
  }

  // using CS = ConvexComputeShader;
  // for (const auto& accumulated : convex_commands_) {
  //   ComputeCommand cmd;
  //   cmd.label = "Convex Geometry";
  //   cmd.pipeline = convex_pipeline_;
  //   cmd.grid_size = ISize(accumulated.size, 1);

  //   CS::FrameData frame_data;
  //   frame_data.input_count = accumulated.size;

  //   CS::BindFrameData(cmd,
  //                     pass.GetTransientsBuffer().EmplaceUniform(frame_data));
  //   CS::BindGeometry(cmd, accumulated.output_buffer->AsBufferView());
  //   CS::BindPolyline(cmd, accumulated.input_buffer->AsBufferView());
  //   CS::BindIndexData(cmd, accumulated.index_buffer->AsBufferView());

  //   if (!pass.AddCommand(std::move(cmd))) {
  //     return false;
  //   }
  // }
  return true;
}

static_assert(sizeof(PolylineComputeShader::LineData) ==
              sizeof(LinearPathComponent));
static_assert(sizeof(PolylineComputeShader::QuadData) ==
              sizeof(QuadraticPathComponent));

}  // namespace impeller
