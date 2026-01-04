#pragma once

/**
 * @file thermoregulation.hpp
 * @brief Human thermoregulation with heat balance.
 */

#include <algorithm>
#include <cmath>

namespace isolated {
namespace biology {

/**
 * @brief Thermoregulation system with heat balance.
 */
class ThermoregulationSystem {
public:
  struct State {
    double core_temp_c = 37.0;
    double skin_temp_c = 33.0;
    double metabolic_heat_w = 80.0;
    double shivering_intensity = 0.0;
    double sweat_rate_ml_hr = 0.0;
    double vasodilation = 0.5; // 0-1
    double fluid_loss_ml = 0.0;
  };

  struct Environment {
    double ambient_temp_c = 20.0;
    double humidity = 0.5;
    double wind_speed_ms = 0.0;
    double radiation_wm2 = 0.0;
  };

  ThermoregulationSystem() = default;

  State step(double dt, const Environment &env, double metabolic_rate = 1.0);

  double compute_heat_loss(const Environment &env) const;
  double compute_sweat_rate(double core_temp) const;
  double compute_shivering(double core_temp) const;

  const State &get_state() const { return state_; }

private:
  State state_;

  double convective_loss(double skin_temp, double ambient, double wind) const;
  double evaporative_loss(double sweat_rate, double humidity) const;
  double radiative_loss(double skin_temp, double ambient) const;
};

// ============================================================================
// INLINE IMPLEMENTATIONS
// ============================================================================

inline double ThermoregulationSystem::convective_loss(double skin,
                                                      double ambient,
                                                      double wind) const {
  double h_c = 8.3 * std::sqrt(std::max(0.1, wind)); // Convection coefficient
  return h_c * 1.8 * (skin - ambient); // Watts (1.8 m² body surface)
}

inline double ThermoregulationSystem::evaporative_loss(double sweat,
                                                       double humidity) const {
  double max_evap = (1.0 - humidity) * 600.0;   // Max evaporative capacity
  double actual_evap = sweat / 1000.0 * 2430.0; // Latent heat of sweat
  return std::min(actual_evap, max_evap);
}

inline double ThermoregulationSystem::radiative_loss(double skin,
                                                     double ambient) const {
  double sigma = 5.67e-8;
  double emissivity = 0.97;
  double area = 1.8; // m²
  double t_skin_k = skin + 273.15;
  double t_amb_k = ambient + 273.15;
  return sigma * emissivity * area *
         (std::pow(t_skin_k, 4) - std::pow(t_amb_k, 4));
}

inline double
ThermoregulationSystem::compute_heat_loss(const Environment &env) const {
  double q_conv = convective_loss(state_.skin_temp_c, env.ambient_temp_c,
                                  env.wind_speed_ms);
  double q_evap = evaporative_loss(state_.sweat_rate_ml_hr, env.humidity);
  double q_rad = radiative_loss(state_.skin_temp_c, env.ambient_temp_c);
  return q_conv + q_evap + q_rad;
}

inline double
ThermoregulationSystem::compute_sweat_rate(double core_temp) const {
  if (core_temp > 37.0) {
    return std::min(2000.0, (core_temp - 37.0) * 500.0);
  }
  return 0.0;
}

inline double
ThermoregulationSystem::compute_shivering(double core_temp) const {
  if (core_temp < 36.5) {
    return std::min(1.0, (36.5 - core_temp) * 2.0);
  }
  return 0.0;
}

inline ThermoregulationSystem::State
ThermoregulationSystem::step(double dt, const Environment &env,
                             double metabolic_rate) {

  // Metabolic heat production
  state_.metabolic_heat_w = 80.0 * metabolic_rate;

  // Thermoregulatory responses
  state_.shivering_intensity = compute_shivering(state_.core_temp_c);
  state_.sweat_rate_ml_hr = compute_sweat_rate(state_.core_temp_c);

  // Add shivering heat
  double shivering_heat = state_.shivering_intensity * 300.0; // Up to 300W
  double total_heat_gen = state_.metabolic_heat_w + shivering_heat;

  // Heat loss
  double heat_loss = compute_heat_loss(env);

  // Net heat balance
  double net_heat = total_heat_gen - heat_loss;

  // Update core temperature (simplified thermal mass)
  double thermal_mass = 250000.0; // J/°C (body mass × specific heat)
  state_.core_temp_c += net_heat * dt / thermal_mass;

  // Skin temperature tracks toward mean of core and ambient
  double target_skin = 0.7 * state_.core_temp_c + 0.3 * env.ambient_temp_c;
  state_.skin_temp_c += (target_skin - state_.skin_temp_c) * 0.1 * dt;

  // Vasodilation response
  if (state_.core_temp_c > 37.5) {
    state_.vasodilation = std::min(1.0, state_.vasodilation + 0.1 * dt);
  } else if (state_.core_temp_c < 36.5) {
    state_.vasodilation = std::max(0.0, state_.vasodilation - 0.1 * dt);
  }

  // Fluid loss from sweating
  state_.fluid_loss_ml += state_.sweat_rate_ml_hr * dt / 3600.0;

  // Clamp temperatures
  state_.core_temp_c = std::clamp(state_.core_temp_c, 25.0, 43.0);
  state_.skin_temp_c = std::clamp(state_.skin_temp_c, 10.0, 42.0);

  return state_;
}

} // namespace biology
} // namespace isolated
