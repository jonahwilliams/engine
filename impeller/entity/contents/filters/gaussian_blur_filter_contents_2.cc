// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/entity/contents/filters/gaussian_blur_filter_contents_2.h"

#include <cmath>
#include <utility>
#include <valarray>

#include "fml/logging.h"
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
                         ISize radius,
                         std::array<float, kMaxBlurSamples> kernel) {
  // Callers are responsible for downscaling large sigmas to values that can be
  // processed by the effects, so ensure the radius won't overflow 'kernel'
  const int width = BlurKernelWidth(radius.width);
  const int height = BlurKernelWidth(radius.height);
  const size_t kernelSize = width * height;
  FML_DCHECK(kernelSize <= kMaxBlurSamples);

  // And the definition of an identity blur should be sufficient that 2sigma^2
  // isn't near zero when there's a non-trivial radius.
  const float twoSigmaSqrdX = 2.0f * sigma_x.sigma * sigma_x.sigma;
  const float twoSigmaSqrdY = 2.0f * sigma_y.sigma * sigma_y.sigma;
  ;

  FML_DCHECK((radius.width == 0 || !ScalarNearlyZero(twoSigmaSqrdX)) &&
             (radius.height == 0 || !ScalarNearlyZero(twoSigmaSqrdY)));

  // Setting the denominator to 1 when the radius is 0 automatically converts
  // the remaining math to the 1D Gaussian distribution. When both radii are 0,
  // it correctly computes a weight of 1.0
  const float sigmaXDenom = radius.width > 0 ? 1.0f / twoSigmaSqrdX : 1.f;
  const float sigmaYDenom = radius.height > 0 ? 1.0f / twoSigmaSqrdY : 1.f;

  float sum = 0.0f;
  for (int x = 0; x < width; x++) {
    float xTerm = static_cast<float>(x - radius.width);
    xTerm = xTerm * xTerm * sigmaXDenom;
    for (int y = 0; y < height; y++) {
      float yTerm = static_cast<float>(y - radius.height);
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

void Rescale(Sigma sigma_x,
             Sigma sigma_y) {
      // We round down here so that when we recalculate sigmas we know they will be below
    // kMaxSigma (but clamp to 1 do we don't have an empty texture).
    ISize rescaledSize = {std::max(sk_float_floor2int(srcBounds.width() * scaleX), 1),
                            std::max(sk_float_floor2int(srcBounds.height() * scaleY), 1)};

    float scaleX = sigma_x.sigma > kMaxLinearBlurSigma ? kMaxLinearBlurSigma / sigma_x.sigma : 1.f;
    float scaleY = sigma_y.sigma > kMaxLinearBlurSigma ? kMaxLinearBlurSigma / sigma_y.sigma : 1.f;
    // We round down here so that when we recalculate sigmas we know they will be below
    // kMaxSigma (but clamp to 1 do we don't have an empty texture).
    ISize rescaledSize = {std::max(sk_float_floor2int(srcBounds.width() * scaleX), 1),
                          std::max(sk_float_floor2int(srcBounds.height() * scaleY), 1)};
    // Compute the sigmas using the actual scale factors used once we integerized the
    // rescaledSize.
    scaleX = static_cast<float>(rescaledSize.width()) / srcBounds.width();
    scaleY = static_cast<float>(rescaledSize.height()) / srcBounds.height();
}

//     std::array<float, skgpu::kMaxBlurSamples> kernel;

GaussianBlurFilterContents::GaussianBlurFilterContents() = default;

GaussianBlurFilterContents::~GaussianBlurFilterContents() = default;

void GaussianBlurFilterContents::SetSigma(Sigma sigma) {
  blur_sigma_ = sigma;
}

void GaussianBlurFilterContents::SetTileMode(Entity::TileMode tile_mode) {
  tile_mode_ = tile_mode;
}

void GaussianBlurFilterContents::SetSourceOverride(
    FilterInput::Ref source_override) {
  source_override_ = std::move(source_override);
}

std::optional<Entity> GaussianBlurFilterContents::RenderFilter(
    const FilterInput::Vector& inputs,
    const ContentContext& renderer,
    const Entity& entity,
    const Matrix& effect_transform,
    const Rect& coverage,
    const std::optional<Rect>& coverage_hint) const {
  using VS = GaussianBlurAlphaDecalPipeline::VertexShader;
  using FS = GaussianBlurAlphaDecalPipeline::FragmentShader;

  //----------------------------------------------------------------------------
  /// Handle inputs.
  ///

  if (inputs.empty()) {
    return std::nullopt;
  }

  // Limit the kernel size to 1000x1000 pixels, like Skia does.
  auto radius = std::min(Radius{blur_sigma_}.radius, 500.0f);
  auto transform = entity.GetTransformation() * effect_transform.Basis();
}

std::optional<Rect> GaussianBlurFilterContents::GetFilterCoverage(
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
}

}  // namespace impeller
