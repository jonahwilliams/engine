// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <impeller/constants.glsl>
#include <impeller/texture.glsl>
#include <impeller/types.glsl>

uniform sampler2D texture_sampler;

uniform GradientInfo {
  f16vec2 center;
  float16_t bias;
  float16_t scale;
  float16_t tile_mode;
  float16_t texture_sampler_y_coord_scale;
  float16_t alpha;
  f16vec2 half_texel;
}
gradient_info;

in f16vec2 v_position;

out f16vec4 frag_color;

void main() {
  f16vec2 coord = v_position - gradient_info.center;
  float angle = atan(-coord.y, -coord.x);

  float16_t t = (float16_t(angle) * k1Over2Pi + 0.5hf + gradient_info.bias) *
                gradient_info.scale;
  frag_color = IPSampleLinearWithTileMode(
      texture_sampler, f16vec2(t, 0.5hf),
      gradient_info.texture_sampler_y_coord_scale, gradient_info.half_texel,
      gradient_info.tile_mode);
  frag_color = f16vec4(frag_color.xyz * frag_color.a, frag_color.a) *
               gradient_info.alpha;
}
