// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <impeller/texture.glsl>

uniform GradientInfo {
  vec2 start_point;
  vec2 end_point;
  vec4 start_color;
  vec4 end_color;
  float tile_mode;
  float alpha;
} gradient_info;

in vec2 v_position;

out vec4 frag_color;

void main() {
  float len = length(gradient_info.end_point - gradient_info.start_point);
  float dot = dot(
    v_position - gradient_info.start_point,
    gradient_info.end_point - gradient_info.start_point
  );
  float t = dot / (len * len);

  t = IPFloatTile(t, gradient_info.tile_mode);

  if (gradient_info.tile_mode == kTileModeDecal && (t < 0 || t >= 1)) {
    frag_color = vec4(0);
    return;
  }
  vec4 mixed_color = mix(gradient_info.start_color, gradient_info.end_color, t);
  frag_color = vec4(mixed_color.xyz * mixed_color.a, mixed_color.a) * gradient_info.alpha;
}
