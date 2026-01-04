#pragma once

/**
 * @file metabolism.hpp
 * @brief Metabolic system with energy balance and substrate utilization.
 */

#include <algorithm>
#include <cmath>

namespace isolated {
namespace biology {

/**
 * @brief Metabolic substrate type.
 */
enum class Substrate { GLUCOSE, FATTY_ACIDS, AMINO_ACIDS, GLYCOGEN, ATP };

/**
 * @brief Metabolic system with energy production and substrate tracking.
 */
class MetabolismSystem {
public:
  struct Config {
    double basal_metabolic_rate = 80.0; // Watts (~1700 kcal/day)
    double body_mass_kg = 70.0;
    double muscle_mass_fraction = 0.4;
  };

  struct State {
    double blood_glucose = 90.0;    // mg/dL
    double liver_glycogen = 100.0;  // grams
    double muscle_glycogen = 400.0; // grams
    double free_fatty_acids = 0.5;  // mmol/L
    double blood_lactate = 1.0;     // mmol/L
    double atp_rate = 80.0;         // Watts
    double respiratory_quotient = 0.85;
    double metabolic_rate = 1.0; // multiplier
  };

  explicit MetabolismSystem(const Config &config);

  State step(double dt, double activity_level = 1.0,
             double ambient_temp_c = 20.0);

  // Energy expenditure
  double compute_total_energy_expenditure(double activity_level) const;
  double compute_thermic_effect(double ambient_temp) const;

  // Substrate utilization
  void consume_glucose(double amount_mg);
  void consume_glycogen(double amount_g, bool muscle = true);
  void add_glucose(double amount_mg);

  const State &get_state() const { return state_; }

private:
  Config config_;
  State state_;

  double compute_substrate_mix(double intensity) const;
};

// ============================================================================
// INLINE IMPLEMENTATIONS
// ============================================================================

inline MetabolismSystem::MetabolismSystem(const Config &config)
    : config_(config) {
  state_.atp_rate = config_.basal_metabolic_rate;
}

inline double
MetabolismSystem::compute_total_energy_expenditure(double activity) const {
  // TEE = BMR × activity factor
  return config_.basal_metabolic_rate * activity;
}

inline double
MetabolismSystem::compute_thermic_effect(double ambient_temp) const {
  // Increased metabolism in cold
  double comfort_temp = 22.0;
  if (ambient_temp < comfort_temp) {
    return 1.0 + 0.05 * (comfort_temp - ambient_temp);
  }
  return 1.0;
}

inline double MetabolismSystem::compute_substrate_mix(double intensity) const {
  // Low intensity = more fat, high intensity = more carbs
  // Returns fraction of carbohydrate oxidation
  return std::clamp(0.3 + 0.5 * (intensity - 1.0), 0.0, 1.0);
}

inline void MetabolismSystem::consume_glucose(double amount_mg) {
  state_.blood_glucose =
      std::max(40.0, state_.blood_glucose - amount_mg / 50.0);
}

inline void MetabolismSystem::consume_glycogen(double amount_g, bool muscle) {
  if (muscle) {
    state_.muscle_glycogen = std::max(0.0, state_.muscle_glycogen - amount_g);
  } else {
    state_.liver_glycogen = std::max(0.0, state_.liver_glycogen - amount_g);
  }
}

inline void MetabolismSystem::add_glucose(double amount_mg) {
  state_.blood_glucose =
      std::min(300.0, state_.blood_glucose + amount_mg / 50.0);
}

inline MetabolismSystem::State
MetabolismSystem::step(double dt, double activity, double ambient_temp) {
  state_.metabolic_rate = activity * compute_thermic_effect(ambient_temp);
  double tee = compute_total_energy_expenditure(state_.metabolic_rate);
  state_.atp_rate = tee;

  // Substrate utilization
  double carb_fraction = compute_substrate_mix(activity);
  state_.respiratory_quotient = 0.7 + 0.3 * carb_fraction;

  // Energy in kJ per second
  double energy_kj = tee * dt / 1000.0;

  // Glucose consumption (4 kcal/g = 16.7 kJ/g)
  double glucose_used_g = energy_kj * carb_fraction / 16.7;
  double glucose_used_mg = glucose_used_g * 1000.0;

  // Try blood glucose first, then glycogen
  if (state_.blood_glucose > 70.0) {
    consume_glucose(glucose_used_mg * 0.3);
  }
  if (activity > 1.5 && state_.muscle_glycogen > 0) {
    consume_glycogen(glucose_used_g * 0.7, true);
  }

  // Glucose homeostasis - liver releases glucose
  if (state_.blood_glucose < 80.0 && state_.liver_glycogen > 0) {
    double release = std::min(state_.liver_glycogen * 0.01 * dt,
                              (80.0 - state_.blood_glucose) * 0.5);
    state_.liver_glycogen -= release;
    state_.blood_glucose += release * 20.0;
  }

  // Lactate production during high intensity
  if (activity > 1.8) {
    state_.blood_lactate += (activity - 1.8) * 0.5 * dt;
  }
  // Lactate clearance
  state_.blood_lactate = std::max(0.5, state_.blood_lactate - 0.1 * dt);

  return state_;
}

} // namespace biology
} // namespace isolated
