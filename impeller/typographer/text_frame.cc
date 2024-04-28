// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/typographer/text_frame.h"
#include "impeller/typographer/font_glyph_pair.h"

namespace impeller {

TextFrame::TextFrame() = default;

TextFrame::TextFrame(std::vector<TextRun>& runs,
                     Rect bounds,
                     bool has_color,
                     bool is_stroke)
    : runs_(std::move(runs)),
      bounds_(bounds),
      has_color_(has_color),
      is_stroke_(is_stroke) {}

TextFrame::~TextFrame() = default;

Rect TextFrame::GetBounds() const {
  return bounds_;
}

size_t TextFrame::GetRunCount() const {
  return runs_.size();
}

bool TextFrame::GetIsStroke() const {
  return is_stroke_;
}

const std::vector<TextRun>& TextFrame::GetRuns() const {
  return runs_;
}

GlyphAtlas::Type TextFrame::GetAtlasType() const {
  return has_color_ ? GlyphAtlas::Type::kColorBitmap
                    : GlyphAtlas::Type::kAlphaBitmap;
}

bool TextFrame::MaybeHasOverlapping() const {
  if (runs_.size() > 1) {
    return true;
  }
  auto glyph_positions = runs_[0].GetGlyphPositions();
  if (glyph_positions.size() > 10) {
    return true;
  }
  if (glyph_positions.size() == 1) {
    return false;
  }
  // To avoid quadradic behavior the overlapping is checked against an
  // accumulated bounds rect. This gives faster but less precise information
  // on text runs.
  auto first_position = glyph_positions[0];
  auto overlapping_rect = Rect::MakeOriginSize(
      first_position.position + first_position.glyph.bounds.GetOrigin(),
      first_position.glyph.bounds.GetSize());
  for (auto i = 1u; i < glyph_positions.size(); i++) {
    auto glyph_position = glyph_positions[i];
    auto glyph_rect = Rect::MakeOriginSize(
        glyph_position.position + glyph_position.glyph.bounds.GetOrigin(),
        glyph_position.glyph.bounds.GetSize());
    auto intersection = glyph_rect.Intersection(overlapping_rect);
    if (intersection.has_value()) {
      return true;
    }
    overlapping_rect = overlapping_rect.Union(glyph_rect);
  }
  return false;
}

// static
Scalar TextFrame::RoundScaledFontSize(Scalar scale, Scalar point_size) {
  // An arbitrarily chosen maximum text scale to ensure that regardless of the
  // CTM, a glyph will fit in the atlas. If we clamp significantly, this may
  // reduce fidelity but is preferable to the alternative of failing to render.
  constexpr Scalar kMaximumTextScale = 48;
  Scalar result = std::round(scale * 100) / 100;
  return std::clamp(result, 0.0f, kMaximumTextScale);
}

void TextFrame::CollectUniqueFontGlyphPairs(FontGlyphMap& glyph_map,
                                            Scalar scale) const {
  for (const TextRun& run : GetRuns()) {
    const Font& font = run.GetFont();
    auto rounded_scale =
        RoundScaledFontSize(scale, font.GetMetrics().point_size);
    auto& set = glyph_map[ScaledFont{
        .font = font, .scale = rounded_scale, .stroke = GetIsStroke()}];
    for (const TextRun::GlyphPosition& glyph_position :
         run.GetGlyphPositions()) {
#if false
// Glyph size error due to RoundScaledFontSize usage above.
if (rounded_scale != scale) {
  auto delta = std::abs(rounded_scale - scale);
  FML_LOG(ERROR) << glyph_position.glyph.bounds.size * delta;
}
#endif
      set.insert(glyph_position.glyph);
    }
  }
}

}  // namespace impeller
