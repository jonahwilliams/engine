// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_LIB_UI_PAINTING_IMPELLER_INTEROP_H_
#define FLUTTER_LIB_UI_PAINTING_IMPELLER_INTEROP_H_

#include <cstdint>

#include "dart_api.h"

namespace flutter {

class ImpellerInterop {
 public:
  static bool IsFormatSupported(int dart_ui_format);

  static Dart_Handle CreateImageFromBuffer(Dart_Handle buffer,
                                           Dart_Handle out_image,
                                           int width,
                                           int height,
                                           int dart_ui_format,
                                           bool generate_mips);
};

}  // namespace flutter

#endif  // FLUTTER_LIB_UI_PAINTING_IMPELLER_INTEROP_H_