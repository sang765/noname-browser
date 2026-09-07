/*
 * Copyright 2024 Google LLC
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

#ifndef MATERIAL_COLOR_DYNAMIC_SCHEME_H_
#define MATERIAL_COLOR_DYNAMIC_SCHEME_H_

#include <cstdint>

#include "wallpaper_api/material_color/hct.h"

namespace material_color {

// Theme variant styles for Dynamic Color.
enum class Variant {
  kMonochrome = 0,
  kNeutral,
  kTonalSpot,
  kVibrant,
  kExpressive,
  kFidelity,
  kContent,
  kRainbow,
  kFruitSalad,
};

// A convenience class for retrieving colors that are constant in hue and
// chroma, but vary in tone.
class TonalPalette final {
 public:
  TonalPalette(const TonalPalette&) = default;
  TonalPalette& operator=(const TonalPalette&) = default;

  // Creates a TonalPalette from a hue (degrees) and chroma.
  static TonalPalette FromHueAndChroma(double hue, double chroma);

  // Creates a TonalPalette from an existing HCT color.
  static TonalPalette FromHct(const Hct& hct);

  // Returns an ARGB color at the given tone (0-100).
  uint32_t Tone(double tone) const;

  // Returns an Hct color at the given tone (0-100).
  Hct GetHct(double tone) const;

  double hue() const { return hue_; }
  double chroma() const { return chroma_; }
  const Hct& key_color() const { return key_color_; }

 private:
  TonalPalette(double hue, double chroma, const Hct& key_color);

  double hue_;
  double chroma_;
  Hct key_color_;
};

// Provides important settings for creating colors dynamically, and 6 color
// palettes. Requires:
// 1. A color (source color).
// 2. A theme (Variant).
// 3. Whether it is dark mode.
// 4. Contrast level (-1 to 1).
//
// Reference: https://github.com/material-foundation/material-color-utilities
class DynamicScheme final {
 public:
  DynamicScheme(const DynamicScheme&) = default;
  DynamicScheme& operator=(const DynamicScheme&) = default;

  // Constructs a DynamicScheme from a source color, variant, and UI state.
  DynamicScheme(const Hct& source_color_hct,
                Variant variant,
                bool is_dark,
                double contrast_level);

  // Source color.
  Hct source_color_hct() const { return source_color_hct_; }
  uint32_t source_color_argb() const { return source_color_argb_; }

  // Theme settings.
  Variant variant() const { return variant_; }
  bool is_dark() const { return is_dark_; }
  double contrast_level() const { return contrast_level_; }

  // Palettes.
  const TonalPalette& primary_palette() const { return primary_palette_; }
  const TonalPalette& secondary_palette() const { return secondary_palette_; }
  const TonalPalette& tertiary_palette() const { return tertiary_palette_; }
  const TonalPalette& neutral_palette() const { return neutral_palette_; }
  const TonalPalette& neutral_variant_palette() const {
    return neutral_variant_palette_;
  }

  // Primary colors.
  uint32_t primary() const;
  uint32_t on_primary() const;
  uint32_t primary_container() const;
  uint32_t on_primary_container() const;
  uint32_t primary_fixed() const;
  uint32_t primary_fixed_dim() const;
  uint32_t on_primary_fixed() const;
  uint32_t on_primary_fixed_variant() const;
  uint32_t inverse_primary() const;

  // Secondary colors.
  uint32_t secondary() const;
  uint32_t on_secondary() const;
  uint32_t secondary_container() const;
  uint32_t on_secondary_container() const;
  uint32_t secondary_fixed() const;
  uint32_t secondary_fixed_dim() const;
  uint32_t on_secondary_fixed() const;
  uint32_t on_secondary_fixed_variant() const;

  // Tertiary colors.
  uint32_t tertiary() const;
  uint32_t on_tertiary() const;
  uint32_t tertiary_container() const;
  uint32_t on_tertiary_container() const;
  uint32_t tertiary_fixed() const;
  uint32_t tertiary_fixed_dim() const;
  uint32_t on_tertiary_fixed() const;
  uint32_t on_tertiary_fixed_variant() const;

  // Error colors.
  uint32_t error() const;
  uint32_t on_error() const;
  uint32_t error_container() const;
  uint32_t on_error_container() const;

  // Surface colors.
  uint32_t surface() const;
  uint32_t surface_dim() const;
  uint32_t surface_bright() const;
  uint32_t surface_container_lowest() const;
  uint32_t surface_container_low() const;
  uint32_t surface_container() const;
  uint32_t surface_container_high() const;
  uint32_t surface_container_highest() const;
  uint32_t on_surface() const;
  uint32_t surface_variant() const;
  uint32_t on_surface_variant() const;
  uint32_t inverse_surface() const;
  uint32_t inverse_on_surface() const;
  uint32_t surface_tint() const;

  // Background colors.
  uint32_t background() const;
  uint32_t on_background() const;

  // Outline colors.
  uint32_t outline() const;
  uint32_t outline_variant() const;

  // Utility colors.
  uint32_t shadow() const;
  uint32_t scrim() const;

 private:
  // Resolves a foreground tone that achieves the desired contrast ratio
  // against a background tone.
  static double ForegroundTone(double bg_tone, double ratio);

  // The WCAG contrast ratio of two tones.
  static double RatioOfTones(double tone1, double tone2);

  // Returns the "highest" surface color (surfaceBright in dark, surfaceDim
  // in light) for contrast calculations.
  uint32_t HighestSurface() const;

  Hct source_color_hct_;
  uint32_t source_color_argb_;
  Variant variant_;
  bool is_dark_;
  double contrast_level_;

  TonalPalette primary_palette_;
  TonalPalette secondary_palette_;
  TonalPalette tertiary_palette_;
  TonalPalette neutral_palette_;
  TonalPalette neutral_variant_palette_;
  TonalPalette error_palette_;
};

}  // namespace material_color

#endif  // MATERIAL_COLOR_DYNAMIC_SCHEME_H_
