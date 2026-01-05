/**
 * @file heat_engine.cpp
 * @brief Implementation of the coupled thermal engine.
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <isolated/thermal/heat_engine.hpp>
#include <omp.h>

namespace isolated {
namespace thermal {

ThermalEngine::ThermalEngine(const ThermalConfig &config) : config_(config) {
  n_cells_ = config_.nx * config_.ny * config_.nz;

  // Allocate fields
  temperature_.resize(n_cells_, constants::ROOM_TEMP);
  enthalpy_.resize(n_cells_, 0.0);
  phase_.resize(n_cells_, Phase::GAS);

  // Default to air
  material_id_.resize(n_cells_, 0);
  material_list_.push_back("air");

  const auto &air = MATERIALS.at("air");
  k_.resize(n_cells_, air.thermal_conductivity);
  cp_.resize(n_cells_, air.specific_heat);
  rho_.resize(n_cells_, air.density);
  emissivity_.resize(n_cells_, air.emissivity);

  // Heat sources
  heat_sources_.resize(n_cells_, 0.0);
  decay_heat_.resize(n_cells_, 0.0);

  // Fluid velocity
  fluid_ux_.resize(n_cells_, 0.0);
  fluid_uy_.resize(n_cells_, 0.0);

  // Preallocate temp buffers (avoid heap allocation in hot loops)
  temp_buffer_.resize(n_cells_, 0.0);
  temp_buffer2_.resize(n_cells_, 0.0);
}

void ThermalEngine::step(double dt) {
  step_conduction(dt);
  step_advection(dt);
  step_sources(dt);
  apply_decay_heat(dt);

  if (config_.enable_radiation) {
    step_radiation(dt);
  }

  step_phase_change(dt);
}

void ThermalEngine::step_conduction(double dt) {
  // Reuse preallocated buffer (zero it with memset - faster than std::fill)
  std::memset(temp_buffer_.data(), 0, n_cells_ * sizeof(double));
  double *__restrict dT = temp_buffer_.data();
  const double dx2 = config_.dx * config_.dx;

#pragma omp parallel for collapse(3)
  for (int z = 0; z < static_cast<int>(config_.nz); ++z) {
    for (int y = 1; y < static_cast<int>(config_.ny) - 1; ++y) {
      for (int x = 1; x < static_cast<int>(config_.nx) - 1; ++x) {
        size_t i = idx(static_cast<size_t>(x), static_cast<size_t>(y), static_cast<size_t>(z));

        double alpha = k_[i] / (rho_[i] * cp_[i]);

        // 6-point stencil (3D Laplacian)
        double laplacian =
            (temperature_[idx(static_cast<size_t>(x + 1), static_cast<size_t>(y), static_cast<size_t>(z))] + temperature_[idx(static_cast<size_t>(x - 1), static_cast<size_t>(y), static_cast<size_t>(z))] +
             temperature_[idx(static_cast<size_t>(x), static_cast<size_t>(y + 1), static_cast<size_t>(z))] + temperature_[idx(static_cast<size_t>(x), static_cast<size_t>(y - 1), static_cast<size_t>(z))] -
             4.0 * temperature_[i]) /
            dx2;

        // Add z-direction if 3D
        if (config_.nz > 1 && z > 0 && z < config_.nz - 1) {
          laplacian +=
              (temperature_[idx(static_cast<size_t>(x), static_cast<size_t>(y), static_cast<size_t>(z + 1))] + temperature_[idx(static_cast<size_t>(x), static_cast<size_t>(y), static_cast<size_t>(z - 1))] -
               2.0 * temperature_[i]) /
              dx2;
        }

        dT[i] = alpha * laplacian * dt;
      }
    }
  }

// Apply temperature changes
#pragma omp parallel for
  for (int i = 0; i < static_cast<int>(n_cells_); ++i) {
    temperature_[i] += dT[i];
  }
}

void ThermalEngine::step_radiation(double dt) {
  const double sigma = constants::STEFAN_BOLTZMANN;
  // Reuse preallocated buffer
  std::memset(temp_buffer_.data(), 0, n_cells_ * sizeof(double));
  double *__restrict dT = temp_buffer_.data();

#pragma omp parallel for
  for (int i = 0; i < static_cast<int>(n_cells_); ++i) {
    double T = temperature_[i];
    if (T < config_.radiation_threshold)
      continue;

    double eps = emissivity_[i];
    if (eps < 0.01)
      continue;

    // Net radiation to surroundings (simplified)
    double T_ambient = constants::ROOM_TEMP;
    double q_rad = sigma * eps * (std::pow(T, 4) - std::pow(T_ambient, 4));

    // Temperature change
    double rho_cp = rho_[i] * cp_[i];
    if (rho_cp > 0) {
      dT[i] = -q_rad * dt / (rho_cp * config_.dx);
    }
  }

#pragma omp parallel for
  for (int i = 0; i < static_cast<int>(n_cells_); ++i) {
    temperature_[i] += dT[i];
  }
}

void ThermalEngine::step_advection(double dt) {
  // Reuse preallocated buffer
  std::memset(temp_buffer_.data(), 0, n_cells_ * sizeof(double));
  double *__restrict dT = temp_buffer_.data();

#pragma omp parallel for collapse(3)
  for (int z = 0; z < static_cast<int>(config_.nz); ++z) {
    for (int y = 1; y < static_cast<int>(config_.ny) - 1; ++y) {
      for (int x = 1; x < static_cast<int>(config_.nx) - 1; ++x) {
        size_t i = idx(static_cast<size_t>(x), static_cast<size_t>(y), static_cast<size_t>(z));

        double ux = fluid_ux_[i];
        double uy = fluid_uy_[i];

        // Upwind advection
        double dTdx = (ux > 0)
                          ? (temperature_[i] - temperature_[idx(static_cast<size_t>(x - 1), static_cast<size_t>(y), static_cast<size_t>(z))]) /
                                config_.dx
                          : (temperature_[idx(static_cast<size_t>(x + 1), static_cast<size_t>(y), static_cast<size_t>(z))] - temperature_[i]) /
                                config_.dx;
        double dTdy = (uy > 0)
                          ? (temperature_[i] - temperature_[idx(static_cast<size_t>(x), static_cast<size_t>(y - 1), static_cast<size_t>(z))]) /
                                config_.dx
                          : (temperature_[idx(static_cast<size_t>(x), static_cast<size_t>(y + 1), static_cast<size_t>(z))] - temperature_[i]) /
                                config_.dx;

        dT[i] = -(ux * dTdx + uy * dTdy) * dt;
      }
    }
  }

#pragma omp parallel for
  for (int i = 0; i < static_cast<int>(n_cells_); ++i) {
    temperature_[i] += dT[i];
  }
}

void ThermalEngine::step_sources(double dt) {
#pragma omp parallel for
  for (int i = 0; i < static_cast<int>(n_cells_); ++i) {
    if (heat_sources_[i] != 0.0) {
      double rho_cp = rho_[i] * cp_[i];
      if (rho_cp > 0) {
        temperature_[i] += heat_sources_[i] * dt / rho_cp;
      }
    }
  }
}

void ThermalEngine::apply_decay_heat(double dt) {
#pragma omp parallel for
  for (int i = 0; i < static_cast<int>(n_cells_); ++i) {
    if (decay_heat_[i] > 0.0) {
      double rho_cp = rho_[i] * cp_[i];
      if (rho_cp > 0) {
        temperature_[i] += decay_heat_[i] * dt / rho_cp;
      }
    }
  }
}

void ThermalEngine::step_phase_change(double dt) {
#pragma omp parallel for
  for (int i = 0; i < static_cast<int>(n_cells_); ++i) {
    const auto &mat_name = material_list_[material_id_[i]];
    auto it = MATERIALS.find(mat_name);
    if (it == MATERIALS.end())
      continue;

    const auto &props = it->second;
    double T = temperature_[i];
    Phase p = phase_[i];

    // Melting
    if (props.melting_point > 0 && props.latent_heat_fusion > 0) {
      double Tm = props.melting_point;
      double Lf = props.latent_heat_fusion;

      if (p == Phase::SOLID && T >= Tm) {
        // Start melting - absorb latent heat
        enthalpy_[i] += rho_[i] * cp_[i] * (T - Tm);
        temperature_[i] = Tm;
        phase_[i] = Phase::MELTING;
      }

      if (p == Phase::MELTING && enthalpy_[i] >= Lf * rho_[i]) {
        // Fully melted
        enthalpy_[i] = 0;
        phase_[i] = Phase::LIQUID;
      }
    }

    // Boiling (similar logic)
    if (props.boiling_point > 0 && props.latent_heat_vaporization > 0) {
      double Tb = props.boiling_point;
      double Lv = props.latent_heat_vaporization;

      if (p == Phase::LIQUID && T >= Tb) {
        enthalpy_[i] += rho_[i] * cp_[i] * (T - Tb);
        temperature_[i] = Tb;
        phase_[i] = Phase::BOILING;
      }

      if (p == Phase::BOILING && enthalpy_[i] >= Lv * rho_[i]) {
        enthalpy_[i] = 0;
        phase_[i] = Phase::GAS;
      }
    }
  }
}

void ThermalEngine::set_material(size_t x, size_t y, size_t z,
                                 const std::string &material_name) {
  auto it =
      std::find(material_list_.begin(), material_list_.end(), material_name);
  uint8_t mat_id;

  if (it == material_list_.end()) {
    mat_id = static_cast<uint8_t>(material_list_.size());
    material_list_.push_back(material_name);
  } else {
    mat_id = static_cast<uint8_t>(std::distance(material_list_.begin(), it));
  }

  size_t i = idx(x, y, z);
  material_id_[i] = mat_id;

  // Update properties
  auto mat_it = MATERIALS.find(material_name);
  if (mat_it != MATERIALS.end()) {
    const auto &props = mat_it->second;
    k_[i] = props.thermal_conductivity;
    cp_[i] = props.specific_heat;
    rho_[i] = props.density;
    emissivity_[i] = props.emissivity;
  }
}

void ThermalEngine::set_temperature(size_t x, size_t y, size_t z,
                                    double temp_k) {
  temperature_[idx(x, y, z)] = temp_k;
}

double ThermalEngine::get_temperature(size_t x, size_t y, size_t z) const {
  return temperature_[idx(x, y, z)];
}

void ThermalEngine::add_heat_source(size_t x, size_t y, size_t z,
                                    double watts) {
  double volume = config_.dx * config_.dx * config_.dx;
  heat_sources_[idx(x, y, z)] += watts / volume;
}

void ThermalEngine::add_equipment(const std::string &id, size_t x, size_t y,
                                  size_t z, double watts) {
  add_heat_source(x, y, z, watts);
  equipment_heat_[{x, y, z}] = watts;
}

void ThermalEngine::register_entity_heat(const std::string &id, size_t x,
                                         size_t y, size_t z, double watts) {
  add_heat_source(x, y, z, watts);
}

void ThermalEngine::set_radioactive_ore(size_t x, size_t y, size_t z,
                                        double watts_per_m3) {
  decay_heat_[idx(x, y, z)] = watts_per_m3;
}

void ThermalEngine::set_fluid_velocity(size_t x, size_t y, size_t z, double ux,
                                       double uy) {
  size_t i = idx(x, y, z);
  fluid_ux_[i] = ux;
  fluid_uy_[i] = uy;
}

} // namespace thermal
} // namespace isolated
