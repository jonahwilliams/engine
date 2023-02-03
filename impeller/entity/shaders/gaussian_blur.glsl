// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// 1D (directional) gaussian blur.
//
// Paths for future optimization:
//   * Remove the uv bounds multiplier in SampleColor by adding optional
//     support for SamplerAddressMode::ClampToBorder in the texture sampler.
//   * Render both blur passes into a smaller texture than the source image
//     (~1/radius size).
//   * If doing the small texture render optimization, cache misses can be
//     reduced in the first pass by sampling the source textures with a mip
//     level of log2(min_radius).

#include <impeller/constants.glsl>
#include <impeller/gaussian.glsl>
#include <impeller/texture.glsl>
#include <impeller/types.glsl>

uniform sampler2D texture_sampler;
uniform sampler2D alpha_mask_sampler;

uniform FragInfo {
  vec2 texture_size;
  vec2 blur_direction;

  // The blur sigma and radius have a linear relationship which is defined
  // host-side, but both are useful controls here. Sigma (pixels per standard
  // deviation) is used to define the gaussian function itself, whereas the
  // radius is used to limit how much of the function is integrated.
  float blur_sigma;
  float blur_radius;

  float src_factor;
  float inner_blur_factor;
  float outer_blur_factor;
}
frag_info;

in vec2 v_texture_coords;
in vec2 v_src_texture_coords;

out vec4 frag_color;

void main() {
  vec4 total_color = vec4(0);
  float gaussian_integral = 0;
  vec2 blur_uv_offset = frag_info.blur_direction / frag_info.texture_size;

  for (float i = -frag_info.blur_radius; i <= frag_info.blur_radius; i++) {
    float gaussian = IPGaussian(i, frag_info.blur_sigma);
    gaussian_integral += gaussian;
    total_color +=
        gaussian *
        Sample(texture_sampler,                       // sampler
               v_texture_coords + blur_uv_offset * i  // texture coordinates
        );
  }

  vec4 blur_color = total_color / gaussian_integral;

  vec4 src_color = Sample(alpha_mask_sampler,   // sampler
                          v_src_texture_coords  // texture coordinates
  );
  float blur_factor = frag_info.inner_blur_factor * float(src_color.a > 0) +
                      frag_info.outer_blur_factor * float(src_color.a == 0);

  frag_color = blur_color * blur_factor + src_color * frag_info.src_factor;
}
