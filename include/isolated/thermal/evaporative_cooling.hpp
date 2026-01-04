#pragma once

/**
 * @file evaporative_cooling.hpp
 * @brief Evaporative cooling model for humid environments.
 *
 * Features:
 * - Psychrometric calculations
 * - Wet-bulb temperature
 * - Evaporation rate from surfaces
 * - Cooling power estimation
 */

#include <algorithm>
#include <cmath>

namespace isolated {
namespace thermal {

// ============================================================================
// PSYCHROMETRIC CALCULATIONS
// ============================================================================

/**
 * @brief Saturation vapor pressure (Magnus formula).
 * @param T Temperature in Celsius
 * @return Saturation vapor pressure in Pa
 */
inline double saturation_vapor_pressure(double T) {
  // Magnus formula (accurate for -40 to 50°C)
  return 610.78 * std::exp(17.27 * T / (T + 237.3));
}

/**
 * @brief Relative humidity from vapor pressure.
 * @param p_vapor Actual vapor pressure (Pa)
 * @param T Temperature (°C)
 * @return Relative humidity (0-1)
 */
inline double relative_humidity(double p_vapor, double T) {
  double p_sat = saturation_vapor_pressure(T);
  return std::clamp(p_vapor / p_sat, 0.0, 1.0);
}

/**
 * @brief Dew point temperature.
 * @param T Temperature (°C)
 * @param RH Relative humidity (0-1)
 * @return Dew point (°C)
 */
inline double dew_point(double T, double RH) {
  if (RH <= 0.0)
    return -273.15; // Absolute zero approx
  double alpha = std::log(RH) + 17.27 * T / (T + 237.3);
  return 237.3 * alpha / (17.27 - alpha);
}

/**
 * @brief Wet-bulb temperature (iterative solve).
 * @param T Dry-bulb temperature (°C)
 * @param RH Relative humidity (0-1)
 * @return Wet-bulb temperature (°C)
 */
inline double wet_bulb_temperature(double T, double RH) {
  // Stull approximation (accurate to 0.3°C for RH > 5%)
  double T_K = T + 273.15;
  double term1 = T * std::atan(0.151977 * std::sqrt(RH * 100.0 + 8.313659));
  double term2 = std::atan(T + RH * 100.0);
  double term3 = std::atan(RH * 100.0 - 1.676331);
  double term4 =
      0.00391838 * std::pow(RH * 100.0, 1.5) * std::atan(0.023101 * RH * 100.0);
  return term1 + term2 - term3 + term4 - 4.686035;
}

// ============================================================================
// EVAPORATIVE COOLING
// ============================================================================

struct EvaporationState {
  double evap_rate = 0.0;       // kg/s of water evaporated
  double cooling_power = 0.0;   // W of cooling
  double humidity_change = 0.0; // kg/m³ moisture added
};

/**
 * @brief Calculate evaporative cooling from wet surface.
 */
class EvaporativeCooling {
public:
  struct Config {
    double latent_heat = 2.26e6;       // J/kg latent heat of vaporization
    double lewis_number = 0.9;         // ~1 for air-water
    double mass_transfer_coeff = 0.01; // kg/(m²·s) typical
  };

  explicit EvaporativeCooling(const Config &config = Config{})
      : config_(config) {}

  /**
   * @brief Calculate evaporation from a wet surface.
   * @param T_surface Surface temperature (°C)
   * @param T_air Air temperature (°C)
   * @param RH Air relative humidity (0-1)
   * @param area Wet surface area (m²)
   * @param air_velocity Air velocity over surface (m/s)
   * @return Evaporation state
   */
  EvaporationState calculate(double T_surface, double T_air, double RH,
                             double area, double air_velocity) const {
    EvaporationState state;

    // Vapor pressure difference drives evaporation
    double p_sat_surface = saturation_vapor_pressure(T_surface);
    double p_vapor_air = saturation_vapor_pressure(T_air) * RH;
    double delta_p = p_sat_surface - p_vapor_air;

    if (delta_p <= 0) {
      // No evaporation (condensation would occur)
      return state;
    }

    // Enhanced mass transfer coefficient with velocity
    double h_m =
        config_.mass_transfer_coeff * (1.0 + 0.5 * std::sqrt(air_velocity));

    // Evaporation rate (Dalton's law simplified)
    // m_dot = h_m * A * (p_sat - p_vapor) / (R_v * T)
    double R_v = 461.5; // J/(kg·K) gas constant for water vapor
    double T_avg = (T_surface + T_air) / 2.0 + 273.15;
    state.evap_rate = h_m * area * delta_p / (R_v * T_avg);

    // Cooling power
    state.cooling_power = state.evap_rate * config_.latent_heat;

    // Humidity addition to air (would need volume for density)
    state.humidity_change = state.evap_rate;

    return state;
  }

  /**
   * @brief Estimate cooling of a human via sweating.
   * @param skin_temp Skin temperature (°C)
   * @param T_air Air temperature (°C)
   * @param RH Relative humidity (0-1)
   * @param sweat_rate Sweat production rate (L/hr)
   * @return Evaporative heat loss (W)
   */
  double human_evaporative_cooling(double skin_temp, double T_air, double RH,
                                   double sweat_rate) const {
    // Maximum evaporative capacity
    double p_sat_skin = saturation_vapor_pressure(skin_temp);
    double p_vapor = saturation_vapor_pressure(T_air) * RH;

    // Evaporative potential
    double max_evap = (p_sat_skin - p_vapor) / p_sat_skin;
    max_evap = std::clamp(max_evap, 0.0, 1.0);

    // Actual evaporation (limited by sweat available and air capacity)
    double sweat_kg_s = sweat_rate / 3600.0; // Convert L/hr to kg/s
    double actual_evap = sweat_kg_s * max_evap;

    return actual_evap * config_.latent_heat;
  }

  /**
   * @brief Heat index calculation (feels-like temperature).
   * @param T Air temperature (°C)
   * @param RH Relative humidity (0-1)
   * @return Heat index (°C)
   */
  static double heat_index(double T, double RH) {
    // Rothfusz regression (°F converted to °C)
    double T_F = T * 9.0 / 5.0 + 32.0;
    double RH_pct = RH * 100.0;

    if (T_F < 80.0)
      return T; // Below threshold

    double HI = -42.379 + 2.04901523 * T_F + 10.14333127 * RH_pct -
                0.22475541 * T_F * RH_pct - 6.83783e-3 * T_F * T_F -
                5.481717e-2 * RH_pct * RH_pct +
                1.22874e-3 * T_F * T_F * RH_pct +
                8.5282e-4 * T_F * RH_pct * RH_pct -
                1.99e-6 * T_F * T_F * RH_pct * RH_pct;

    // Convert back to Celsius
    return (HI - 32.0) * 5.0 / 9.0;
  }

private:
  Config config_;
};

} // namespace thermal
} // namespace isolated
