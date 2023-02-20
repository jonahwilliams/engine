// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/entity/contents/text_color_contents.h"
#include "impeller/entity/contents/content_context.h"
#include "impeller/entity/contents/texture_contents.h"
#include "impeller/renderer/render_pass.h"

#include <iostream>

namespace impeller {

TextColorContents::TextColorContents() {}

TextColorContents::~TextColorContents() {}

void TextColorContents::SetTextContents(
    std::shared_ptr<TextContents> text_contents) {
  text_contents_ = std::move(text_contents);
}

void TextColorContents::SetColorSourceProc(
    std::shared_ptr<ColorSourceContents> color_contents) {
  color_contents_ = std::move(color_contents);
}

void TextColorContents::SetTransform(Matrix transform) {
  transform_ = transform;
}

// |Contents|
std::optional<Rect> TextColorContents::GetCoverage(const Entity& entity) const {
  Entity fake_entity;
  fake_entity.SetTransformation(transform_);
  return text_contents_->GetCoverage(fake_entity);
}

// |Contents|
bool TextColorContents::Render(const ContentContext& renderer,
                               const Entity& entity,
                               RenderPass& pass) const {
  Entity fake_entity;
  fake_entity.SetTransformation(transform_);
  auto coverage = text_contents_->GetCoverage(fake_entity);
  if (!coverage.has_value() || coverage->IsEmpty()) {
    return true;
  }
  auto pass_size = ISize::Ceil(coverage->size);
  auto texture = renderer.MakeSubpass(
      pass_size,
      [&](const ContentContext& context, RenderPass& pass) {
        Entity sub_entity;
        sub_entity.SetStencilDepth(entity.GetStencilDepth());
        sub_entity.SetTransformation(transform_);
        sub_entity.SetContents(text_contents_);
        sub_entity.SetBlendMode(BlendMode::kSource);

        if (!sub_entity.Render(renderer, pass)) {
          return false;
        }
        auto geom_rect =
            Rect::MakeXYWH(coverage->origin.x, coverage->origin.y,
                           coverage->size.width, coverage->size.height);
        color_contents_->SetGeometry(Geometry::MakeRect(geom_rect));

        // Reset the transformation as the rect created from the text contents
        // coverage will already be transformed to global coordinates.
        sub_entity.SetTransformation(Matrix());
        sub_entity.SetContents(color_contents_);
        sub_entity.SetBlendMode(BlendMode::kSourceIn);

        return sub_entity.Render(renderer, pass);
      });

  auto texture_contents = TextureContents::MakeRect(*coverage);
  texture_contents->SetTexture(texture);
  texture_contents->SetSourceRect(Rect::MakeSize(texture->GetSize()));
  fake_entity.SetTransformation(Matrix());

  return texture_contents->Render(renderer, fake_entity, pass);
}

}  // namespace impeller
