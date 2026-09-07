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

#include "wallpaper_api/material_color/dynamic_scheme.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace material_color {
namespace {

// ---------------------------------------------------------------------------
// Math utilities (from @material/material-color-utilities)
// ---------------------------------------------------------------------------

double SanitizeDegreesDouble(double degrees) {
  degrees = std::fmod(degrees, 360.0);
  if (degrees < 0.0) {
    degrees += 360.0;
  }
  return degrees;
}

double Lerp(double start, double stop, double amount) {
  return (1.0 - amount) * start + amount * stop;
}

double ClampDouble(double min, double max, double input) {
  if (input < min) return min;
  if (input > max) return max;
  return input;
}

// ---------------------------------------------------------------------------
// ContrastCurve (from @material/material-color-utilities)
// ---------------------------------------------------------------------------

double ContrastCurveGet(double low,
                        double normal,
                        double medium,
                        double high,
                        double contrast_level) {
  if (contrast_level <= -1.0) return low;
  if (contrast_level < 0.0)
    return Lerp(low, normal, (contrast_level + 1.0) / 1.0);
  if (contrast_level < 0.5)
    return Lerp(normal, medium, contrast_level / 0.5);
  if (contrast_level < 1.0)
    return Lerp(medium, high, (contrast_level - 0.5) / 0.5);
  return high;
}

// ---------------------------------------------------------------------------
// WCAG contrast utilities (from @material/material-color-utilities)
// ---------------------------------------------------------------------------

double RatioOfTones(double tone1, double tone2) {
  tone1 = ClampDouble(0.0, 100.0, tone1);
  tone2 = ClampDouble(0.0, 100.0, tone2);
  double lighter = std::max(tone1, tone2);
  double darker = std::min(tone1, tone2);
  return (lighter + 5.0) / (darker + 5.0);
}

double ForegroundTone(double bg_tone, double ratio) {
  double lighter = ratio * (bg_tone + 5.0) - 5.0;
  double darker = (bg_tone + 5.0) / ratio - 5.0;
  lighter = ClampDouble(0.0, 100.0, lighter);
  darker = ClampDouble(0.0, 100.0, darker);
  double lighter_ratio = RatioOfTones(lighter, bg_tone);
  double darker_ratio = RatioOfTones(bg_tone, darker);
  bool prefer_lighter = std::round(bg_tone) < 60.0;
  if (prefer_lighter) {
    bool negligible =
        std::abs(lighter_ratio - darker_ratio) < 0.1 &&
        lighter_ratio < ratio && darker_ratio < ratio;
    return (lighter_ratio >= ratio || lighter_ratio >= darker_ratio ||
            negligible)
               ? lighter
               : darker;
  }
  return (darker_ratio >= ratio || darker_ratio >= lighter_ratio)
             ? darker
             : lighter;
}

// Resolves a foreground tone, adjusting only if contrast is insufficient.
double ResolveForegroundTone(double bg_lstar,
                             double initial_fg_lstar,
                             double desired_ratio) {
  if (RatioOfTones(bg_lstar, initial_fg_lstar) >= desired_ratio)
    return initial_fg_lstar;
  return ForegroundTone(bg_lstar, desired_ratio);
}

// ---------------------------------------------------------------------------
// Piecewise hue functions (from @material/material-color-utilities)
// ---------------------------------------------------------------------------

double GetPiecewiseHue(double source_hue,
                       const double breakpoints[],
                       const double hues[],
                       size_t count) {
  for (size_t i = 0; i + 1 < count; ++i) {
    if (source_hue >= breakpoints[i] && source_hue < breakpoints[i + 1]) {
      return SanitizeDegreesDouble(hues[i]);
    }
  }
  return source_hue;
}

double GetRotatedHue(double source_hue,
                     const double breakpoints[],
                     const double rotations[],
                     size_t count) {
  double rotation =
      GetPiecewiseHue(source_hue, breakpoints, rotations, count);
  bool matched = false;
  for (size_t i = 0; i + 1 < count; ++i) {
    if (source_hue >= breakpoints[i] && source_hue < breakpoints[i + 1]) {
      matched = true;
      break;
    }
  }
  if (!matched)
    rotation = 0.0;
  return SanitizeDegreesDouble(source_hue + rotation);
}

// ---------------------------------------------------------------------------
// Variant helpers
// ---------------------------------------------------------------------------

bool IsFidelity(Variant v) {
  return v == Variant::kFidelity || v == Variant::kContent;
}

bool IsMonochrome(Variant v) {
  return v == Variant::kMonochrome;
}

double FindDesiredChromaByTone(double hue,
                               double chroma,
                               double tone,
                               bool by_decreasing) {
  double answer = tone;
  Hct closest = Hct::FromHct(hue, chroma, tone);
  if (closest.chroma() < chroma) {
    double peak = closest.chroma();
    while (closest.chroma() < chroma) {
      answer += by_decreasing ? -1.0 : 1.0;
      Hct potential = Hct::FromHct(hue, chroma, answer);
      if (peak > potential.chroma())
        break;
      if (std::abs(potential.chroma() - chroma) < 0.4)
        break;
      double p_delta = std::abs(potential.chroma() - chroma);
      double c_delta = std::abs(closest.chroma() - chroma);
      if (p_delta < c_delta)
        closest = potential;
      peak = std::max(peak, potential.chroma());
    }
  }
  return answer;
}

// ---------------------------------------------------------------------------
// TonalPalette key color finder (from @material/material-color-utilities)
// ---------------------------------------------------------------------------

Hct FindKeyColor(double hue, double chroma) {
  const double kMaxChroma = 200.0;
  const double kPivot = 50.0;
  const double kStep = 1.0;
  const double kEps = 0.01;

  auto max_ch = [hue, kMaxChroma](double t) {
    return Hct::FromHct(hue, kMaxChroma, t).chroma();
  };

  double lo = 0.0;
  double hi = 100.0;
  while (lo < hi) {
    double mid = std::floor((lo + hi) / 2.0);
    bool ascending = max_ch(mid) < max_ch(mid + kStep);
    bool sufficient = max_ch(mid) >= chroma - kEps;
    if (sufficient) {
      if (std::abs(lo - kPivot) < std::abs(hi - kPivot)) {
        hi = mid;
      } else {
        if (lo == mid)
          return Hct::FromHct(hue, chroma, lo);
        lo = mid;
      }
    } else {
      if (ascending)
        lo = mid + kStep;
      else
        hi = mid;
    }
  }
  return Hct::FromHct(hue, chroma, lo);
}

}  // namespace

// ---------------------------------------------------------------------------
// TonalPalette
// ---------------------------------------------------------------------------

TonalPalette::TonalPalette(double hue, double chroma, const Hct& key_color)
    : hue_(hue), chroma_(chroma), key_color_(key_color) {}

TonalPalette TonalPalette::FromHueAndChroma(double hue, double chroma) {
  return TonalPalette(hue, chroma, FindKeyColor(hue, chroma));
}

TonalPalette TonalPalette::FromHct(const Hct& hct) {
  return TonalPalette(hct.hue(), hct.chroma(), hct);
}

uint32_t TonalPalette::Tone(double tone) const {
  return GetHct(tone).ToInt();
}

Hct TonalPalette::GetHct(double tone) const {
  return Hct::FromHct(hue_, chroma_, tone);
}

// ---------------------------------------------------------------------------
// DynamicScheme
// ---------------------------------------------------------------------------

DynamicScheme::DynamicScheme(const Hct& source_color_hct,
                             Variant variant,
                             bool is_dark,
                             double contrast_level)
    : source_color_hct_(source_color_hct),
      source_color_argb_(source_color_hct.ToInt()),
      variant_(variant),
      is_dark_(is_dark),
      contrast_level_(contrast_level) {
  double hue = source_color_hct.hue();
  double chroma = source_color_hct.chroma();

  switch (variant) {
    case Variant::kMonochrome:
      primary_palette_ = TonalPalette::FromHueAndChroma(hue, 0.0);
      secondary_palette_ = TonalPalette::FromHueAndChroma(hue, 0.0);
      tertiary_palette_ = TonalPalette::FromHueAndChroma(hue + 60.0, 0.0);
      neutral_palette_ = TonalPalette::FromHueAndChroma(hue, 0.0);
      neutral_variant_palette_ = TonalPalette::FromHueAndChroma(hue, 0.0);
      break;

    case Variant::kNeutral:
      primary_palette_ = TonalPalette::FromHueAndChroma(hue, 12.0);
      secondary_palette_ = TonalPalette::FromHueAndChroma(hue, 8.0);
      tertiary_palette_ = TonalPalette::FromHueAndChroma(hue + 60.0, 16.0);
      neutral_palette_ = TonalPalette::FromHueAndChroma(hue, 2.0);
      neutral_variant_palette_ = TonalPalette::FromHueAndChroma(hue, 2.0);
      break;

    case Variant::kTonalSpot:
      primary_palette_ = TonalPalette::FromHueAndChroma(hue, 36.0);
      secondary_palette_ = TonalPalette::FromHueAndChroma(hue, 16.0);
      tertiary_palette_ = TonalPalette::FromHueAndChroma(hue + 60.0, 24.0);
      neutral_palette_ = TonalPalette::FromHueAndChroma(hue, 6.0);
      neutral_variant_palette_ = TonalPalette::FromHueAndChroma(hue, 8.0);
      break;

    case Variant::kVibrant: {
      static const double kBp[] = {0, 41, 61, 101, 131, 181, 251, 301, 360};
      static const double kSR[] = {18, 15, 10, 12, 15, 18, 15, 12, 12};
      static const double kTR[] = {35, 30, 20, 25, 30, 35, 30, 25, 25};
      primary_palette_ =
          TonalPalette::FromHueAndChroma(hue, 200.0);
      secondary_palette_ = TonalPalette::FromHueAndChroma(
          GetRotatedHue(hue, kBp, kSR, 9), 24.0);
      tertiary_palette_ = TonalPalette::FromHueAndChroma(
          GetRotatedHue(hue, kBp, kTR, 9), 32.0);
      neutral_palette_ = TonalPalette::FromHueAndChroma(hue, 10.0);
      neutral_variant_palette_ =
          TonalPalette::FromHueAndChroma(hue, 12.0);
      break;
    }

    case Variant::kExpressive: {
      static const double kBp[] = {0, 21, 51, 121, 151, 191, 271, 321, 360};
      static const double kSR[] = {45, 95, 45, 20, 45, 90, 45, 45, 45};
      static const double kTR[] = {120, 120, 20, 45, 20, 15, 20, 120, 120};
      primary_palette_ = TonalPalette::FromHueAndChroma(
          SanitizeDegreesDouble(hue + 240.0), 40.0);
      secondary_palette_ = TonalPalette::FromHueAndChroma(
          GetRotatedHue(hue, kBp, kSR, 9), 24.0);
      tertiary_palette_ = TonalPalette::FromHueAndChroma(
          GetRotatedHue(hue, kBp, kTR, 9), 32.0);
      neutral_palette_ = TonalPalette::FromHueAndChroma(
          SanitizeDegreesDouble(hue + 15.0), 8.0);
      neutral_variant_palette_ = TonalPalette::FromHueAndChroma(
          SanitizeDegreesDouble(hue + 15.0), 12.0);
      break;
    }

    case Variant::kFidelity:
      primary_palette_ = TonalPalette::FromHueAndChroma(hue, chroma);
      secondary_palette_ = TonalPalette::FromHueAndChroma(
          hue, std::max(chroma - 32.0, chroma * 0.5));
      tertiary_palette_ =
          TonalPalette::FromHueAndChroma(hue + 60.0, chroma / 2.0);
      neutral_palette_ =
          TonalPalette::FromHueAndChroma(hue, chroma / 8.0);
      neutral_variant_palette_ =
          TonalPalette::FromHueAndChroma(hue, chroma / 8.0 + 4.0);
      break;

    case Variant::kContent:
      primary_palette_ = TonalPalette::FromHueAndChroma(hue, chroma);
      secondary_palette_ = TonalPalette::FromHueAndChroma(
          hue, std::max(chroma - 32.0, chroma * 0.5));
      tertiary_palette_ =
          TonalPalette::FromHueAndChroma(hue + 60.0, chroma / 2.0);
      neutral_palette_ =
          TonalPalette::FromHueAndChroma(hue, chroma / 8.0);
      neutral_variant_palette_ =
          TonalPalette::FromHueAndChroma(hue, chroma / 8.0 + 4.0);
      break;

    case Variant::kRainbow:
      primary_palette_ = TonalPalette::FromHueAndChroma(hue, 48.0);
      secondary_palette_ = TonalPalette::FromHueAndChroma(hue, 16.0);
      tertiary_palette_ =
          TonalPalette::FromHueAndChroma(hue + 60.0, 24.0);
      neutral_palette_ = TonalPalette::FromHueAndChroma(hue, 0.0);
      neutral_variant_palette_ =
          TonalPalette::FromHueAndChroma(hue, 0.0);
      break;

    case Variant::kFruitSalad:
      primary_palette_ =
          TonalPalette::FromHueAndChroma(hue - 50.0, 48.0);
      secondary_palette_ =
          TonalPalette::FromHueAndChroma(hue - 50.0, 36.0);
      tertiary_palette_ = TonalPalette::FromHueAndChroma(hue, 36.0);
      neutral_palette_ = TonalPalette::FromHueAndChroma(hue, 10.0);
      neutral_variant_palette_ =
          TonalPalette::FromHueAndChroma(hue, 16.0);
      break;
  }

  error_palette_ = TonalPalette::FromHueAndChroma(25.0, 84.0);
}

// ---------------------------------------------------------------------------
// Static contrast helpers
// ---------------------------------------------------------------------------

double DynamicScheme::RatioOfTones(double t1, double t2) {
  return material_color::RatioOfTones(t1, t2);
}

double DynamicScheme::ForegroundTone(double bg, double ratio) {
  return material_color::ForegroundTone(bg, ratio);
}

uint32_t DynamicScheme::HighestSurface() const {
  return is_dark_ ? surface_bright() : surface_dim();
}

// ---------------------------------------------------------------------------
// Surface colors
// ---------------------------------------------------------------------------

uint32_t DynamicScheme::background() const {
  return neutral_palette_.Tone(is_dark_ ? 6 : 98);
}

uint32_t DynamicScheme::on_background() const {
  double bg = is_dark_ ? 6.0 : 98.0;
  double ratio = ContrastCurveGet(3.0, 3.0, 4.5, 7.0, contrast_level_);
  return neutral_palette_.Tone(ForegroundTone(bg, ratio));
}

uint32_t DynamicScheme::surface() const {
  return neutral_palette_.Tone(is_dark_ ? 6 : 98);
}

uint32_t DynamicScheme::surface_dim() const {
  if (is_dark_)
    return neutral_palette_.Tone(6);
  return neutral_palette_.Tone(
      ContrastCurveGet(87.0, 87.0, 80.0, 75.0, contrast_level_));
}

uint32_t DynamicScheme::surface_bright() const {
  if (!is_dark_)
    return neutral_palette_.Tone(98);
  return neutral_palette_.Tone(
      ContrastCurveGet(24.0, 24.0, 29.0, 34.0, contrast_level_));
}

uint32_t DynamicScheme::surface_container_lowest() const {
  if (is_dark_)
    return neutral_palette_.Tone(
        ContrastCurveGet(4.0, 4.0, 2.0, 0.0, contrast_level_));
  return neutral_palette_.Tone(100);
}

uint32_t DynamicScheme::surface_container_low() const {
  if (is_dark_)
    return neutral_palette_.Tone(
        ContrastCurveGet(10.0, 10.0, 11.0, 12.0, contrast_level_));
  return neutral_palette_.Tone(
      ContrastCurveGet(96.0, 96.0, 96.0, 95.0, contrast_level_));
}

uint32_t DynamicScheme::surface_container() const {
  if (is_dark_)
    return neutral_palette_.Tone(
        ContrastCurveGet(12.0, 12.0, 16.0, 20.0, contrast_level_));
  return neutral_palette_.Tone(
      ContrastCurveGet(94.0, 94.0, 92.0, 90.0, contrast_level_));
}

uint32_t DynamicScheme::surface_container_high() const {
  if (is_dark_)
    return neutral_palette_.Tone(
        ContrastCurveGet(17.0, 17.0, 21.0, 25.0, contrast_level_));
  return neutral_palette_.Tone(
      ContrastCurveGet(92.0, 92.0, 88.0, 85.0, contrast_level_));
}

uint32_t DynamicScheme::surface_container_highest() const {
  if (is_dark_)
    return neutral_palette_.Tone(
        ContrastCurveGet(22.0, 22.0, 26.0, 30.0, contrast_level_));
  return neutral_palette_.Tone(
      ContrastCurveGet(90.0, 90.0, 84.0, 80.0, contrast_level_));
}

uint32_t DynamicScheme::on_surface() const {
  double bg = is_dark_ ? 6.0 : 98.0;
  double ratio = ContrastCurveGet(4.5, 7.0, 11.0, 21.0, contrast_level_);
  return neutral_palette_.Tone(ForegroundTone(bg, ratio));
}

uint32_t DynamicScheme::surface_variant() const {
  return neutral_variant_palette_.Tone(is_dark_ ? 30 : 90);
}

uint32_t DynamicScheme::on_surface_variant() const {
  double bg = is_dark_ ? 30.0 : 90.0;
  double ratio = ContrastCurveGet(3.0, 4.5, 7.0, 11.0, contrast_level_);
  return neutral_variant_palette_.Tone(ForegroundTone(bg, ratio));
}

uint32_t DynamicScheme::inverse_surface() const {
  return neutral_palette_.Tone(is_dark_ ? 90 : 20);
}

uint32_t DynamicScheme::inverse_on_surface() const {
  double bg = is_dark_ ? 90.0 : 20.0;
  double ratio = ContrastCurveGet(4.5, 7.0, 11.0, 21.0, contrast_level_);
  return neutral_palette_.Tone(ForegroundTone(bg, ratio));
}

uint32_t DynamicScheme::surface_tint() const {
  return primary_palette_.Tone(is_dark_ ? 80 : 40);
}

// ---------------------------------------------------------------------------
// Primary
// ---------------------------------------------------------------------------

uint32_t DynamicScheme::primary() const {
  if (IsMonochrome(variant_))
    return primary_palette_.Tone(is_dark_ ? 100.0 : 0.0);
  return primary_palette_.Tone(is_dark_ ? 80.0 : 40.0);
}

uint32_t DynamicScheme::on_primary() const {
  double tone = is_dark_ ? 20.0 : 100.0;
  if (IsMonochrome(variant_))
    tone = is_dark_ ? 10.0 : 90.0;
  double bg_lstar = Hct::FromInt(primary()).tone();
  double ratio = ContrastCurveGet(4.5, 7.0, 11.0, 21.0, contrast_level_);
  return primary_palette_.Tone(
      ResolveForegroundTone(bg_lstar, tone, ratio));
}

uint32_t DynamicScheme::primary_container() const {
  if (IsFidelity(variant_))
    return primary_palette_.Tone(source_color_hct_.tone());
  if (IsMonochrome(variant_))
    return primary_palette_.Tone(is_dark_ ? 85.0 : 25.0);
  return primary_palette_.Tone(is_dark_ ? 30.0 : 90.0);
}

uint32_t DynamicScheme::on_primary_container() const {
  if (IsFidelity(variant_)) {
    double container_lstar =
        Hct::FromInt(primary_palette_.Tone(source_color_hct_.tone())).tone();
    return primary_palette_.Tone(ForegroundTone(container_lstar, 4.5));
  }
  if (IsMonochrome(variant_))
    return primary_palette_.Tone(is_dark_ ? 0.0 : 100.0);
  return primary_palette_.Tone(is_dark_ ? 90.0 : 30.0);
}

uint32_t DynamicScheme::primary_fixed() const {
  return primary_palette_.Tone(IsMonochrome(variant_) ? 40.0 : 90.0);
}

uint32_t DynamicScheme::primary_fixed_dim() const {
  return primary_palette_.Tone(IsMonochrome(variant_) ? 30.0 : 80.0);
}

uint32_t DynamicScheme::on_primary_fixed() const {
  return primary_palette_.Tone(IsMonochrome(variant_) ? 100.0 : 10.0);
}

uint32_t DynamicScheme::on_primary_fixed_variant() const {
  return primary_palette_.Tone(IsMonochrome(variant_) ? 90.0 : 30.0);
}

uint32_t DynamicScheme::inverse_primary() const {
  return primary_palette_.Tone(is_dark_ ? 40.0 : 80.0);
}

// ---------------------------------------------------------------------------
// Secondary
// ---------------------------------------------------------------------------

uint32_t DynamicScheme::secondary() const {
  return secondary_palette_.Tone(is_dark_ ? 80 : 40);
}

uint32_t DynamicScheme::on_secondary() const {
  double tone = is_dark_ ? 20.0 : 100.0;
  if (IsMonochrome(variant_))
    tone = is_dark_ ? 10.0 : 100.0;
  double bg_lstar = Hct::FromInt(secondary()).tone();
  double ratio = ContrastCurveGet(4.5, 7.0, 11.0, 21.0, contrast_level_);
  return secondary_palette_.Tone(
      ResolveForegroundTone(bg_lstar, tone, ratio));
}

uint32_t DynamicScheme::secondary_container() const {
  if (IsMonochrome(variant_))
    return secondary_palette_.Tone(is_dark_ ? 30.0 : 85.0);
  double initial = is_dark_ ? 30.0 : 90.0;
  if (!IsFidelity(variant_))
    return secondary_palette_.Tone(initial);
  double adjusted = FindDesiredChromaByTone(
      secondary_palette_.hue(), secondary_palette_.chroma(), initial,
      !is_dark_);
  return secondary_palette_.Tone(adjusted);
}

uint32_t DynamicScheme::on_secondary_container() const {
  if (IsMonochrome(variant_))
    return secondary_palette_.Tone(is_dark_ ? 90.0 : 10.0);
  if (!IsFidelity(variant_))
    return secondary_palette_.Tone(is_dark_ ? 90.0 : 30.0);
  double bg_lstar = Hct::FromInt(secondary_container()).tone();
  return secondary_palette_.Tone(ForegroundTone(bg_lstar, 4.5));
}

uint32_t DynamicScheme::secondary_fixed() const {
  return secondary_palette_.Tone(IsMonochrome(variant_) ? 80.0 : 90.0);
}

uint32_t DynamicScheme::secondary_fixed_dim() const {
  return secondary_palette_.Tone(IsMonochrome(variant_) ? 70.0 : 80.0);
}

uint32_t DynamicScheme::on_secondary_fixed() const {
  return secondary_palette_.Tone(10.0);
}

uint32_t DynamicScheme::on_secondary_fixed_variant() const {
  return secondary_palette_.Tone(IsMonochrome(variant_) ? 25.0 : 30.0);
}

// ---------------------------------------------------------------------------
// Tertiary
// ---------------------------------------------------------------------------

uint32_t DynamicScheme::tertiary() const {
  if (IsMonochrome(variant_))
    return tertiary_palette_.Tone(is_dark_ ? 90.0 : 25.0);
  return tertiary_palette_.Tone(is_dark_ ? 80.0 : 40.0);
}

uint32_t DynamicScheme::on_tertiary() const {
  double tone = is_dark_ ? 20.0 : 100.0;
  if (IsMonochrome(variant_))
    tone = is_dark_ ? 10.0 : 90.0;
  double bg_lstar = Hct::FromInt(tertiary()).tone();
  double ratio = ContrastCurveGet(4.5, 7.0, 11.0, 21.0, contrast_level_);
  return tertiary_palette_.Tone(
      ResolveForegroundTone(bg_lstar, tone, ratio));
}

uint32_t DynamicScheme::tertiary_container() const {
  if (IsMonochrome(variant_))
    return tertiary_palette_.Tone(is_dark_ ? 60.0 : 49.0);
  if (!IsFidelity(variant_))
    return tertiary_palette_.Tone(is_dark_ ? 30.0 : 90.0);
  double proposed =
      tertiary_palette_.GetHct(source_color_hct_.tone()).tone();
  return tertiary_palette_.Tone(proposed);
}

uint32_t DynamicScheme::on_tertiary_container() const {
  if (IsMonochrome(variant_))
    return tertiary_palette_.Tone(is_dark_ ? 0.0 : 100.0);
  if (!IsFidelity(variant_))
    return tertiary_palette_.Tone(is_dark_ ? 90.0 : 30.0);
  double bg_lstar = Hct::FromInt(tertiary_container()).tone();
  return tertiary_palette_.Tone(ForegroundTone(bg_lstar, 4.5));
}

uint32_t DynamicScheme::tertiary_fixed() const {
  return tertiary_palette_.Tone(IsMonochrome(variant_) ? 40.0 : 90.0);
}

uint32_t DynamicScheme::tertiary_fixed_dim() const {
  return tertiary_palette_.Tone(IsMonochrome(variant_) ? 30.0 : 80.0);
}

uint32_t DynamicScheme::on_tertiary_fixed() const {
  return tertiary_palette_.Tone(IsMonochrome(variant_) ? 100.0 : 10.0);
}

uint32_t DynamicScheme::on_tertiary_fixed_variant() const {
  return tertiary_palette_.Tone(IsMonochrome(variant_) ? 90.0 : 30.0);
}

// ---------------------------------------------------------------------------
// Error
// ---------------------------------------------------------------------------

uint32_t DynamicScheme::error() const {
  return error_palette_.Tone(is_dark_ ? 80.0 : 40.0);
}

uint32_t DynamicScheme::on_error() const {
  return error_palette_.Tone(is_dark_ ? 20.0 : 100.0);
}

uint32_t DynamicScheme::error_container() const {
  return error_palette_.Tone(is_dark_ ? 30.0 : 90.0);
}

uint32_t DynamicScheme::on_error_container() const {
  if (IsMonochrome(variant_))
    return error_palette_.Tone(is_dark_ ? 90.0 : 10.0);
  return error_palette_.Tone(is_dark_ ? 90.0 : 30.0);
}

// ---------------------------------------------------------------------------
// Outline
// ---------------------------------------------------------------------------

uint32_t DynamicScheme::outline() const {
  double bg = is_dark_ ? 30.0 : 90.0;
  double ratio = ContrastCurveGet(1.5, 3.0, 4.5, 7.0, contrast_level_);
  return neutral_variant_palette_.Tone(ForegroundTone(bg, ratio));
}

uint32_t DynamicScheme::outline_variant() const {
  return neutral_variant_palette_.Tone(is_dark_ ? 30.0 : 80.0);
}

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------

uint32_t DynamicScheme::shadow() const {
  return neutral_palette_.Tone(0);
}

uint32_t DynamicScheme::scrim() const {
  return neutral_palette_.Tone(0);
}

}  // namespace material_color
