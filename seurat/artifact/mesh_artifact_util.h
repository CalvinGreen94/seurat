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

#ifndef VR_SEURAT_ARTIFACT_MESH_ARTIFACT_UTIL_H_
#define VR_SEURAT_ARTIFACT_MESH_ARTIFACT_UTIL_H_

#include <memory>

#include "seurat/artifact/artifact.h"
#include "seurat/artifact/artifact_processor.h"
#include "seurat/base/status.h"

namespace seurat {
namespace artifact {

// Flips mesh faces to point toward the origin, thus the eye.
//
// Front faces have counter-clockwise orientation when viewed from the origin,
// to match PlaneFromTriangle's definition of the positive halfspace of the
// plane.
//
// The Artifact.mesh is replaced.  All other elements are unmodified.
class FlipMeshFacesTransform : public ArtifactProcessor {
 public:
  ~FlipMeshFacesTransform() override = default;

  // ArtifactProcessor implementation
  base::Status Process(Artifact* artifact) const override;
};

}  // namespace artifact
}  // namespace seurat

#endif  // VR_SEURAT_ARTIFACT_MESH_ARTIFACT_UTIL_H_
