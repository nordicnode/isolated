/**
 * @file lbm_engine.cpp
 * @brief Optimized LBM fluid solver implementation.
 *
 * Optimizations:
 * - SIMD-friendly data layout
 * - Minimized function calls in hot loops
 * - Cache-optimized streaming
 * - Precomputed constants
 */

#include <algorithm>
#include <cmath>
#include <cstring>
#include <isolated/fluids/lbm_cuda.cuh>
#include <isolated/fluids/lbm_engine.hpp>

namespace isolated {
namespace fluids {

// Precomputed constants for hot loops
namespace {
constexpr double CS2 = 1.0 / 3.0;
constexpr double CS4 = CS2 * CS2;
constexpr double INV_CS2 = 3.0;
constexpr double INV_2CS4 = 4.5;

// Inlined lattice weights
alignas(64) constexpr double W[19] = {
    1.0 / 3.0,                                      // q=0
    1.0 / 18.0, 1.0 / 18.0, 1.0 / 18.0,             // q=1-3
    1.0 / 18.0, 1.0 / 18.0, 1.0 / 18.0,             // q=4-6
    1.0 / 36.0, 1.0 / 36.0, 1.0 / 36.0, 1.0 / 36.0, // q=7-10
    1.0 / 36.0, 1.0 / 36.0, 1.0 / 36.0, 1.0 / 36.0, // q=11-14
    1.0 / 36.0, 1.0 / 36.0, 1.0 / 36.0, 1.0 / 36.0  // q=15-18
};

// Inlined velocities (SoA for SIMD)
alignas(64) constexpr int CX[19] = {0,  1, -1, 0, 0,  0, 0, 1, -1, 1,
                                    -1, 1, -1, 1, -1, 0, 0, 0, 0};
alignas(64) constexpr int CY[19] = {0,  0, 0, 1, -1, 0, 0,  1, 1, -1,
                                    -1, 0, 0, 0, 0,  1, -1, 1, -1};
alignas(64) constexpr int CZ[19] = {0, 0, 0, 0,  0,  1, -1, 0,  0, 0,
                                    0, 1, 1, -1, -1, 1, 1,  -1, -1};

// Opposite directions
alignas(64) constexpr int OPP[19] = {0, 2,  1,  4,  3,  6,  5,  10, 9, 8,
                                     7, 14, 13, 12, 11, 18, 17, 16, 15};

// Precomputed c·c products for equilibrium
alignas(64) constexpr double CC[19] = {0, 1, 1, 1, 1, 1, 1, 2, 2, 2,
                                       2, 2, 2, 2, 2, 2, 2, 2, 2};
} // namespace

LBMEngine::LBMEngine(const LBMConfig &config) : config_(config) {
  n_cells_ = config_.nx * config_.ny * config_.nz;

  // Allocate aligned distribution functions for SIMD
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

  // MRT relaxation times
  tau_.resize(19, 1.0);
  tau_[0] = 1.0;
  tau_[1] = 1.1;
  tau_[2] = 1.1;
  tau_[3] = 1.0;
}

void LBMEngine::initialize_uniform(
    const std::unordered_map<std::string, double> &fractions) {
#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(n_cells_); ++i) {
    rho_[i] = 1.0;
    ux_[i] = uy_[i] = uz_[i] = 0.0;

    // Initialize to equilibrium (rho=1, u=0)
    for (int q = 0; q < 19; ++q) {
      f_[q][i] = W[q]; // feq = w_q * rho when u=0
    }
  }

  for (const auto &[name, frac] : fractions) {
    species_density_[name].resize(n_cells_, frac);
  }
}

void LBMEngine::add_species(const GasSpecies &species) {
  species_.push_back(species);
  species_density_[species.name].resize(n_cells_, 0.0);
}

void LBMEngine::step(double dt) {
  if (config_.use_gpu) {
    if (!gpu_buffers_.initialized) {
      gpu_buffers_.allocate(n_cells_);
      if (!gpu_constant_uploaded_) {
        cuda::upload_constants();
        gpu_constant_uploaded_ = true;
      }
      synchronize_to_device();
    }

    // Run full step on GPU
    // 1. Collide + Stream (f -> f_tmp -> f) - updates state
    // We assume tau[0] is main relaxation time for now since simple kernel
    double omega = 1.0 / tau_[0];
    cuda::launch_collide_stream(gpu_buffers_, omega, config_.nx, config_.ny,
                                config_.nz);

    // 2. Compute macroscopic (for stability check or next step)
    // Note: kernel_collide_stream already computes rho/u internally for
    // equilibrium, but if we want them available in global memory for
    // visualization/stability check:
    cuda::launch_compute_macroscopic(gpu_buffers_, config_.nx, config_.ny,
                                     config_.nz);

    return;
  }

  compute_macroscopic();

  if (config_.enable_les) {
    compute_turbulent_viscosity();
  }

  switch (config_.collision_mode) {
  case CollisionMode::BGK:
    collide_bgk();
    break;
  case CollisionMode::MRT:
  case CollisionMode::REGULARIZED:
    collide_mrt();
    break;
  }

  stream();
  apply_boundary_conditions();
}

void LBMEngine::compute_macroscopic() {
  const size_t nx = config_.nx;
  const size_t ny = config_.ny;

  // Get raw pointers for better vectorization
  const uint8_t *__restrict solid = solid_.data();
  double *__restrict rho = rho_.data();
  double *__restrict ux = ux_.data();
  double *__restrict uy = uy_.data();
  double *__restrict uz = uz_.data();

#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(n_cells_); ++i) {
    if (solid[i])
      continue;

    double r = 0.0, vx = 0.0, vy = 0.0, vz = 0.0;

// Unrolled loop with direct array access
    for (int q = 0; q < 19; ++q) {
      double fq = f_[q][i];
      r += fq;
      vx += CX[q] * fq;
      vy += CY[q] * fq;
      vz += CZ[q] * fq;
    }

    double inv_rho = 1.0 / (r + 1e-10);
    rho[i] = r;
    ux[i] = vx * inv_rho;
    uy[i] = vy * inv_rho;
    uz[i] = vz * inv_rho;
  }
}

void LBMEngine::collide_bgk() {
  const double omega = 1.0 / tau_[0];

  const uint8_t *__restrict solid = solid_.data();
  const double *__restrict rho = rho_.data();
  const double *__restrict vx = ux_.data();
  const double *__restrict vy = uy_.data();
  const double *__restrict vz = uz_.data();

#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(n_cells_); ++i) {
    if (solid[i])
      continue;

    const double r = rho[i];
    const double ux = vx[i], uy = vy[i], uz = vz[i];
    const double u2 = ux * ux + uy * uy + uz * uz;
    const double u2_term = 1.0 - 1.5 * u2;

    // Fully unrolled equilibrium + collision
    for (int q = 0; q < 19; ++q) {
      double cu = CX[q] * ux + CY[q] * uy + CZ[q] * uz;
      double f_eq = W[q] * r * (u2_term + 3.0 * cu + 4.5 * cu * cu);
      f_[q][i] += omega * (f_eq - f_[q][i]);
    }
  }
}

void LBMEngine::collide_mrt() {
  const uint8_t *__restrict solid = solid_.data();
  const double *__restrict rho_ptr = rho_.data();
  const double *__restrict vx = ux_.data();
  const double *__restrict vy = uy_.data();
  const double *__restrict vz = uz_.data();
  const double *__restrict nu_t = config_.enable_les ? nu_t_.data() : nullptr;

#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(n_cells_); ++i) {
    if (solid[i])
      continue;

    const double r = rho_ptr[i];
    const double ux = vx[i], uy = vy[i], uz = vz[i];
    const double u2 = ux * ux + uy * uy + uz * uz;
    const double u2_term = 1.0 - 1.5 * u2;

    // Effective viscosity
    double nu_eff = 0.1;
    if (nu_t)
      nu_eff += nu_t[i];
    const double omega_nu = 1.0 / (3.0 * nu_eff + 0.5);

    for (int q = 0; q < 19; ++q) {
      double cu = CX[q] * ux + CY[q] * uy + CZ[q] * uz;
      double f_eq = W[q] * r * (u2_term + 3.0 * cu + 4.5 * cu * cu);
      double omega = (q < 3) ? 1.0 : omega_nu;
      f_[q][i] += omega * (f_eq - f_[q][i]);
    }
  }
}

void LBMEngine::stream() {
  const size_t nx = config_.nx;
  const size_t ny = config_.ny;
  const size_t nz = config_.nz;

  // Optimized streaming with precomputed neighbor indices
#pragma omp parallel for collapse(2) schedule(static)
  for (int y = 0; y < static_cast<int>(ny); ++y) {
    for (int x = 0; x < static_cast<int>(nx); ++x) {
      for (int z = 0; z < static_cast<int>(nz); ++z) {
        const size_t i = static_cast<size_t>(x) + nx * (static_cast<size_t>(y) + ny * static_cast<size_t>(z));

        for (int q = 0; q < 19; ++q) {
          // Pull scheme: where did this distribution come from?
          size_t sx = (static_cast<size_t>(x) + nx - static_cast<size_t>(CX[q])) % nx;
          size_t sy = (static_cast<size_t>(y) + ny - static_cast<size_t>(CY[q])) % ny;
          size_t sz = (static_cast<size_t>(z) + nz - static_cast<size_t>(CZ[q])) % nz;
          size_t j = sx + nx * (sy + ny * sz);

          f_tmp_[q][i] = f_[q][j];
        }
      }
    }
  }

  // Pointer swap (O(1))
  std::swap(f_, f_tmp_);
}

void LBMEngine::apply_boundary_conditions() {
  const uint8_t *__restrict solid = solid_.data();

#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(n_cells_); ++i) {
    if (!solid[i])
      continue;

    // Bounce-back
    for (int q = 1; q < 19; ++q) {
      std::swap(f_[q][i], f_[OPP[q]][i]);
    }
  }
}

void LBMEngine::compute_turbulent_viscosity() {
  const double cs2 = config_.smagorinsky_cs * config_.smagorinsky_cs;
  const double dx2 = config_.dx * config_.dx;
  const double coeff = cs2 * dx2;

  const double *__restrict rho_ptr = rho_.data();
  const double *__restrict vx = ux_.data();
  const double *__restrict vy = uy_.data();
  const double *__restrict vz = uz_.data();
  double *__restrict nu_t = nu_t_.data();

#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(n_cells_); ++i) {
    const double r = rho_ptr[i];
    const double ux = vx[i], uy = vy[i], uz = vz[i];
    const double u2 = ux * ux + uy * uy + uz * uz;
    const double u2_term = 1.0 - 1.5 * u2;

    double S_mag_sq = 0.0;
    for (int q = 0; q < 19; ++q) {
      double cu = CX[q] * ux + CY[q] * uy + CZ[q] * uz;
      double f_eq = W[q] * r * (u2_term + 3.0 * cu + 4.5 * cu * cu);
      double f_neq = f_[q][i] - f_eq;
      S_mag_sq += f_neq * f_neq;
    }

    nu_t[i] = coeff * std::sqrt(S_mag_sq);
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
  for (int i = 0; i < static_cast<int>(n_cells_); ++i) {
    double u = std::sqrt(ux_[i] * ux_[i] + uy_[i] * uy_[i] + uz_[i] * uz_[i]);
    if (u > max_u) max_u = u;
  }
  return max_u * config_.dt / config_.dx;
}

bool LBMEngine::is_stable() const { return compute_cfl() < 0.5; }

void LBMEngine::synchronize_to_host() {
  if (config_.use_gpu && gpu_buffers_.initialized) {
    // We only sync macroscopic fields to CPU for now
    // If we needed distribution functions f_ back, we'd copy them too
    // But usually we only need density/velocity for other systems
    cuda::copy_from_device(gpu_buffers_, rho_, ux_, uy_, uz_);
  }
}

void LBMEngine::synchronize_to_device() {
  if (config_.use_gpu && gpu_buffers_.initialized) {
    cuda::copy_to_device(gpu_buffers_, rho_, ux_, uy_, uz_, solid_);
    // Note: copy_to_device also re-initializes f_ based on rho/u equilibrium
    // This is a simplification; for exact state restore we'd need f_ array
    // transfer
  }
}

void LBMEngine::wait() {
  if (config_.use_gpu) {
#ifdef __CUDACC__
    cudaDeviceSynchronize();
#else
    // If compiled with host compiler but linking against cuda lib, we need a
    // wrapper But LBMEngine.cpp is compiled as C++ (unless we changed it in
    // CMake?) Actually CMake GLOB_RECURSE includes .cu files now, but .cpp
    // files are still compiled with CXX compiler. cudaDeviceSynchronize is a
    // runtime API, available if we include <cuda_runtime.h>. LBMEngine.cpp
    // includes <isolated/fluids/lbm_cuda.cuh> which includes <cuda_runtime.h>
    // if __CUDACC__ is defined. If NOT __CUDACC__, we don't have it. We should
    // add a wrapper in lbm_cuda.cu exposed in .cuh
    cuda::device_synchronize();
#endif
  }
}

void LBMEngine::sample_at_positions(
    const std::vector<std::pair<size_t, size_t>> &positions,
    std::vector<cuda::FluidSample> &out) {
  if (!config_.use_gpu || !gpu_buffers_.initialized) {
    // CPU fallback: sample directly from CPU arrays
    out.resize(positions.size());
    for (size_t i = 0; i < positions.size(); ++i) {
      size_t idx = positions[i].first + config_.nx * positions[i].second;
      out[i].rho = rho_[idx];
      out[i].ux = ux_[idx];
      out[i].uy = uy_[idx];
      out[i].uz = uz_[idx];
    }
    return;
  }

  // GPU path: use gather kernel
  std::vector<size_t> indices(positions.size());
  for (size_t i = 0; i < positions.size(); ++i) {
    indices[i] = positions[i].first + config_.nx * positions[i].second;
  }

  cuda::gather_samples(gpu_buffers_, sparse_buffers_, indices, out, config_.nx,
                       config_.ny);
}

} // namespace fluids
} // namespace isolated
