// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/tessellator/tessellator.h"

#include "third_party/libtess2/Include/tesselator.h"

namespace impeller {

static void* HeapAlloc(void* userData, unsigned int size) {
  return malloc(size);
}

static void* HeapRealloc(void* userData, void* ptr, unsigned int size) {
  return realloc(ptr, size);
}

static void HeapFree(void* userData, void* ptr) {
  free(ptr);
}

// Note: these units are "number of entities" for bucket size and not in KB.
static TESSalloc alloc = {
    HeapAlloc, HeapRealloc, HeapFree, 0, /* =userData */
    16,                                  /* =meshEdgeBucketSize */
    16,                                  /* =meshVertexBucketSize */
    16,                                  /* =meshFaceBucketSize */
    16,                                  /* =dictNodeBucketSize */
    16,                                  /* =regionBucketSize */
    0                                    /* =extraVertices */
};

static int ToTessWindingRule(FillType fill_type) {
  switch (fill_type) {
    case FillType::kOdd:
      return TESS_WINDING_ODD;
    case FillType::kNonZero:
      return TESS_WINDING_NONZERO;
    case FillType::kPositive:
      return TESS_WINDING_POSITIVE;
    case FillType::kNegative:
      return TESS_WINDING_NEGATIVE;
    case FillType::kAbsGeqTwo:
      return TESS_WINDING_ABS_GEQ_TWO;
  }
  return TESS_WINDING_ODD;
}

constexpr int kVertexSize = 2;
constexpr int kPolygonSize = 3;

class TessellationListener : public PathListener {
 public:
  explicit TessellationListener(TESStesselator* tessellator)
      : tessellator_(tessellator) {}

  ~TessellationListener() = default;

  void OnContour(const Point data[], size_t contour_size) override {
    if (contour_size == 0u) {
      return;
    }
    ::tessAddContour(tessellator_,   // the C tessellator
                     kVertexSize,    //
                     data,           //
                     sizeof(Point),  //
                     contour_size    //
    );
  }

  Tessellator::Result Finish(const Tessellator::BuilderCallback& callback,
                             FillType fill_type) {
    auto result = ::tessTesselate(tessellator_,                  // tessellator
                                  ToTessWindingRule(fill_type),  // winding
                                  TESS_POLYGONS,                 // element type
                                  kPolygonSize,                  // polygon size
                                  kVertexSize,                   // vertex size
                                  nullptr  // normal (null is automatic)
    );

    if (result != 1) {
      return Tessellator::Result::kTessellationError;
    }

    int vertexItemCount = tessGetVertexCount(tessellator_) * kVertexSize;
    auto vertices = tessGetVertices(tessellator_);
    int elementItemCount = tessGetElementCount(tessellator_) * kPolygonSize;
    auto elements = tessGetElements(tessellator_);
    // libtess uses an int index internally due to usage of -1 as a sentinel
    // value.
    std::vector<uint16_t> indices(elementItemCount);
    for (int i = 0; i < elementItemCount; i++) {
      indices[i] = static_cast<uint16_t>(elements[i]);
    }
    if (!callback(vertices, vertexItemCount, indices.data(),
                  elementItemCount)) {
      return Tessellator::Result::kInputError;
    }

    return Tessellator::Result::kSuccess;
  }

 private:
  TESStesselator* tessellator_;
};

Tessellator::Tessellator()
    : c_tessellator_(::tessNewTess(&alloc), &DestroyTessellator) {}

Tessellator::~Tessellator() = default;

Tessellator::Result Tessellator::Tessellate(
    FillType fill_type,
    Scalar scale,
    const Path& path,
    const BuilderCallback& callback) const {
  if (!callback) {
    return Result::kInputError;
  }

  auto tessellator = c_tessellator_.get();
  if (!tessellator) {
    return Result::kTessellationError;
  }
  auto listener = TessellationListener(tessellator);
  path.CreatePolyline(scale, listener);
  return listener.Finish(callback, fill_type);
}

void DestroyTessellator(TESStesselator* tessellator) {
  if (tessellator != nullptr) {
    ::tessDeleteTess(tessellator);
  }
}

}  // namespace impeller
