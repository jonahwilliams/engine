// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "half.h"

#include "flutter/fml/trace_event.h"

namespace impeller {

_Half ScalarToHalf(Scalar f) {
#ifdef FML_OS_WIN
  FML_LOG(ERROR) << "ScalarToHalf conversion on unsupported platform.";
#endif
  return static_cast<_Half>(f);
}

}  // namespace impeller
