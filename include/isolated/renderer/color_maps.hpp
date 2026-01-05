#pragma once

/**
 * @file color_maps.hpp
 * @brief Color gradient utilities for simulation visualization.
 */

#include "raylib.h"
#include <algorithm>
#include <cmath>

namespace isolated {
namespace renderer {

/**
 * @brief Linearly interpolate between two colors.
 */
inline Color lerp_color(Color a, Color b, float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  return {static_cast<unsigned char>(a.r + (b.r - a.r) * t),
          static_cast<unsigned char>(a.g + (b.g - a.g) * t),
          static_cast<unsigned char>(a.b + (b.b - a.b) * t),
          static_cast<unsigned char>(a.a + (b.a - a.a) * t)};
}

/**
 * @brief Map fluid density to color (blue=low, white=normal, red=high).
 */
inline Color pressure_to_color(double density, double min_rho = 0.8,
                               double max_rho = 1.2) {
  // Normalize to 0-1 range
  double normalized = (density - min_rho) / (max_rho - min_rho);
  normalized = std::clamp(normalized, 0.0, 1.0);

  // Blue (low) -> White (1.0) -> Red (high)
  if (normalized < 0.5) {
    // Blue to White
    float t = static_cast<float>(normalized * 2.0);
    return lerp_color({0, 100, 255, 200}, {255, 255, 255, 200}, t);
  } else {
    // White to Red
    float t = static_cast<float>((normalized - 0.5) * 2.0);
    return lerp_color({255, 255, 255, 200}, {255, 50, 50, 200}, t);
  }
}

/**
 * @brief Map temperature to color (blue=cold, red=hot).
 */
inline Color temperature_to_color(double temp_k, double min_temp = 200.0,
                                  double max_temp = 600.0) {
  double normalized = (temp_k - min_temp) / (max_temp - min_temp);
  normalized = std::clamp(normalized, 0.0, 1.0);

  // Blue (cold) -> Cyan -> Green -> Yellow -> Red (hot)
  if (normalized < 0.25) {
    float t = static_cast<float>(normalized * 4.0);
    return lerp_color({0, 0, 128, 200}, {0, 200, 255, 200}, t);
  } else if (normalized < 0.5) {
    float t = static_cast<float>((normalized - 0.25) * 4.0);
    return lerp_color({0, 200, 255, 200}, {0, 255, 100, 200}, t);
  } else if (normalized < 0.75) {
    float t = static_cast<float>((normalized - 0.5) * 4.0);
    return lerp_color({0, 255, 100, 200}, {255, 255, 0, 200}, t);
  } else {
    float t = static_cast<float>((normalized - 0.75) * 4.0);
    return lerp_color({255, 255, 0, 200}, {255, 50, 0, 200}, t);
  }
}

/**
 * @brief Map oxygen fraction to color (red=low, green=normal).
 */
inline Color oxygen_to_color(double o2_fraction, double danger = 0.16,
                             double normal = 0.21) {
  if (o2_fraction < danger) {
    return {255, 0, 0, 180}; // Danger zone
  } else if (o2_fraction < normal) {
    float t = static_cast<float>((o2_fraction - danger) / (normal - danger));
    return lerp_color({255, 100, 0, 150}, {100, 200, 100, 100}, t);
  }
  return {100, 200, 100, 50}; // Normal - very transparent
}

} // namespace renderer
} // namespace isolated
