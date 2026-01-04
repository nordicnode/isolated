#pragma once

/**
 * @file respiration.hpp
 * @brief Respiratory system with gas exchange and hemoglobin binding.
 */

#include <algorithm>
#include <cmath>

namespace isolated {
namespace biology {

/**
 * @brief Hemoglobin oxygen binding model with Hill equation.
 */
class HemoglobinModel {
public:
  struct Config {
    double p50_base = 26.6; // mmHg at pH 7.4
    double hill_coefficient = 2.7;
    double hemoglobin_normal = 14.0; // g/dL
  };

  explicit HemoglobinModel(const Config &config);

  double compute_saturation(double pO2_mmhg, double pH = 7.4,
                            double pCO2_mmhg = 40.0,
                            double temp_c = 37.0) const;

  double compute_oxygen_content(double pO2_mmhg, double hemoglobin_gdl,
                                double pH = 7.4) const;

  double get_p50() const { return p50_current_; }

private:
  Config config_;
  double p50_current_;

  void update_p50(double pH, double pCO2, double temp);
};

/**
 * @brief Respiratory system with alveolar gas exchange.
 */
class RespiratorySystem {
public:
  struct Config {
    double tidal_volume_ml = 500.0;
    double dead_space_ml = 150.0;
    double respiratory_rate = 12.0;
    double fio2 = 0.21;
    double atmospheric_pressure = 760.0;
  };

  struct State {
    double pao2 = 95.0;  // Arterial O2 (mmHg)
    double paco2 = 40.0; // Arterial CO2 (mmHg)
    double sao2 = 0.97;  // Arterial O2 saturation
    double minute_ventilation;
    double alveolar_ventilation;
    double vo2 = 250.0;  // O2 consumption (mL/min)
    double vco2 = 200.0; // CO2 production (mL/min)
  };

  explicit RespiratorySystem(const Config &config);

  State step(double dt, double ambient_po2, double ambient_pco2,
             double metabolic_rate = 1.0);

  void set_respiratory_rate(double rate) { config_.respiratory_rate = rate; }
  void set_tidal_volume(double vol) { config_.tidal_volume_ml = vol; }

  const State &get_state() const { return state_; }

private:
  Config config_;
  State state_;
  HemoglobinModel hemoglobin_;

  double alveolar_gas_equation(double fio2, double paco2) const;
};

// ============================================================================
// INLINE IMPLEMENTATIONS
// ============================================================================

inline HemoglobinModel::HemoglobinModel(const Config &config)
    : config_(config), p50_current_(config.p50_base) {}

inline double HemoglobinModel::compute_saturation(double pO2, double pH,
                                                  double pCO2,
                                                  double temp) const {
  // Bohr effect: shift P50 with pH and CO2
  double delta_pH = pH - 7.4;
  double delta_pCO2 = pCO2 - 40.0;
  double delta_temp = temp - 37.0;

  double p50 = config_.p50_base;
  p50 *= std::pow(10.0, -0.48 * delta_pH); // Bohr effect
  p50 *= std::pow(10.0, 0.0024 * delta_pCO2);
  p50 *= std::pow(10.0, 0.024 * delta_temp);

  // Hill equation
  double n = config_.hill_coefficient;
  double ratio = std::pow(pO2 / p50, n);
  return ratio / (1.0 + ratio);
}

inline double HemoglobinModel::compute_oxygen_content(double pO2, double hgb,
                                                      double pH) const {
  double sao2 = compute_saturation(pO2, pH);
  // CaO2 = (Hb × 1.34 × SaO2) + (0.003 × PaO2)
  return (hgb * 1.34 * sao2) + (0.003 * pO2);
}

inline RespiratorySystem::RespiratorySystem(const Config &config)
    : config_(config), hemoglobin_(HemoglobinModel::Config{}) {
  state_.minute_ventilation =
      config_.tidal_volume_ml * config_.respiratory_rate / 1000.0;
  state_.alveolar_ventilation =
      (config_.tidal_volume_ml - config_.dead_space_ml) *
      config_.respiratory_rate / 1000.0;
}

inline double RespiratorySystem::alveolar_gas_equation(double fio2,
                                                       double paco2) const {
  double pio2 = (config_.atmospheric_pressure - 47) * fio2;
  return pio2 - (paco2 / 0.8); // Simplified with RQ = 0.8
}

inline RespiratorySystem::State RespiratorySystem::step(double dt,
                                                        double ambient_po2,
                                                        double ambient_pco2,
                                                        double metabolic_rate) {
  // Update ventilation
  state_.minute_ventilation =
      config_.tidal_volume_ml * config_.respiratory_rate / 1000.0;
  state_.alveolar_ventilation =
      (config_.tidal_volume_ml - config_.dead_space_ml) *
      config_.respiratory_rate / 1000.0;

  // Metabolic demand
  state_.vo2 = 250.0 * metabolic_rate;
  state_.vco2 = 200.0 * metabolic_rate;

  // Alveolar gas equation
  double pao2_ideal = alveolar_gas_equation(config_.fio2, state_.paco2);

  // Simple equilibration toward ideal
  double tau = 5.0; // Time constant
  state_.pao2 += (pao2_ideal - state_.pao2) * dt / tau;

  // CO2 clearance proportional to alveolar ventilation
  double paco2_target = 40.0 * (state_.vco2 / 200.0) /
                        std::max(0.1, state_.alveolar_ventilation / 4.2);
  state_.paco2 += (paco2_target - state_.paco2) * dt / tau;

  // Clamp values
  state_.pao2 = std::clamp(state_.pao2, 20.0, 150.0);
  state_.paco2 = std::clamp(state_.paco2, 15.0, 80.0);

  // Update saturation
  state_.sao2 = hemoglobin_.compute_saturation(state_.pao2);

  return state_;
}

} // namespace biology
} // namespace isolated
