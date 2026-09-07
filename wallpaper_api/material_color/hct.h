/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MATERIAL_COLOR_HCT_H_
#define MATERIAL_COLOR_HCT_H_

#include <cstdint>

namespace material_color {

// HCT (Hue-Chroma-Tone) color space.
//
// A perceptually accurate color measurement system built on CAM16 hue/chroma
// and L* from L*a*b*. Provides a link between color appearance, contrast
// ratios, and accessibility.
//
// Reference: https://material.io/blog/science-of-color-design
class Hct final {
 public:
  Hct(const Hct&) = default;
  Hct& operator=(const Hct&) = default;

  // Creates an Hct from an ARGB integer.
  static Hct FromInt(uint32_t argb);

  // Creates an Hct from hue (degrees), chroma, and tone (L*).
  // Chroma may be reduced if the requested value is out of gamut.
  static Hct FromHct(double hue, double chroma, double tone);

  // Converts back to an ARGB integer.
  uint32_t ToInt() const;

  // Hue in degrees, [0, 360).
  double hue() const { return hue_; }

  // Chroma. Non-negative, varies by hue/tone.
  double chroma() const { return chroma_; }

  // Tone (L*). [0, 100].
  double tone() const { return tone_; }

 private:
  Hct(uint32_t argb, double hue, double chroma, double tone);

  uint32_t argb_;
  double hue_;
  double chroma_;
  double tone_;
};

}  // namespace material_color

#endif  // MATERIAL_COLOR_HCT_H_
