// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/entity/contents/content_context.h"
#include "impeller/entity/entity.h"
#include "impeller/renderer/render_pass.h"
#include "impeller/renderer/vertex_buffer_builder.h"

namespace impeller {

bool RenderBackdropAlpha(const ContentContext& renderer,
                         const Entity& entity,
                         RenderPass& pass,
                         Rect coverage,
                         Color background_color,
                         Scalar alpha) {
  if (!renderer.GetDeviceCapabilities().SupportsFramebufferFetch()) {
    return false;
  }

  using VS = BackdropAlphaPipeline::VertexShader;
  using FS = BackdropAlphaPipeline::FragmentShader;

  HostBuffer& host_buffer = renderer.GetTransientsBuffer();

  std::array<VS::PerVertexData, 4> vertices = {
      VS::PerVertexData{Point(coverage.GetLeft(), coverage.GetTop())},
      VS::PerVertexData{Point(coverage.GetRight(), coverage.GetTop())},
      VS::PerVertexData{Point(coverage.GetLeft(), coverage.GetBottom())},
      VS::PerVertexData{Point(coverage.GetRight(), coverage.GetBottom())},
  };

  auto options = OptionsFromPass(pass);
  options.blend_mode = BlendMode::kSource;
  options.primitive_type = PrimitiveType::kTriangleStrip;

  pass.SetCommandLabel("Backdrop Alpha");
  pass.SetVertexBuffer(
      CreateVertexBuffer(vertices, renderer.GetTransientsBuffer()));
  pass.SetPipeline(renderer.GetBackdropAlphaPipeline(options));

  VS::FrameInfo frame_info;
  FS::FragInfo frag_info;

  frame_info.mvp = entity.GetShaderTransform(pass);
  VS::BindFrameInfo(pass, host_buffer.EmplaceUniform(frame_info));
  frag_info.alpha = alpha;
  frag_info.color = background_color;
  FS::BindFragInfo(pass, host_buffer.EmplaceUniform(frag_info));

  return pass.Draw().ok();
}

}  // namespace impeller
