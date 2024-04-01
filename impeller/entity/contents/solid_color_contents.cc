// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "solid_color_contents.h"

#include "impeller/entity/contents/content_context.h"
#include "impeller/entity/entity.h"
#include "impeller/entity/geometry/geometry.h"
#include "impeller/geometry/path.h"
#include "impeller/renderer/render_pass.h"

namespace impeller {

SolidColorContents::SolidColorContents() = default;

void SolidColorContents::SetColor(Color color) {
  color_ = color;
}

Color SolidColorContents::GetColor() const {
  return color_.WithAlpha(color_.alpha * GetOpacityFactor());
}

bool SolidColorContents::IsSolidColor() const {
  return true;
}

bool SolidColorContents::IsOpaque() const {
  return GetColor().IsOpaque();
}

std::optional<Rect> SolidColorContents::GetCoverage(
    const Entity& entity) const {
  if (GetColor().IsTransparent()) {
    return std::nullopt;
  }

  auto* geometry = GetGeometry();
  if (geometry == nullptr) {
    return std::nullopt;
  }
  return geometry->GetCoverage(entity.GetTransform());
};

bool SolidColorContents::Render(const ContentContext& renderer,
                                const Entity& entity,
                                RenderPass& pass) const {
  auto capture = entity.GetCapture().CreateChild("SolidColorContents");
  using VS = SolidFillPipeline::VertexShader;

  VS::FrameInfo frame_info;
  frame_info.color = capture.AddColor("Color", GetColor()).Premultiply();

  PipelineBuilderCallback pipeline_callback =
      [&renderer](ContentContextOptions options) {
        return renderer.GetSolidFillPipeline(options);
      };
  return ColorSourceContents::DrawGeometry<VS>(
      renderer, entity, pass, pipeline_callback, frame_info,
      [](RenderPass& pass) {
        pass.SetCommandLabel("Solid Fill");
        return true;
      });
}

std::unique_ptr<SolidColorContents> SolidColorContents::Make(const Path& path,
                                                             Color color) {
  auto contents = std::make_unique<SolidColorContents>();
 // contents->SetGeometry(Geometry::MakeFillPath(path));
  contents->SetColor(color);
  return contents;
}

std::optional<Color> SolidColorContents::AsBackgroundColor(
    const Entity& entity,
    ISize target_size) const {
  auto* geometry = GetGeometry();
  if (!geometry) {
    return std::nullopt;
  }
  Rect target_rect = Rect::MakeSize(target_size);
  return geometry->CoversArea(entity.GetTransform(), target_rect)
             ? GetColor()
             : std::optional<Color>();
}

bool SolidColorContents::ApplyColorFilter(
    const ColorFilterProc& color_filter_proc) {
  color_ = color_filter_proc(color_);
  return true;
}

}  // namespace impeller
