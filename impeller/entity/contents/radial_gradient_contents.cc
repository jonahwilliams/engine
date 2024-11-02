// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "radial_gradient_contents.h"

#include "fml/logging.h"
#include "impeller/entity/contents/clip_contents.h"
#include "impeller/entity/contents/content_context.h"
#include "impeller/entity/contents/gradient_generator.h"
#include "impeller/entity/entity.h"
#include "impeller/entity/geometry/geometry.h"
#include "impeller/geometry/gradient.h"
#include "impeller/geometry/scalar.h"
#include "impeller/renderer/render_pass.h"

namespace impeller {

RadialGradientContents::RadialGradientContents() = default;

RadialGradientContents::~RadialGradientContents() = default;

void RadialGradientContents::SetCenterAndRadius(Point center, Scalar radius) {
  center_ = center;
  radius_ = radius;
}

void RadialGradientContents::SetTileMode(Entity::TileMode tile_mode) {
  tile_mode_ = tile_mode;
}

void RadialGradientContents::SetColors(std::vector<Color> colors) {
  colors_ = std::move(colors);
}

void RadialGradientContents::SetStops(std::vector<Scalar> stops) {
  stops_ = std::move(stops);
}

const std::vector<Color>& RadialGradientContents::GetColors() const {
  return colors_;
}

const std::vector<Scalar>& RadialGradientContents::GetStops() const {
  return stops_;
}

bool RadialGradientContents::IsOpaque(const Matrix& transform) const {
  if (GetOpacityFactor() < 1 || tile_mode_ == Entity::TileMode::kDecal) {
    return false;
  }
  for (auto color : colors_) {
    if (!color.IsOpaque()) {
      return false;
    }
  }
  return !AppliesAlphaForStrokeCoverage(transform);
}

// Only applied when using decal or clamp.
bool RadialGradientContents::FastRadialGradient(const ContentContext& renderer,
                                                const Entity& entity,
                                                RenderPass& pass) const {
  using VS = FastGradientPipeline::VertexShader;
  using FS = FastGradientPipeline::FragmentShader;

  VertexBufferBuilder<VS::PerVertexData> vtx_builder;

  auto geom_callback = [&](const ContentContext& renderer, const Entity& entity,
                           RenderPass& pass,
                           const Geometry* geometry) -> GeometryResult {
    std::optional<Rect> maybe_coverage = geometry->GetCoverage(Matrix());
    if (!maybe_coverage.has_value()) {
      return {};
    }
    Rect coverage = maybe_coverage.value();

    // For the first two colors, tessellate an entire circle, with the center
    // point using color0 and the outer points using color1. Connect these
    // points so that every triange includes the center vertex.
    vtx_builder.AppendVertex({.position = center_, .color = colors_[0]});

    // TODO: replace with generator once we can configure triangle mode.
    std::vector<Point> unit_circle(360);
    for (auto i = 0u; i < 360; i++) {
      unit_circle[i] =
          Point(cos((i / 360.0) * 2 * 3.14), sin((i / 360.0) * 2 * 3.14));
    }

    Scalar current_radius = radius_ * stops_[1];
    size_t current_start = 1;
    size_t current_end = 360;

    for (auto i = 0u; i < unit_circle.size(); i++) {
      vtx_builder.AppendVertex(
          {.position = (current_radius * unit_circle[i] + center_),
           .color = colors_[1]});
    }

    // Now connect using the index buffer.
    for (auto i = current_start; i < current_end; i++) {
      vtx_builder.AppendIndex(0u);
      vtx_builder.AppendIndex(i - 1);
      vtx_builder.AppendIndex(i);
    }
    vtx_builder.AppendIndex(0u);
    vtx_builder.AppendIndex(current_end - 1);
    vtx_builder.AppendIndex(current_start);

    // Now generate rings for all subsequent stops.
    size_t current_stop = 2;
    Rect current_rect = Rect::MakeOriginSize(
        center_, Size::MakeWH(current_radius * 2, current_radius * 2));
    while (current_stop < stops_.size()) {
      current_radius = radius_ * stops_[current_stop];
      current_start += 360;
      current_end += 360;

      for (auto i = 0u; i < unit_circle.size(); i++) {
        vtx_builder.AppendVertex(
            {.position = (current_radius * unit_circle[i] + center_),
             .color = colors_[current_stop]});
      }

      for (auto i = current_start; i < current_end; i++) {
        vtx_builder.AppendIndex(i);
        vtx_builder.AppendIndex(i - 360);
        vtx_builder.AppendIndex(i + 1);

        vtx_builder.AppendIndex(i - 360);
        vtx_builder.AppendIndex(i + 1);
        vtx_builder.AppendIndex(i - 359);
      }

      current_stop++;
    }

    if (current_rect.Contains(coverage)) {
      return GeometryResult{
          .type = PrimitiveType::kTriangle,
          .vertex_buffer =
              vtx_builder.CreateVertexBuffer(renderer.GetTransientsBuffer()),
          .transform = entity.GetShaderTransform(pass),
      };
    }

    // Now generate an enclosing final ring.
    if (tile_mode_ == Entity::TileMode::kClamp) {
      current_radius = radius_ * 5;  // TODO: figure this one out.
      current_start += 360;
      current_end += 360;

      for (auto i = 0u; i < unit_circle.size(); i++) {
        vtx_builder.AppendVertex(
            {.position = (current_radius * unit_circle[i] + center_),
             .color = colors_.back()});
      }

      for (auto i = current_start; i < current_end; i++) {
        vtx_builder.AppendIndex(i);
        vtx_builder.AppendIndex(i - 360);
        vtx_builder.AppendIndex(i + 1);

        vtx_builder.AppendIndex(i - 360);
        vtx_builder.AppendIndex(i + 1);
        vtx_builder.AppendIndex(i - 359);
      }
    }

    return GeometryResult{
        .type = PrimitiveType::kTriangle,
        .vertex_buffer =
            vtx_builder.CreateVertexBuffer(renderer.GetTransientsBuffer()),
        .transform = entity.GetShaderTransform(pass),
    };
  };

  pass.SetLabel("LinearGradient");

  VS::FrameInfo frame_info;

  PipelineBuilderCallback pipeline_callback =
      [&renderer](ContentContextOptions options) {
        return renderer.GetFastGradientPipeline(options);
      };
  return ColorSourceContents::DrawGeometry<VS>(
      renderer, entity, pass, pipeline_callback, frame_info,
      [this, &renderer, &entity](RenderPass& pass) {
        auto& host_buffer = renderer.GetTransientsBuffer();

        FS::FragInfo frag_info;
        frag_info.alpha =
            GetOpacityFactor() *
            GetGeometry()->ComputeAlphaCoverage(entity.GetTransform());

        FS::BindFragInfo(pass, host_buffer.EmplaceUniform(frag_info));

        return true;
      },
      /*force_stencil=*/true, geom_callback);
}

bool RadialGradientContents::Render(const ContentContext& renderer,
                                    const Entity& entity,
                                    RenderPass& pass) const {
  return FastRadialGradient(renderer, entity, pass);

  // if (renderer.GetDeviceCapabilities().SupportsSSBO()) {
  //   return RenderSSBO(renderer, entity, pass);
  // }
  // return RenderTexture(renderer, entity, pass);
}

bool RadialGradientContents::RenderSSBO(const ContentContext& renderer,
                                        const Entity& entity,
                                        RenderPass& pass) const {
  using VS = RadialGradientSSBOFillPipeline::VertexShader;
  using FS = RadialGradientSSBOFillPipeline::FragmentShader;

  VS::FrameInfo frame_info;
  frame_info.matrix = GetInverseEffectTransform();

  PipelineBuilderCallback pipeline_callback =
      [&renderer](ContentContextOptions options) {
        return renderer.GetRadialGradientSSBOFillPipeline(options);
      };
  return ColorSourceContents::DrawGeometry<VS>(
      renderer, entity, pass, pipeline_callback, frame_info,
      [this, &renderer, &entity](RenderPass& pass) {
        FS::FragInfo frag_info;
        frag_info.center = center_;
        frag_info.radius = radius_;
        frag_info.tile_mode = static_cast<Scalar>(tile_mode_);
        frag_info.decal_border_color = decal_border_color_;
        frag_info.alpha =
            GetOpacityFactor() *
            GetGeometry()->ComputeAlphaCoverage(entity.GetTransform());

        auto& host_buffer = renderer.GetTransientsBuffer();
        auto colors = CreateGradientColors(colors_, stops_);

        frag_info.colors_length = colors.size();
        auto color_buffer =
            host_buffer.Emplace(colors.data(), colors.size() * sizeof(StopData),
                                DefaultUniformAlignment());

        pass.SetCommandLabel("RadialGradientSSBOFill");
        FS::BindFragInfo(
            pass, renderer.GetTransientsBuffer().EmplaceUniform(frag_info));
        FS::BindColorData(pass, color_buffer);

        return true;
      });
}

bool RadialGradientContents::RenderTexture(const ContentContext& renderer,
                                           const Entity& entity,
                                           RenderPass& pass) const {
  using VS = RadialGradientFillPipeline::VertexShader;
  using FS = RadialGradientFillPipeline::FragmentShader;

  auto gradient_data = CreateGradientBuffer(colors_, stops_);
  auto gradient_texture =
      CreateGradientTexture(gradient_data, renderer.GetContext());
  if (gradient_texture == nullptr) {
    return false;
  }

  VS::FrameInfo frame_info;
  frame_info.matrix = GetInverseEffectTransform();

  PipelineBuilderCallback pipeline_callback =
      [&renderer](ContentContextOptions options) {
        return renderer.GetRadialGradientFillPipeline(options);
      };
  return ColorSourceContents::DrawGeometry<VS>(
      renderer, entity, pass, pipeline_callback, frame_info,
      [this, &renderer, &gradient_texture, &entity](RenderPass& pass) {
        FS::FragInfo frag_info;
        frag_info.center = center_;
        frag_info.radius = radius_;
        frag_info.tile_mode = static_cast<Scalar>(tile_mode_);
        frag_info.decal_border_color = decal_border_color_;
        frag_info.texture_sampler_y_coord_scale =
            gradient_texture->GetYCoordScale();
        frag_info.alpha =
            GetOpacityFactor() *
            GetGeometry()->ComputeAlphaCoverage(entity.GetTransform());
        frag_info.half_texel =
            Vector2(0.5 / gradient_texture->GetSize().width,
                    0.5 / gradient_texture->GetSize().height);

        SamplerDescriptor sampler_desc;
        sampler_desc.min_filter = MinMagFilter::kLinear;
        sampler_desc.mag_filter = MinMagFilter::kLinear;

        pass.SetCommandLabel("RadialGradientFill");

        FS::BindFragInfo(
            pass, renderer.GetTransientsBuffer().EmplaceUniform(frag_info));
        FS::BindTextureSampler(
            pass, gradient_texture,
            renderer.GetContext()->GetSamplerLibrary()->GetSampler(
                sampler_desc));

        return true;
      });
}

bool RadialGradientContents::ApplyColorFilter(
    const ColorFilterProc& color_filter_proc) {
  for (Color& color : colors_) {
    color = color_filter_proc(color);
  }
  decal_border_color_ = color_filter_proc(decal_border_color_);
  return true;
}

}  // namespace impeller
