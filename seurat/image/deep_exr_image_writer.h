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

#ifndef VR_SEURAT_IMAGE_DEEP_EXR_IMAGE_WRITER_H_
#define VR_SEURAT_IMAGE_DEEP_EXR_IMAGE_WRITER_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "seurat/base/status.h"
#include "seurat/image/image_writer.h"

namespace seurat {
namespace image {

// This class implements deep EXR writing.
class DeepExrImageWriter : public ImageWriter {
 public:
  // Create a DeepExrImageWriter that writes a deep EXR of |image_width| x
  // |image_height| dimensions.  |sample_counts| holds the number of samples per
  // pixel in the image, in row-major order.
  static base::Status Create(int image_width, int image_height,
                             std::vector<int> sample_counts,
                             std::unique_ptr<DeepExrImageWriter>* writer);
  ~DeepExrImageWriter() override;

  // ImageWriter implementation.
  void AddChannel(const Channel& channel) override;
  base::Status Write(std::string* exr_contents) const override;

 private:
  DeepExrImageWriter();

  // Initializes this DeepExrImageWriter with image parameters.
  base::Status Init(int image_width, int image_height,
                    std::vector<int> sample_counts);

  // The size of the image.
  int image_width_;
  int image_height_;

  // The per-pixel sample counts of the image, in row-major order.
  std::vector<int> sample_counts_;

  // The channels to be written to the image.
  std::vector<Channel> channels_;
};

}  // namespace image
}  // namespace seurat

#endif  // VR_SEURAT_IMAGE_DEEP_EXR_IMAGE_WRITER_H_
