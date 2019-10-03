// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:ui' as ui;

void main() {
  test('hashValues is resilient to incorrectly implemented equality', () {
    final BadEquality badEquality = BadEquality();

    expect(ui.hashValues([badEquality]), 42);
  });
}

class BadEquality {
  @override
  bool operator ==(Object other) => true;

  int get hashCode => 42;
}