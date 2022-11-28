// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/entity/contents/rrect_shadow_contents.h"
#include <optional>

#include "impeller/entity/contents/content_context.h"
#include "impeller/entity/entity.h"
#include "impeller/geometry/path.h"
#include "impeller/geometry/path_builder.h"
#include "impeller/renderer/render_pass.h"
#include "impeller/renderer/vertex_buffer_builder.h"
#include "impeller/tessellator/tessellator.h"

namespace impeller {

RRectShadowContents::RRectShadowContents() = default;

RRectShadowContents::~RRectShadowContents() = default;

void RRectShadowContents::SetRRect(std::optional<Rect> rect,
                                   Scalar corner_radius) {
  rect_ = rect;
  corner_radius_ = corner_radius;
}

void RRectShadowContents::SetSigma(Sigma sigma) {
  sigma_ = sigma;
}

void RRectShadowContents::SetColor(Color color) {
  color_ = color.Premultiply();
}

std::optional<Rect> RRectShadowContents::GetCoverage(
    const Entity& entity) const {
  if (!rect_.has_value()) {
    return std::nullopt;
  }

  Scalar radius = Radius{sigma_}.radius;

  auto ltrb = rect_->GetLTRB();
  Rect bounds = Rect::MakeLTRB(ltrb[0] - radius, ltrb[1] - radius,
                               ltrb[2] + radius, ltrb[3] + radius);
  return bounds.TransformBounds(entity.GetTransformation());
};

bool RRectShadowContents::Render(const ContentContext& renderer,
                                 const Entity& entity,
                                 RenderPass& pass) const {
  if (!rect_.has_value()) {
    return true;
  }

  using VS = RRectBlurPipeline::VertexShader;
  VertexBufferBuilder<VS::PerVertexData> vtx_builder;

  auto blur_radius = Radius{sigma_}.radius;
  auto positive_rect = rect_->GetPositive();
  {
    auto left = -blur_radius;
    auto top = -blur_radius;
    auto right = positive_rect.size.width + blur_radius;
    auto bottom = positive_rect.size.height + blur_radius;

    vtx_builder.AddVertices({
        {Point(left, top)},
        {Point(right, top)},
        {Point(left, bottom)},
        {Point(right, bottom)},
    });
    vtx_builder.AddIndices({0, 1, 2, 1, 2, 3});
  }

  Command cmd;
  cmd.label = "RRect Shadow";
  cmd.stencil_reference = entity.GetStencilDepth();

  cmd.BindVertices(vtx_builder.CreateVertexBuffer(pass.GetTransientsBuffer()));

  if (sigma_.sigma > 0) {
    return RenderWithSigma(renderer, entity, pass, std::move(cmd));
  }
  return RenderNoSigma(renderer, entity, pass, std::move(cmd));
}

bool RRectShadowContents::RenderWithSigma(const ContentContext& renderer,
                                          const Entity& entity,
                                          RenderPass& pass,
                                          Command cmd) const {
  using FS = RRectBlurPipeline::FragmentShader;
  using VS = RRectBlurPipeline::VertexShader;

  auto positive_rect = rect_->GetPositive();
  auto opts = OptionsFromPassAndEntity(pass, entity);
  opts.primitive_type = PrimitiveType::kTriangle;
  cmd.pipeline = renderer.GetRRectBlurPipeline(opts);

  VS::VertInfo vert_info;
  vert_info.mvp = Matrix::MakeOrthographic(pass.GetRenderTargetSize()) *
                  entity.GetTransformation() *
                  Matrix::MakeTranslation({positive_rect.origin});
  VS::BindVertInfo(cmd, pass.GetTransientsBuffer().EmplaceUniform(vert_info));

  FS::FragInfo frag_info;
  frag_info.color = color_;
  frag_info.blur_sigma = sigma_.sigma;
  frag_info.rect_size = Point(positive_rect.size);
  frag_info.corner_radius =
      std::min(corner_radius_, std::min(positive_rect.size.width / 2.0f,
                                        positive_rect.size.height / 2.0f));
  FS::BindFragInfo(cmd, pass.GetTransientsBuffer().EmplaceUniform(frag_info));

  if (!pass.AddCommand(std::move(cmd))) {
    return false;
  }

  return true;
}

bool RRectShadowContents::RenderNoSigma(const ContentContext& renderer,
                                        const Entity& entity,
                                        RenderPass& pass,
                                        Command cmd) const {
  using FS = RRectBlurNoSigmaPipeline::FragmentShader;
  using VS = RRectBlurNoSigmaPipeline::VertexShader;

  auto positive_rect = rect_->GetPositive();
  auto opts = OptionsFromPassAndEntity(pass, entity);
  opts.primitive_type = PrimitiveType::kTriangle;
  cmd.pipeline = renderer.GetRRectBlurNoSigmaPipeline(opts);

  VS::VertInfo vert_info;
  vert_info.mvp = Matrix::MakeOrthographic(pass.GetRenderTargetSize()) *
                  entity.GetTransformation() *
                  Matrix::MakeTranslation({positive_rect.origin});
  VS::BindVertInfo(cmd, pass.GetTransientsBuffer().EmplaceUniform(vert_info));

  FS::FragInfo frag_info;
  frag_info.color = color_;
  frag_info.rect_size = Point(positive_rect.size);
  FS::BindFragInfo(cmd, pass.GetTransientsBuffer().EmplaceUniform(frag_info));

  if (!pass.AddCommand(std::move(cmd))) {
    return false;
  }

  return true;
}

}  // namespace impeller
