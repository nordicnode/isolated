#pragma once

/**
 * @file instabilities.hpp
 * @brief Fluid instability models.
 *
 * Features:
 * - Kelvin-Helmholtz instability at shear layers
 * - Rayleigh-Taylor instability at density interfaces
 * - Richardson number stability criterion
 */

#include <algorithm>
#include <cmath>
#include <vector>

namespace isolated {
namespace fluids {

// ============================================================================
// RICHARDSON NUMBER & STABILITY
// ============================================================================

/**
 * @brief Calculate gradient Richardson number.
 *
 * Ri = (g/ρ)(∂ρ/∂z) / (∂u/∂z)²
 *
 * Ri < 0.25: Kelvin-Helmholtz instability likely
 * Ri > 1.0: Stable stratification
 */
inline double richardson_number(double g, double rho, double drho_dz,
                                double du_dz) {
  if (std::abs(du_dz) < 1e-10) {
    return (drho_dz > 0) ? 1e10 : -1e10; // Stable or unstable limit
  }
  return (g / rho) * drho_dz / (du_dz * du_dz);
}

/**
 * @brief Kelvin-Helmholtz instability growth rate.
 *
 * For inviscid parallel flow with density jump:
 * σ = k * (ρ₁ρ₂)^0.5 / (ρ₁+ρ₂) * |Δu|
 *
 * where k is wavenumber.
 */
inline double kh_growth_rate(double k, double rho1, double rho2,
                             double delta_u) {
  double rho_sum = rho1 + rho2;
  if (rho_sum < 1e-10)
    return 0.0;
  return k * std::sqrt(rho1 * rho2) / rho_sum * std::abs(delta_u);
}

/**
 * @brief Critical wavenumber for KH instability with gravity.
 *
 * k_crit = g(ρ₁-ρ₂) / (ρ₁ρ₂ Δu²)
 *
 * Instability only for k > k_crit when heavier fluid is on top.
 */
inline double kh_critical_wavenumber(double g, double rho1, double rho2,
                                     double delta_u) {
  if (std::abs(delta_u) < 1e-10)
    return 1e10; // No instability
  return g * std::abs(rho1 - rho2) / (rho1 * rho2 * delta_u * delta_u);
}

// ============================================================================
// INTERFACE PERTURBATION MODEL
// ============================================================================

struct InterfacePoint {
  double x, y;        // Position
  double amplitude;   // Perturbation amplitude
  double phase;       // Phase of oscillation
  double growth_rate; // Local growth rate
};

/**
 * @brief Kelvin-Helmholtz instability model for a shear interface.
 */
class KHInstabilityModel {
public:
  struct Config {
    double wavelength = 1.0;         // Dominant wavelength (m)
    double initial_amplitude = 0.01; // Initial perturbation
    double viscosity = 1e-5;         // Kinematic viscosity (damping)
    double gravity = 9.81;
  };

  explicit KHInstabilityModel(const Config &config = Config{})
      : config_(config) {}

  /**
   * @brief Analyze interface stability between two layers.
   * @return True if unstable (KH waves will grow)
   */
  bool is_unstable(double rho_upper, double rho_lower, double u_upper,
                   double u_lower) const {
    double delta_u = u_upper - u_lower;
    double Ri =
        richardson_number(config_.gravity, (rho_upper + rho_lower) / 2.0,
                          (rho_lower - rho_upper) / config_.wavelength,
                          delta_u / config_.wavelength);
    return Ri < 0.25;
  }

  /**
   * @brief Calculate amplitude growth over time.
   */
  double evolve_amplitude(double amplitude, double dt, double rho1, double rho2,
                          double delta_u) const {
    double k = 2.0 * M_PI / config_.wavelength;
    double sigma = kh_growth_rate(k, rho1, rho2, delta_u);

    // Damping from viscosity
    double damping = 2.0 * config_.viscosity * k * k;
    double net_growth = sigma - damping;

    if (net_growth > 0) {
      return amplitude * std::exp(net_growth * dt);
    } else {
      return amplitude * std::exp(net_growth * dt); // Decay
    }
  }

  /**
   * @brief Add mixing enhancement to diffusion coefficient.
   *
   * KH mixing increases effective diffusivity when unstable.
   */
  double mixing_enhancement(double base_diffusivity, double rho1, double rho2,
                            double delta_u) const {
    double k = 2.0 * M_PI / config_.wavelength;
    double sigma = kh_growth_rate(k, rho1, rho2, delta_u);

    // Enhanced mixing proportional to growth rate
    double enhancement = 1.0 + sigma * config_.wavelength;
    return base_diffusivity * std::max(1.0, enhancement);
  }

private:
  Config config_;
};

// ============================================================================
// RAYLEIGH-TAYLOR INSTABILITY
// ============================================================================

/**
 * @brief Rayleigh-Taylor instability growth rate.
 *
 * For heavy fluid on top of light fluid:
 * σ = √(Atwood * g * k)
 *
 * where Atwood = (ρ_heavy - ρ_light)/(ρ_heavy + ρ_light)
 */
inline double rt_growth_rate(double g, double rho_heavy, double rho_light,
                             double wavenumber) {
  double atwood = (rho_heavy - rho_light) / (rho_heavy + rho_light);
  if (atwood <= 0)
    return 0.0; // Stable configuration
  return std::sqrt(atwood * g * wavenumber);
}

/**
 * @brief Rayleigh-Taylor instability model.
 */
class RTInstabilityModel {
public:
  struct Config {
    double gravity = 9.81;
    double surface_tension = 0.0; // N/m (stabilizing)
    double viscosity = 1e-5;
  };

  explicit RTInstabilityModel(const Config &config = Config{})
      : config_(config) {}

  /**
   * @brief Check if configuration is RT unstable.
   */
  bool is_unstable(double rho_upper, double rho_lower) const {
    return rho_upper > rho_lower; // Heavy on top
  }

  /**
   * @brief Most unstable wavelength considering surface tension.
   */
  double critical_wavelength(double rho_heavy, double rho_light) const {
    if (config_.surface_tension <= 0)
      return 0.0; // All wavelengths unstable
    double drho = rho_heavy - rho_light;
    if (drho <= 0)
      return 1e10; // Stable
    // λ_crit = 2π √(σ / (g Δρ))
    return 2.0 * M_PI *
           std::sqrt(config_.surface_tension / (config_.gravity * drho));
  }

  double growth_rate(double rho_heavy, double rho_light,
                     double wavelength) const {
    double k = 2.0 * M_PI / wavelength;
    return rt_growth_rate(config_.gravity, rho_heavy, rho_light, k);
  }

private:
  Config config_;
};

} // namespace fluids
} // namespace isolated
