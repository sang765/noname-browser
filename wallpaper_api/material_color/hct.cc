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

#define _USE_MATH_DEFINES
#include "wallpaper_api/material_color/hct.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace material_color {
namespace {

// ---------------------------------------------------------------------------
// math_utils (from @material/material-color-utilities)
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

int ClampInt(int min, int max, int input) {
  if (input < min) return min;
  if (input > max) return max;
  return input;
}

double Signum(double num) {
  if (num < 0.0) return -1.0;
  if (num == 0.0) return 0.0;
  return 1.0;
}

double SanitizeRadians(double angle) {
  return std::fmod(angle + M_PI * 8.0, M_PI * 2.0);
}

// 1x3 * 3x3 matrix multiply.
void MatrixMultiply(const double row[3], const double matrix[3][3],
                    double out[3]) {
  out[0] = row[0] * matrix[0][0] + row[1] * matrix[0][1] +
           row[2] * matrix[0][2];
  out[1] = row[0] * matrix[1][0] + row[1] * matrix[1][1] +
           row[2] * matrix[1][2];
  out[2] = row[0] * matrix[2][0] + row[1] * matrix[2][1] +
           row[2] * matrix[2][2];
}

// ---------------------------------------------------------------------------
// color_utils (from @material/material-color-utilities)
// ---------------------------------------------------------------------------

// sRGB-to-XYZ matrix (D65).
const double kSRgbToXyz[3][3] = {
    {0.41233895, 0.35762064, 0.18051042},
    {0.2126, 0.7152, 0.0722},
    {0.01932141, 0.11916382, 0.95034478},
};

// XYZ-to-sRGB matrix.
const double kXyzToSRgb[3][3] = {
    {3.2413774792388685, -1.5376652402851851, -0.49885366846268053},
    {-0.9691452513005321, 1.8758853451067872, 0.04156585616912061},
    {0.05562093689691305, -0.20395524564742123, 1.0571799111220335},
};

const double kWhitePointD65[3] = {95.047, 100.0, 108.883};

const double kYFromLinRgb[3] = {0.2126, 0.7152, 0.0722};

double LabF(double t) {
  const double e = 216.0 / 24389.0;
  const double kappa = 24389.0 / 27.0;
  if (t > e) {
    return std::cbrt(t);
  }
  return (kappa * t + 16.0) / 116.0;
}

double LabInvF(double ft) {
  const double e = 216.0 / 24389.0;
  const double kappa = 24389.0 / 27.0;
  double ft3 = ft * ft * ft;
  if (ft3 > e) return ft3;
  return (116.0 * ft - 16.0) / kappa;
}

double Linearized(int rgb_component) {
  double normalized = rgb_component / 255.0;
  if (normalized <= 0.040449936) {
    return normalized / 12.92 * 100.0;
  }
  return std::pow((normalized + 0.055) / 1.055, 2.4) * 100.0;
}

int Delinearized(double rgb_component) {
  double normalized = rgb_component / 100.0;
  double delinearized = 0.0;
  if (normalized <= 0.0031308) {
    delinearized = normalized * 12.92;
  } else {
    delinearized = 1.055 * std::pow(normalized, 1.0 / 2.4) - 0.055;
  }
  return ClampInt(0, 255, static_cast<int>(std::round(delinearized * 255.0)));
}

uint32_t ArgbFromRgb(int red, int green, int blue) {
  return (static_cast<uint32_t>(255) << 24) |
         (static_cast<uint32_t>(red & 0xFF) << 16) |
         (static_cast<uint32_t>(green & 0xFF) << 8) |
         (static_cast<uint32_t>(blue & 0xFF));
}

uint32_t ArgbFromLinRgb(const double lin_rgb[3]) {
  return ArgbFromRgb(Delinearized(lin_rgb[0]), Delinearized(lin_rgb[1]),
                     Delinearized(lin_rgb[2]));
}

uint32_t ArgbFromXyz(double x, double y, double z) {
  double linear_r =
      kXyzToSRgb[0][0] * x + kXyzToSRgb[0][1] * y + kXyzToSRgb[0][2] * z;
  double linear_g =
      kXyzToSRgb[1][0] * x + kXyzToSRgb[1][1] * y + kXyzToSRgb[1][2] * z;
  double linear_b =
      kXyzToSRgb[2][0] * x + kXyzToSRgb[2][1] * y + kXyzToSRgb[2][2] * z;
  const double lin_rgb[3] = {linear_r, linear_g, linear_b};
  return ArgbFromLinRgb(lin_rgb);
}

void XyzFromArgb(uint32_t argb, double out[3]) {
  double r = Linearized((argb >> 16) & 0xFF);
  double g = Linearized((argb >> 8) & 0xFF);
  double b = Linearized(argb & 0xFF);
  double rgb[3] = {r, g, b};
  MatrixMultiply(rgb, kSRgbToXyz, out);
}

double YFromLstar(double lstar) {
  return 100.0 * LabInvF((lstar + 16.0) / 116.0);
}

double LstarFromY(double y) {
  return LabF(y / 100.0) * 116.0 - 16.0;
}

uint32_t ArgbFromLstar(double lstar) {
  double y = YFromLstar(lstar);
  int component = Delinearized(y);
  return ArgbFromRgb(component, component, component);
}

double LstarFromArgb(uint32_t argb) {
  double xyz[3];
  XyzFromArgb(argb, xyz);
  return LstarFromY(xyz[1]);
}

// ---------------------------------------------------------------------------
// ViewingConditions (from @material/material-color-utilities)
// ---------------------------------------------------------------------------

struct ViewingConditions {
  double n;
  double aw;
  double nbb;
  double ncb;
  double c;
  double nc;
  double rgb_d[3];
  double fl;
  double fl_root;
  double z;
};

ViewingConditions MakeViewingConditions(
    const double white_point[3] = kWhitePointD65,
    double adapting_luminance =
        (200.0 / M_PI) * YFromLstar(50.0) / 100.0,
    double background_lstar = 50.0, double surround = 2.0,
    bool discounting_illuminant = false) {
  double r_w = white_point[0] * 0.401288 + white_point[1] * 0.650173 +
               white_point[2] * -0.051461;
  double g_w = white_point[0] * -0.250268 + white_point[1] * 1.204414 +
               white_point[2] * 0.045854;
  double b_w = white_point[0] * -0.002079 + white_point[1] * 0.048952 +
               white_point[2] * 0.953127;
  double f = 0.8 + surround / 10.0;
  double c = f >= 0.9 ? Lerp(0.59, 0.69, (f - 0.9) * 10.0)
                       : Lerp(0.525, 0.59, (f - 0.8) * 10.0);
  double d = discounting_illuminant
                 ? 1.0
                 : f * (1.0 - (1.0 / 3.6) * std::exp((-adapting_luminance - 42.0) / 92.0));
  d = std::max(0.0, std::min(1.0, d));
  double nc = f;
  double rgb_d[3] = {d * (100.0 / r_w) + 1.0 - d,
                     d * (100.0 / g_w) + 1.0 - d,
                     d * (100.0 / b_w) + 1.0 - d};
  double k = 1.0 / (5.0 * adapting_luminance + 1.0);
  double k4 = k * k * k * k;
  double k4f = 1.0 - k4;
  double fl = k4 * adapting_luminance +
              0.1 * k4f * k4f * std::cbrt(5.0 * adapting_luminance);
  double n = YFromLstar(background_lstar) / white_point[1];
  double z = 1.48 + std::sqrt(n);
  double nbb = 0.725 / std::pow(n, 0.2);
  double ncb = nbb;
  double rgb_a_factors[3] = {
      std::pow((fl * rgb_d[0] * r_w) / 100.0, 0.42),
      std::pow((fl * rgb_d[1] * g_w) / 100.0, 0.42),
      std::pow((fl * rgb_d[2] * b_w) / 100.0, 0.42)};
  double rgb_a[3] = {(400.0 * rgb_a_factors[0]) / (rgb_a_factors[0] + 27.13),
                     (400.0 * rgb_a_factors[1]) / (rgb_a_factors[1] + 27.13),
                     (400.0 * rgb_a_factors[2]) / (rgb_a_factors[2] + 27.13)};
  double aw = (2.0 * rgb_a[0] + rgb_a[1] + 0.05 * rgb_a[2]) * nbb;

  ViewingConditions vc;
  vc.n = n;
  vc.aw = aw;
  vc.nbb = nbb;
  vc.ncb = ncb;
  vc.c = c;
  vc.nc = nc;
  vc.rgb_d[0] = rgb_d[0];
  vc.rgb_d[1] = rgb_d[1];
  vc.rgb_d[2] = rgb_d[2];
  vc.fl = fl;
  vc.fl_root = std::pow(fl, 0.25);
  vc.z = z;
  return vc;
}

// Default viewing conditions (sRGB-like).
const ViewingConditions kDefaultVc = MakeViewingConditions();

// ---------------------------------------------------------------------------
// Cam16 (from @material/material-color-utilities)
// ---------------------------------------------------------------------------

struct Cam16 {
  double hue;
  double chroma;
  double j;
  double q;
  double m;
  double s;
  double jstar;
  double astar;
  double bstar;
};

Cam16 Cam16FromIntInViewingConditions(uint32_t argb,
                                     const ViewingConditions& vc) {
  int red = (argb >> 16) & 0xFF;
  int green = (argb >> 8) & 0xFF;
  int blue = argb & 0xFF;
  double red_l = Linearized(red);
  double green_l = Linearized(green);
  double blue_l = Linearized(blue);

  // XYZ
  double x = 0.41233895 * red_l + 0.35762064 * green_l + 0.18051042 * blue_l;
  double y = 0.2126 * red_l + 0.7152 * green_l + 0.0722 * blue_l;
  double z = 0.01932141 * red_l + 0.11916382 * green_l + 0.95034478 * blue_l;

  // Cone response
  double r_c = 0.401288 * x + 0.650173 * y - 0.051461 * z;
  double g_c = -0.250268 * x + 1.204414 * y + 0.045854 * z;
  double b_c = -0.002079 * x + 0.048952 * y + 0.953127 * z;

  // Discount illuminant
  double r_d = vc.rgb_d[0] * r_c;
  double g_d = vc.rgb_d[1] * g_c;
  double b_d = vc.rgb_d[2] * b_c;

  // Chromatic adaptation
  double r_af = std::pow((vc.fl * std::abs(r_d)) / 100.0, 0.42);
  double g_af = std::pow((vc.fl * std::abs(g_d)) / 100.0, 0.42);
  double b_af = std::pow((vc.fl * std::abs(b_d)) / 100.0, 0.42);
  double r_a = Signum(r_d) * 400.0 * r_af / (r_af + 27.13);
  double g_a = Signum(g_d) * 400.0 * g_af / (g_af + 27.13);
  double b_a = Signum(b_d) * 400.0 * b_af / (b_af + 27.13);

  // Redness-greenness, yellowness-blueness
  double a = (11.0 * r_a + -12.0 * g_a + b_a) / 11.0;
  double b = (r_a + g_a - 2.0 * b_a) / 9.0;

  // Auxiliary
  double u = (20.0 * r_a + 20.0 * g_a + 21.0 * b_a) / 20.0;
  double p2 = (40.0 * r_a + 20.0 * g_a + b_a) / 20.0;

  // Hue
  double atan2 = std::atan2(b, a);
  double atan_degrees = atan2 * 180.0 / M_PI;
  double hue = SanitizeDegreesDouble(atan_degrees);
  double hue_radians = hue * M_PI / 180.0;

  // Achromatic response
  double ac = p2 * vc.nbb;

  // CAM16 lightness and brightness
  double j =
      100.0 * std::pow(ac / vc.aw, vc.c * vc.z);
  double q = (4.0 / vc.c) * std::sqrt(j / 100.0) * (vc.aw + 4.0) * vc.fl_root;

  // Chroma
  double hue_prime = hue < 20.14 ? hue + 360.0 : hue;
  double e_hue = 0.25 * (std::cos(hue_prime * M_PI / 180.0 + 2.0) + 3.8);
  double p1 = (50000.0 / 13.0) * e_hue * vc.nc * vc.ncb;
  double t = p1 * std::sqrt(a * a + b * b) / (u + 0.305);
  double alpha =
      std::pow(t, 0.9) * std::pow(1.64 - std::pow(0.29, vc.n), 0.73);
  double c = alpha * std::sqrt(j / 100.0);
  double m = c * vc.fl_root;
  double s =
      50.0 * std::sqrt((alpha * vc.c) / (vc.aw + 4.0));

  // CAM16-UCS
  double jstar = ((1.0 + 100.0 * 0.007) * j) / (1.0 + 0.007 * j);
  double mstar = (1.0 / 0.0228) * std::log(1.0 + 0.0228 * m);
  double astar = mstar * std::cos(hue_radians);
  double bstar = mstar * std::sin(hue_radians);

  Cam16 result;
  result.hue = hue;
  result.chroma = c;
  result.j = j;
  result.q = q;
  result.m = m;
  result.s = s;
  result.jstar = jstar;
  result.astar = astar;
  result.bstar = bstar;
  return result;
}

Cam16 Cam16FromInt(uint32_t argb) {
  return Cam16FromIntInViewingConditions(argb, kDefaultVc);
}

uint32_t Cam16Viewed(const Cam16& cam, const ViewingConditions& vc) {
  double alpha =
      (cam.chroma == 0.0 || cam.j == 0.0)
          ? 0.0
          : cam.chroma / std::sqrt(cam.j / 100.0);
  double t = std::pow(
      alpha / std::pow(1.64 - std::pow(0.29, vc.n), 0.73), 1.0 / 0.9);
  double h_rad = cam.hue * M_PI / 180.0;
  double e_hue = 0.25 * (std::cos(h_rad + 2.0) + 3.8);
  double ac =
      vc.aw * std::pow(cam.j / 100.0, 1.0 / vc.c / vc.z);
  double p1 = e_hue * (50000.0 / 13.0) * vc.nc * vc.ncb;
  double p2 = ac / vc.nbb;
  double h_sin = std::sin(h_rad);
  double h_cos = std::cos(h_rad);

  double gamma = (23.0 * (p2 + 0.305) * t) /
                 (23.0 * p1 + 11.0 * t * h_cos + 108.0 * t * h_sin);
  double a2 = gamma * h_cos;
  double b2 = gamma * h_sin;
  double r_a = (460.0 * p2 + 451.0 * a2 + 288.0 * b2) / 1403.0;
  double g_a = (460.0 * p2 - 891.0 * a2 - 261.0 * b2) / 1403.0;
  double b_a = (460.0 * p2 - 220.0 * a2 - 6300.0 * b2) / 1403.0;

  double r_c_base =
      std::max(0.0, (27.13 * std::abs(r_a)) / (400.0 - std::abs(r_a)));
  double r_c = Signum(r_a) * (100.0 / vc.fl) * std::pow(r_c_base, 1.0 / 0.42);
  double g_c_base =
      std::max(0.0, (27.13 * std::abs(g_a)) / (400.0 - std::abs(g_a)));
  double g_c = Signum(g_a) * (100.0 / vc.fl) * std::pow(g_c_base, 1.0 / 0.42);
  double b_c_base =
      std::max(0.0, (27.13 * std::abs(b_a)) / (400.0 - std::abs(b_a)));
  double b_c = Signum(b_a) * (100.0 / vc.fl) * std::pow(b_c_base, 1.0 / 0.42);

  double r_f = r_c / vc.rgb_d[0];
  double g_f = g_c / vc.rgb_d[1];
  double b_f = b_c / vc.rgb_d[2];

  double x = 1.86206786 * r_f - 1.01125463 * g_f + 0.14918677 * b_f;
  double y = 0.38752654 * r_f + 0.62144744 * g_f - 0.00897398 * b_f;
  double z = -0.01584150 * r_f - 0.03412294 * g_f + 1.04996444 * b_f;

  return ArgbFromXyz(x, y, z);
}

uint32_t Cam16ToInt(const Cam16& cam) {
  return Cam16Viewed(cam, kDefaultVc);
}

// ---------------------------------------------------------------------------
// HctSolver (from @material/material-color-utilities)
// ---------------------------------------------------------------------------

const double kScaledDiscountFromLinRgb[3][3] = {
    {0.001200833568784504, 0.002389694492170889, 0.0002795742885861124},
    {0.0005891086651375999, 0.0029785502573438758, 0.0003270666104008398},
    {0.00010146692491640572, 0.0005364214359186694, 0.0032979401770712076},
};

const double kLinRgbFromScaledDiscount[3][3] = {
    {1373.2198709594231, -1100.4251190754821, -7.278681089101213},
    {-271.815969077903, 559.6580465940733, -32.46047482791194},
    {1.9622899599665666, -57.173814538844006, 308.7233197812385},
};

const int kCriticalPlanesCount = 256;
double kCriticalPlanes[kCriticalPlanesCount] = {
    0.015176349177441876, 0.045529047532325624, 0.07588174588720938,
    0.10623444424209313,  0.13658714259697685,  0.16693984095186062,
    0.19729253930674434,  0.2276452376616281,   0.2579979360165119,
    0.28835063437139563,  0.3188300904430532,   0.350925934958123,
    0.3848314933096426,   0.42057480301049466,  0.458183274052838,
    0.4976837250274023,   0.5391024159806381,   0.5824650784040898,
    0.6277969426914107,   0.6751227633498623,   0.7244668422128921,
    0.775853049866786,    0.829304845476233,    0.8848452951698498,
    0.942497089126609,    1.0022825574869039,   1.0642236851973577,
    1.1283421258858297,   1.1946592148522128,   1.2631959812511864,
    1.3339731595349034,   1.407011200216447,    1.4823302800086415,
    1.5599503113873272,   1.6398909516233677,   1.7221716113234105,
    1.8068114625156377,   1.8938294463134073,   1.9832442801866852,
    2.075074464868551,    2.1693382909216234,   2.2660538449872063,
    2.36523901573795,     2.4669114995532007,   2.5710888059345764,
    2.6777882626779785,   2.7870270208169257,   2.898822059350997,
    3.0131901897720907,   3.1301480604002863,   3.2497121605402226,
    3.3718988244681087,   3.4967242352587946,   3.624204428461639,
    3.754355295633311,    3.887192587735158,    4.022731918402185,
    4.160988767090289,    4.301978482107941,    4.445716283538092,
    4.592217266055746,    4.741496401646282,    4.893568542229298,
    5.048448422192488,    5.20615066083972,     5.3666897647573375,
    5.5300801301023865,   5.696336044816294,    5.865471690767354,
    6.037501145825082,    6.212438385869475,    6.390297286737924,
    6.571091626112461,    6.7548350853498045,   6.941541251256611,
    7.131223617812143,    7.323895587840543,    7.5195704746346665,
    7.7182615035334345,   7.919981813454504,    8.124744458384042,
    8.332562408825165,    8.543448553206703,    8.757415699253682,
    8.974476575321063,    9.194643831691977,    9.417930041841839,
    9.644347703669503,    9.873909240696694,    10.106627003236781,
    10.342513269534024,   10.58158024687427,    10.8238400726681,
    11.069304815507364,   11.317986476196008,   11.569896988756009,
    11.825048221409341,   12.083451977536606,   12.345119996613247,
    12.610063955123938,   12.878295467455942,   13.149826086772048,
    13.42466730586372,    13.702830557985108,   13.984327217668513,
    14.269168601521828,   14.55736596900856,    14.848930523210871,
    15.143873411576273,   15.44220572664832,    15.743938506781891,
    16.04908273684337,    16.35764934889634,    16.66964922287304,
    16.985093187232053,   17.30399201960269,    17.62635644741625,
    17.95219714852476,    18.281524751807332,   18.614349837764564,
    18.95068293910138,    19.290534541298456,   19.633915083172692,
    19.98083495742689,    20.331304511189067,   20.685334046541502,
    21.042933821039977,   21.404114048223256,   21.76888489811322,
    22.137256497705877,   22.50923893145328,    22.884842241736916,
    23.264076429332462,   23.6469514538663,     24.033477234264016,
    24.42366364919083,    24.817520537484558,   25.21505769858089,
    25.61628489293138,    26.021211842414342,   26.429848230738664,
    26.842203703840827,   27.258287870275353,   27.678110301598522,
    28.10168053274597,    28.529008062403893,   28.96010235337422,
    29.39497283293396,    29.83362889318845,    30.276079891419332,
    30.722335150426627,   31.172403958865512,   31.62629557157785,
    32.08401920991837,    32.54558406207592,    33.010999283389665,
    33.4802739966603,     33.953417292456834,   34.430438229418264,
    34.911345834551085,   35.39614910352207,    35.88485700094671,
    36.37747846067349,    36.87402238606382,    37.37449765026789,
    37.87891309649659,    38.38727753828926,    38.89959975977785,
    39.41588851594697,    39.93615253289054,    40.460400508064545,
    40.98864111053629,    41.520882981230194,   42.05713473317016,
    42.597404951718396,   43.141702194811224,   43.6900349931913,
    44.24241185063697,    44.798841244188324,   45.35933162437017,
    45.92389141541209,    46.49252901546552,    47.065252796817916,
    47.64207110610409,    48.22299226451468,    48.808024568002054,
    49.3971762874833,     49.9904556690408,     50.587870934119984,
    51.189430279724725,   51.79514187861014,    52.40501387947288,
    53.0190544071392,     53.637271562750364,   54.259673423945976,
    54.88626804504493,    55.517063457223934,   56.15206766869424,
    56.79128866487574,    57.43473440856916,    58.08241284012621,
    58.734331877617365,   59.39049941699807,    60.05092333227251,
    60.715611475655585,   61.38457167773311,    62.057811747619894,
    62.7353394731159,     63.417162620860914,   64.10328893648692,
    64.79372614476921,    65.48848194977529,    66.18756403501224,
    66.89098006357258,    67.59873767827808,    68.31084450182222,
    69.02730813691093,    69.74813616640164,    70.47333615344107,
    71.20291564160104,    71.93688215501312,    72.67524319850172,
    73.41800625771542,    74.16517879925733,    74.9167682708136,
    75.67278210128072,    76.43322770089146,    77.1981124613393,
    77.96744375590167,    78.74122893956174,    79.51947534912904,
    80.30219030335869,    81.08938110306934,    81.88105503125999,
    82.67721935322541,    83.4778813166706,     84.28304815182372,
    85.09272707154808,    85.90692527145302,    86.72564993000343,
    87.54890820862819,    88.3767072518277,     89.2090541872801,
    90.04595612594655,    90.88742016217518,    91.73345337380438,
    92.58406282226491,    93.43925555268066,    94.29903859396902,
    95.16341895893969,    96.03240364439274,    96.9059996312159,
    97.78421388448044,    98.6670533535366,     99.55452497210776,
};

bool AreInCyclicOrder(double a, double b, double c) {
  double delta_ab = SanitizeRadians(b - a);
  double delta_ac = SanitizeRadians(c - a);
  return delta_ab < delta_ac;
}

bool IsBounded(double x) { return 0.0 <= x && x <= 100.0; }

double ChromaticAdaptation(double component) {
  double af = std::pow(std::abs(component), 0.42);
  return Signum(component) * 400.0 * af / (af + 27.13);
}

double HueOf(const double lin_rgb[3]) {
  double scaled_discount[3];
  MatrixMultiply(lin_rgb, kScaledDiscountFromLinRgb, scaled_discount);
  double r_a = ChromaticAdaptation(scaled_discount[0]);
  double g_a = ChromaticAdaptation(scaled_discount[1]);
  double b_a = ChromaticAdaptation(scaled_discount[2]);
  double a2 = (11.0 * r_a + -12.0 * g_a + b_a) / 11.0;
  double b2 = (r_a + g_a - 2.0 * b_a) / 9.0;
  return std::atan2(b2, a2);
}

double Intercept(double source, double mid, double target) {
  return (mid - source) / (target - source);
}

void LerpPoint(const double source[3], double t, const double target[3],
               double out[3]) {
  out[0] = source[0] + (target[0] - source[0]) * t;
  out[1] = source[1] + (target[1] - source[1]) * t;
  out[2] = source[2] + (target[2] - source[2]) * t;
}

void SetCoordinate(const double source[3], double coordinate,
                   const double target[3], int axis, double out[3]) {
  double t = Intercept(source[axis], coordinate, target[axis]);
  LerpPoint(source, t, target, out);
}

void NthVertex(double y, int n, double out[3]) {
  const double k_r = kYFromLinRgb[0];
  const double k_g = kYFromLinRgb[1];
  const double k_b = kYFromLinRgb[2];
  double coord_a = (n % 4 <= 1) ? 0.0 : 100.0;
  double coord_b = (n % 2 == 0) ? 0.0 : 100.0;
  if (n < 4) {
    double g = coord_a;
    double b = coord_b;
    double r = (y - g * k_g - b * k_b) / k_r;
    if (IsBounded(r)) {
      out[0] = r;
      out[1] = g;
      out[2] = b;
      return;
    }
  } else if (n < 8) {
    double b = coord_a;
    double r = coord_b;
    double g = (y - r * k_r - b * k_b) / k_g;
    if (IsBounded(g)) {
      out[0] = r;
      out[1] = g;
      out[2] = b;
      return;
    }
  } else {
    double r = coord_a;
    double g = coord_b;
    double b = (y - r * k_r - g * k_g) / k_b;
    if (IsBounded(b)) {
      out[0] = r;
      out[1] = g;
      out[2] = b;
      return;
    }
  }
  out[0] = -1.0;
  out[1] = -1.0;
  out[2] = -1.0;
}

void BisectToSegment(double y, double target_hue,
                     double left[3], double right[3]) {
  left[0] = -1.0;
  left[1] = -1.0;
  left[2] = -1.0;
  right[0] = -1.0;
  right[1] = -1.0;
  right[2] = -1.0;
  double left_hue = 0.0;
  double right_hue = 0.0;
  bool initialized = false;
  bool uncut = true;

  for (int n = 0; n < 12; n++) {
    double mid[3];
    NthVertex(y, n, mid);
    if (mid[0] < 0) continue;
    double mid_hue = HueOf(mid);
    if (!initialized) {
      left[0] = mid[0];
      left[1] = mid[1];
      left[2] = mid[2];
      right[0] = mid[0];
      right[1] = mid[1];
      right[2] = mid[2];
      left_hue = mid_hue;
      right_hue = mid_hue;
      initialized = true;
      continue;
    }
    if (uncut || AreInCyclicOrder(left_hue, mid_hue, right_hue)) {
      uncut = false;
      if (AreInCyclicOrder(left_hue, target_hue, mid_hue)) {
        right[0] = mid[0];
        right[1] = mid[1];
        right[2] = mid[2];
        right_hue = mid_hue;
      } else {
        left[0] = mid[0];
        left[1] = mid[1];
        left[2] = mid[2];
        left_hue = mid_hue;
      }
    }
  }
}

void Midpoint(const double a[3], const double b[3], double out[3]) {
  out[0] = (a[0] + b[0]) / 2.0;
  out[1] = (a[1] + b[1]) / 2.0;
  out[2] = (a[2] + b[2]) / 2.0;
}

int CriticalPlaneBelow(double x) {
  return static_cast<int>(std::floor(x - 0.5));
}

int CriticalPlaneAbove(double x) {
  return static_cast<int>(std::ceil(x - 0.5));
}

void BisectToLimit(double y, double target_hue, double out[3]) {
  double left[3], right[3];
  BisectToSegment(y, target_hue, left, right);
  double left_hue = HueOf(left);

  for (int axis = 0; axis < 3; axis++) {
    if (left[axis] != right[axis]) {
      int l_plane, r_plane;
      if (left[axis] < right[axis]) {
        l_plane = CriticalPlaneBelow(
            Delinearized(left[axis]));
        r_plane = CriticalPlaneAbove(
            Delinearized(right[axis]));
      } else {
        l_plane = CriticalPlaneAbove(
            Delinearized(left[axis]));
        r_plane = CriticalPlaneBelow(
            Delinearized(right[axis]));
      }
      for (int i = 0; i < 8; i++) {
        if (std::abs(r_plane - l_plane) <= 1) break;
        int m_plane = static_cast<int>(std::floor((l_plane + r_plane) / 2.0));
        double mid_coord = kCriticalPlanes[m_plane];
        double mid[3];
        SetCoordinate(left, mid_coord, right, axis, mid);
        double mid_hue = HueOf(mid);
        if (AreInCyclicOrder(left_hue, target_hue, mid_hue)) {
          right[0] = mid[0];
          right[1] = mid[1];
          right[2] = mid[2];
          r_plane = m_plane;
        } else {
          left[0] = mid[0];
          left[1] = mid[1];
          left[2] = mid[2];
          left_hue = mid_hue;
          l_plane = m_plane;
        }
      }
    }
  }
  Midpoint(left, right, out);
}

double InverseChromaticAdaptation(double adapted) {
  double adapted_abs = std::abs(adapted);
  double base = std::max(0.0, 27.13 * adapted_abs / (400.0 - adapted_abs));
  return Signum(adapted) * std::pow(base, 1.0 / 0.42);
}

uint32_t FindResultByJ(double hue_radians, double chroma, double y) {
  const ViewingConditions& vc = kDefaultVc;
  double t_inner_coeff =
      1.0 / std::pow(1.64 - std::pow(0.29, vc.n), 0.73);
  double e_hue = 0.25 * (std::cos(hue_radians + 2.0) + 3.8);
  double p1 = e_hue * (50000.0 / 13.0) * vc.nc * vc.ncb;
  double h_sin = std::sin(hue_radians);
  double h_cos = std::cos(hue_radians);

  double j = std::sqrt(y) * 11.0;

  for (int iteration_round = 0; iteration_round < 5; iteration_round++) {
    double j_normalized = j / 100.0;
    double alpha =
        (chroma == 0.0 || j == 0.0) ? 0.0 : chroma / std::sqrt(j_normalized);
    double t = std::pow(alpha * t_inner_coeff, 1.0 / 0.9);
    double ac = vc.aw * std::pow(j_normalized, 1.0 / vc.c / vc.z);
    double p2 = ac / vc.nbb;
    double gamma = (23.0 * (p2 + 0.305) * t) /
                   (23.0 * p1 + 11.0 * t * h_cos + 108.0 * t * h_sin);
    double a = gamma * h_cos;
    double b = gamma * h_sin;
    double r_a = (460.0 * p2 + 451.0 * a + 288.0 * b) / 1403.0;
    double g_a = (460.0 * p2 - 891.0 * a - 261.0 * b) / 1403.0;
    double b_a = (460.0 * p2 - 220.0 * a - 6300.0 * b) / 1403.0;

    double r_c_scaled = InverseChromaticAdaptation(r_a);
    double g_c_scaled = InverseChromaticAdaptation(g_a);
    double b_c_scaled = InverseChromaticAdaptation(b_a);

    double lin_rgb[3];
    double scaled[3] = {r_c_scaled, g_c_scaled, b_c_scaled};
    MatrixMultiply(scaled, kLinRgbFromScaledDiscount, lin_rgb);

    if (lin_rgb[0] < 0 || lin_rgb[1] < 0 || lin_rgb[2] < 0) {
      return 0;
    }
    double fnj = kYFromLinRgb[0] * lin_rgb[0] + kYFromLinRgb[1] * lin_rgb[1] +
                 kYFromLinRgb[2] * lin_rgb[2];
    if (fnj <= 0) return 0;

    if (iteration_round == 4 || std::abs(fnj - y) < 0.002) {
      if (lin_rgb[0] > 100.01 || lin_rgb[1] > 100.01 || lin_rgb[2] > 100.01) {
        return 0;
      }
      return ArgbFromLinRgb(lin_rgb);
    }
    // Newton's method
    j = j - (fnj - y) * j / (2.0 * fnj);
  }
  return 0;
}

uint32_t SolveToInt(double hue_degrees, double chroma, double lstar) {
  if (chroma < 0.0001 || lstar < 0.0001 || lstar > 99.9999) {
    return ArgbFromLstar(lstar);
  }
  hue_degrees = SanitizeDegreesDouble(hue_degrees);
  double hue_radians = hue_degrees / 180.0 * M_PI;
  double y = YFromLstar(lstar);

  uint32_t exact = FindResultByJ(hue_radians, chroma, y);
  if (exact != 0) return exact;

  double lin_rgb[3];
  BisectToLimit(y, hue_radians, lin_rgb);
  return ArgbFromLinRgb(lin_rgb);
}

}  // namespace

// ---------------------------------------------------------------------------
// Hct public API
// ---------------------------------------------------------------------------

Hct::Hct(uint32_t argb, double hue, double chroma, double tone)
    : argb_(argb), hue_(hue), chroma_(chroma), tone_(tone) {}

Hct Hct::FromInt(uint32_t argb) {
  Cam16 cam = Cam16FromInt(argb);
  double tone = LstarFromArgb(argb);
  return Hct(argb, cam.hue, cam.chroma, tone);
}

Hct Hct::FromHct(double hue, double chroma, double tone) {
  uint32_t argb = SolveToInt(hue, chroma, tone);
  return FromInt(argb);
}

uint32_t Hct::ToInt() const { return argb_; }

}  // namespace material_color
