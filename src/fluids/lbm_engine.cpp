/**
 * @file lbm_engine.cpp
 * @brief Implementation of the LBM fluid solver.
 */

#include <algorithm>
#include <cmath>
#include <isolated/fluids/lbm_engine.hpp>

namespace isolated {
namespace fluids {

LBMEngine::LBMEngine(const LBMConfig &config) : config_(config) {
  n_cells_ = config_.nx * config_.ny * config_.nz;

  // Allocate distribution functions
  for (int q = 0; q < 19; ++q) {
    f_[q].resize(n_cells_, 0.0);
    f_tmp_[q].resize(n_cells_, 0.0);
  }

  // Allocate macroscopic fields
  rho_.resize(n_cells_, 1.0);
  ux_.resize(n_cells_, 0.0);
  uy_.resize(n_cells_, 0.0);
  uz_.resize(n_cells_, 0.0);

  // Material flags
  solid_.resize(n_cells_, 0);

  // Turbulent viscosity
  if (config_.enable_les) {
    nu_t_.resize(n_cells_, 0.0);
  }

  // MRT relaxation times (diagonal S matrix)
  tau_.resize(19, 1.0);
  // Set specific relaxation rates for MRT
  // s_e, s_eps, s_q, s_nu, etc.
  tau_[0] = 1.0; // density conserved
  tau_[1] = 1.1; // energy
  tau_[2] = 1.1; // energy square
  tau_[3] = 1.0; // momentum conserved
                 // ... remaining set to viscosity-based values
}

void LBMEngine::initialize_uniform(
    const std::unordered_map<std::string, double> &fractions) {
#pragma omp parallel for
  for (size_t i = 0; i < n_cells_; ++i) {
    rho_[i] = 1.0;
    ux_[i] = uy_[i] = uz_[i] = 0.0;

    // Initialize to equilibrium
    for (int q = 0; q < 19; ++q) {
      f_[q][i] = compute_equilibrium(q, 1.0, 0.0, 0.0, 0.0);
    }
  }

  // Initialize species densities
  for (const auto &[name, frac] : fractions) {
    species_density_[name].resize(n_cells_, frac);
  }
}

void LBMEngine::add_species(const GasSpecies &species) {
  species_.push_back(species);
  species_density_[species.name].resize(n_cells_, 0.0);
}

void LBMEngine::step(double dt) {
  // 1. Compute macroscopic quantities
  compute_macroscopic();

  // 2. Turbulence model (if enabled)
  if (config_.enable_les) {
    compute_turbulent_viscosity();
  }

  // 3. Collision step
  switch (config_.collision_mode) {
  case CollisionMode::BGK:
    collide_bgk();
    break;
  case CollisionMode::MRT:
    collide_mrt();
    break;
  case CollisionMode::REGULARIZED:
    collide_mrt(); // Fallback to MRT
    break;
  }

  // 4. Streaming step
  stream();

  // 5. Boundary conditions
  apply_boundary_conditions();
}

void LBMEngine::compute_macroscopic() {
#pragma omp parallel for
  for (size_t i = 0; i < n_cells_; ++i) {
    if (solid_[i])
      continue;

    double rho = 0.0;
    double ux = 0.0, uy = 0.0, uz = 0.0;

    for (int q = 0; q < 19; ++q) {
      rho += f_[q][i];
      ux += D3Q19::c[q][0] * f_[q][i];
      uy += D3Q19::c[q][1] * f_[q][i];
      uz += D3Q19::c[q][2] * f_[q][i];
    }

    rho_[i] = rho;
    double inv_rho = 1.0 / std::max(rho, 1e-10);
    ux_[i] = ux * inv_rho;
    uy_[i] = uy * inv_rho;
    uz_[i] = uz * inv_rho;
  }
}

void LBMEngine::collide_bgk() {
  const double omega = 1.0 / tau_[0];

#pragma omp parallel for
  for (size_t i = 0; i < n_cells_; ++i) {
    if (solid_[i])
      continue;

    double rho = rho_[i];
    double ux = ux_[i], uy = uy_[i], uz = uz_[i];

    for (int q = 0; q < 19; ++q) {
      double f_eq = compute_equilibrium(q, rho, ux, uy, uz);
      f_[q][i] += omega * (f_eq - f_[q][i]);
    }
  }
}

void LBMEngine::collide_mrt() {
// MRT collision in moment space
// Simplified: using diagonal S matrix
#pragma omp parallel for
  for (size_t i = 0; i < n_cells_; ++i) {
    if (solid_[i])
      continue;

    double rho = rho_[i];
    double ux = ux_[i], uy = uy_[i], uz = uz_[i];

    // Get effective viscosity (base + turbulent)
    double nu_eff = 0.1; // Base kinematic viscosity
    if (config_.enable_les && !nu_t_.empty()) {
      nu_eff += nu_t_[i];
    }
    double omega_nu = 1.0 / (3.0 * nu_eff + 0.5);

    for (int q = 0; q < 19; ++q) {
      double f_eq = compute_equilibrium(q, rho, ux, uy, uz);
      // Simplified MRT: relax non-conserved moments faster
      double omega = (q < 3) ? 1.0 : omega_nu;
      f_[q][i] += omega * (f_eq - f_[q][i]);
    }
  }
}

void LBMEngine::stream() {
// Copy to temporary buffer with periodic boundaries
#pragma omp parallel for collapse(3)
  for (size_t z = 0; z < config_.nz; ++z) {
    for (size_t y = 0; y < config_.ny; ++y) {
      for (size_t x = 0; x < config_.nx; ++x) {
        size_t i = idx(x, y, z);

        for (int q = 0; q < 19; ++q) {
          // Source position (with periodic wrap)
          int sx = (x - D3Q19::c[q][0] + config_.nx) % config_.nx;
          int sy = (y - D3Q19::c[q][1] + config_.ny) % config_.ny;
          int sz = (z - D3Q19::c[q][2] + config_.nz) % config_.nz;

          f_tmp_[q][i] = f_[q][idx(sx, sy, sz)];
        }
      }
    }
  }

  // Swap buffers
  std::swap(f_, f_tmp_);
}

void LBMEngine::apply_boundary_conditions() {
// Bounce-back for solid cells
#pragma omp parallel for
  for (size_t i = 0; i < n_cells_; ++i) {
    if (!solid_[i])
      continue;

    for (int q = 1; q < 19; ++q) {
      int q_opp = D3Q19::opposite[q];
      std::swap(f_[q][i], f_[q_opp][i]);
    }
  }
}

void LBMEngine::compute_turbulent_viscosity() {
  const double cs = config_.smagorinsky_cs;
  const double dx = config_.dx;

#pragma omp parallel for
  for (size_t i = 0; i < n_cells_; ++i) {
    // Compute strain rate magnitude from non-equilibrium stress
    double S_mag = 0.0;

    double rho = rho_[i];
    double ux = ux_[i], uy = uy_[i], uz = uz_[i];

    for (int q = 0; q < 19; ++q) {
      double f_neq = f_[q][i] - compute_equilibrium(q, rho, ux, uy, uz);
      S_mag += f_neq * f_neq;
    }
    S_mag = std::sqrt(S_mag);

    // Smagorinsky model: nu_t = (Cs * dx)^2 * |S|
    nu_t_[i] = cs * cs * dx * dx * S_mag;
  }
}

void LBMEngine::set_solid(size_t x, size_t y, size_t z, bool is_solid) {
  solid_[idx(x, y, z)] = is_solid ? 1 : 0;
}

double LBMEngine::get_density(size_t x, size_t y, size_t z) const {
  return rho_[idx(x, y, z)];
}

std::array<double, 3> LBMEngine::get_velocity(size_t x, size_t y,
                                              size_t z) const {
  size_t i = idx(x, y, z);
  return {ux_[i], uy_[i], uz_[i]};
}

double LBMEngine::get_species_density(const std::string &name, size_t x,
                                      size_t y, size_t z) const {
  auto it = species_density_.find(name);
  if (it != species_density_.end()) {
    return it->second[idx(x, y, z)];
  }
  return 0.0;
}

double LBMEngine::compute_cfl() const {
  double max_u = 0.0;
  for (size_t i = 0; i < n_cells_; ++i) {
    double u = std::sqrt(ux_[i] * ux_[i] + uy_[i] * uy_[i] + uz_[i] * uz_[i]);
    max_u = std::max(max_u, u);
  }
  return max_u * config_.dt / config_.dx;
}

bool LBMEngine::is_stable() const {
  return compute_cfl() < 0.5; // CFL should be < 0.5 for LBM
}

} // namespace fluids
} // namespace isolated
