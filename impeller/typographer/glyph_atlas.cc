// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include "impeller/typographer/glyph_atlas.h"

namespace impeller {

GlyphAtlasContext::GlyphAtlasContext()
    : atlas_(std::make_shared<GlyphAtlas>(GlyphAtlas::Type::kAlphaBitmap)) {}

GlyphAtlasContext::~GlyphAtlasContext() {}

std::shared_ptr<GlyphAtlas> GlyphAtlasContext::GetGlyphAtlas() const {
  return atlas_;
}

void GlyphAtlasContext::UpdateGlyphAtlas(std::shared_ptr<GlyphAtlas> atlas) {
  atlas_ = atlas;
}

std::shared_ptr<void> GlyphAtlasContext::GetExtraData() const {
  return data_;
}

void GlyphAtlasContext::SetExtraData(std::shared_ptr<void> data) {
  data_ = data;
}

GlyphAtlas::GlyphAtlas(Type type) : type_(type) {}

GlyphAtlas::~GlyphAtlas() = default;

bool GlyphAtlas::IsValid() const {
  return !!texture_;
}

GlyphAtlas::Type GlyphAtlas::GetType() const {
  return type_;
}

const std::shared_ptr<Texture>& GlyphAtlas::GetTexture() const {
  return texture_;
}

void GlyphAtlas::SetTexture(std::shared_ptr<Texture> texture) {
  texture_ = std::move(texture);
}

void GlyphAtlas::AddTypefaceGlyphPosition(const FontGlyphPair& pair,
                                          Rect rect) {
  positions_[pair] = rect;
}

std::optional<Rect> GlyphAtlas::FindFontGlyphPosition(
    const FontGlyphPair& pair) const {
  auto found = positions_.find(pair);
  if (found == positions_.end()) {
    return std::nullopt;
  }
  return found->second;
}

size_t GlyphAtlas::GetGlyphCount() const {
  return positions_.size();
}

size_t GlyphAtlas::IterateGlyphs(
    const std::function<bool(const FontGlyphPair& pair, const Rect& rect)>&
        iterator) const {
  if (!iterator) {
    return 0u;
  }

  size_t count = 0u;
  for (const auto& position : positions_) {
    count++;
    if (!iterator(position.first, position.second)) {
      return count;
    }
  }
  return count;
}

size_t GlyphAtlas::IterateSubsetGlyphs(
    const FontGlyphPair::Vector& glyphs,
    const std::function<bool(const FontGlyphPair& pair, const Rect& rect)>&
        iterator) const {
  if (!iterator) {
    return 0u;
  }

  size_t count = 0u;
  for (const auto& glyph : glyphs) {
    auto position = positions_.find(glyph);
    if (position == positions_.end()) {
      continue;
    }
    count++;
    if (!iterator(position->first, position->second)) {
      return count;
    }
  }
  return count;
}

bool GlyphAtlas::HasSamePairs(const FontGlyphPair::Vector& new_glyphs) {
  for (const auto& pair : new_glyphs) {
    if (positions_.find(pair) == positions_.end()) {
      return false;
    }
  }
  return true;
}

FontGlyphPair::Vector GlyphAtlas::CollectNewGlyphs(
    const FontGlyphPair::Vector& glyphs) {
  FontGlyphPair::Vector new_glyphs;
  for (const auto& pair : glyphs) {
    if (positions_.find(pair) == positions_.end()) {
      new_glyphs.push_back(pair);
    }
  }
  return new_glyphs;
}

}  // namespace impeller
