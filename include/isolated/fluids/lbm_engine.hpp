#pragma once

/**
 * @file lbm_engine.hpp
 * @brief High-performance Lattice Boltzmann Method fluid solver.
 *
 * Features:
 * - D3Q19 lattice with MRT collision
 * - LES turbulence (Smagorinsky)
 * - Multi-species gas support
 * - SIMD-optimized operations
 */

#include <array>
#include <cmath>
#include <omp.h>
#include <string>
#include <unordered_map>
#include <vector>

#include <isolated/core/constants.hpp>
#include <isolated/fluids/lattice.hpp>

namespace isolated {
namespace fluids {

/**
 * @brief Gas species properties.
 */
struct GasSpecies {
  std::string name;
  double molecular_weight; // g/mol
  double viscosity;        // Pa·s at STP
  double diffusivity;      // m²/s
};

/**
 * @brief Collision operator type.
 */
enum class CollisionMode {
  BGK,        // Single relaxation time
  MRT,        // Multiple relaxation time
  REGULARIZED // Regularized for stability
};

/**
 * @brief LBM configuration.
 */
struct LBMConfig {
  size_t nx = 100;
  size_t ny = 100;
  size_t nz = 1;
  bool use_3d = false;
  bool enable_les = false;
  double smagorinsky_cs = 0.1;
  CollisionMode collision_mode = CollisionMode::MRT;
  double gravity = 0.0;
  double dx = 1.0;
  double dt = 1.0;
};

/**
 * @brief High-performance LBM fluid solver.
 */
class LBMEngine {
public:
  explicit LBMEngine(const LBMConfig &config);
  ~LBMEngine() = default;

  // Initialization
  void
  initialize_uniform(const std::unordered_map<std::string, double> &fractions);
  void add_species(const GasSpecies &species);

  // Simulation step
  void step(double dt);

  // Boundary conditions
  void set_solid(size_t x, size_t y, size_t z, bool is_solid);

  // Accessors
  double get_density(size_t x, size_t y, size_t z) const;
  std::array<double, 3> get_velocity(size_t x, size_t y, size_t z) const;
  double get_species_density(const std::string &name, size_t x, size_t y,
                             size_t z) const;

  // Stability
  double compute_cfl() const;
  bool is_stable() const;

private:
  LBMConfig config_;
  size_t n_cells_;

  // Distribution functions (SoA layout for cache efficiency)
  std::array<std::vector<double>, 19> f_;     // Current distributions
  std::array<std::vector<double>, 19> f_tmp_; // Temporary for streaming

  // Macroscopic fields
  std::vector<double> rho_;
  std::vector<double> ux_, uy_, uz_;

  // Material flags
  std::vector<uint8_t> solid_;

  // Species data
  std::vector<GasSpecies> species_;
  std::unordered_map<std::string, std::vector<double>> species_density_;

  // Relaxation parameters
  std::vector<double> tau_;  // Relaxation times (MRT)
  std::vector<double> nu_t_; // Turbulent viscosity (LES)

  // Internal methods
  size_t idx(size_t x, size_t y, size_t z) const;
  void compute_macroscopic();
  void collide_bgk();
  void collide_mrt();
  void stream();
  void apply_boundary_conditions();
  double compute_equilibrium(int q, double rho, double ux, double uy,
                             double uz) const;
  void compute_turbulent_viscosity();
};

// === Inline implementations ===

inline size_t LBMEngine::idx(size_t x, size_t y, size_t z) const {
  return x + config_.nx * (y + config_.ny * z);
}

inline double LBMEngine::compute_equilibrium(int q, double rho, double ux,
                                             double uy, double uz) const {
  const auto &c = D3Q19::c[q];
  double cu = c[0] * ux + c[1] * uy + c[2] * uz;
  double u2 = ux * ux + uy * uy + uz * uz;
  return D3Q19::w[q] * rho *
         (1.0 + cu / constants::CS2 +
          0.5 * cu * cu / (constants::CS2 * constants::CS2) -
          0.5 * u2 / constants::CS2);
}

} // namespace fluids
} // namespace isolated
