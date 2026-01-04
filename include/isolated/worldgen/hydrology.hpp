#pragma once

/**
 * @file hydrology.hpp
 * @brief Underground hydrology system.
 *
 * Features:
 * - Aquifer modeling
 * - Water table dynamics
 * - Flooding mechanics
 * - Pressure-driven flow
 * - Erosion
 */

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

namespace isolated {
namespace worldgen {

// ============================================================================
// AQUIFER
// ============================================================================

struct Aquifer {
  double volume = 1e6;          // m³ water capacity
  double current_volume = 5e5;  // m³ current water
  double recharge_rate = 100.0; // m³/day from surface
  double porosity = 0.3;        // Fraction (0.1-0.4 typical)
  double permeability = 1e-12;  // m² (Darcy's law)

  size_t x, y, z;       // Location in world grid
  size_t extent_x = 50; // Size in cells
  size_t extent_y = 50;
  size_t extent_z = 10;

  double saturation() const { return current_volume / volume; }

  double pressure_head() const {
    // Pressure = ρgh
    return saturation() * extent_z * 10.0; // meters of head
  }
};

// ============================================================================
// WATER TABLE
// ============================================================================

struct WaterTable {
  std::vector<double> height; // Water table height at each (x,y)
  size_t nx, ny;
  double base_height = 0.0;

  void initialize(size_t width, size_t depth, double initial_height) {
    nx = width;
    ny = depth;
    height.resize(nx * ny, initial_height);
    base_height = initial_height;
  }

  double &at(size_t x, size_t y) { return height[x + nx * y]; }

  double at(size_t x, size_t y) const { return height[x + nx * y]; }
};

// ============================================================================
// FLOODING
// ============================================================================

struct FloodState {
  double water_level = 0.0;  // Current flood height (m)
  double inflow_rate = 0.0;  // m³/s water entering
  double outflow_rate = 0.0; // m³/s water draining
  double area = 100.0;       // m² floor area

  double volume() const { return water_level * area; }

  bool is_flooded() const { return water_level > 0.1; }
  bool is_dangerous() const { return water_level > 1.0; }
};

// ============================================================================
// HYDROLOGY SYSTEM
// ============================================================================

class HydrologySystem {
public:
  struct Config {
    double water_density = 1000.0; // kg/m³
    double gravity = 9.81;
    double drainage_coeff = 0.01; // Drainage rate coefficient
    double erosion_rate = 1e-6;   // m/s per unit flow
  };

  explicit HydrologySystem(const Config &config = Config{}) : config_(config) {}

  void add_aquifer(const Aquifer &aq) { aquifers_.push_back(aq); }

  void initialize_water_table(size_t nx, size_t ny, double height) {
    water_table_.initialize(nx, ny, height);
  }

  void add_flood_zone(const std::string &id, double area) {
    FloodState fs;
    fs.area = area;
    flood_zones_[id] = fs;
  }

  /**
   * @brief Update hydrology.
   * @param dt Time step (seconds)
   * @param rainfall Surface rainfall (mm/hr)
   */
  void step(double dt, double rainfall = 0.0) {
    update_aquifers(dt, rainfall);
    update_water_table(dt);
    update_flooding(dt);
  }

  // === Flood control ===

  void breach_aquifer(size_t aquifer_idx, double breach_area) {
    if (aquifer_idx >= aquifers_.size())
      return;

    auto &aq = aquifers_[aquifer_idx];
    double flow = calculate_darcy_flow(aq, breach_area);

    // Water flows into connected flood zones
    for (auto &[id, zone] : flood_zones_) {
      zone.inflow_rate += flow;
    }
  }

  void pump_water(const std::string &zone_id, double rate_m3s) {
    auto it = flood_zones_.find(zone_id);
    if (it != flood_zones_.end()) {
      it->second.outflow_rate += rate_m3s;
    }
  }

  // === Erosion ===

  double calculate_erosion(double flow_velocity, double rock_hardness) const {
    // Erosion rate proportional to velocity squared
    double erosion = config_.erosion_rate * flow_velocity * flow_velocity;
    erosion /= rock_hardness; // Harder rock erodes slower
    return erosion;
  }

  // === Accessors ===

  const std::vector<Aquifer> &aquifers() const { return aquifers_; }
  std::vector<Aquifer> &aquifers() { return aquifers_; }
  const WaterTable &water_table() const { return water_table_; }
  WaterTable &water_table() { return water_table_; }

  FloodState *get_flood_zone(const std::string &id) {
    auto it = flood_zones_.find(id);
    return it != flood_zones_.end() ? &it->second : nullptr;
  }

private:
  Config config_;
  std::vector<Aquifer> aquifers_;
  WaterTable water_table_;
  std::unordered_map<std::string, FloodState> flood_zones_;

  void update_aquifers(double dt, double rainfall) {
    for (auto &aq : aquifers_) {
      // Recharge from rainfall
      double recharge = rainfall / 1000.0 * aq.extent_x * aq.extent_y;
      aq.current_volume += recharge * dt / 3600.0;

      // Daily recharge
      aq.current_volume += aq.recharge_rate * dt / 86400.0;

      // Cap at capacity
      aq.current_volume = std::min(aq.current_volume, aq.volume);
    }
  }

  void update_water_table(double dt) {
    // Simple diffusion of water table height
    std::vector<double> new_height = water_table_.height;
    double diffusivity = 0.1; // m²/s

    for (size_t y = 1; y < water_table_.ny - 1; ++y) {
      for (size_t x = 1; x < water_table_.nx - 1; ++x) {
        double laplacian =
            water_table_.at(x + 1, y) + water_table_.at(x - 1, y) +
            water_table_.at(x, y + 1) + water_table_.at(x, y - 1) -
            4.0 * water_table_.at(x, y);
        new_height[x + water_table_.nx * y] += diffusivity * laplacian * dt;
      }
    }

    water_table_.height = new_height;
  }

  void update_flooding(double dt) {
    for (auto &[id, zone] : flood_zones_) {
      // Net flow
      double net_flow = zone.inflow_rate - zone.outflow_rate;
      double volume_change = net_flow * dt;

      // Update level
      double old_volume = zone.volume();
      double new_volume = std::max(0.0, old_volume + volume_change);
      zone.water_level = new_volume / zone.area;

      // Natural drainage
      if (zone.water_level > 0) {
        double drainage = config_.drainage_coeff * zone.water_level * dt;
        zone.water_level = std::max(0.0, zone.water_level - drainage);
      }

      // Reset flow rates for next step
      zone.inflow_rate = 0.0;
    }
  }

  double calculate_darcy_flow(const Aquifer &aq, double area) const {
    // Q = -K * A * dh/dl
    double K = aq.permeability * config_.water_density * config_.gravity / 1e-3;
    double gradient = aq.pressure_head() / 10.0; // Simplified
    return K * area * gradient;
  }
};

} // namespace worldgen
} // namespace isolated
