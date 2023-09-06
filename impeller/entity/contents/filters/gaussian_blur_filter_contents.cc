// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/entity/contents/filters/gaussian_blur_filter_contents.h"

#include <cmath>
#include <utility>
#include <valarray>

#include "impeller/base/strings.h"
#include "impeller/base/validation.h"
#include "impeller/core/formats.h"
#include "impeller/core/sampler_descriptor.h"
#include "impeller/entity/contents/content_context.h"
#include "impeller/entity/contents/filters/filter_contents.h"
#include "impeller/geometry/rect.h"
#include "impeller/geometry/scalar.h"
#include "impeller/renderer/command_buffer.h"
#include "impeller/renderer/render_pass.h"
#include "impeller/renderer/render_target.h"
#include "impeller/renderer/sampler_library.h"

namespace impeller {

// The maximum sigma that can be computed without downscaling is based on the
// number of uniforms and texture samples the effects will make in a single
// pass. For 1D passes, the number of samples is equal to
// `BlurLinearKernelWidth`; for 2D passes, it is equal to
// `BlurKernelWidth(radiusX)*BlurKernelWidth(radiusY)`. This maps back to
// different maximum sigmas depending on the approach used, as well as the ratio
// between the sigmas for the X and Y axes if a 2D blur is performed.
constexpr size_t kMaxBlurSamples = 28;

constexpr Scalar kMaxLinearBlurSigma = 4.f;

// The kernel width of a Gaussian blur of the given pixel radius, for when all
// pixels are sampled.
constexpr int BlurKernelWidth(int radius) {
  return 2 * radius + 1;
}

void Compute2DBlurKernel(Sigma sigma_x,
                         Sigma sigma_y,
                         std::array<float, kMaxBlurSamples> kernel) {
  // Callers are responsible for downscaling large sigmas to values that can be
  // processed by the effects, so ensure the radius won't overflow 'kernel'
  const int width = BlurKernelWidth(static_cast<Radius>(sigma_x).radius);
  const int height = BlurKernelWidth(static_cast<Radius>(sigma_y).radius);
  const size_t kernelSize = width * height;
  FML_DCHECK(kernelSize <= kMaxBlurSamples);

  // And the definition of an identity blur should be sufficient that 2sigma^2
  // isn't near zero when there's a non-trivial radius.
  const float twoSigmaSqrdX = 2.0f * sigma_x.sigma * sigma_x.sigma;
  const float twoSigmaSqrdY = 2.0f * sigma_y.sigma * sigma_y.sigma;

  // FML_DCHECK((radius.width == 0 || !ScalarNearlyZero(twoSigmaSqrdX)) &&
  //            (radius.height == 0 || !ScalarNearlyZero(twoSigmaSqrdY)));

  // Setting the denominator to 1 when the radius is 0 automatically converts
  // the remaining math to the 1D Gaussian distribution. When both radii are 0,
  // it correctly computes a weight of 1.0
  const float sigmaXDenom =
      static_cast<Radius>(sigma_x).radius > 0 ? 1.0f / twoSigmaSqrdX : 1.f;
  const float sigmaYDenom =
      static_cast<Radius>(sigma_y).radius > 0 ? 1.0f / twoSigmaSqrdY : 1.f;

  float sum = 0.0f;
  for (int x = 0; x < width; x++) {
    float xTerm = static_cast<float>(x - static_cast<Radius>(sigma_x).radius);
    xTerm = xTerm * xTerm * sigmaXDenom;
    for (int y = 0; y < height; y++) {
      float yTerm = static_cast<float>(y - static_cast<Radius>(sigma_y).radius);
      float xyTerm = std::exp(-(xTerm + yTerm * yTerm * sigmaYDenom));
      // Note that the constant term (1/(sqrt(2*pi*sigma^2)) of the Gaussian
      // is dropped here, since we renormalize the kernel below.
      kernel[y * width + x] = xyTerm;
      sum += xyTerm;
    }
  }
  // Normalize the kernel
  float scale = 1.0f / sum;
  for (size_t i = 0; i < kernelSize; i++) {
    kernel[i] *= scale;
  }
  // Zero remainder of the array
  memset(kernel.data() + kernelSize, 0,
         sizeof(float) * (kernel.size() - kernelSize));
}

DirectionalGaussianBlurFilterContents::DirectionalGaussianBlurFilterContents() =
    default;

DirectionalGaussianBlurFilterContents::
    ~DirectionalGaussianBlurFilterContents() = default;

void DirectionalGaussianBlurFilterContents::SetSigma(Sigma sigma) {
  blur_sigma_ = sigma;
}

void DirectionalGaussianBlurFilterContents::SetSecondarySigma(Sigma sigma) {
  secondary_blur_sigma_ = sigma;
}

void DirectionalGaussianBlurFilterContents::SetDirection(Vector2 direction) {
  blur_direction_ = direction.Normalize();
  if (blur_direction_.IsZero()) {
    blur_direction_ = Vector2(0, 1);
  }
}

void DirectionalGaussianBlurFilterContents::SetBlurStyle(BlurStyle blur_style) {
  blur_style_ = blur_style;

  switch (blur_style) {
    case FilterContents::BlurStyle::kNormal:
      src_color_factor_ = false;
      inner_blur_factor_ = true;
      outer_blur_factor_ = true;
      break;
    case FilterContents::BlurStyle::kSolid:
      src_color_factor_ = true;
      inner_blur_factor_ = false;
      outer_blur_factor_ = true;
      break;
    case FilterContents::BlurStyle::kOuter:
      src_color_factor_ = false;
      inner_blur_factor_ = false;
      outer_blur_factor_ = true;
      break;
    case FilterContents::BlurStyle::kInner:
      src_color_factor_ = false;
      inner_blur_factor_ = true;
      outer_blur_factor_ = false;
      break;
  }
}

void DirectionalGaussianBlurFilterContents::SetTileMode(
    Entity::TileMode tile_mode) {
  tile_mode_ = tile_mode;
}

void DirectionalGaussianBlurFilterContents::SetSourceOverride(
    FilterInput::Ref source_override) {
  source_override_ = std::move(source_override);
}

void DirectionalGaussianBlurFilterContents::UpdateSamplerDescriptor(
    SamplerDescriptor& input_descriptor,
    SamplerDescriptor& source_descriptor,
    const ContentContext& renderer) const {
  switch (tile_mode_) {
    case Entity::TileMode::kDecal:
      if (renderer.GetDeviceCapabilities().SupportsDecalSamplerAddressMode()) {
        input_descriptor.width_address_mode = SamplerAddressMode::kDecal;
        input_descriptor.height_address_mode = SamplerAddressMode::kDecal;
        source_descriptor.width_address_mode = SamplerAddressMode::kDecal;
        source_descriptor.height_address_mode = SamplerAddressMode::kDecal;
      }
      break;
    case Entity::TileMode::kClamp:
      input_descriptor.width_address_mode = SamplerAddressMode::kClampToEdge;
      input_descriptor.height_address_mode = SamplerAddressMode::kClampToEdge;
      source_descriptor.width_address_mode = SamplerAddressMode::kClampToEdge;
      source_descriptor.height_address_mode = SamplerAddressMode::kClampToEdge;
      break;
    case Entity::TileMode::kMirror:
      input_descriptor.width_address_mode = SamplerAddressMode::kMirror;
      input_descriptor.height_address_mode = SamplerAddressMode::kMirror;
      source_descriptor.width_address_mode = SamplerAddressMode::kMirror;
      source_descriptor.height_address_mode = SamplerAddressMode::kMirror;
      break;
    case Entity::TileMode::kRepeat:
      input_descriptor.width_address_mode = SamplerAddressMode::kRepeat;
      input_descriptor.height_address_mode = SamplerAddressMode::kRepeat;
      source_descriptor.width_address_mode = SamplerAddressMode::kRepeat;
      source_descriptor.height_address_mode = SamplerAddressMode::kRepeat;
      break;
  }
}

std::optional<Entity> DirectionalGaussianBlurFilterContents::RenderFilter(
    const FilterInput::Vector& inputs,
    const ContentContext& renderer,
    const Entity& entity,
    const Matrix& effect_transform,
    const Rect& coverage,
    const std::optional<Rect>& coverage_hint) const {
  using VS = GaussianBlurAlphaDecalPipeline::VertexShader;
  using FS = GaussianBlurAlphaDecalPipeline::FragmentShader;

  bool is_first_pass = !source_override_;

  //----------------------------------------------------------------------------
  /// Handle inputs.
  ///

  if (inputs.empty()) {
    return std::nullopt;
  }

  // Limit the kernel size to 1000x1000 pixels, like Skia does.
  auto radius = std::min(Radius{blur_sigma_}.radius, 500.0f);

  auto transform = entity.GetTransformation() * effect_transform.Basis();
  auto transformed_blur_radius =
      transform.TransformDirection(blur_direction_ * radius);

  auto transformed_blur_radius_length = transformed_blur_radius.GetLength();

  // Input 0 snapshot.

  // std::optional<Rect> expanded_coverage_hint;
  // if (coverage_hint.has_value()) {
  //   auto r =
  //       Size(transformed_blur_radius_length, transformed_blur_radius_length)
  //           .Abs();
  //   expanded_coverage_hint =
  //       is_first_pass ? Rect(coverage_hint.value().origin - r,
  //                            Size(coverage_hint.value().size + r * 2))
  //                     : coverage_hint;
  // }
  auto input_snapshot =
      inputs[0]->GetSnapshot("GaussianBlur", renderer, entity, nullptr);
  if (!input_snapshot.has_value()) {
    return std::nullopt;
  }

  if (blur_sigma_.sigma < kEhCloseEnough) {
    return Entity::FromSnapshot(
        input_snapshot.value(), entity.GetBlendMode(),
        entity.GetStencilDepth());  // No blur to render.
  }

  // If the radius length is < .5, the shader will take at most 1 sample,
  // resulting in no blur.
  if (transformed_blur_radius_length < .5) {
    return Entity::FromSnapshot(
        input_snapshot.value(), entity.GetBlendMode(),
        entity.GetStencilDepth());  // No blur to render.
  }

  // A matrix that rotates the snapshot space such that the blur direction is
  // +X.
  auto texture_rotate = Matrix::MakeRotationZ(
      transformed_blur_radius.Normalize().AngleTo({1, 0}));

  // Converts local pass space to screen space. This is just the snapshot space
  // rotated such that the blur direction is +X.
  auto pass_transform = texture_rotate * input_snapshot->transform;

  // The pass texture coverage, but rotated such that the blur is in the +X
  // direction, and expanded to include the blur radius. This is used for UV
  // projection and as a source for the pass size. Note that it doesn't matter
  // which direction the space is rotated in when grabbing the pass size.
  auto pass_texture_rect = Rect::MakeSize(input_snapshot->texture->GetSize())
                               .TransformBounds(pass_transform);
  pass_texture_rect.origin.x -= transformed_blur_radius_length;
  pass_texture_rect.size.width += transformed_blur_radius_length * 2;

  // Source override snapshot.

  auto source = source_override_ ? source_override_ : inputs[0];
  auto source_snapshot = source->GetSnapshot("GaussianBlur(Override)", renderer,
                                             entity, GetCoverageHint());
  if (!source_snapshot.has_value()) {
    return std::nullopt;
  }

  // UV mapping.

  auto pass_uv_project = [&texture_rotate,
                          &pass_texture_rect](Snapshot& input) {
    auto uv_matrix = Matrix::MakeScale(1 / Vector2(input.texture->GetSize())) *
                     (texture_rotate * input.transform).Invert();
    return pass_texture_rect.GetTransformedPoints(uv_matrix);
  };

  auto input_uvs = pass_uv_project(input_snapshot.value());
  auto source_uvs = pass_uv_project(source_snapshot.value());

  //----------------------------------------------------------------------------
  /// Render to texture.
  ///

  ContentContext::SubpassCallback subpass_callback = [&](const ContentContext&
                                                             renderer,
                                                         RenderPass& pass) {
    auto& host_buffer = pass.GetTransientsBuffer();

    VertexBufferBuilder<VS::PerVertexData> vtx_builder;
    vtx_builder.AddVertices({
        {Point(0, 0), input_uvs[0], source_uvs[0]},
        {Point(1, 0), input_uvs[1], source_uvs[1]},
        {Point(1, 1), input_uvs[3], source_uvs[3]},
        {Point(0, 0), input_uvs[0], source_uvs[0]},
        {Point(1, 1), input_uvs[3], source_uvs[3]},
        {Point(0, 1), input_uvs[2], source_uvs[2]},
    });
    auto vtx_buffer = vtx_builder.CreateVertexBuffer(host_buffer);

    VS::FrameInfo frame_info;
    frame_info.mvp = Matrix::MakeOrthographic(ISize(1, 1));
    frame_info.texture_sampler_y_coord_scale =
        input_snapshot->texture->GetYCoordScale();
    frame_info.alpha_mask_sampler_y_coord_scale =
        source_snapshot->texture->GetYCoordScale();

    FS::BlurInfo frag_info;
    auto r = Radius{transformed_blur_radius_length};
    frag_info.blur_sigma = Sigma{r}.sigma;
    frag_info.blur_radius = std::round(r.radius);

    // The blur direction is in input UV space.
    frag_info.blur_uv_offset =
        pass_transform.Invert().TransformDirection(Vector2(1, 0)).Normalize() /
        Point(input_snapshot->GetCoverage().value().size);
    Command cmd;
    DEBUG_COMMAND_INFO(cmd, SPrintF("Gaussian Blur Filter (Radius=%.2f)",
                                    transformed_blur_radius_length));
    cmd.BindVertices(vtx_buffer);

    auto options = OptionsFromPass(pass);
    options.blend_mode = BlendMode::kSource;
    auto input_descriptor = input_snapshot->sampler_descriptor;
    auto source_descriptor = source_snapshot->sampler_descriptor;
    UpdateSamplerDescriptor(input_descriptor, source_descriptor, renderer);
    input_descriptor.mag_filter = MinMagFilter::kLinear;
    input_descriptor.min_filter = MinMagFilter::kLinear;

    bool has_alpha_mask = blur_style_ != BlurStyle::kNormal;
    bool has_decal_specialization =
        tile_mode_ == Entity::TileMode::kDecal &&
        !renderer.GetDeviceCapabilities().SupportsDecalSamplerAddressMode();

    if (has_alpha_mask && has_decal_specialization) {
      cmd.pipeline = renderer.GetGaussianBlurAlphaDecalPipeline(options);
    } else if (has_alpha_mask) {
      cmd.pipeline = renderer.GetGaussianBlurAlphaPipeline(options);
    } else if (has_decal_specialization) {
      cmd.pipeline = renderer.GetGaussianBlurDecalPipeline(options);
    } else {
      cmd.pipeline = renderer.GetGaussianBlurPipeline(options);
    }

    FS::BindTextureSampler(
        cmd, input_snapshot->texture,
        renderer.GetContext()->GetSamplerLibrary()->GetSampler(
            input_descriptor));
    VS::BindFrameInfo(cmd, host_buffer.EmplaceUniform(frame_info));
    FS::BindBlurInfo(cmd, host_buffer.EmplaceUniform(frag_info));

    if (has_alpha_mask) {
      FS::MaskInfo mask_info;
      mask_info.src_factor = src_color_factor_;
      mask_info.inner_blur_factor = inner_blur_factor_;
      mask_info.outer_blur_factor = outer_blur_factor_;

      FS::BindAlphaMaskSampler(
          cmd, source_snapshot->texture,
          renderer.GetContext()->GetSamplerLibrary()->GetSampler(
              source_descriptor));
      FS::BindMaskInfo(cmd, host_buffer.EmplaceUniform(mask_info));
    }

    return pass.AddCommand(std::move(cmd));
  };

  // Radius 4, 920, 1124 -> 956, 1123 -> 956, 1260

  // The scale curve is computed such that the maximum kernel size is 14 for
  // each direction. The scale down is only performed in the first pass.
  Vector2 scale = {1.0, 1.0};
  if (is_first_pass) {
    auto transformed_x = transform.GetMaxBasisLength() * blur_sigma_.sigma;
    auto transformed_y =
        transform.GetMaxBasisLength() * secondary_blur_sigma_.sigma;
    FML_LOG(ERROR) << transformed_x << "," << transformed_y;
    float scaled_sigma_x = transformed_x > kMaxLinearBlurSigma
                               ? kMaxLinearBlurSigma / transformed_x
                               : 1.f;
    float scaled_sigma_y = transformed_y > kMaxLinearBlurSigma
                               ? (kMaxLinearBlurSigma / transformed_y)
                               : 1.f;

    scale.x = scaled_sigma_x;
    scale.y = scaled_sigma_y;
  }
  if (is_first_pass) {
    FML_LOG(ERROR) << "scale: " << scale;
  }

  Vector2 scaled_size = pass_texture_rect.size * scale;
  ISize floored_size = ISize(scaled_size.x, scaled_size.y);

  auto out_texture = renderer.MakeSubpass("Directional Gaussian Blur Filter",
                                          floored_size, subpass_callback);

  if (!out_texture) {
    return std::nullopt;
  }

  SamplerDescriptor sampler_desc;
  sampler_desc.min_filter = MinMagFilter::kLinear;
  sampler_desc.mag_filter = MinMagFilter::kLinear;
  sampler_desc.width_address_mode = SamplerAddressMode::kClampToEdge;
  sampler_desc.width_address_mode = SamplerAddressMode::kClampToEdge;

  return Entity::FromSnapshot(
      Snapshot{.texture = out_texture,
               .transform = texture_rotate.Invert() *
                            Matrix::MakeTranslation(pass_texture_rect.origin) *
                            Matrix::MakeScale((1 / scale) *
                                              (scaled_size / floored_size)),
               .sampler_descriptor = sampler_desc,
               .opacity = input_snapshot->opacity},
      entity.GetBlendMode(), entity.GetStencilDepth());
}

std::optional<Rect> DirectionalGaussianBlurFilterContents::GetFilterCoverage(
    const FilterInput::Vector& inputs,
    const Entity& entity,
    const Matrix& effect_transform) const {
  if (inputs.empty()) {
    return std::nullopt;
  }

  auto coverage = inputs[0]->GetCoverage(entity);
  if (!coverage.has_value()) {
    return std::nullopt;
  }

  auto transform = inputs[0]->GetTransform(entity) * effect_transform.Basis();
  auto transformed_blur_vector =
      transform.TransformDirection(blur_direction_ * Radius{blur_sigma_}.radius)
          .Abs();
  auto extent = coverage->size + transformed_blur_vector * 2;
  return Rect(coverage->origin - transformed_blur_vector,
              Size(extent.x, extent.y));
}

}  // namespace impeller
