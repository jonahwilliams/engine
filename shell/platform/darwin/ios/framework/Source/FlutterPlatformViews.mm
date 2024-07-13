// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unordered_map>
#include "flow/surface_frame.h"
#import "flutter/shell/platform/darwin/ios/framework/Source/FlutterPlatformViews_Internal.h"
#include "fml/logging.h"

#include <Metal/Metal.h>

#include "flutter/fml/make_copyable.h"
#include "flutter/fml/platform/darwin/scoped_nsobject.h"
#import "flutter/shell/platform/darwin/ios/framework/Source/FlutterOverlayView.h"
#import "flutter/shell/platform/darwin/ios/framework/Source/FlutterView.h"
#import "flutter/shell/platform/darwin/ios/ios_surface.h"

FLUTTER_ASSERT_ARC

@implementation UIView (FirstResponder)
- (BOOL)flt_hasFirstResponderInViewHierarchySubtree {
  if (self.isFirstResponder) {
    return YES;
  }
  for (UIView* subview in self.subviews) {
    if (subview.flt_hasFirstResponderInViewHierarchySubtree) {
      return YES;
    }
  }
  return NO;
}
@end

// Determines if the `clip_rect` from a clipRect mutator contains the
// `platformview_boundingrect`.
//
// `clip_rect` is in its own coordinate space. The rect needs to be transformed by
// `transform_matrix` to be in the coordinate space where the PlatformView is displayed.
//
// `platformview_boundingrect` is the final bounding rect of the PlatformView in the coordinate
// space where the PlatformView is displayed.
static bool ClipRectContainsPlatformViewBoundingRect(const SkRect& clip_rect,
                                                     const SkRect& platformview_boundingrect,
                                                     const SkMatrix& transform_matrix) {
  SkRect transformed_rect = transform_matrix.mapRect(clip_rect);
  return transformed_rect.contains(platformview_boundingrect);
}

// Determines if the `clipRRect` from a clipRRect mutator contains the
// `platformview_boundingrect`.
//
// `clip_rrect` is in its own coordinate space. The rrect needs to be transformed by
// `transform_matrix` to be in the coordinate space where the PlatformView is displayed.
//
// `platformview_boundingrect` is the final bounding rect of the PlatformView in the coordinate
// space where the PlatformView is displayed.
static bool ClipRRectContainsPlatformViewBoundingRect(const SkRRect& clip_rrect,
                                                      const SkRect& platformview_boundingrect,
                                                      const SkMatrix& transform_matrix) {
  SkVector upper_left = clip_rrect.radii(SkRRect::Corner::kUpperLeft_Corner);
  SkVector upper_right = clip_rrect.radii(SkRRect::Corner::kUpperRight_Corner);
  SkVector lower_right = clip_rrect.radii(SkRRect::Corner::kLowerRight_Corner);
  SkVector lower_left = clip_rrect.radii(SkRRect::Corner::kLowerLeft_Corner);
  SkScalar transformed_upper_left_x = transform_matrix.mapRadius(upper_left.x());
  SkScalar transformed_upper_left_y = transform_matrix.mapRadius(upper_left.y());
  SkScalar transformed_upper_right_x = transform_matrix.mapRadius(upper_right.x());
  SkScalar transformed_upper_right_y = transform_matrix.mapRadius(upper_right.y());
  SkScalar transformed_lower_right_x = transform_matrix.mapRadius(lower_right.x());
  SkScalar transformed_lower_right_y = transform_matrix.mapRadius(lower_right.y());
  SkScalar transformed_lower_left_x = transform_matrix.mapRadius(lower_left.x());
  SkScalar transformed_lower_left_y = transform_matrix.mapRadius(lower_left.y());
  SkRect transformed_clip_rect = transform_matrix.mapRect(clip_rrect.rect());
  SkRRect transformed_rrect;
  SkVector corners[] = {{transformed_upper_left_x, transformed_upper_left_y},
                        {transformed_upper_right_x, transformed_upper_right_y},
                        {transformed_lower_right_x, transformed_lower_right_y},
                        {transformed_lower_left_x, transformed_lower_left_y}};
  transformed_rrect.setRectRadii(transformed_clip_rect, corners);
  return transformed_rrect.contains(platformview_boundingrect);
}

namespace flutter {
// Becomes NO if Apple's API changes and blurred backdrop filters cannot be applied.
BOOL canApplyBlurBackdrop = YES;

std::shared_ptr<FlutterPlatformViewLayer> FlutterPlatformViewLayerPool::GetNextLayer() {
  layers_mutex_.lock();

  std::shared_ptr<FlutterPlatformViewLayer> result;
  if (available_layer_index_ < layers_.size()) {
    result = layers_[available_layer_index_];
    available_layer_index_++;
  }

  layers_mutex_.unlock();

  return result;
}

void FlutterPlatformViewLayerPool::CreateLayer(GrDirectContext* gr_context,
                                               const std::shared_ptr<IOSContext>& ios_context,
                                               MTLPixelFormat pixel_format) {
  std::shared_ptr<FlutterPlatformViewLayer> layer;
  fml::scoped_nsobject<UIView> overlay_view;
  fml::scoped_nsobject<UIView> overlay_view_wrapper;

  bool impeller_enabled = !!ios_context->GetImpellerContext();
  if (!gr_context && !impeller_enabled) {
    overlay_view.reset([[FlutterOverlayView alloc] init]);
    overlay_view_wrapper.reset([[FlutterOverlayView alloc] init]);

    auto ca_layer = fml::scoped_nsobject<CALayer>{[overlay_view.get() layer]};
    std::unique_ptr<IOSSurface> ios_surface = IOSSurface::Create(ios_context, ca_layer);
    std::unique_ptr<Surface> surface = ios_surface->CreateGPUSurface();

    layer = std::make_shared<FlutterPlatformViewLayer>(std::move(overlay_view),
                                                       std::move(overlay_view_wrapper),
                                                       std::move(ios_surface), std::move(surface));
  } else {
    CGFloat screenScale = [UIScreen mainScreen].scale;
    overlay_view.reset([[FlutterOverlayView alloc] initWithContentsScale:screenScale
                                                             pixelFormat:pixel_format]);
    overlay_view_wrapper.reset([[FlutterOverlayView alloc] initWithContentsScale:screenScale
                                                                     pixelFormat:pixel_format]);

    auto ca_layer = fml::scoped_nsobject<CALayer>{[overlay_view.get() layer]};
    std::unique_ptr<IOSSurface> ios_surface = IOSSurface::Create(ios_context, ca_layer);
    std::unique_ptr<Surface> surface = ios_surface->CreateGPUSurface(gr_context);

    layer = std::make_shared<FlutterPlatformViewLayer>(std::move(overlay_view),
                                                       std::move(overlay_view_wrapper),
                                                       std::move(ios_surface), std::move(surface));
    layer->gr_context = gr_context;
  }
  // The overlay view wrapper masks the overlay view.
  // This is required to keep the backing surface size unchanged between frames.
  //
  // Otherwise, changing the size of the overlay would require a new surface,
  // which can be very expensive.
  //
  // This is the case of an animation in which the overlay size is changing in every frame.
  //
  // +------------------------+
  // |   overlay_view         |
  // |    +--------------+    |              +--------------+
  // |    |    wrapper   |    |  == mask =>  | overlay_view |
  // |    +--------------+    |              +--------------+
  // +------------------------+
  layer->overlay_view_wrapper.get().clipsToBounds = YES;
  [layer->overlay_view_wrapper.get() addSubview:layer->overlay_view];

  layers_mutex_.lock();
  layers_.push_back(layer);
  layers_mutex_.unlock();
}

void FlutterPlatformViewLayerPool::RecycleLayers() {
  available_layer_index_ = 0;
}

std::vector<std::shared_ptr<FlutterPlatformViewLayer>>
FlutterPlatformViewLayerPool::GetUnusedLayers() {
  layers_mutex_.lock();
  std::vector<std::shared_ptr<FlutterPlatformViewLayer>> results;
  for (size_t i = available_layer_index_; i < layers_.size(); i++) {
    results.push_back(layers_[i]);
  }
  layers_mutex_.unlock();
  return results;
}

void FlutterPlatformViewsController::SetFlutterView(UIView* flutter_view) {
  flutter_view_.reset(flutter_view);
}

void FlutterPlatformViewsController::SetFlutterViewController(
    UIViewController<FlutterViewResponder>* flutter_view_controller) {
  flutter_view_controller_.reset(flutter_view_controller);
}

UIViewController<FlutterViewResponder>* FlutterPlatformViewsController::getFlutterViewController() {
  return flutter_view_controller_.get();
}

void FlutterPlatformViewsController::OnMethodCall(FlutterMethodCall* call, FlutterResult result) {
  if ([[call method] isEqualToString:@"create"]) {
    OnCreate(call, result);
  } else if ([[call method] isEqualToString:@"dispose"]) {
    OnDispose(call, result);
  } else if ([[call method] isEqualToString:@"acceptGesture"]) {
    OnAcceptGesture(call, result);
  } else if ([[call method] isEqualToString:@"rejectGesture"]) {
    OnRejectGesture(call, result);
  } else {
    result(FlutterMethodNotImplemented);
  }
}

void FlutterPlatformViewsController::OnCreate(FlutterMethodCall* call, FlutterResult result) {
  NSDictionary<NSString*, id>* args = [call arguments];

  int64_t viewId = [args[@"id"] longLongValue];
  NSString* viewTypeString = args[@"viewType"];
  std::string viewType(viewTypeString.UTF8String);

  if (views_.count(viewId) != 0) {
    result([FlutterError errorWithCode:@"recreating_view"
                               message:@"trying to create an already created view"
                               details:[NSString stringWithFormat:@"view id: '%lld'", viewId]]);
  }

  NSObject<FlutterPlatformViewFactory>* factory = factories_[viewType].get();
  if (factory == nil) {
    result([FlutterError
        errorWithCode:@"unregistered_view_type"
              message:[NSString stringWithFormat:@"A UIKitView widget is trying to create a "
                                                 @"PlatformView with an unregistered type: < %@ >",
                                                 viewTypeString]
              details:@"If you are the author of the PlatformView, make sure `registerViewFactory` "
                      @"is invoked.\n"
                      @"See: "
                      @"https://docs.flutter.dev/development/platform-integration/"
                      @"platform-views#on-the-platform-side-1 for more details.\n"
                      @"If you are not the author of the PlatformView, make sure to call "
                      @"`GeneratedPluginRegistrant.register`."]);
    return;
  }

  id params = nil;
  if ([factory respondsToSelector:@selector(createArgsCodec)]) {
    NSObject<FlutterMessageCodec>* codec = [factory createArgsCodec];
    if (codec != nil && args[@"params"] != nil) {
      FlutterStandardTypedData* paramsData = args[@"params"];
      params = [codec decode:paramsData.data];
    }
  }

  NSObject<FlutterPlatformView>* embedded_view = [factory createWithFrame:CGRectZero
                                                           viewIdentifier:viewId
                                                                arguments:params];
  UIView* platform_view = [embedded_view view];
  // Set a unique view identifier, so the platform view can be identified in unit tests.
  platform_view.accessibilityIdentifier =
      [NSString stringWithFormat:@"platform_view[%lld]", viewId];
  views_[viewId] = fml::scoped_nsobject<NSObject<FlutterPlatformView>>(embedded_view);

  FlutterTouchInterceptingView* touch_interceptor = [[FlutterTouchInterceptingView alloc]
                  initWithEmbeddedView:platform_view
               platformViewsController:GetWeakPtr()
      gestureRecognizersBlockingPolicy:gesture_recognizers_blocking_policies_[viewType]];

  touch_interceptors_[viewId] =
      fml::scoped_nsobject<FlutterTouchInterceptingView>(touch_interceptor);

  ChildClippingView* clipping_view = [[ChildClippingView alloc] initWithFrame:CGRectZero];
  [clipping_view addSubview:touch_interceptor];
  root_views_[viewId] = fml::scoped_nsobject<UIView>(clipping_view);

  result(nil);
}

void FlutterPlatformViewsController::OnDispose(FlutterMethodCall* call, FlutterResult result) {
  NSNumber* arg = [call arguments];
  int64_t viewId = [arg longLongValue];

  if (views_.count(viewId) == 0) {
    result([FlutterError errorWithCode:@"unknown_view"
                               message:@"trying to dispose an unknown"
                               details:[NSString stringWithFormat:@"view id: '%lld'", viewId]]);
    return;
  }
  // We wait for next submitFrame to dispose views.
  views_to_dispose_.insert(viewId);
  result(nil);
}

void FlutterPlatformViewsController::OnAcceptGesture(FlutterMethodCall* call,
                                                     FlutterResult result) {
  NSDictionary<NSString*, id>* args = [call arguments];
  int64_t viewId = [args[@"id"] longLongValue];

  if (views_.count(viewId) == 0) {
    result([FlutterError errorWithCode:@"unknown_view"
                               message:@"trying to set gesture state for an unknown view"
                               details:[NSString stringWithFormat:@"view id: '%lld'", viewId]]);
    return;
  }

  FlutterTouchInterceptingView* view = touch_interceptors_[viewId].get();
  [view releaseGesture];

  result(nil);
}

void FlutterPlatformViewsController::OnRejectGesture(FlutterMethodCall* call,
                                                     FlutterResult result) {
  NSDictionary<NSString*, id>* args = [call arguments];
  int64_t viewId = [args[@"id"] longLongValue];

  if (views_.count(viewId) == 0) {
    result([FlutterError errorWithCode:@"unknown_view"
                               message:@"trying to set gesture state for an unknown view"
                               details:[NSString stringWithFormat:@"view id: '%lld'", viewId]]);
    return;
  }

  FlutterTouchInterceptingView* view = touch_interceptors_[viewId].get();
  [view blockGesture];

  result(nil);
}

void FlutterPlatformViewsController::RegisterViewFactory(
    NSObject<FlutterPlatformViewFactory>* factory,
    NSString* factoryId,
    FlutterPlatformViewGestureRecognizersBlockingPolicy gestureRecognizerBlockingPolicy) {
  std::string idString([factoryId UTF8String]);
  FML_CHECK(factories_.count(idString) == 0);
  factories_[idString] = fml::scoped_nsobject<NSObject<FlutterPlatformViewFactory>>(factory);
  gesture_recognizers_blocking_policies_[idString] = gestureRecognizerBlockingPolicy;
}

void FlutterPlatformViewsController::BeginFrame(SkISize frame_size) {
  ResetFrameState();
  frame_size_ = frame_size;
}

void FlutterPlatformViewsController::CancelFrame() {
  ResetFrameState();
}

PostPrerollResult FlutterPlatformViewsController::PostPrerollAction(
    const fml::RefPtr<fml::RasterThreadMerger>& raster_thread_merger) {
  return PostPrerollResult::kSuccess;
}

void FlutterPlatformViewsController::EndFrame(
    bool should_resubmit_frame,
    const fml::RefPtr<fml::RasterThreadMerger>& raster_thread_merger) {}

void FlutterPlatformViewsController::PushFilterToVisitedPlatformViews(
    const std::shared_ptr<const DlImageFilter>& filter,
    const SkRect& filter_rect) {
  for (int64_t id : visited_platform_views_) {
    EmbeddedViewParams params = current_composition_params_[id];
    params.PushImageFilter(filter, filter_rect);
    current_composition_params_[id] = params;
  }
}

void FlutterPlatformViewsController::PrerollCompositeEmbeddedView(
    int64_t view_id,
    std::unique_ptr<EmbeddedViewParams> params) {
  SkRect view_bounds = SkRect::Make(frame_size_);
  std::unique_ptr<EmbedderViewSlice> view;
  view = std::make_unique<DisplayListEmbedderViewSlice>(view_bounds);
  slices_.insert_or_assign(view_id, std::move(view));

  composition_order_.push_back(view_id);

  if (current_composition_params_.count(view_id) == 1 &&
      current_composition_params_[view_id] == *params.get()) {
    // Do nothing if the params didn't change.
    return;
  }
  current_composition_params_[view_id] = EmbeddedViewParams(*params.get());
  views_to_recomposite_.insert(view_id);
}

size_t FlutterPlatformViewsController::EmbeddedViewCount() {
  return composition_order_.size();
}

UIView* FlutterPlatformViewsController::GetPlatformViewByID(int64_t view_id) {
  return [GetFlutterTouchInterceptingViewByID(view_id) embeddedView];
}

FlutterTouchInterceptingView* FlutterPlatformViewsController::GetFlutterTouchInterceptingViewByID(
    int64_t view_id) {
  if (views_.empty()) {
    return nil;
  }
  return touch_interceptors_[view_id].get();
}

long FlutterPlatformViewsController::FindFirstResponderPlatformViewId() {
  for (auto const& [id, root_view] : root_views_) {
    if (((UIView*)root_view.get()).flt_hasFirstResponderInViewHierarchySubtree) {
      return id;
    }
  }
  return -1;
}

int FlutterPlatformViewsController::CountClips(const MutatorsStack& mutators_stack) {
  std::vector<std::shared_ptr<Mutator>>::const_reverse_iterator iter = mutators_stack.Bottom();
  int clipCount = 0;
  while (iter != mutators_stack.Top()) {
    if ((*iter)->IsClipType()) {
      clipCount++;
    }
    ++iter;
  }
  return clipCount;
}

void FlutterPlatformViewsController::ClipViewSetMaskView(UIView* clipView) {
  FML_DCHECK([[NSThread currentThread] isMainThread]);
  if (clipView.maskView) {
    return;
  }
  UIView* flutterView = flutter_view_.get();
  CGRect frame =
      CGRectMake(-clipView.frame.origin.x, -clipView.frame.origin.y,
                 CGRectGetWidth(flutterView.bounds), CGRectGetHeight(flutterView.bounds));
  clipView.maskView = [mask_view_pool_.get() getMaskViewWithFrame:frame];
}

// This method is only called when the `embedded_view` needs to be re-composited at the current
// frame. See: `CompositeWithParams` for details.
void FlutterPlatformViewsController::ApplyMutators(const MutatorsStack& mutators_stack,
                                                   UIView* embedded_view,
                                                   const SkRect& bounding_rect) {
  FML_DCHECK([[NSThread currentThread] isMainThread]);
  if (flutter_view_ == nullptr) {
    return;
  }

  FML_DCHECK(CATransform3DEqualToTransform(embedded_view.layer.transform, CATransform3DIdentity));
  ResetAnchor(embedded_view.layer);
  ChildClippingView* clipView = (ChildClippingView*)embedded_view.superview;

  SkMatrix transformMatrix;
  NSMutableArray* blurFilters = [[NSMutableArray alloc] init];
  FML_DCHECK(!clipView.maskView ||
             [clipView.maskView isKindOfClass:[FlutterClippingMaskView class]]);
  if (clipView.maskView) {
    [mask_view_pool_.get() insertViewToPoolIfNeeded:(FlutterClippingMaskView*)(clipView.maskView)];
    clipView.maskView = nil;
  }
  CGFloat screenScale = [UIScreen mainScreen].scale;
  auto iter = mutators_stack.Begin();
  while (iter != mutators_stack.End()) {
    switch ((*iter)->GetType()) {
      case kTransform: {
        transformMatrix.preConcat((*iter)->GetMatrix());
        break;
      }
      case kClipRect: {
        if (ClipRectContainsPlatformViewBoundingRect((*iter)->GetRect(), bounding_rect,
                                                     transformMatrix)) {
          break;
        }
        ClipViewSetMaskView(clipView);
        [(FlutterClippingMaskView*)clipView.maskView clipRect:(*iter)->GetRect()
                                                       matrix:transformMatrix];
        break;
      }
      case kClipRRect: {
        if (ClipRRectContainsPlatformViewBoundingRect((*iter)->GetRRect(), bounding_rect,
                                                      transformMatrix)) {
          break;
        }
        ClipViewSetMaskView(clipView);
        [(FlutterClippingMaskView*)clipView.maskView clipRRect:(*iter)->GetRRect()
                                                        matrix:transformMatrix];
        break;
      }
      case kClipPath: {
        // TODO(cyanglaz): Find a way to pre-determine if path contains the PlatformView boudning
        // rect. See `ClipRRectContainsPlatformViewBoundingRect`.
        // https://github.com/flutter/flutter/issues/118650
        ClipViewSetMaskView(clipView);
        [(FlutterClippingMaskView*)clipView.maskView clipPath:(*iter)->GetPath()
                                                       matrix:transformMatrix];
        break;
      }
      case kOpacity:
        embedded_view.alpha = (*iter)->GetAlphaFloat() * embedded_view.alpha;
        break;
      case kBackdropFilter: {
        // Only support DlBlurImageFilter for BackdropFilter.
        if (!canApplyBlurBackdrop || !(*iter)->GetFilterMutation().GetFilter().asBlur()) {
          break;
        }
        CGRect filterRect =
            flutter::GetCGRectFromSkRect((*iter)->GetFilterMutation().GetFilterRect());
        // `filterRect` is in global coordinates. We need to convert to local space.
        filterRect = CGRectApplyAffineTransform(
            filterRect, CGAffineTransformMakeScale(1 / screenScale, 1 / screenScale));
        // `filterRect` reprents the rect that should be filtered inside the `flutter_view_`.
        // The `PlatformViewFilter` needs the frame inside the `clipView` that needs to be
        // filtered.
        if (CGRectIsNull(CGRectIntersection(filterRect, clipView.frame))) {
          break;
        }
        CGRect intersection = CGRectIntersection(filterRect, clipView.frame);
        CGRect frameInClipView = [flutter_view_.get() convertRect:intersection toView:clipView];
        // sigma_x is arbitrarily chosen as the radius value because Quartz sets
        // sigma_x and sigma_y equal to each other. DlBlurImageFilter's Tile Mode
        // is not supported in Quartz's gaussianBlur CAFilter, so it is not used
        // to blur the PlatformView.
        CGFloat blurRadius = (*iter)->GetFilterMutation().GetFilter().asBlur()->sigma_x();
        UIVisualEffectView* visualEffectView = [[UIVisualEffectView alloc]
            initWithEffect:[UIBlurEffect effectWithStyle:UIBlurEffectStyleLight]];
        PlatformViewFilter* filter = [[PlatformViewFilter alloc] initWithFrame:frameInClipView
                                                                    blurRadius:blurRadius
                                                              visualEffectView:visualEffectView];
        if (!filter) {
          canApplyBlurBackdrop = NO;
        } else {
          [blurFilters addObject:filter];
        }
        break;
      }
    }
    ++iter;
  }

  if (canApplyBlurBackdrop) {
    [clipView applyBlurBackdropFilters:blurFilters];
  }

  // The UIKit frame is set based on the logical resolution (points) instead of physical.
  // (https://developer.apple.com/library/archive/documentation/DeviceInformation/Reference/iOSDeviceCompatibility/Displays/Displays.html).
  // However, flow is based on the physical resolution. For example, 1000 pixels in flow equals
  // 500 points in UIKit for devices that has screenScale of 2. We need to scale the transformMatrix
  // down to the logical resoltion before applying it to the layer of PlatformView.
  transformMatrix.postScale(1 / screenScale, 1 / screenScale);

  // Reverse the offset of the clipView.
  // The clipView's frame includes the final translate of the final transform matrix.
  // Thus, this translate needs to be reversed so the platform view can layout at the correct
  // offset.
  //
  // Note that the transforms are not applied to the clipping paths because clipping paths happen on
  // the mask view, whose origin is always (0,0) to the flutter_view.
  transformMatrix.postTranslate(-clipView.frame.origin.x, -clipView.frame.origin.y);

  embedded_view.layer.transform = flutter::GetCATransform3DFromSkMatrix(transformMatrix);
}

// Composite the PlatformView with `view_id`.
//
// Every frame, during the paint traversal of the layer tree, this method is called for all
// the PlatformViews in `views_to_recomposite_`.
//
// Note that `views_to_recomposite_` does not represent all the views in the view hierarchy,
// if a PlatformView does not change its composition parameter from last frame, it is not
// included in the `views_to_recomposite_`.
void FlutterPlatformViewsController::CompositeWithParams(int64_t view_id,
                                                         const EmbeddedViewParams& params) {
  FML_DCHECK([[NSThread currentThread] isMainThread]);
  CGRect frame = CGRectMake(0, 0, params.sizePoints().width(), params.sizePoints().height());
  FlutterTouchInterceptingView* touchInterceptor = touch_interceptors_[view_id].get();
#if FLUTTER_RUNTIME_MODE == FLUTTER_RUNTIME_MODE_DEBUG
  FML_DCHECK(CGPointEqualToPoint([touchInterceptor embeddedView].frame.origin, CGPointZero));
  if (non_zero_origin_views_.find(view_id) == non_zero_origin_views_.end() &&
      !CGPointEqualToPoint([touchInterceptor embeddedView].frame.origin, CGPointZero)) {
    non_zero_origin_views_.insert(view_id);
    NSLog(
        @"A Embedded PlatformView's origin is not CGPointZero.\n"
         "  View id: %@\n"
         "  View info: \n %@ \n"
         "A non-zero origin might cause undefined behavior.\n"
         "See https://github.com/flutter/flutter/issues/109700 for more details.\n"
         "If you are the author of the PlatformView, please update the implementation of the "
         "PlatformView to have a (0, 0) origin.\n"
         "If you have a valid case of using a non-zero origin, "
         "please leave a comment at https://github.com/flutter/flutter/issues/109700 with details.",
        @(view_id), [touchInterceptor embeddedView]);
  }
#endif
  touchInterceptor.layer.transform = CATransform3DIdentity;
  touchInterceptor.frame = frame;
  touchInterceptor.alpha = 1;

  const MutatorsStack& mutatorStack = params.mutatorsStack();
  UIView* clippingView = root_views_[view_id].get();
  // The frame of the clipping view should be the final bounding rect.
  // Because the translate matrix in the Mutator Stack also includes the offset,
  // when we apply the transforms matrix in |ApplyMutators|, we need
  // to remember to do a reverse translate.
  const SkRect& rect = params.finalBoundingRect();
  CGFloat screenScale = [UIScreen mainScreen].scale;
  clippingView.frame = CGRectMake(rect.x() / screenScale, rect.y() / screenScale,
                                  rect.width() / screenScale, rect.height() / screenScale);
  ApplyMutators(mutatorStack, touchInterceptor, rect);
}

DlCanvas* FlutterPlatformViewsController::CompositeEmbeddedView(int64_t view_id) {
  return slices_[view_id]->canvas();
}

void FlutterPlatformViewsController::Reset() {
  FML_DCHECK([[NSThread currentThread] isMainThread]);
  // for (int64_t view_id : active_composition_order_) {
  //   UIView* sub_view = root_views_[view_id].get();
  //   [sub_view removeFromSuperview];
  // }
  root_views_.clear();
  touch_interceptors_.clear();
  views_.clear();
  composition_order_.clear();
  slices_.clear();
  current_composition_params_.clear();
  clip_count_.clear();
  views_to_recomposite_.clear();
  layer_pool_->RecycleLayers();
  visited_platform_views_.clear();
}

bool FlutterPlatformViewsController::SubmitFrame(GrDirectContext* gr_context,
                                                 const std::shared_ptr<IOSContext>& ios_context,
                                                 std::unique_ptr<SurfaceFrame> background_frame) {
  TRACE_EVENT0("flutter", "FlutterPlatformViewsController::SubmitFrame");

  if (flutter_view_ == nullptr || composition_order_.empty()) {
    return background_frame->Submit();
  }

  DlCanvas* background_canvas = background_frame->Canvas();

  // Resolve all pending GPU operations before allocating a new surface.
  // This does nothing on Impeller.
  background_canvas->Flush();

  // Clipping the background canvas before drawing the picture recorders requires
  // saving and restoring the clip context.
  DlAutoCanvasRestore save(background_canvas, /*do_save=*/true);

  // Maps a platform view id to a vector of `FlutterPlatformViewLayer`.
  LayersMap platform_view_layers;

  auto did_submit = true;
  auto num_platform_views = composition_order_.size();

  // Pending Submit Callbacks
  std::vector<SurfaceFrame::DeferredSubmit> callbacks;

  for (size_t i = 0; i < num_platform_views; i++) {
    int64_t platform_view_id = composition_order_[i];
    EmbedderViewSlice* slice = slices_[platform_view_id].get();
    slice->end_recording();

    // Check if the current picture contains overlays that intersect with the
    // current platform view or any of the previous platform views.
    for (size_t j = i + 1; j > 0; j--) {
      int64_t current_platform_view_id = composition_order_[j - 1];
      SkRect platform_view_rect =
          current_composition_params_[current_platform_view_id].finalBoundingRect();

      std::vector<SkIRect> intersection_rects = slice->region(platform_view_rect).getRects();
      const SkIRect rounded_in_platform_view_rect = platform_view_rect.roundIn();
      // Ignore intersections of single width/height on the edge of the platform view.
      // This is to address the following performance issue when interleaving adjacent
      // platform views and layers:
      // Since we `roundOut` both platform view rects and the layer rects, as long as
      // the coordinate is fractional, there will be an intersection of a single pixel width
      // (or height) after rounding out, even if they do not intersect before rounding out.
      // We have to round out both platform view rect and the layer rect.
      // Rounding in platform view rect will result in missing pixel on the intersection edge.
      // Rounding in layer rect will result in missing pixel on the edge of the layer on top
      // of the platform view.
      for (auto it = intersection_rects.begin(); it != intersection_rects.end(); /*no-op*/) {
        // If intersection_rect does not intersect with the *rounded in* platform
        // view rect, then the intersection must be a single pixel width (or height) on edge.
        if (!SkIRect::Intersects(*it, rounded_in_platform_view_rect)) {
          it = intersection_rects.erase(it);
        } else {
          ++it;
        }
      }

      auto allocation_size = intersection_rects.size();

      // For testing purposes, the overlay id is used to find the overlay view.
      // This is the index of the layer for the current platform view.
      auto overlay_id = platform_view_layers[current_platform_view_id].size();

      // If the max number of allocations per platform view is exceeded,
      // then join all the rects into a single one.
      if (allocation_size > kMaxLayerAllocations) {
        SkIRect joined_rect = SkIRect::MakeEmpty();
        for (const SkIRect& rect : intersection_rects) {
          joined_rect.join(rect);
        }
        // Replace the rects in the intersection rects list for a single rect that is
        // the union of all the rects in the list.
        intersection_rects.clear();
        intersection_rects.push_back(joined_rect);
      }
      for (SkIRect& joined_rect : intersection_rects) {
        // Get the intersection rect between the current rect
        // and the platform view rect.
        joined_rect.intersect(platform_view_rect.roundOut());
        // Clip the background canvas, so it doesn't contain any of the pixels drawn
        // on the overlay layer.
        background_canvas->ClipRect(SkRect::Make(joined_rect), DlCanvas::ClipOp::kDifference);
        // Get a new host layer.
        std::shared_ptr<FlutterPlatformViewLayer> layer = GetExistingLayer();
        if (!layer) {
          // TODO: Create all missing layers in a single task. Also figure out if this can
          // deadlock during engine shutdown.
          fml::AutoResetWaitableEvent latch;
          platform_task_runner_->PostTask([&] {
            CreateLayer(gr_context, ios_context, ((FlutterView*)flutter_view_.get()).pixelFormat);
            latch.Signal();
          });
          latch.Wait();
          layer = GetExistingLayer();
          FML_CHECK(layer);
        }

        {
          std::unique_ptr<SurfaceFrame> frame = layer->surface->AcquireFrame(frame_size_);
          // If frame is null, AcquireFrame already printed out an error message.
          if (!frame) {
            continue;
          }
          DlCanvas* overlay_canvas = frame->Canvas();
          int restore_count = overlay_canvas->GetSaveCount();
          overlay_canvas->Save();
          overlay_canvas->ClipRect(SkRect::Make(joined_rect));
          // overlay_canvas->Clear(DlColor::kTransparent());
          slice->render_into(overlay_canvas);
          overlay_canvas->RestoreToCount(restore_count);

          // This flutter view is never the last in a frame, since we always submit the
          // underlay view last.
          frame->set_submit_info(
              {.frame_boundary = false,
               .present_with_transaction = true,
               .submit_receiver = [&callbacks](const SurfaceFrame::DeferredSubmit& cb) {
                 callbacks.push_back(cb);
               }});

          layer->did_submit_last_frame = frame->Submit();
        }

        did_submit &= layer->did_submit_last_frame;
        platform_view_layers[current_platform_view_id].push_back(
            {.rect = joined_rect,
             .view_id = current_platform_view_id,
             .overlay_id = static_cast<int64_t>(overlay_id),
             .layer = layer});
        overlay_id++;
      }
    }
    slice->render_into(background_canvas);
  }

  // Manually trigger the SkAutoCanvasRestore before we submit the frame
  save.Restore();

  background_frame->set_submit_info(
      {.present_with_transaction = true,
       .submit_receiver = [&callbacks](const SurfaceFrame::DeferredSubmit& cb) {
         callbacks.push_back(cb);
       }});
  did_submit &= background_frame->Submit();

  // Mark all layers as available, so they can be used in the next frame.
  std::vector<std::shared_ptr<FlutterPlatformViewLayer>> unused_layers =
      layer_pool_->GetUnusedLayers();
  layer_pool_->RecycleLayers();

  fml::AutoResetWaitableEvent latch;

  auto task = [this, &latch, platform_view_layers = std::move(platform_view_layers),
               callbacks = std::move(callbacks),
               unused_layers = std::move(unused_layers)]() mutable {
    TRACE_EVENT0("flutter", "FlutterPlatformViewsController::SubmitFrame::CATransaction");

    [CATransaction begin];

    // Configure Flutter overlay views.
    for (const auto& [key, layers] : platform_view_layers) {
      for (const auto& layer_data : layers) {
        layer_data.layer->UpdateViewState(flutter_view_, layer_data.rect, layer_data.view_id,
                                          layer_data.overlay_id);
      }
    }

    // Dispose unused Flutter Views.
    DisposeViews();

    // Composite Platform Views.
    for (auto view_id : views_to_recomposite_) {
      CompositeWithParams(view_id, current_composition_params_[view_id]);
    }

    for (const auto& cb : callbacks) {
      cb();
    }

    // Organize the layers by their z indexes.
    auto active_composition_order = BringLayersIntoView(platform_view_layers, composition_order_);

    // If a layer was allocated in the previous frame, but it's not used in the current frame,
    // then it can be removed from the scene.
    RemoveUnusedLayers(unused_layers, composition_order_, active_composition_order);

    // If the frame is submitted with embedded platform views,
    // there should be a |[CATransaction begin]| call in this frame prior to all the drawing.
    // If that case, we need to commit the transaction.
    [CATransaction commit];
    latch.Signal();
  };

  platform_task_runner_->PostTask(task);
  latch.Wait();
  return did_submit;
}

std::vector<int64_t> FlutterPlatformViewsController::BringLayersIntoView(
    LayersMap layer_map,
    const std::vector<int64_t>& composition_order) {
  FML_DCHECK(flutter_view_);
  UIView* flutter_view = flutter_view_.get();

  std::vector<int64_t> active_composition_order;

  NSMutableArray* desired_platform_subviews = [NSMutableArray array];
  for (size_t i = 0; i < composition_order.size(); i++) {
    int64_t platform_view_id = composition_order[i];
    std::vector<LayerData> layers = layer_map[platform_view_id];
    UIView* platform_view_root = root_views_[platform_view_id].get();
    [desired_platform_subviews addObject:platform_view_root];
    for (const auto& layer_data : layers) {
      [desired_platform_subviews addObject:layer_data.layer->overlay_view_wrapper];
    }
    active_composition_order.push_back(platform_view_id);
  }

  NSSet* desired_platform_subviews_set = [NSSet setWithArray:desired_platform_subviews];
  NSArray* existing_platform_subviews = [flutter_view.subviews
      filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(id object,
                                                                        NSDictionary* bindings) {
        return [desired_platform_subviews_set containsObject:object];
      }]];
  // Manipulate view hierarchy only if needed, to address a performance issue where
  // `BringLayersIntoView` is called even when view hierarchy stays the same.
  // See: https://github.com/flutter/flutter/issues/121833
  // TODO(hellohuanlin): investigate if it is possible to skip unnecessary BringLayersIntoView.
  if (![desired_platform_subviews isEqualToArray:existing_platform_subviews]) {
    for (UIView* subview in desired_platform_subviews) {
      // `addSubview` will automatically reorder subview if it is already added.
      [flutter_view addSubview:subview];
    }
  }
  return active_composition_order;
}

std::shared_ptr<FlutterPlatformViewLayer> FlutterPlatformViewsController::GetExistingLayer() {
  return layer_pool_->GetNextLayer();
}

void FlutterPlatformViewsController::CreateLayer(GrDirectContext* gr_context,
                                                 const std::shared_ptr<IOSContext>& ios_context,
                                                 MTLPixelFormat pixel_format) {
  layer_pool_->CreateLayer(gr_context, ios_context, pixel_format);
}

void FlutterPlatformViewLayer::UpdateViewState(UIView* flutter_view,
                                               SkIRect rect,
                                               int64_t view_id,
                                               int64_t overlay_id) {
  UIView* overlay_view_wrapper = this->overlay_view_wrapper.get();
  auto screenScale = [UIScreen mainScreen].scale;
  // Set the size of the overlay view wrapper.
  // This wrapper view masks the overlay view.
  overlay_view_wrapper.frame = CGRectMake(rect.x() / screenScale, rect.y() / screenScale,
                                          rect.width() / screenScale, rect.height() / screenScale);
  // Set a unique view identifier, so the overlay_view_wrapper can be identified in XCUITests.
  overlay_view_wrapper.accessibilityIdentifier =
      [NSString stringWithFormat:@"platform_view[%lld].overlay[%lld]", view_id, overlay_id];

  UIView* overlay_view = this->overlay_view.get();
  // Set the size of the overlay view.
  // This size is equal to the device screen size.
  overlay_view.frame = [flutter_view convertRect:flutter_view.bounds toView:overlay_view_wrapper];
  // Set a unique view identifier, so the overlay_view can be identified in XCUITests.
  overlay_view.accessibilityIdentifier =
      [NSString stringWithFormat:@"platform_view[%lld].overlay_view[%lld]", view_id, overlay_id];
}

void FlutterPlatformViewsController::RemoveUnusedLayers(
    const std::vector<std::shared_ptr<FlutterPlatformViewLayer>>& unused_layers,
    const std::vector<int64_t>& composition_order,
    const std::vector<int64_t>& active_composition_order) {
  for (const std::shared_ptr<FlutterPlatformViewLayer>& layer : unused_layers) {
    [layer->overlay_view_wrapper removeFromSuperview];
  }

  std::unordered_set<int64_t> composition_order_set;
  for (int64_t view_id : composition_order) {
    composition_order_set.insert(view_id);
  }
  // Remove unused platform views.
  for (int64_t view_id : active_composition_order) {
    if (composition_order_set.find(view_id) == composition_order_set.end()) {
      UIView* platform_view_root = root_views_[view_id].get();
      [platform_view_root removeFromSuperview];
    }
  }
}

void FlutterPlatformViewsController::DisposeViews() {
  if (views_to_dispose_.empty()) {
    return;
  }

  FML_DCHECK([[NSThread currentThread] isMainThread]);

  std::unordered_set<int64_t> views_to_composite(composition_order_.begin(),
                                                 composition_order_.end());
  std::unordered_set<int64_t> views_to_delay_dispose;
  for (int64_t viewId : views_to_dispose_) {
    if (views_to_composite.count(viewId)) {
      views_to_delay_dispose.insert(viewId);
      continue;
    }
    UIView* root_view = root_views_[viewId].get();
    [root_view removeFromSuperview];
    views_.erase(viewId);
    touch_interceptors_.erase(viewId);
    root_views_.erase(viewId);
    current_composition_params_.erase(viewId);
    clip_count_.erase(viewId);
    views_to_recomposite_.erase(viewId);
  }

  views_to_dispose_ = std::move(views_to_delay_dispose);
}

void FlutterPlatformViewsController::ResetFrameState() {
  slices_.clear();
  composition_order_.clear();
  visited_platform_views_.clear();
}

}  // namespace flutter

// This recognizers delays touch events from being dispatched to the responder chain until it failed
// recognizing a gesture.
//
// We only fail this recognizer when asked to do so by the Flutter framework (which does so by
// invoking an acceptGesture method on the platform_views channel). And this is how we allow the
// Flutter framework to delay or prevent the embedded view from getting a touch sequence.
@interface DelayingGestureRecognizer : UIGestureRecognizer <UIGestureRecognizerDelegate>

// Indicates that if the `DelayingGestureRecognizer`'s state should be set to
// `UIGestureRecognizerStateEnded` during next `touchesEnded` call.
@property(nonatomic) BOOL shouldEndInNextTouchesEnded;

// Indicates that the `DelayingGestureRecognizer`'s `touchesEnded` has been invoked without
// setting the state to `UIGestureRecognizerStateEnded`.
@property(nonatomic) BOOL touchedEndedWithoutBlocking;

@property(nonatomic, readonly) UIGestureRecognizer* forwardingRecognizer;

- (instancetype)initWithTarget:(id)target
                        action:(SEL)action
          forwardingRecognizer:(UIGestureRecognizer*)forwardingRecognizer;
@end

// While the DelayingGestureRecognizer is preventing touches from hitting the responder chain
// the touch events are not arriving to the FlutterView (and thus not arriving to the Flutter
// framework). We use this gesture recognizer to dispatch the events directly to the FlutterView
// while during this phase.
//
// If the Flutter framework decides to dispatch events to the embedded view, we fail the
// DelayingGestureRecognizer which sends the events up the responder chain. But since the events
// are handled by the embedded view they are not delivered to the Flutter framework in this phase
// as well. So during this phase as well the ForwardingGestureRecognizer dispatched the events
// directly to the FlutterView.
@interface ForwardingGestureRecognizer : UIGestureRecognizer <UIGestureRecognizerDelegate>
- (instancetype)initWithTarget:(id)target
       platformViewsController:
           (fml::WeakPtr<flutter::FlutterPlatformViewsController>)platformViewsController;
@end

@interface FlutterTouchInterceptingView ()
@property(nonatomic, weak, readonly) UIView* embeddedView;
@property(nonatomic, readonly) DelayingGestureRecognizer* delayingRecognizer;
@property(nonatomic, readonly) FlutterPlatformViewGestureRecognizersBlockingPolicy blockingPolicy;
@end

@implementation FlutterTouchInterceptingView
- (instancetype)initWithEmbeddedView:(UIView*)embeddedView
             platformViewsController:
                 (fml::WeakPtr<flutter::FlutterPlatformViewsController>)platformViewsController
    gestureRecognizersBlockingPolicy:
        (FlutterPlatformViewGestureRecognizersBlockingPolicy)blockingPolicy {
  self = [super initWithFrame:embeddedView.frame];
  if (self) {
    self.multipleTouchEnabled = YES;
    _embeddedView = embeddedView;
    embeddedView.autoresizingMask =
        (UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight);

    [self addSubview:embeddedView];

    ForwardingGestureRecognizer* forwardingRecognizer =
        [[ForwardingGestureRecognizer alloc] initWithTarget:self
                                    platformViewsController:platformViewsController];

    _delayingRecognizer = [[DelayingGestureRecognizer alloc] initWithTarget:self
                                                                     action:nil
                                                       forwardingRecognizer:forwardingRecognizer];
    _blockingPolicy = blockingPolicy;

    [self addGestureRecognizer:_delayingRecognizer];
    [self addGestureRecognizer:forwardingRecognizer];
  }
  return self;
}

- (void)releaseGesture {
  self.delayingRecognizer.state = UIGestureRecognizerStateFailed;
}

- (void)blockGesture {
  switch (_blockingPolicy) {
    case FlutterPlatformViewGestureRecognizersBlockingPolicyEager:
      // We block all other gesture recognizers immediately in this policy.
      self.delayingRecognizer.state = UIGestureRecognizerStateEnded;
      break;
    case FlutterPlatformViewGestureRecognizersBlockingPolicyWaitUntilTouchesEnded:
      if (self.delayingRecognizer.touchedEndedWithoutBlocking) {
        // If touchesEnded of the `DelayingGesureRecognizer` has been already invoked,
        // we want to set the state of the `DelayingGesureRecognizer` to
        // `UIGestureRecognizerStateEnded` as soon as possible.
        self.delayingRecognizer.state = UIGestureRecognizerStateEnded;
      } else {
        // If touchesEnded of the `DelayingGesureRecognizer` has not been invoked,
        // We will set a flag to notify the `DelayingGesureRecognizer` to set the state to
        // `UIGestureRecognizerStateEnded` when touchesEnded is called.
        self.delayingRecognizer.shouldEndInNextTouchesEnded = YES;
      }
      break;
    default:
      break;
  }
}

// We want the intercepting view to consume the touches and not pass the touches up to the parent
// view. Make the touch event method not call super will not pass the touches up to the parent view.
// Hence we overide the touch event methods and do nothing.
- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
}

- (void)touchesEnded:(NSSet*)touches withEvent:(UIEvent*)event {
}

- (id)accessibilityContainer {
  return self.flutterAccessibilityContainer;
}

@end

@implementation DelayingGestureRecognizer

- (instancetype)initWithTarget:(id)target
                        action:(SEL)action
          forwardingRecognizer:(UIGestureRecognizer*)forwardingRecognizer {
  self = [super initWithTarget:target action:action];
  if (self) {
    self.delaysTouchesBegan = YES;
    self.delaysTouchesEnded = YES;
    self.delegate = self;
    _shouldEndInNextTouchesEnded = NO;
    _touchedEndedWithoutBlocking = NO;
    _forwardingRecognizer = forwardingRecognizer;
  }
  return self;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldBeRequiredToFailByGestureRecognizer:(UIGestureRecognizer*)otherGestureRecognizer {
  // The forwarding gesture recognizer should always get all touch events, so it should not be
  // required to fail by any other gesture recognizer.
  return otherGestureRecognizer != _forwardingRecognizer && otherGestureRecognizer != self;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldRequireFailureOfGestureRecognizer:(UIGestureRecognizer*)otherGestureRecognizer {
  return otherGestureRecognizer == self;
}

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  self.touchedEndedWithoutBlocking = NO;
  [super touchesBegan:touches withEvent:event];
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  if (self.shouldEndInNextTouchesEnded) {
    self.state = UIGestureRecognizerStateEnded;
    self.shouldEndInNextTouchesEnded = NO;
  } else {
    self.touchedEndedWithoutBlocking = YES;
  }
  [super touchesEnded:touches withEvent:event];
}

- (void)touchesCancelled:(NSSet*)touches withEvent:(UIEvent*)event {
  self.state = UIGestureRecognizerStateFailed;
}
@end

@implementation ForwardingGestureRecognizer {
  // Weak reference to FlutterPlatformViewsController. The FlutterPlatformViewsController has
  // a reference to the FlutterViewController, where we can dispatch pointer events to.
  //
  // The lifecycle of FlutterPlatformViewsController is bind to FlutterEngine, which should always
  // outlives the FlutterViewController. And ForwardingGestureRecognizer is owned by a subview of
  // FlutterView, so the ForwardingGestureRecognizer never out lives FlutterViewController.
  // Therefore, `_platformViewsController` should never be nullptr.
  fml::WeakPtr<flutter::FlutterPlatformViewsController> _platformViewsController;
  // Counting the pointers that has started in one touch sequence.
  NSInteger _currentTouchPointersCount;
  // We can't dispatch events to the framework without this back pointer.
  // This gesture recognizer retains the `FlutterViewController` until the
  // end of a gesture sequence, that is all the touches in touchesBegan are concluded
  // with |touchesCancelled| or |touchesEnded|.
  fml::scoped_nsobject<UIViewController<FlutterViewResponder>> _flutterViewController;
}

- (instancetype)initWithTarget:(id)target
       platformViewsController:
           (fml::WeakPtr<flutter::FlutterPlatformViewsController>)platformViewsController {
  self = [super initWithTarget:target action:nil];
  if (self) {
    self.delegate = self;
    FML_DCHECK(platformViewsController.get() != nullptr);
    _platformViewsController = std::move(platformViewsController);
    _currentTouchPointersCount = 0;
  }
  return self;
}

- (void)touchesBegan:(NSSet*)touches withEvent:(UIEvent*)event {
  FML_DCHECK(_currentTouchPointersCount >= 0);
  if (_currentTouchPointersCount == 0) {
    // At the start of each gesture sequence, we reset the `_flutterViewController`,
    // so that all the touch events in the same sequence are forwarded to the same
    // `_flutterViewController`.
    _flutterViewController.reset(_platformViewsController->getFlutterViewController());
  }
  [_flutterViewController.get() touchesBegan:touches withEvent:event];
  _currentTouchPointersCount += touches.count;
}

- (void)touchesMoved:(NSSet*)touches withEvent:(UIEvent*)event {
  [_flutterViewController.get() touchesMoved:touches withEvent:event];
}

- (void)touchesEnded:(NSSet*)touches withEvent:(UIEvent*)event {
  [_flutterViewController.get() touchesEnded:touches withEvent:event];
  _currentTouchPointersCount -= touches.count;
  // Touches in one touch sequence are sent to the touchesEnded method separately if different
  // fingers stop touching the screen at different time. So one touchesEnded method triggering does
  // not necessarially mean the touch sequence has ended. We Only set the state to
  // UIGestureRecognizerStateFailed when all the touches in the current touch sequence is ended.
  if (_currentTouchPointersCount == 0) {
    self.state = UIGestureRecognizerStateFailed;
    _flutterViewController.reset(nil);
  }
}

- (void)touchesCancelled:(NSSet*)touches withEvent:(UIEvent*)event {
  // In the event of platform view is removed, iOS generates a "stationary" change type instead of
  // "cancelled" change type.
  // Flutter needs all the cancelled touches to be "cancelled" change types in order to correctly
  // handle gesture sequence.
  // We always override the change type to "cancelled".
  [_flutterViewController.get() forceTouchesCancelled:touches];
  _currentTouchPointersCount -= touches.count;
  if (_currentTouchPointersCount == 0) {
    self.state = UIGestureRecognizerStateFailed;
    _flutterViewController.reset(nil);
  }
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:
        (UIGestureRecognizer*)otherGestureRecognizer {
  return YES;
}
@end
