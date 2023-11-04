// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:typed_data';

import 'package:ui/ui.dart' as ui;

import '../scene_painting.dart';
import 'canvas.dart';
import 'canvaskit_api.dart';
import 'image.dart';
import 'native_memory.dart';
import 'render_canvas_factory.dart';
import 'renderer.dart';
import 'surface.dart';

/// Implements [ui.Picture] on top of [SkPicture].
class CkPicture implements ScenePicture {
  CkPicture(SkPicture skPicture) {
    _ref = UniqueRef<SkPicture>(this, skPicture, 'Picture');
  }

  late final UniqueRef<SkPicture> _ref;

  SkPicture get skiaObject => _ref.nativeObject;

  @override
  ui.Rect get cullRect => fromSkRect(skiaObject.cullRect());

  @override
  int get approximateBytesUsed => skiaObject.approximateBytesUsed();

  @override
  bool get debugDisposed {
    bool? result;
    assert(() {
      result = _isDisposed;
      return true;
    }());

    if (result != null) {
      return result!;
    }

    throw StateError(
        'Picture.debugDisposed is only available when asserts are enabled.');
  }

  /// This is set to true when [dispose] is called and is never reset back to
  /// false.
  ///
  /// This extra flag is necessary on top of [rawSkiaObject] because
  /// [rawSkiaObject] being null does not indicate permanent deletion.
  bool _isDisposed = false;

  /// The stack trace taken when [dispose] was called.
  ///
  /// Returns null if [dispose] has not been called. Returns null in non-debug
  /// modes.
  StackTrace? _debugDisposalStackTrace;

  /// Throws an [AssertionError] if this picture was disposed.
  ///
  /// The [mainErrorMessage] is used as the first line in the error message. It
  /// is expected to end with a period, e.g. "Failed to draw picture." The full
  /// message will also explain that the error is due to the fact that the
  /// picture was disposed and include the stack trace taken when the picture
  /// was disposed.
  bool debugCheckNotDisposed(String mainErrorMessage) {
    if (_isDisposed) {
      throw StateError(
        '$mainErrorMessage\n'
        'The picture has been disposed. When the picture was disposed the '
        'stack trace was:\n'
        '$_debugDisposalStackTrace',
      );
    }
    return true;
  }

  @override
  void dispose() {
    assert(debugCheckNotDisposed('Cannot dispose picture.'));
    assert(() {
      _debugDisposalStackTrace = StackTrace.current;
      return true;
    }());
    ui.Picture.onDispose?.call(this);
    _isDisposed = true;
    _ref.dispose();
  }

  @override
  Future<ui.Image> toImage(int width, int height) async {
    return toImageSync(width, height);
  }

  @override
  CkImage toImageSync(int width, int height) {
    assert(debugCheckNotDisposed('Cannot convert picture to image.'));
    if (RenderCanvasFactory.instance.pictureToImageSurface.usingSoftwareBackend) {
      return toImageSyncSoftware(width, height);
    }

    final CkSurface ckSurface = CanvasKitRenderer.instance.rasterizer.createOffscreenSurface(ui.Size(width.toDouble(), height.toDouble()));
    final CkCanvas ckCanvas = ckSurface.getCanvas();
    ckCanvas.clear(const ui.Color(0x00000000));
    ckCanvas.drawPicture(this);
    final SkImage skImage = ckSurface.surface.makeImageSnapshot();
    ckSurface.dispose();
    return CkImage(skImage);
  }

  CkImage toImageSyncSoftware(int width, int height) {
    final Surface surface = RenderCanvasFactory.instance.pictureToImageSurface;
    final CkSurface ckSurface = surface
        .createOrUpdateSurface(ui.Size(width.toDouble(), height.toDouble()));
    final CkCanvas ckCanvas = ckSurface.getCanvas();
    ckCanvas.clear(const ui.Color(0x00000000));
    ckCanvas.drawPicture(this);
    final SkImage skImage = ckSurface.surface.makeImageSnapshot();
    final SkImageInfo imageInfo = SkImageInfo(
      alphaType: canvasKit.AlphaType.Premul,
      colorType: canvasKit.ColorType.RGBA_8888,
      colorSpace: SkColorSpaceSRGB,
      width: width.toDouble(),
      height: height.toDouble(),
    );
    final Uint8List pixels = skImage.readPixels(0, 0, imageInfo);
    final SkImage? rasterImage =
        canvasKit.MakeImage(imageInfo, pixels, (4 * width).toDouble());
    if (rasterImage == null) {
      throw StateError('Unable to convert image pixels into SkImage.');
    }
    return CkImage(rasterImage);
  }
}
