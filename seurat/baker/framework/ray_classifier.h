/*
Copyright 2017 Google Inc. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS-IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#ifndef VR_SEURAT_BAKER_FRAMEWORK_RAY_CLASSIFIER_H_
#define VR_SEURAT_BAKER_FRAMEWORK_RAY_CLASSIFIER_H_

#include <vector>

#include "ion/math/matrix.h"
#include "ion/math/vector.h"
#include "absl/types/span.h"
#include "seurat/baker/framework/frame.h"
#include "seurat/baker/framework/ray_bundle.h"
#include "seurat/baker/framework/texture_sizer.h"
#include "seurat/base/array2d.h"
#include "seurat/base/array2d_view.h"
#include "seurat/geometry/raytracer.h"

namespace seurat {
namespace baker {

// Transforms RayBundles into 'solid' samples and 'freespace' rays corresponding
// to Frames.
class RayClassifier {
 public:
  struct ClassifiedRays {
    // The (ray index, intersection index) of samples in a RayBundle
    // corresponding to solid points on a Frame.
    std::vector<RayBundle::RayIntersectionIndex> solid_samples;

    // The index of rays in a RayBundle corresponding to freespace samples on a
    // Frame.
    std::vector<int> freespace_rays;
  };

  virtual ~RayClassifier() = default;

  // Initializes (or resets) the classifier to trace rays through the given set
  // of |frames|.
  //
  // Note:  |frames| must persist through all subsequent calls to
  // ClassifyRays().
  virtual void Init(absl::Span<const Frame> frames) = 0;

  // Returns a vector of ClassifiedRays mapping frames to solid & freespace
  // constraints from the given |RayBundle|.
  //
  // In other words, the returned vector runs parallel to the |frames| from
  // which this RayClassifier was constructed.
  virtual std::vector<ClassifiedRays> ClassifyRays(
      const RayBundle& bundle) const = 0;
};

// A RayClassifier which projects samples towards the origin to find solid
// samples.
//
// A sample is classified as 'solid' if its intersection point is closest to a
// particular frame (in which case, it is a "primary sample"), or within
// |secondary_frame_threshold| of that frame (in which case it is a "secondary
// sample").
//
// A ray is classified as 'freespace' if it does not have any solid samples on a
// frame and the frame will render before a primary sample on that ray.
//
// Secondary assignments are useful/necessary to inpaint regions between
// adjacent frames by duplicating texture data.
//
// A note on terminology:
//  * 'solid' constraints are point samples corresponding to intersections of a
//    RayBundle's ray with the underlying scene geometry.
//  * 'freespace' constraints are actual rays from the original RayBundle.
//
// For example, if the RayBundle is backed by a layered depth image, with pixels
// containing multiple samples at different depths, then solid constraints
// correspond to samples of the LDI and freespace constraints are generated by
// pixels of the LDI.
class ProjectingRayClassifier : public RayClassifier {
 public:
  // As described above, freespace samples are determined based on draw-order of
  // geometry, relative to each ray.
  //
  // The relative draw order depends on how the geometry will be rendered at
  // runtime.
  //
  // This enum specifies how this is expected to happen.
  enum class RenderingMode {
    // Specifies that geometry will be rendered with a conventional z-buffer.
    //
    // As a result, draw order is based on actual geometric depth.
    kZBuffer,
    // Specifies that geometry will be rendered according to its
    // Frame.draw_order.  The result is that potential alpha-sorting artifacts
    // will be mitigated by assigning freespace samples appropriately.
    kDrawOrder,
  };

  ProjectingRayClassifier(int thread_count, RenderingMode rendering_mode,
                          float secondary_frame_threshold)
      : thread_count_(thread_count),
        rendering_mode_(rendering_mode),
        secondary_frame_threshold_(secondary_frame_threshold) {}
  ~ProjectingRayClassifier() override = default;

  // RayClassifier implementation.
  void Init(absl::Span<const Frame> frames) override;
  std::vector<ClassifiedRays> ClassifyRays(
      const RayBundle& bundle) const override;

 private:
  // Returns a map from frame index to the set of samples which correspond to
  // that frame as "solid samples".
  //
  // Samples are assigned to frames by intersecting rays from the *origin* to
  // the sample point through the frame's quad:
  //
  //             |
  //       sample|
  //    +-ray-->*|
  // origin      |
  //            Frame
  //
  // Note that the origin (headbox center) is used to ensure that solid samples
  // are "warped" in a consistent manner.  In other words, samples of the same
  // original geometry which were seen from two different cameras will intersect
  // at the *same point* on the frame.
  //
  // This is in contrast to traditional lightfield planar depth correction which
  // would result in ghosting artifacts.
  //
  // An intersection point is assigned to a frame as a "solid sample" if either:
  //
  //  1. The frame is closest to the sample, among all frames intersecting the
  //  origin->sample ray.  These are "primary" frames.
  //  2. Or, the sample is within secondary_frame_threshold/||sample - origin||
  //  distance from the frame.  These are "secondary" frames.
  //
  // The returned |solid_samples_per_frame| are sorted in ascending order.
  void CollectSolidSamples(
      const RayBundle& bundle,
      absl::Span<std::vector<RayBundle::RayIntersectionIndex>>
          solid_samples_per_frame,
      absl::Span<std::vector<int>> primary_frames_per_ray) const;

  // Returns a map from frame index to the set of freespace rays which
  // correspond to that frame.
  //
  // Rays are assigned to frames by intersecting rays from the *view camera*
  // to the sample point through the frame's quad:
  //
  // Note that this is different than for solid samples.
  //
  // camera+--
  //          \
  //       ray --
  //             \
  //              --
  //                \
  // *           |   --> sample
  // origin      |
  //            Frame
  //
  // A ray is assigned to a frame as a freespace ray if:
  //  1. The frame does not have any solid samples assigned from that ray.
  //  2. The frame will render before at least one of the frames
  //     associated with that ray's samples.
  void CollectFreespaceRays(
      const RayBundle& bundle,
      absl::Span<const std::vector<RayBundle::RayIntersectionIndex>>
          solid_samples_per_frame,
      absl::Span<const std::vector<int>> primary_frames_per_ray,
      absl::Span<std::vector<int>> freespace_rays_per_frame) const;

  // The maximum number of threads to use.
  const int thread_count_;

  // Used to simulate rendering in order to carve silhouettes (via freespace
  // samples) where necessary.
  const RenderingMode rendering_mode_;

  // The threshold used to decide whether to assign a sample to a 'secondary"
  // frame.
  //
  // A ray sample with endpoint, S, is a assigned to a secondary frame, F, if
  //   [distance from S to F] / ||S - 0|| < secondary_frame_threshold
  //
  // Larger values result in samples being duplicated to more frames, helping to
  // inpaint seams between partitions.
  //
  // See CollectFreespaceSamples() for more details.
  const float secondary_frame_threshold_;

  // All frames being considered.
  absl::Span<const Frame> frames_;

  // Traces rays through triangle soup consisting of two triangles for each
  // Frame.
  //
  // The i'th frame corresponds to the triangles with indices 2i and 2i + 1.
  std::unique_ptr<geometry::Raytracer> raytracer_;
};

// Wraps another RayClassifier to dilate Frames to collect neighboring samples
// from *outside* the Frame's quad which, when rasterized with a filter of the
// specified size, could influence the texture values *within* the frame.
class DilatingRayClassifier : public RayClassifier {
 public:
  DilatingRayClassifier(float texture_filter_size,
                        std::unique_ptr<TextureSizer> texture_sizer,
                        std::unique_ptr<RayClassifier> delegate)
      : texture_filter_radius_(texture_filter_size),
        texture_sizer_(std::move(texture_sizer)),
        delegate_(std::move(delegate)) {}
  ~DilatingRayClassifier() override = default;

  // RayClassifier implementation.
  void Init(absl::Span<const Frame> frames) override;
  std::vector<ClassifiedRays> ClassifyRays(
      const RayBundle& bundle) const override {
    return delegate_->ClassifyRays(bundle);
  }

 private:
  // The radius of the filter-kernel used to filter the resulting textures.
  //
  // This value is relative to the texture resolution.  A filter with a 3x3
  // pixel footprint should have a radius of 1.5.
  const float texture_filter_radius_;

  // Determines the texture resolution for each frame to know how much to
  // enlarge each Frame.
  const std::unique_ptr<TextureSizer> texture_sizer_;

  // The RayClassifier to wrap.
  const std::unique_ptr<RayClassifier> delegate_;

  // Temporary storage for the dilated frames.
  std::vector<Frame> dilated_frames_;
};

}  // namespace baker
}  // namespace seurat

#endif  // VR_SEURAT_BAKER_FRAMEWORK_RAY_CLASSIFIER_H_
