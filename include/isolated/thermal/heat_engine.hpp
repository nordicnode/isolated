#pragma once

/**
 * @file heat_engine.hpp
 * @brief Coupled thermal simulation engine.
 *
 * Features:
 * - 3D finite difference conduction
 * - Stefan-Boltzmann radiation
 * - Velocity-dependent convection
 * - Enthalpy-based phase changes
 */

#include <cstdint>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include <isolated/core/constants.hpp>
#include <isolated/thermal/materials.hpp>

namespace isolated {
namespace thermal {

/**
 * @brief Phase state enumeration.
 */
enum class Phase : uint8_t {
  SOLID = 0,
  MELTING = 1,
  LIQUID = 2,
  BOILING = 3,
  GAS = 4
};

/**
 * @brief Thermal engine configuration.
 */
struct ThermalConfig {
  size_t nx = 100;
  size_t ny = 100;
  size_t nz = 1;
  double dx = 1.0;
  bool enable_radiation = true;
  double radiation_threshold = 500.0; // K - only radiate above this
};

/**
 * @brief Coupled thermal simulation engine.
 */
class ThermalEngine {
public:
  explicit ThermalEngine(const ThermalConfig &config);
  ~ThermalEngine() = default;

  // Simulation step
  void step(double dt);

  // Material setup
  void set_material(size_t x, size_t y, size_t z,
                    const std::string &material_name);

  // Temperature access
  void set_temperature(size_t x, size_t y, size_t z, double temp_k);
  double get_temperature(size_t x, size_t y, size_t z) const;

  // Heat sources
  void add_heat_source(size_t x, size_t y, size_t z, double watts);
  void add_equipment(const std::string &id, size_t x, size_t y, size_t z,
                     double watts);
  void register_entity_heat(const std::string &id, size_t x, size_t y, size_t z,
                            double watts);
  void set_radioactive_ore(size_t x, size_t y, size_t z, double watts_per_m3);
  
  /**
   * @brief Injects a transient pulse of energy into a cell.
   * Used for biological heat, explosions, etc.
   * @param x Grid X
   * @param y Grid Y
   * @param z Grid Z
   * @param joules Energy added in Joules (Watts * dt)
   */
  void inject_heat(size_t x, size_t y, size_t z, double joules);

  // Fluid coupling
  void set_fluid_velocity(size_t x, size_t y, size_t z, double ux, double uy);

private:
  ThermalConfig config_;
  size_t n_cells_;

  // Temperature and enthalpy fields
  std::vector<double> temperature_;
  std::vector<double> enthalpy_;
  std::vector<Phase> phase_;

  // Material properties per cell
  std::vector<uint8_t> material_id_;
  std::vector<std::string> material_list_;
  std::vector<double> k_;   // Thermal conductivity
  std::vector<double> cp_;  // Specific heat
  std::vector<double> rho_; // Density
  std::vector<double> emissivity_;

  // Heat sources
  std::vector<double> heat_sources_;
  std::vector<double> decay_heat_;

  // Hash functor for coordinate tuples (defined as proper struct to avoid ODR
  // issues)
  struct CoordHash {
    size_t
    operator()(const std::tuple<size_t, size_t, size_t> &t) const noexcept {
      return std::get<0>(t) ^ (std::get<1>(t) << 16) ^ (std::get<2>(t) << 32);
    }
  };
  std::unordered_map<std::tuple<size_t, size_t, size_t>, double, CoordHash>
      equipment_heat_;

  // Fluid velocity for convection
  std::vector<double> fluid_ux_, fluid_uy_;

  // Reusable temp buffers (avoid heap allocation in hot loops)
  std::vector<double> temp_buffer_;
  std::vector<double> temp_buffer2_;

  // Internal methods
  size_t idx(size_t x, size_t y, size_t z) const;
  void step_conduction(double dt);
  void step_radiation(double dt);
  void step_advection(double dt);
  void step_sources(double dt);
  void step_phase_change(double dt);
  void apply_decay_heat(double dt);
};

// === Inline implementations ===

inline size_t ThermalEngine::idx(size_t x, size_t y, size_t z) const {
  return x + config_.nx * (y + config_.ny * z);
}

} // namespace thermal
} // namespace isolated
