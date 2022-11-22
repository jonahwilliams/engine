// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <impeller/color.glsl>
#include <impeller/texture.glsl>
#include <impeller/types.glsl>

uniform sampler2D y_texture;
uniform sampler2D uv_texture;

// These values must correspond to the order of the items in the
// 'YUVColorSpace' enum class.
const float16_t kBT601LimitedRange = 0.0hf;
const float16_t kBT601FullRange = 1.0hf;

uniform FragInfo {
  float16_t texture_sampler_y_coord_scale;
  float16_t yuv_color_space;
  mat4 matrix;
}
frag_info;

in f16vec2 v_position;
out f16vec4 frag_color;

void main() {
  f16vec3 yuv;
  f16vec3 yuv_offset = f16vec3(0.0hf, 0.5hf, 0.5hf);
  if (frag_info.yuv_color_space == kBT601LimitedRange) {
    yuv_offset.x = 16.0hf / 255.0hf;
  }

  yuv.x =
      IPSample(y_texture, v_position, frag_info.texture_sampler_y_coord_scale)
          .x;
  yuv.yz =
      IPSample(uv_texture, v_position, frag_info.texture_sampler_y_coord_scale)
          .xy;
  frag_color = f16mat4(frag_info.matrix) * f16vec4(yuv - yuv_offset, 1.0hf);
}
