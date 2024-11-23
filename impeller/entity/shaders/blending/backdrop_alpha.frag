#version 450

// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <impeller/color.glsl>
#include <impeller/texture.glsl>
#include <impeller/types.glsl>

// Warning: if any of the constant values or layouts are changed in this
// file, then the hard-coded constant value in
// impeller/renderer/backend/vulkan/binding_helpers_vk.cc
layout(input_attachment_index = 0) uniform subpassInputMS uSub;

vec4 ReadDestination() {
  return (subpassLoad(uSub, 0) + subpassLoad(uSub, 1) + subpassLoad(uSub, 2) +
          subpassLoad(uSub, 3)) /
         vec4(4.0);
}

uniform FragInfo {
  vec4 color;
  float alpha;
}
frag_info;

out vec4 frag_color;

void main() {
  vec4 backdrop = ReadDestination();
  if (backdrop.w <= 0.0) {
    frag_color = frag_info.color;
  } else {
    frag_color = (1.0 - frag_info.alpha) * frag_info.color +
                 frag_info.alpha * ReadDestination();
  }
}
