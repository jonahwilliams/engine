// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
part of dart.ui;

/// TODO STUFF
///
/// Format names are structured in the following manner: First give the order of
/// the color channels:
///   * "rgba" means red, green, blue, and alpha.
///   * "bgra" means blue, green, red, and alpha.
///
/// Next the bit size of each channel:
///   * "rgba8" means that each channel is 8 bits and the total size is 32 bits.
///   * "r32" means that the red channel takes up all 32 bits.
///
/// Finally the encoding of the bits used in the channel.
///
/// See also:
///   * https://developer.apple.com/documentation/metal/mtlpixelformat?language=objc
///   * https://docs.vulkan.org/spec/latest/chapters/formats.html
enum TextureFormat {
  /// Impeller Only Formats

  /// Uncompressed format with one 8-bit normalized unsigned integer component.
  ///
  /// Equivalent to VK_FORMAT_R8_UNORM on Vulkan, MTLPixelFormatR8Unorm on
  /// Metal.
  ///
  /// Not supported on the Skia rendering backend.
  r8,

  /// Uncompressed format with one 8-bit normalized signed integer component.
  ///
  /// Equivalent to VK_FORMAT_R8_SNORM on Vulkan, MTLPixelFormatR8Snorm on
  /// Metal.
  ///
  /// Not supported on the Skia rendering backend.
  r8Snorm,

  /// Uncompressed format with one 8-bit unsigned integer component.
  ///
  /// Equivalent to VK_FORMAT_R8_UINT on Vulkan, MTLPixelFormatR8Uint on
  /// Metal.
  ///
  /// Not supported on the Skia rendering backend.
  r8Uint,

  /// Uncompressed format with one 8-bit normalized signed integer component.
  ///
  /// Equivalent to VK_FORMAT_R8_SINT on Vulkan, MTLPixelFormatR8Sint on
  /// Metal.
  ///
  /// Not supported on the Skia rendering backend.
  r8Sint,

  /// 16-Bit Pixel Formats

  /// Uncompressed format with one 16-bit unsigned integer component.
  ///
  /// Equivalent to VK_FORMAT_R16_UINT on Vulkan, MTLPixelFormatR16Uint on
  /// Metal.
  ///
  /// Not supported on the Skia rendering backend.
  r16Uint,

  /// Uncompressed format with one 16-bit signed integer component.
  ///
  /// Equivalent to VK_FORMAT_R16_SINT on Vulkan, MTLPixelFormatR16Sint on
  /// Metal.
  ///
  /// Not supported on the Skia rendering backend.
  r16Sint,

  /// Uncompressed format with one 16-bit floating-point component.
  ///
  /// Equivalent to VK_FORMAT_R16_SFLOAT on Vulkan, MTLPixelFormatR16Float on
  /// Metal.
  ///
  /// Not supported on the Skia rendering backend.
  r16Float,

  /// Uncompressed format with two 8-bit normalized unsigned integer components.
  ///
  /// Equivalent to VK_FORMAT_R8G8_UNORM on Vulkan, MTLPixelFormatRG8Unorm on
  /// Metal.
  ///
  /// Not supported on the Skia rendering backend.
  rg88Unorm,

  /// Uncompressed format with two 8-bit normalized signed integer components.
  ///
  /// Equivalent to VK_FORMAT_R8G8_SNORM on Vulkan, MTLPixelFormatRG8Snorm on
  /// Metal.
  ///
  /// Not supported on the Skia rendering backend.
  rg88Snrom,

  /// Uncompressed format with two 8-bit unsigned integer components.
  ///
  /// Equivalent to VK_FORMAT_R8G8_UINT on Vulkan, MTLPixelFormatRG8Uint on
  /// Metal.
  ///
  /// Not supported on the Skia rendering backend.
  rg88Uint,

  /// Uncompressed format with two 8-bit signed integer components.
  ///
  /// Equivalent to VK_FORMAT_R8G8_SINT on Vulkan, MTLPixelFormatRG8Sint on
  /// Metal.
  ///
  /// Not supported on the Skia rendering backend.
  rg88Sint,

  /// 32-Bit Pixel Formats

  /// Uncompressed format with one 32-bit unsigned integer component.
  ///
  /// Equivalent to VK_FORMAT_R32_UINT on Vulkan, MTLPixelFormatR32Uint on
  /// Metal.
  ///
  /// Not supported on the Skia rendering backend.
  r32Uint,

  /// Uncompressed format with one 32-bit signed integer component.
  ///
  /// Equivalent to VK_FORMAT_R32_SINT on Vulkan, MTLPixelFormatR32Sint on
  /// Metal.
  ///
  /// Not supported on the Skia rendering backend.
  r32Sint,

  /// Uncompressed format with one 32-bit floating-point component.
  ///
  /// Equivalent to VK_FORMAT_R32_SFLOAT on Vulkan, MTLPixelFormatR32Float on
  /// Metal.
  ///
  /// Not supported on the Skia rendering backend.
  r32Float,

  /// Uncompressed format with two 16-bit unsigned integer components.
  ///
  /// Equivalent to VK_FORMAT_R16G16_UINT on Vulkan, MTLPixelFormatRG16Uint on
  /// Metal.
  ///
  /// Not supported on the Skia rendering backend.
  rg16Uint,

  /// Uncompressed format with two 16-bit signed integer components.
  ///
  /// Equivalent to VK_FORMAT_R16G16_SINT on Vulkan, MTLPixelFormatRG16Sint on
  /// Metal.
  ///
  /// Not supported on the Skia rendering backend.
  rg16Sint,

  /// Uncompressed format with two 16-bit floating-point components.
  ///
  /// Equivalent to VK_FORMAT_R16G16_SFLOAT on Vulkan, MTLPixelFormatRG16Float on
  /// Metal.
  ///
  /// Not supported on the Skia rendering backend.
  rg16Float,

  /// Uncompressed format with four 8-bit normalized unsigned integer components
  /// in RGBA order.
  ///
  /// Equivalent to VK_FORMAT_R8G8B8A8_UNORM on Vulkan, MTLPixelFormatRGBA8Unorm
  /// on Metal.
  ///
  /// This is an alias for [PixelFormat.rgba8888].
  rgba8Unorm,

  /// Uncompressed format with four 8-bit normalized signed integer components
  /// in RGBA order.
  ///
  /// Equivalent to VK_FORMAT_R8G8B8A8_SNORM on Vulkan, MTLPixelFormatRGBA8Snorm
  /// on Metal.
  ///
  /// Not supported on the Skia rendering backend.
  rgba8Snorm,

  /// Uncompressed format with four 8-bit unsigned integer components in RGBA
  /// order.
  ///
  /// Equivalent to VK_FORMAT_R8G8B8A8_UINT on Vulkan, MTLPixelFormatRGBA8Uint on
  /// Metal.
  ///
  /// Not supported on the Skia rendering backend.
  rgba8Uint,

  /// Uncompressed format with four 8-bit signed integer components in RGBA
  /// order.
  ///
  /// Equivalent to VK_FORMAT_R8G8B8A8_SINT on Vulkan, MTLPixelFormatRGBA8Uint on
  /// Metal.
  ///
  /// Not supported on the Skia rendering backend.
  rgba8Sint,

  /// Uncompressed format with four 8-bit normalized unsigned integer components
  /// in BGRA order.
  ///
  /// Equivalent to VK_FORMAT_B8G8R8A8_UNORM on Vulkan, MTLPixelFormatBGRA8Unorm
  /// on Metal.
  ///
  /// This is an alias for [PixelFormat.bgra8888].
  bgra8Unorm,

  /// Uncompressed format with four 8-bit normalized unsigned integer components
  /// in BGRA order with conversion between sRGB and linear space.
  ///
  /// Equivalent to VK_FORMAT_B8G8R8A8_SRGB on Vulkan,
  /// MTLPixelFormatBGRA8Unorm_sRGB on Metal.
  ///
  /// Not supported on the Skia rendering backend.
  bgra8UnormSrgb,

  /// Uncompressed format with four 8-bit normalized unsigned integer components
  /// in RGBA order with conversion between sRGB and linear space.
  ///
  /// Equivalent to VK_FORMAT_R8G8B8A8_SRGB on Vulkan,
  /// MTLPixelFormatRGBA8Unorm_sRGB on Metal.
  ///
  /// Not supported on the Skia rendering backend.
  rgba8UnormSrgb,

  /// 64-Bit Pixel Formats

  /// Uncompressed format with two 32-bit unsigned integer components.
  ///
  /// Equivalent to VK_FORMAT_R32G32_UINT on Vulkan, MTLPixelFormatRG32Uint on
  /// Metal.
  ///
  /// Not supported on the Skia rendering backend.
  rg32Uint,

  /// Uncompressed format with two 32-bit signed integer components.
  ///
  /// Equivalent to VK_FORMAT_R32G32_SINT on Vulkan, MTLPixelFormatRG32Sint on
  /// Metal.
  ///
  /// Not supported on the Skia rendering backend.
  rg32Sint,

  /// Uncompressed format with two 32-bit floating-point components.
  ///
  /// Equivalent to VK_FORMAT_R32G32_SFLOAT on Vulkan, MTLPixelFormatRG32Float on
  /// Metal.
  ///
  /// Not supported on the Skia rendering backend.
  rg32Float,

  /// Uncompressed format with four 16-bit unsigned integer components in RGBA
  /// order.
  ///
  /// Equivalent to VK_FORMAT_R16G16B16A16_UINT on Vulkan,
  /// MTLPixelFormatRGBA16Uint on Metal.
  ///
  /// Not supported on the Skia rendering backend.
  rgba16Uint,

  /// Uncompressed format with four 16-bit signed integer components in RGBA
  /// order.
  ///
  /// Equivalent to VK_FORMAT_R16G16B16A16_SINT on Vulkan,
  /// MTLPixelFormatRGBA16Sint on Metal.
  ///
  /// Not supported on the Skia rendering backend.
  rgba16Sint,

  /// Uncompressed format with four 16-bit floating-point components in RGBA
  /// order.
  ///
  /// Equivalent to VK_FORMAT_R16G16B16A16_SFLOAT on Vulkan,
  /// MTLPixelFormatRGBA16Float on Metal.
  ///
  /// Not supported on the Skia rendering backend.
  rgba16Float,

  /// 128-Bit Pixel Formats

  /// Uncompressed format with four 32-bit unsigned integer components in RGBA
  /// order.
  ///
  /// Equivalent to VK_FORMAT_R32G32B32A32_UINT on Vulkan,
  /// MTLPixelFormatRGBA32Uint on Metal.
  ///
  /// Not supported on the Skia rendering backend.
  rgba32Uint,

  /// Uncompressed format with four 32-bit signed integer components in RGBA
  /// order.
  ///
  /// Equivalent to VK_FORMAT_R32G32B32A32_SINT on Vulkan,
  /// MTLPixelFormatRGBA32Sint on Metal.
  ///
  /// Not supported on the Skia rendering backend.
  rgba32Sint,

  /// Uncompressed format with four 32-bit floating-point components in RGBA
  /// order.
  ///
  /// Equivalent to VK_FORMAT_R32G32B32A32_SFLOAT on Vulkan,
  /// MTLPixelFormatRGBA32Float on Metal.
  ///
  /// Alias for [PixelFormat.rgbaFloat32].
  rgba32Float,
}

/// Determine if the provided [format] is supported by the current backend.
///
/// As noted in the documentation for each [format] enum member, some formats
/// are not supported in all backends.
bool isFormatSupported(TextureFormat format) {
  if (!_impellerEnabled) {
    return false;
  }
  return _isFormatSupported(format.index);
}

/// Create an image from the data stored in [buffer] with dimensions [width] and
/// [height], with bytes formatted according to the provided [format].
///
/// Image creation cannot be performed if the GPU is unavailable. on the iOS
/// platform this happens when the application is backgrounded.
///
/// Only supported on the Impeller rendering backend.
Image createImageFromBuffer(Uint8List buffer, {
  required int width,
  required int height,
  required TextureFormat format,
  bool mipMaps = false,
}) {
  if (!_impellerEnabled) {
    throw Exception('CreateImageFromBuffer only supported with Impeller rendering engine.');
  }
  final _Image image = _Image._();
  final String? errorMessage = _createImageFromBuffer(buffer, image, width, height, format.index, mipMaps);
  if (errorMessage != null) {
    throw Exception(errorMessage);
  }
  return Image._(image, width, height);
}

@Native<Bool Function(Int32)>(symbol: 'ImpellerInterop::IsFormatSupported')
external bool _isFormatSupported(int textureFormat);


@Native<Handle Function(Handle, Handle, Int32, Int32, Int32, Bool)>(symbol: 'ImpellerInterop::CreateImageFromBuffer')
external String? _createImageFromBuffer(Uint8List buffer, _Image image , int width, int height, int textureFormat, bool mipMaps);
