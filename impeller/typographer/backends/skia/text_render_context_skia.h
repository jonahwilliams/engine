// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "flutter/fml/macros.h"
#include "impeller/typographer/text_render_context.h"
#include "third_party/skia/src/gpu/GrRectanizer.h"  // nogncheck

class SkBitmap;

namespace impeller {

class TextRenderContextSkia : public TextRenderContext {
 public:
  TextRenderContextSkia(std::shared_ptr<Context> context);

  ~TextRenderContextSkia() override;

  // |TextRenderContext|
  std::shared_ptr<GlyphAtlas> CreateGlyphAtlas(
      GlyphAtlas::Type type,
      std::shared_ptr<GlyphAtlasContext> atlas_context,
      FrameIterator iterator) const override;

 private:
  bool AttemptToRecycleOldAtlas(
      GlyphAtlas::Type type,
      std::shared_ptr<GlyphAtlas> prev_atlas,
      std::shared_ptr<GlyphAtlasContext> atlas_context,
      const FontGlyphPair::Vector& font_glyph_pairs) const;

  FML_DISALLOW_COPY_AND_ASSIGN(TextRenderContextSkia);
};

class SkiaAtlasData {
 public:
  SkiaAtlasData();

  ~SkiaAtlasData();

  bool IsValid() const;

  std::shared_ptr<GrRectanizer> GetRectPacker() const;

  std::shared_ptr<SkBitmap> GetBitmap() const;

  const std::vector<Rect>& GetGlyphPositions() const;

  ISize GetSize() const;

  void SetRectPacker(std::shared_ptr<GrRectanizer> rect_packer);

  void SetBitmap(std::shared_ptr<SkBitmap> bitmap);

  void SetGlyphPositions(std::vector<Rect> glyph_positions);

  void SetSize(ISize size);

 private:
  std::shared_ptr<GrRectanizer> rect_packer_;
  std::vector<Rect> glyph_positions_;
  std::shared_ptr<SkBitmap> bitmap_;
  ISize size_;
};

}  // namespace impeller
