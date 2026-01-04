#pragma once

/**
 * @file multiphase.hpp
 * @brief Multi-phase physics: condensation, aerosols, and combustion.
 */

#include <array>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace isolated {
namespace fluids {

// ============================================================================
// PHASE CHANGE SYSTEM (Condensation/Evaporation)
// ============================================================================

/**
 * @brief Water vapor phase change system using Magnus-Tetens equation.
 */
class PhaseChangeSystem {
public:
  struct Config {
    double condensation_rate = 0.01;
    double evaporation_rate = 0.005;
    double rh_threshold = 0.95;
  };

  struct StepResult {
    double max_rh;
    double total_condensed;
    double total_evaporated;
  };

  PhaseChangeSystem(size_t nx, size_t ny, const Config &config);

  StepResult step(double dt, const std::vector<double> &h2o_density,
                  const std::vector<double> &temperature);

  const std::vector<double> &get_vapor_change() const { return vapor_change_; }
  const std::vector<double> &get_liquid_water() const { return liquid_water_; }
  const std::vector<double> &get_relative_humidity() const { return rh_; }

private:
  size_t nx_, ny_, n_cells_;
  Config config_;

  std::vector<double> liquid_water_;
  std::vector<double> vapor_change_;
  std::vector<double> rh_;

  // Magnus-Tetens saturation vapor pressure
  static double saturation_pressure(double temp_c) {
    return 6.1094 * std::exp((17.625 * temp_c) / (temp_c + 243.04));
  }
};

// ============================================================================
// AEROSOL SYSTEM (Particle Transport)
// ============================================================================

enum class ParticleType : uint8_t { SMOKE = 0, DUST = 1, ASH = 2, DROPLET = 3 };

struct Particle {
  float x, y;
  float vx = 0, vy = 0;
  float mass = 1e-9f;     // kg
  float lifetime = 60.0f; // seconds
  ParticleType type = ParticleType::SMOKE;
};

/**
 * @brief Lagrangian aerosol particle system.
 */
class AerosolSystem {
public:
  struct Config {
    double gravity = 9.81;
    double drag_coefficient = 0.47;
    double brownian_diffusion = 1e-5;
    size_t max_particles = 10000;
  };

  AerosolSystem(size_t nx, size_t ny, const Config &config);

  void spawn_particles(float x, float y, size_t count = 10,
                       ParticleType type = ParticleType::SMOKE);

  void step(double dt, const std::vector<double> &fluid_ux,
            const std::vector<double> &fluid_uy,
            const std::vector<uint8_t> &solid);

  const std::vector<Particle> &get_particles() const { return particles_; }
  size_t particle_count() const { return particles_.size(); }

private:
  size_t nx_, ny_;
  Config config_;
  std::vector<Particle> particles_;
  std::mt19937 rng_{42};

  size_t idx(size_t x, size_t y) const { return x + nx_ * y; }
};

// ============================================================================
// COMBUSTION SYSTEM (Fire Model)
// ============================================================================

/**
 * @brief Simple combustion model with fuel, O2 consumption, and heat release.
 */
class CombustionSystem {
public:
  struct Config {
    double ignition_temp = 573.0;      // K (~300°C)
    double heat_release_rate = 1e6;    // J/kg fuel
    double o2_consumption_rate = 1.2;  // kg O2 per kg fuel
    double co2_production_rate = 2.75; // kg CO2 per kg fuel
    double smoke_rate = 100.0;         // particles per kg fuel
    double burn_rate = 0.01;           // kg fuel per second
  };

  struct StepResult {
    size_t burning_cells;
    double total_heat;
    double o2_consumed;
    double co2_produced;
  };

  CombustionSystem(size_t nx, size_t ny, const Config &config);

  void add_fuel(size_t x, size_t y, double amount_kg);
  void ignite(size_t x, size_t y);

  StepResult step(double dt, const std::vector<double> &o2_density,
                  const std::vector<double> &temperature,
                  AerosolSystem *aerosol = nullptr);

  const std::vector<double> &get_heat_output() const { return heat_output_; }
  const std::vector<double> &get_fuel() const { return fuel_; }
  const std::vector<bool> &get_burning() const { return burning_; }

private:
  size_t nx_, ny_, n_cells_;
  Config config_;

  std::vector<double> fuel_;
  std::vector<bool> burning_;
  std::vector<double> heat_output_;
  std::vector<double> o2_change_;
  std::vector<double> co2_change_;

  std::mt19937 rng_{42};

  size_t idx(size_t x, size_t y) const { return x + nx_ * y; }
};

} // namespace fluids
} // namespace isolated
