#pragma once

/**
 * @file ventilation.hpp
 * @brief Advanced ventilation mechanics with V/Q mismatch and respiratory work.
 */

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace isolated {
namespace biology {

/**
 * @brief Lung zone classification (West zones).
 */
enum class LungZone : uint8_t {
  ZONE_1 = 1, // PA > Pa > Pv (apex, minimal perfusion)
  ZONE_2 = 2, // Pa > PA > Pv (mid lung)
  ZONE_3 = 3  // Pa > Pv > PA (base, maximal perfusion)
};

/**
 * @brief Ventilation support mode.
 */
enum class VentilationMode : uint8_t {
  SPONTANEOUS = 0,
  CPAP = 1,       // Continuous positive airway pressure
  BIPAP = 2,      // Bi-level positive airway pressure
  MECHANICAL = 3, // Full mechanical ventilation
  HIGH_FLOW = 4   // High-flow nasal cannula
};

/**
 * @brief Advanced ventilation system with V/Q matching.
 */
class VentilationSystem {
public:
  struct Config {
    double functional_residual_capacity = 2400.0; // mL
    double vital_capacity = 4800.0;               // mL
    double compliance = 100.0;                    // mL/cmH2O
    double airway_resistance = 2.0;               // cmH2O/L/s
  };

  struct State {
    // Ventilation parameters
    double tidal_volume = 500.0;       // mL
    double respiratory_rate = 12.0;    // breaths/min
    double minute_ventilation = 6.0;   // L/min
    double alveolar_ventilation = 4.2; // L/min

    // Dead space
    double anatomic_dead_space = 150.0; // mL
    double physiologic_dead_space = 180.0;
    double dead_space_fraction = 0.30; // Vd/Vt ratio

    // V/Q matching (5-compartment model)
    double vq_ratio_mean = 0.8;
    double shunt_fraction = 0.02; // Qs/Qt
    double dead_space_ventilation = 0.0;

    // Work of breathing
    double work_of_breathing = 0.5; // J/L
    double respiratory_muscle_fatigue = 0.0;
    double oxygen_cost_of_breathing = 2.0; // mL O2/min

    // Positive pressure effects
    double peep = 0.0; // cmH2O
    double pip = 0.0;  // Peak inspiratory pressure
    double mean_airway_pressure = 0.0;
    double intrinsic_peep = 0.0; // Auto-PEEP

    // Gas exchange efficiency
    double aa_gradient = 10.0; // A-a gradient (mmHg)
    double pf_ratio = 400.0;   // PaO2/FiO2
  };

  explicit VentilationSystem(const Config &config);

  State step(double dt, double pao2, double paco2, double fio2 = 0.21);

  // V/Q matching
  double compute_vq_mismatch(double cardiac_output, double alveolar_vent) const;
  double compute_shunt(double mixed_venous_o2, double arterial_o2) const;
  double compute_aa_gradient(double pao2, double paco2, double fio2) const;

  // Work of breathing
  double compute_elastic_work(double tidal_vol, double compliance) const;
  double compute_resistive_work(double flow, double resistance) const;
  void accumulate_fatigue(double dt, double work);

  // Positive pressure ventilation
  void apply_ventilation_support(VentilationMode mode, double pressure);
  double compute_peep_effect(double peep) const;

  const State &get_state() const { return state_; }
  const Config &get_config() const { return config_; }

private:
  Config config_;
  State state_;
  VentilationMode current_mode_ = VentilationMode::SPONTANEOUS;
  double support_pressure_ = 0.0;

  void update_dead_space(double paco2);
  void update_vq_matching(double cardiac_output);
  void update_work_of_breathing();
};

// ============================================================================
// INLINE IMPLEMENTATIONS
// ============================================================================

inline VentilationSystem::VentilationSystem(const Config &config)
    : config_(config) {}

inline double VentilationSystem::compute_vq_mismatch(double co,
                                                     double va) const {
  // Normal V/Q ratio ~0.8 (4.2L alveolar vent / 5L cardiac output)
  if (co < 0.1)
    return 10.0; // Dead space
  double vq = va / co;

  // Deviation from ideal
  double ideal_vq = 0.8;
  return std::abs(vq - ideal_vq) / ideal_vq;
}

inline double VentilationSystem::compute_shunt(double svo2, double sao2) const {
  // Berggren shunt equation (simplified)
  // Qs/Qt = (CcO2 - CaO2) / (CcO2 - CvO2)
  double cco2 = 1.0; // Ideal pulmonary capillary O2 content (normalized)
  double cao2 = sao2;
  double cvo2 = svo2;

  if (std::abs(cco2 - cvo2) < 0.01)
    return 0.0;
  return std::clamp((cco2 - cao2) / (cco2 - cvo2), 0.0, 1.0);
}

inline double VentilationSystem::compute_aa_gradient(double pao2, double paco2,
                                                     double fio2) const {
  // Alveolar gas equation: PAO2 = FiO2 × (Patm - PH2O) - PaCO2/RQ
  double patm = 760.0;
  double ph2o = 47.0;
  double rq = 0.8;

  double pao2_ideal = fio2 * (patm - ph2o) - paco2 / rq;
  return std::max(0.0, pao2_ideal - pao2);
}

inline double VentilationSystem::compute_elastic_work(double vt,
                                                      double compliance) const {
  // Elastic work = 0.5 × Vt² / Compliance
  return 0.5 * (vt / 1000.0) * (vt / 1000.0) / (compliance / 1000.0);
}

inline double
VentilationSystem::compute_resistive_work(double flow,
                                          double resistance) const {
  // Resistive work = Resistance × Flow² × Ti
  double ti = 1.5; // Inspiratory time (s)
  return resistance * flow * flow * ti;
}

inline void VentilationSystem::accumulate_fatigue(double dt, double work) {
  // Fatigue accumulates when work exceeds sustainable threshold
  double sustainable_work = 0.8; // J/L

  if (work > sustainable_work) {
    state_.respiratory_muscle_fatigue += (work - sustainable_work) * 0.01 * dt;
  } else {
    state_.respiratory_muscle_fatigue -= 0.005 * dt; // Recovery
  }

  state_.respiratory_muscle_fatigue =
      std::clamp(state_.respiratory_muscle_fatigue, 0.0, 1.0);
}

inline void VentilationSystem::apply_ventilation_support(VentilationMode mode,
                                                         double pressure) {
  current_mode_ = mode;
  support_pressure_ = pressure;

  switch (mode) {
  case VentilationMode::CPAP:
    state_.peep = pressure;
    break;
  case VentilationMode::BIPAP:
    state_.peep = pressure * 0.4; // EPAP
    state_.pip = pressure;        // IPAP
    break;
  case VentilationMode::MECHANICAL:
    state_.peep = 5.0;
    state_.pip = pressure;
    break;
  case VentilationMode::HIGH_FLOW:
    // HFNC provides small amount of PEEP
    state_.peep = std::min(5.0, pressure / 10.0);
    break;
  default:
    state_.peep = 0.0;
    state_.pip = 0.0;
    break;
  }

  state_.mean_airway_pressure = state_.peep + (state_.pip - state_.peep) / 3.0;
}

inline double VentilationSystem::compute_peep_effect(double peep) const {
  // PEEP improves FRC and reduces shunt
  double frc_increase = peep * 20.0; // ~20mL per cmH2O
  double shunt_reduction = std::min(0.1, peep * 0.005);
  return shunt_reduction;
}

inline void VentilationSystem::update_dead_space(double paco2) {
  // Bohr equation for physiologic dead space
  // Vd/Vt = (PaCO2 - PECO2) / PaCO2
  double peco2 = paco2 * 0.7; // Expired CO2 (estimated)

  if (paco2 > 0) {
    state_.dead_space_fraction = (paco2 - peco2) / paco2;
  }

  state_.physiologic_dead_space =
      state_.tidal_volume * state_.dead_space_fraction;
  state_.alveolar_ventilation =
      (state_.tidal_volume - state_.physiologic_dead_space) *
      state_.respiratory_rate / 1000.0;
}

inline void VentilationSystem::update_vq_matching(double cardiac_output) {
  // 5-compartment lung model (simplified)
  state_.vq_ratio_mean =
      state_.alveolar_ventilation / std::max(0.1, cardiac_output);

  // Shunt increases with atelectasis, decreases with PEEP
  double base_shunt = 0.02;
  double peep_effect = compute_peep_effect(state_.peep);
  state_.shunt_fraction = std::max(0.0, base_shunt - peep_effect);
}

inline void VentilationSystem::update_work_of_breathing() {
  // Total work = elastic + resistive
  double flow = state_.tidal_volume / 2.0 / 1000.0; // L/s (assuming Ti=2s)

  double elastic =
      compute_elastic_work(state_.tidal_volume, config_.compliance);
  double resistive = compute_resistive_work(flow, config_.airway_resistance);

  state_.work_of_breathing = elastic + resistive;

  // Oxygen cost of breathing (~1-2% of total VO2 normally)
  state_.oxygen_cost_of_breathing =
      state_.work_of_breathing * state_.respiratory_rate * 0.5;

  // Mechanical ventilation reduces WOB
  if (current_mode_ == VentilationMode::MECHANICAL) {
    state_.work_of_breathing *= 0.1;
    state_.oxygen_cost_of_breathing *= 0.1;
  } else if (current_mode_ == VentilationMode::BIPAP) {
    state_.work_of_breathing *= 0.5;
    state_.oxygen_cost_of_breathing *= 0.5;
  }
}

inline VentilationSystem::State
VentilationSystem::step(double dt, double pao2, double paco2, double fio2) {
  // Update minute ventilation
  state_.minute_ventilation =
      state_.tidal_volume * state_.respiratory_rate / 1000.0;

  // Dead space calculations
  update_dead_space(paco2);

  // V/Q matching (assume CO ~5 L/min for now)
  update_vq_matching(5.0);

  // Work of breathing
  update_work_of_breathing();
  accumulate_fatigue(dt, state_.work_of_breathing);

  // Gas exchange efficiency
  state_.aa_gradient = compute_aa_gradient(pao2, paco2, fio2);
  state_.pf_ratio = pao2 / fio2;

  // Respiratory fatigue affects rate and depth
  if (state_.respiratory_muscle_fatigue > 0.5) {
    state_.tidal_volume *= (1.0 - state_.respiratory_muscle_fatigue * 0.3);
    state_.respiratory_rate *= (1.0 + state_.respiratory_muscle_fatigue * 0.2);
  }

  // Auto-PEEP from incomplete exhalation
  if (state_.respiratory_rate > 25) {
    state_.intrinsic_peep = (state_.respiratory_rate - 25) * 0.2;
  } else {
    state_.intrinsic_peep = 0.0;
  }

  return state_;
}

} // namespace biology
} // namespace isolated
