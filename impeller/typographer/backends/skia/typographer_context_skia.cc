// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/typographer/backends/skia/typographer_context_skia.h"

#include <cstddef>
#include <memory>
#include <numeric>
#include <utility>
#include <vector>

#include "flutter/fml/logging.h"
#include "flutter/fml/trace_event.h"
#include "fml/closure.h"

#include "impeller/core/allocator.h"
#include "impeller/core/buffer_view.h"
#include "impeller/core/formats.h"
#include "impeller/core/host_buffer.h"
#include "impeller/core/platform.h"
#include "impeller/core/texture_descriptor.h"
#include "impeller/geometry/size.h"
#include "impeller/renderer/command_buffer.h"
#include "impeller/renderer/render_pass.h"
#include "impeller/renderer/render_target.h"
#include "impeller/typographer/backends/skia/typeface_skia.h"
#include "impeller/typographer/font_glyph_pair.h"
#include "impeller/typographer/glyph_atlas.h"
#include "impeller/typographer/rectangle_packer.h"
#include "impeller/typographer/typographer_context.h"
#include "include/core/SkColor.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPixelRef.h"
#include "include/core/SkSize.h"

#include "include/private/base/SkPoint_impl.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace impeller {

// TODO(bdero): We might be able to remove this per-glyph padding if we fix
//              the underlying causes of the overlap.
//              https://github.com/flutter/flutter/issues/114563
constexpr auto kPadding = 2;

std::shared_ptr<TypographerContext> TypographerContextSkia::Make() {
  return std::make_shared<TypographerContextSkia>();
}

TypographerContextSkia::TypographerContextSkia() = default;

TypographerContextSkia::~TypographerContextSkia() = default;

std::shared_ptr<GlyphAtlasContext>
TypographerContextSkia::CreateGlyphAtlasContext(GlyphAtlas::Type type) const {
  return std::make_shared<GlyphAtlasContext>(type);
}

static SkImageInfo GetImageInfo(const GlyphAtlas& atlas, Size size) {
  switch (atlas.GetType()) {
    case GlyphAtlas::Type::kAlphaBitmap:
      return SkImageInfo::MakeA8(SkISize{static_cast<int32_t>(size.width),
                                         static_cast<int32_t>(size.height)});
    case GlyphAtlas::Type::kColorBitmap:
      return SkImageInfo::MakeN32Premul(size.width, size.height);
  }
  FML_UNREACHABLE();
}

/// Append as many glyphs to the texture as will fit, and return the first index
/// of [extra_pairs] that did not fit.
static size_t AppendToExistingAtlas(
    const std::shared_ptr<GlyphAtlas>& atlas,
    const std::vector<FontGlyphPair>& extra_pairs,
    std::vector<Rect>& glyph_positions,
    ISize atlas_size,
    int64_t height_adjustment,
    const std::shared_ptr<RectanglePacker>& rect_packer) {
  TRACE_EVENT0("impeller", __FUNCTION__);
  if (!rect_packer || atlas_size.IsEmpty()) {
    return 0;
  }

  for (size_t i = 0; i < extra_pairs.size(); i++) {
    const FontGlyphPair& pair = extra_pairs[i];
    const auto glyph_size =
        ISize::Ceil(pair.glyph.bounds.GetSize() * pair.scaled_font.scale);
    IPoint16 location_in_atlas;
    if (!rect_packer->AddRect(glyph_size.width + kPadding,   //
                              glyph_size.height + kPadding,  //
                              &location_in_atlas             //
                              )) {
      return i;
    }
    glyph_positions.push_back(Rect::MakeXYWH(
        location_in_atlas.x(),                      //
        location_in_atlas.y() + height_adjustment,  //
        glyph_size.width,                           //
        glyph_size.height                           //
        ));
  }

  return extra_pairs.size();
}

static size_t PairsFitInAtlasOfSize(
    const std::vector<FontGlyphPair>& pairs,
    const ISize& atlas_size,
    std::vector<Rect>& glyph_positions,
    int64_t height_adjustment,
    const std::shared_ptr<RectanglePacker>& rect_packer,
    size_t start_index) {
  FML_DCHECK(!atlas_size.IsEmpty());

  for (size_t i = start_index; i < pairs.size(); i++) {
    const auto& pair = pairs[i];
    const auto glyph_size =
        ISize::Ceil(pair.glyph.bounds.GetSize() * pair.scaled_font.scale);
    IPoint16 location_in_atlas;
    if (!rect_packer->AddRect(glyph_size.width + kPadding,   //
                              glyph_size.height + kPadding,  //
                              &location_in_atlas             //
                              )) {
      return i;
    }
    glyph_positions.push_back(Rect::MakeXYWH(
        location_in_atlas.x(),                      //
        location_in_atlas.y() + height_adjustment,  //
        glyph_size.width,                           //
        glyph_size.height                           //
        ));
  }

  return pairs.size();
}

static ISize ComputeNextAtlasSize(
    const std::shared_ptr<GlyphAtlasContext>& atlas_context,
    const std::vector<FontGlyphPair>& extra_pairs,
    std::vector<Rect>& glyph_positions,
    size_t glyph_index_start,
    int64_t max_texture_height) {
  // Because we can't grow the skyline packer horizontally, pick a reasonable
  // large width for all atlases.
  static constexpr int64_t kAtlasWidth = 4096;
  static constexpr int64_t kMinAtlasHeight = 1024;

  ISize current_size = ISize(kAtlasWidth, kMinAtlasHeight);
  if (atlas_context->GetAtlasSize().height > current_size.height) {
    current_size.height = atlas_context->GetAtlasSize().height * 2;
  }

  auto height_adjustment = atlas_context->GetAtlasSize().height;
  while (current_size.height <= max_texture_height) {
    std::shared_ptr<RectanglePacker> rect_packer;
    if (atlas_context->GetRectPacker() || glyph_index_start) {
      rect_packer = RectanglePacker::Factory(
          kAtlasWidth,
          current_size.height - atlas_context->GetAtlasSize().height);
    } else {
      rect_packer = RectanglePacker::Factory(kAtlasWidth, current_size.height);
    }
    glyph_positions.erase(glyph_positions.begin() + glyph_index_start,
                          glyph_positions.end());
    atlas_context->UpdateRectPacker(rect_packer);
    auto next_index = PairsFitInAtlasOfSize(extra_pairs, current_size,
                                            glyph_positions, height_adjustment,
                                            rect_packer, glyph_index_start);
    if (next_index == extra_pairs.size()) {
      return current_size;
    }
    current_size = ISize(current_size.width, current_size.height * 2);
  }
  return {};
}

static void DrawGlyph(SkCanvas* canvas,
                      SkPoint position,
                      const ScaledFont& scaled_font,
                      const Glyph& glyph,
                      bool has_color) {
  const auto& metrics = scaled_font.font.GetMetrics();
  SkGlyphID glyph_id = glyph.index;

  SkFont sk_font(
      TypefaceSkia::Cast(*scaled_font.font.GetTypeface()).GetSkiaTypeface(),
      metrics.point_size, metrics.scaleX, metrics.skewX);
  sk_font.setEdging(SkFont::Edging::kAntiAlias);
  sk_font.setHinting(SkFontHinting::kSlight);
  sk_font.setEmbolden(metrics.embolden);

  auto glyph_color = has_color ? scaled_font.color.ToARGB() : SK_ColorBLACK;

  SkPaint glyph_paint;
  glyph_paint.setColor(glyph_color);
  canvas->resetMatrix();
  canvas->scale(scaled_font.scale, scaled_font.scale);

  canvas->drawGlyphs(
      1u,         // count
      &glyph_id,  // glyphs
      &position,  // positions
      SkPoint::Make(-glyph.bounds.GetLeft(), -glyph.bounds.GetTop()),  // origin
      sk_font,                                                         // font
      glyph_paint                                                      // paint
  );
}

static bool UpdateAtlasBitmap(const GlyphAtlas& atlas,
                              std::shared_ptr<BlitPass>& blit_pass,
                              HostBuffer& host_buffer,
                              const std::shared_ptr<Texture>& texture,
                              const std::vector<FontGlyphPair>& new_pairs,
                              size_t start_index,
                              size_t end_index) {
  TRACE_EVENT0("impeller", __FUNCTION__);

  bool has_color = atlas.GetType() == GlyphAtlas::Type::kColorBitmap;

  // Render rows together.
  struct State {
    Rect position;
    FontGlyphPair pair;
  };
  std::vector<State> states;

  // Attempt to render all glyphs within a single row together.
  auto flush = [&]() -> bool {
    if (states.empty()) {
      return true;
    }
    Rect bounds = states[0].position;
    for (auto i = 1u; i < states.size(); i++) {
      bounds = bounds.Union(states[i].position);
    }
    Point offset = bounds.GetLeftTop();
    SkBitmap bitmap;
    bitmap.setInfo(GetImageInfo(atlas, bounds.GetSize()));
    if (!bitmap.tryAllocPixels()) {
      return false;
    }
    auto surface = SkSurfaces::WrapPixels(bitmap.pixmap());
    if (!surface) {
      return false;
    }
    auto canvas = surface->getCanvas();
    if (!canvas) {
      return false;
    }

    for (auto& state : states) {
      SkPoint position = SkPoint::Make(
          (state.position.GetX() - offset.x) / state.pair.scaled_font.scale,
          (state.position.GetY() - offset.y) / state.pair.scaled_font.scale);
      DrawGlyph(canvas, position, state.pair.scaled_font, state.pair.glyph,
                has_color);
    }
    auto buffer_view = host_buffer.Emplace(
        bitmap.getAddr(0, 0), bounds.GetSize().Area() * (has_color ? 4u : 1u),
        DefaultUniformAlignment());

    if (!blit_pass->AddCopy(
            buffer_view, texture,
            IRect::MakeXYWH(bounds.GetLeft(), bounds.GetTop(),
                            bounds.GetWidth(), bounds.GetHeight()))) {
      return false;
    }
    return true;
  };

  for (size_t i = start_index; i < end_index; i++) {
    const FontGlyphPair& pair = new_pairs[i];
    auto pos = atlas.FindFontGlyphBounds(pair);
    if (!pos.has_value()) {
      continue;
    }
    Size size = pos->GetSize();
    if (size.IsEmpty()) {
      continue;
    }
    if (!states.empty() && states.back().position.GetX() == pos->GetX()) {
      if (!flush()) {
        return false;
      }
    }
    states.push_back(State{.position = pos.value(), .pair = pair});
  }
  return flush();
}

std::shared_ptr<GlyphAtlas> TypographerContextSkia::CreateGlyphAtlas(
    Context& context,
    GlyphAtlas::Type type,
    HostBuffer& host_buffer,
    const std::shared_ptr<GlyphAtlasContext>& atlas_context,
    const FontGlyphMap& font_glyph_map) const {
  TRACE_EVENT0("impeller", __FUNCTION__);
  if (!IsValid()) {
    return nullptr;
  }
  std::shared_ptr<GlyphAtlas> last_atlas = atlas_context->GetGlyphAtlas();
  FML_DCHECK(last_atlas->GetType() == type);

  if (font_glyph_map.empty()) {
    return last_atlas;
  }

  // ---------------------------------------------------------------------------
  // Step 1: Determine if the atlas type and font glyph pairs are compatible
  //         with the current atlas and reuse if possible.
  // ---------------------------------------------------------------------------
  std::vector<FontGlyphPair> new_glyphs;
  for (const auto& font_value : font_glyph_map) {
    const ScaledFont& scaled_font = font_value.first;
    const FontGlyphAtlas* font_glyph_atlas = last_atlas->GetFontGlyphAtlas(
        scaled_font.font, scaled_font.scale, scaled_font.color);
    if (font_glyph_atlas) {
      for (const Glyph& glyph : font_value.second) {
        if (!font_glyph_atlas->FindGlyphBounds(glyph)) {
          new_glyphs.emplace_back(scaled_font, glyph);
        }
      }
    } else {
      for (const Glyph& glyph : font_value.second) {
        new_glyphs.emplace_back(scaled_font, glyph);
      }
    }
  }
  if (new_glyphs.size() == 0) {
    return last_atlas;
  }

  // ---------------------------------------------------------------------------
  // Step 2: Determine if the additional missing glyphs can be appended to the
  //         existing bitmap without recreating the atlas.
  // ---------------------------------------------------------------------------
  std::vector<Rect> glyph_positions;
  glyph_positions.reserve(new_glyphs.size());
  size_t first_missing_index = 0;

  if (last_atlas->GetTexture()) {
    // Append all glyphs that fit into the current atlas.
    first_missing_index = AppendToExistingAtlas(
        last_atlas, new_glyphs, glyph_positions, atlas_context->GetAtlasSize(),
        atlas_context->GetHeightAdjustment(), atlas_context->GetRectPacker());

    // ---------------------------------------------------------------------------
    // Step 3a: Record the positions in the glyph atlas of the newly added
    //          glyphs.
    // ---------------------------------------------------------------------------
    for (size_t i = 0; i < first_missing_index; i++) {
      last_atlas->AddTypefaceGlyphPosition(new_glyphs[i], glyph_positions[i]);
    }

    std::shared_ptr<CommandBuffer> cmd_buffer = context.CreateCommandBuffer();
    std::shared_ptr<BlitPass> blit_pass = cmd_buffer->CreateBlitPass();

    fml::ScopedCleanupClosure closure([&]() {
      blit_pass->EncodeCommands(context.GetResourceAllocator());
      context.GetCommandQueue()->Submit({std::move(cmd_buffer)});
    });

    // ---------------------------------------------------------------------------
    // Step 4a: Draw new font-glyph pairs into the a host buffer and encode
    // the uploads into the blit pass.
    // ---------------------------------------------------------------------------
    if (!UpdateAtlasBitmap(*last_atlas, blit_pass, host_buffer,
                           last_atlas->GetTexture(), new_glyphs, 0,
                           first_missing_index)) {
      return nullptr;
    }

    // If all glyphs fit, just return the old atlas.
    if (first_missing_index == new_glyphs.size()) {
      return last_atlas;
    }
  }

  int64_t height_adjustment = atlas_context->GetAtlasSize().height;
  const int64_t max_texture_height =
      context.GetResourceAllocator()->GetMaxTextureSizeSupported().height;

  // IF the current atlas size is as big as it can get, then "GC" and create an
  // atlas with only the required glyphs. OpenGLES cannot reliably perform the
  // blit required here, as 1) it requires attaching textures as read and write
  // framebuffers which has substantially smaller size limits that max textures
  // and 2) is missing a GLES 2.0 implementation and cap check.
  bool blit_old_atlas = true;
  std::shared_ptr<GlyphAtlas> new_atlas = last_atlas;
  if (atlas_context->GetAtlasSize().height >= max_texture_height ||
      context.GetBackendType() == Context::BackendType::kOpenGLES) {
    blit_old_atlas = false;
    first_missing_index = 0;
    glyph_positions.clear();
    height_adjustment = 0;
    new_atlas = std::make_shared<GlyphAtlas>(type);
    atlas_context->UpdateRectPacker(nullptr);
    atlas_context->UpdateGlyphAtlas(new_atlas, {0, 0}, 0);
  }

  // A new glyph atlas must be created.
  ISize atlas_size = ComputeNextAtlasSize(atlas_context,        //
                                          new_glyphs,           //
                                          glyph_positions,      //
                                          first_missing_index,  //
                                          max_texture_height    //
  );

  atlas_context->UpdateGlyphAtlas(new_atlas, atlas_size, height_adjustment);
  if (atlas_size.IsEmpty()) {
    return nullptr;
  }
  FML_DCHECK(new_glyphs.size() == glyph_positions.size());

  TextureDescriptor descriptor;
  switch (type) {
    case GlyphAtlas::Type::kAlphaBitmap:
      descriptor.format =
          context.GetCapabilities()->GetDefaultGlyphAtlasFormat();
      break;
    case GlyphAtlas::Type::kColorBitmap:
      descriptor.format = PixelFormat::kR8G8B8A8UNormInt;
      break;
  }
  descriptor.size = atlas_size;
  descriptor.storage_mode = StorageMode::kDevicePrivate;
  descriptor.usage = TextureUsage::kShaderRead;
  std::shared_ptr<Texture> new_texture =
      context.GetResourceAllocator()->CreateTexture(descriptor);
  if (!new_texture) {
    return nullptr;
  }

  new_texture->SetLabel("GlyphAtlas");

  std::shared_ptr<CommandBuffer> cmd_buffer = context.CreateCommandBuffer();
  std::shared_ptr<BlitPass> blit_pass = cmd_buffer->CreateBlitPass();

  // The R8/A8 textures used for certain glyphs is not supported as color
  // attachments in most graphics drivers. For other textures, most framebuffer
  // attachments have a much smaller size limit than the max texture size.
  {
    TRACE_EVENT0("flutter", "ClearGlyphAtlas");
    size_t byte_size =
        new_texture->GetTextureDescriptor().GetByteSizeOfBaseMipLevel();
    BufferView buffer_view =
        host_buffer.Emplace(nullptr, byte_size, DefaultUniformAlignment());

    ::memset(buffer_view.buffer->OnGetContents() + buffer_view.range.offset, 0,
             byte_size);
    buffer_view.buffer->Flush();
    blit_pass->AddCopy(buffer_view, new_texture);
  }

  fml::ScopedCleanupClosure closure([&]() {
    blit_pass->EncodeCommands(context.GetResourceAllocator());
    context.GetCommandQueue()->Submit({std::move(cmd_buffer)});
  });

  // Blit the old texture to the top left of the new atlas.
  if (new_atlas->GetTexture() && blit_old_atlas) {
    blit_pass->AddCopy(new_atlas->GetTexture(), new_texture,
                       IRect::MakeSize(new_atlas->GetTexture()->GetSize()),
                       {0, 0});
  }

  // Now append all remaining glyphs. This should never have any missing data...
  new_atlas->SetTexture(std::move(new_texture));

  // ---------------------------------------------------------------------------
  // Step 3a: Record the positions in the glyph atlas of the newly added
  //          glyphs.
  // ---------------------------------------------------------------------------
  for (size_t i = first_missing_index; i < glyph_positions.size(); i++) {
    new_atlas->AddTypefaceGlyphPosition(new_glyphs[i], glyph_positions[i]);
  }

  // ---------------------------------------------------------------------------
  // Step 4a: Draw new font-glyph pairs into the a host buffer and encode
  // the uploads into the blit pass.
  // ---------------------------------------------------------------------------
  if (!UpdateAtlasBitmap(*new_atlas, blit_pass, host_buffer,
                         new_atlas->GetTexture(), new_glyphs,
                         first_missing_index, new_glyphs.size())) {
    return nullptr;
  }
  // ---------------------------------------------------------------------------
  // Step 8b: Record the texture in the glyph atlas.
  // ---------------------------------------------------------------------------

  return new_atlas;
}

}  // namespace impeller
