/**
 * @file multiphase.cpp
 * @brief Implementation of multi-phase physics systems.
 */

#include <algorithm>
#include <cmath>
#include <isolated/fluids/multiphase.hpp>

namespace isolated {
namespace fluids {

// ============================================================================
// PHASE CHANGE SYSTEM
// ============================================================================

PhaseChangeSystem::PhaseChangeSystem(size_t nx, size_t ny, const Config &config)
    : nx_(nx), ny_(ny), n_cells_(nx * ny), config_(config) {
  liquid_water_.resize(n_cells_, 0.0);
  vapor_change_.resize(n_cells_, 0.0);
  rh_.resize(n_cells_, 0.0);
}

PhaseChangeSystem::StepResult
PhaseChangeSystem::step(double dt, const std::vector<double> &h2o_density,
                        const std::vector<double> &temperature) {
  StepResult result{0.0, 0.0, 0.0};

  for (size_t i = 0; i < n_cells_; ++i) {
    vapor_change_[i] = 0.0;

    double temp_c = temperature[i] - 273.15;
    double p_sat = saturation_pressure(temp_c);

    // Approximate vapor pressure from density (ideal gas)
    double p_vapor = h2o_density[i] * 461.5 * temperature[i];
    double rh = p_vapor / std::max(p_sat, 1e-10);
    rh_[i] = rh * 100.0; // Store as percentage

    result.max_rh = std::max(result.max_rh, rh_[i]);

    if (rh > config_.rh_threshold) {
      // Condensation
      double excess = (rh - config_.rh_threshold) * p_sat;
      double condensed = excess * config_.condensation_rate * dt;
      vapor_change_[i] = -condensed;
      liquid_water_[i] += condensed;
      result.total_condensed += condensed;
    } else if (liquid_water_[i] > 0 && rh < config_.rh_threshold) {
      // Evaporation
      double deficit = (config_.rh_threshold - rh) * p_sat;
      double evaporated =
          std::min(liquid_water_[i], deficit * config_.evaporation_rate * dt);
      vapor_change_[i] = evaporated;
      liquid_water_[i] -= evaporated;
      result.total_evaporated += evaporated;
    }
  }

  return result;
}

// ============================================================================
// AEROSOL SYSTEM
// ============================================================================

AerosolSystem::AerosolSystem(size_t nx, size_t ny, const Config &config)
    : nx_(nx), ny_(ny), config_(config) {
  particles_.reserve(config_.max_particles);
}

void AerosolSystem::spawn_particles(float x, float y, size_t count,
                                    ParticleType type) {
  std::normal_distribution<float> pos_dist(0.0f, 0.5f);
  std::normal_distribution<float> vel_dist(0.0f, 0.1f);

  for (size_t i = 0; i < count && particles_.size() < config_.max_particles;
       ++i) {
    Particle p;
    p.x = x + pos_dist(rng_);
    p.y = y + pos_dist(rng_);
    p.vx = vel_dist(rng_);
    p.vy = vel_dist(rng_);
    p.type = type;

    switch (type) {
    case ParticleType::SMOKE:
      p.mass = 1e-9f;
      p.lifetime = 60.0f;
      break;
    case ParticleType::DUST:
      p.mass = 1e-8f;
      p.lifetime = 120.0f;
      break;
    case ParticleType::ASH:
      p.mass = 1e-7f;
      p.lifetime = 30.0f;
      break;
    case ParticleType::DROPLET:
      p.mass = 1e-6f;
      p.lifetime = 10.0f;
      break;
    }

    particles_.push_back(p);
  }
}

void AerosolSystem::step(double dt, const std::vector<double> &fluid_ux,
                         const std::vector<double> &fluid_uy,
                         const std::vector<uint8_t> &solid) {
  std::normal_distribution<float> brownian(
      0.0f,
      static_cast<float>(std::sqrt(2.0 * config_.brownian_diffusion * dt)));

  for (auto it = particles_.begin(); it != particles_.end();) {
    Particle &p = *it;

    // Decrease lifetime
    p.lifetime -= static_cast<float>(dt);

    // Check removal conditions
    if (p.lifetime <= 0 || p.x < 0 || p.x >= nx_ || p.y < 0 || p.y >= ny_) {
      it = particles_.erase(it);
      continue;
    }

    // Get fluid velocity at particle position
    size_t ix = static_cast<size_t>(p.x);
    size_t iy = static_cast<size_t>(p.y);
    size_t i = idx(ix, iy);

    // Check solid
    if (!solid.empty() && solid[i]) {
      it = particles_.erase(it);
      continue;
    }

    // Advect with fluid
    float ux = static_cast<float>(fluid_ux[i]);
    float uy = static_cast<float>(fluid_uy[i]);

    // Apply gravity (settling)
    float settling = static_cast<float>(config_.gravity * p.mass /
                                        (6.0 * M_PI * 1.8e-5 * 1e-6) * dt);

    // Update velocity with drag
    p.vx = 0.9f * p.vx + 0.1f * ux;
    p.vy = 0.9f * p.vy + 0.1f * uy - settling;

    // Add Brownian motion
    p.vx += brownian(rng_);
    p.vy += brownian(rng_);

    // Update position
    p.x += p.vx * static_cast<float>(dt);
    p.y += p.vy * static_cast<float>(dt);

    ++it;
  }
}

// ============================================================================
// COMBUSTION SYSTEM
// ============================================================================

CombustionSystem::CombustionSystem(size_t nx, size_t ny, const Config &config)
    : nx_(nx), ny_(ny), n_cells_(nx * ny), config_(config) {
  fuel_.resize(n_cells_, 0.0);
  burning_.resize(n_cells_, false);
  heat_output_.resize(n_cells_, 0.0);
  o2_change_.resize(n_cells_, 0.0);
  co2_change_.resize(n_cells_, 0.0);
}

void CombustionSystem::add_fuel(size_t x, size_t y, double amount_kg) {
  fuel_[idx(x, y)] += amount_kg;
}

void CombustionSystem::ignite(size_t x, size_t y) {
  if (fuel_[idx(x, y)] > 0) {
    burning_[idx(x, y)] = true;
  }
}

CombustionSystem::StepResult
CombustionSystem::step(double dt, const std::vector<double> &o2_density,
                       const std::vector<double> &temperature,
                       AerosolSystem *aerosol) {
  StepResult result{0, 0.0, 0.0, 0.0};

  std::fill(heat_output_.begin(), heat_output_.end(), 0.0);
  std::fill(o2_change_.begin(), o2_change_.end(), 0.0);
  std::fill(co2_change_.begin(), co2_change_.end(), 0.0);

  for (size_t y = 0; y < ny_; ++y) {
    for (size_t x = 0; x < nx_; ++x) {
      size_t i = idx(x, y);

      // Check ignition conditions
      if (!burning_[i] && fuel_[i] > 0 &&
          temperature[i] >= config_.ignition_temp && o2_density[i] > 0.15) {
        burning_[i] = true;
      }

      if (!burning_[i] || fuel_[i] <= 0)
        continue;

      // Check O2 availability
      if (o2_density[i] < 0.10) {
        burning_[i] = false; // Extinguish
        continue;
      }

      // Burn fuel
      double fuel_burned = std::min(fuel_[i], config_.burn_rate * dt);
      fuel_[i] -= fuel_burned;

      // Outputs
      heat_output_[i] = fuel_burned * config_.heat_release_rate;
      o2_change_[i] = -fuel_burned * config_.o2_consumption_rate;
      co2_change_[i] = fuel_burned * config_.co2_production_rate;

      result.burning_cells++;
      result.total_heat += heat_output_[i];
      result.o2_consumed += -o2_change_[i];
      result.co2_produced += co2_change_[i];

      // Generate smoke
      if (aerosol) {
        size_t smoke_count =
            static_cast<size_t>(fuel_burned * config_.smoke_rate);
        if (smoke_count > 0) {
          std::uniform_real_distribution<float> pos_jitter(-0.5f, 0.5f);
          aerosol->spawn_particles(x + pos_jitter(rng_), y + pos_jitter(rng_),
                                   smoke_count, ParticleType::SMOKE);
        }
      }

      // Extinguish if no fuel left
      if (fuel_[i] <= 0) {
        burning_[i] = false;
      }
    }
  }

  return result;
}

} // namespace fluids
} // namespace isolated
