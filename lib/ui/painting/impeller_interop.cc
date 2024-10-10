// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/lib/ui/painting/impeller_interop.h"

#include "dart_api.h"
#include "fml/memory/ref_ptr.h"
#include "impeller/base/strings.h"
#include "impeller/core/device_buffer_descriptor.h"
#include "impeller/core/formats.h"
#include "impeller/display_list/dl_image_impeller.h"
#include "impeller/renderer/blit_pass.h"
#include "impeller/renderer/command_buffer.h"
#include "impeller/renderer/context.h"
#include "lib/ui/io_manager.h"
#include "lib/ui/painting/image.h"
#include "lib/ui/ui_dart_state.h"
#include "tonic/converter/dart_converter.h"

namespace flutter {

enum class ImpellerUIFormats {
  // Impeller Only Formats
  kR8Unorm,
  kR8Snorm,
  kR8Uint,
  kR8Sint,
  kR16Uint,
  kR16Sint,
  kR16Float,
  kRG88Unorm,
  kRG88Snrom,
  kRG88Uint,
  kRG88Sint,
  kR32Uint,
  kR32Sint,
  kR32Float,
  kRG16Uint,
  kRG16Sint,
  kRG16Float,
  kRGBA8Unorm,
  kRGBA8Snorm,
  kRGBA8Uint,
  kRGBA8Sint,
  kBGRA8Unorm,
  kBGRA8UnormSrgb,
  kRGBA8UnormSrgb,
  kRG32Uint,
  kRG32Sint,
  kRG32Float,
  kRGBA16Uint,
  kRGBA16Sint,
  kRGBA16Float,
  kRGBA32Uint,
  kRGBA32Sint,
  kRGBA32Float,
};

impeller::PixelFormat FromDartUIFormat(ImpellerUIFormats format) {
  switch (format) {
    case ImpellerUIFormats::kR8Unorm:
      return impeller::PixelFormat::kR8UNormInt;
    case ImpellerUIFormats::kR8Snorm:
      return impeller::PixelFormat::kR8SNormInt;
    case ImpellerUIFormats::kR8Uint:
      return impeller::PixelFormat::kR8UInt;
    case ImpellerUIFormats::kR8Sint:
      return impeller::PixelFormat::kR8SInt;
    case ImpellerUIFormats::kR16Uint:
      return impeller::PixelFormat::kR16UInt;
    case ImpellerUIFormats::kR16Sint:
      return impeller::PixelFormat::kR16SInt;
    case ImpellerUIFormats::kR16Float:
      return impeller::PixelFormat::kR16Float;
    case ImpellerUIFormats::kRG88Unorm:
      return impeller::PixelFormat::kR8G8UNormInt;
    // case ImpellerUIFormats::kRG88Snrom:
    //   return impeller::PixelFormat::kR8G8SNormInt;
    // case ImpellerUIFormats::kRG88Uint:
    //   return impeller::PixelFormat::kR8G8UInt;
    // case ImpellerUIFormats::kRG88Sint:
    //   return impeller::PixelFormat::kR8G8SInt;
    // case ImpellerUIFormats::kR32Uint:
    //   return impeller::PixelFormat::R32UInt;
    // case ImpellerUIFormats::kR32Sint:
    //   return impeller::PixelFormat::kR32SInt;
    // case ImpellerUIFormats::kR32Float:
    //   return impeller::PixelFormat::kR32Float;
    // case ImpellerUIFormats::kRG16Uint:
    //   return impeller::PixelFormat::kR16G16UInt;
    // case ImpellerUIFormats::kRG16Sint:
    //   return impeller::PixelFormat::kR16G16SInt;
    // case ImpellerUIFormats::kRG16Float:
    //   return impeller::PixelFormat::kR16G16Float;
    case ImpellerUIFormats::kRGBA8Unorm:
      return impeller::PixelFormat::kR8G8B8A8UNormInt;
    // case ImpellerUIFormats::kRGBA8Snorm:
    //   return impeller::PixelFormat::kR8G8B8A8SNormInt;
    // case ImpellerUIFormats::kRGBA8Uint:
    //   return impeller::PixelFormat::kR8G8B8A8UInt;
    // case ImpellerUIFormats::kRGBA8Sint:
    //   return impeller::PixelFormat::kR8G8B8A8SInt;
    case ImpellerUIFormats::kBGRA8Unorm:
      return impeller::PixelFormat::kB8G8R8A8UNormInt;
    case ImpellerUIFormats::kBGRA8UnormSrgb:
      return impeller::PixelFormat::kB8G8R8A8UNormIntSRGB;
    case ImpellerUIFormats::kRGBA8UnormSrgb:
      return impeller::PixelFormat::kR8G8B8A8UNormIntSRGB;
    // case ImpellerUIFormats::kRG32Uint:
    //   return impeller::PixelFormat::kR32G32UInt;
    // case ImpellerUIFormats::kRG32Sint:
    //   return impeller::PixelFormat::kR32G32SInt;
    // case ImpellerUIFormats::kRG32Float:
    //   return impeller::PixelFormat::kR32G32Float;
    // case ImpellerUIFormats::kRGBA16Uint:
    //   return impeller::PixelFormat::kR16G16B16A16UInt;
    // case ImpellerUIFormats::kRGBA16Sint:
    //   return impeller::PixelFormat::kR16G16B16A16SInt;
    // case ImpellerUIFormats::kRGBA16Float:
    //   return impeller::PixelFormat::kR16G16B16A16Float;
    // case ImpellerUIFormats::kRGBA32Uint:
    //   return impeller::PixelFormat::kR32G32B32A32UInt;
    // case ImpellerUIFormats::kRGBA32Sint:
    //   return impeller::PixelFormat::kR32G32B32A32Sint;
    case ImpellerUIFormats::kRGBA32Float:
      return impeller::PixelFormat::kR32G32B32A32Float;
    default:
      // TODO
      return impeller::PixelFormat::kR16Float;
  }

  FML_UNREACHABLE();
}

bool ImpellerInterop::IsFormatSupported(int dart_ui_format) {
  return true;
}

static Dart_Handle CreateImageFromBufferInternal(
    const fml::WeakPtr<IOManager>& io_manager,
    Dart_Handle buffer_handle,
    Dart_Handle out_image,
    int width,
    int height,
    int dart_ui_format,
    bool generate_mips) {
  impeller::TextureDescriptor tex_desc;
  tex_desc.size = impeller::ISize(width, height);
  tex_desc.mip_count = generate_mips ? tex_desc.size.MipCount() : 1;
  tex_desc.storage_mode = impeller::StorageMode::kDevicePrivate;
  tex_desc.compression_type = impeller::CompressionType::kLossy;
  tex_desc.format =
      FromDartUIFormat(static_cast<ImpellerUIFormats>(dart_ui_format));
  tex_desc.usage = impeller::TextureUsage::kShaderRead;

  impeller::DeviceBufferDescriptor desc;
  desc.size = tex_desc.GetByteSizeOfBaseMipLevel();
  desc.storage_mode = impeller::StorageMode::kHostVisible;

  std::shared_ptr<impeller::DeviceBuffer> src_buffer =
      io_manager->GetImpellerContext()->GetResourceAllocator()->CreateBuffer(
          desc);
  if (!src_buffer) {
    return tonic::ToDart(
        "Failed to allocate staging buffer for texture upload.");
  }
  auto dst_texture =
      io_manager->GetImpellerContext()->GetResourceAllocator()->CreateTexture(
          tex_desc);
  if (!dst_texture) {
    return tonic::ToDart("Failed to allocate texture for texture upload.");
  }

  Dart_TypedData_Type type;
  void* data = nullptr;
  intptr_t num_acquired = 0;
  FML_CHECK(!Dart_IsError(
      Dart_TypedDataAcquireData(buffer_handle, &type, &data, &num_acquired)));

  if (static_cast<size_t>(num_acquired) != desc.size) {
    return tonic::ToDart(impeller::SPrintF(
        "Mismatched buffer length, expected %d but got %d",
        static_cast<int>(desc.size), static_cast<int>(num_acquired)));
  }

  if (!src_buffer->CopyHostBuffer(reinterpret_cast<const uint8_t*>(data),
                                  impeller::Range{0, desc.size})) {
    return tonic::ToDart("Failed to copy data into staging buffer.");
  }

  std::shared_ptr<impeller::CommandBuffer> cmd_buffer =
      io_manager->GetImpellerContext()->CreateCommandBuffer();
  std::shared_ptr<impeller::BlitPass> blit_pass = cmd_buffer->CreateBlitPass();

  blit_pass->AddCopy(impeller::DeviceBuffer::AsBufferView(src_buffer),
                     dst_texture);
  if (tex_desc.mip_count > 1) {
    blit_pass->GenerateMipmap(dst_texture);
  }
  blit_pass->EncodeCommands(
      io_manager->GetImpellerContext()->GetResourceAllocator());

  if (!io_manager->GetImpellerContext()
           ->GetCommandQueue()
           ->Submit({cmd_buffer})
           .ok()) {
    return tonic::ToDart("Failed to submit image upload command buffer");
  }

  fml::RefPtr<CanvasImage> canvas_image = CanvasImage::Create();
  canvas_image->set_image(
      impeller::DlImageImpeller::Make(std::move(dst_texture)));
  canvas_image->AssociateWithDartWrapper(out_image);

  return Dart_Null();
}

Dart_Handle ImpellerInterop::CreateImageFromBuffer(Dart_Handle buffer_handle,
                                                   Dart_Handle out_image,
                                                   int width,
                                                   int height,
                                                   int dart_ui_format,
                                                   bool generate_mips) {
  auto io_manager = UIDartState::Current()->GetIOManager();
  if (!io_manager || !io_manager->GetImpellerContext()) {
    return tonic::ToDart("No Impeller context available");
  }
  Dart_Handle result;
  io_manager->GetIsGpuDisabledSyncSwitch()->Execute(
      fml::SyncSwitch::Handlers()
          .SetIfTrue(
              [&] { result = tonic::ToDart("No GPU Context available"); })
          .SetIfFalse([&] {
            result = CreateImageFromBufferInternal(
                io_manager, buffer_handle, out_image, width, height,
                dart_ui_format, generate_mips);
          }));
  return result;
}

}  // namespace flutter