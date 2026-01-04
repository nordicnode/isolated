#pragma once

/**
 * @file blood_chemistry.hpp
 * @brief Blood chemistry with Henderson-Hasselbalch pH model.
 */

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>

namespace isolated {
namespace biology {

/**
 * @brief Acid-base disorder classification.
 */
enum class AcidBaseDisorder {
  NORMAL = 0,
  METABOLIC_ACIDOSIS = 1,
  METABOLIC_ALKALOSIS = 2,
  RESPIRATORY_ACIDOSIS = 3,
  RESPIRATORY_ALKALOSIS = 4,
  MIXED = 5
};

/**
 * @brief Arterial blood gas values.
 */
struct BloodGasValues {
  double pH = 7.40;
  double pCO2 = 40.0;       // mmHg
  double pO2 = 95.0;        // mmHg
  double HCO3 = 24.0;       // mEq/L
  double base_excess = 0.0; // mEq/L
  double lactate = 1.0;     // mmol/L
  double sao2 = 0.97;       // fraction
};

/**
 * @brief Serum electrolyte panel.
 */
struct ElectrolytePanel {
  double sodium = 140.0;     // mEq/L
  double potassium = 4.0;    // mEq/L
  double chloride = 102.0;   // mEq/L
  double bicarbonate = 24.0; // mEq/L
  double calcium = 9.5;      // mg/dL
  double magnesium = 2.0;    // mg/dL
  double phosphate = 3.5;    // mg/dL

  double anion_gap() const { return sodium - (chloride + bicarbonate); }
};

/**
 * @brief Blood chemistry simulation system.
 */
class BloodChemistrySystem {
public:
  BloodChemistrySystem() = default;

  // Step result
  struct StepResult {
    double pH;
    double pCO2;
    double HCO3;
    double base_excess;
    double lactate;
    double anion_gap;
    double potassium;
    std::string disorder;
  };

  StepResult step(double dt);

  // pH calculation
  double compute_ph();
  double compute_base_excess();
  AcidBaseDisorder classify_disorder() const;

  // Lactate dynamics
  void add_lactate(double amount_mmol);
  void clear_lactate(double dt);

  // Cardiac risk factors
  std::unordered_map<std::string, double> get_cardiac_risk_factors() const;

  // Public state
  BloodGasValues abg;
  ElectrolytePanel electrolytes;
  double lactate = 1.0;

private:
  static constexpr double pKa_ = 6.1; // Carbonic acid
  double lactate_clearance_rate_ = 0.5;
};

// === Inline implementations ===

inline double BloodChemistrySystem::compute_ph() {
  double hco3 = std::max(1.0, electrolytes.bicarbonate);
  double pco2 = std::max(1.0, abg.pCO2);

  double ph = pKa_ + std::log10(hco3 / (0.03 * pco2));
  abg.pH = std::clamp(ph, 6.8, 7.8);
  return abg.pH;
}

inline double BloodChemistrySystem::compute_base_excess() {
  double be = electrolytes.bicarbonate - 24.4 + 16.2 * (abg.pH - 7.4);
  abg.base_excess = be;
  return be;
}

inline void BloodChemistrySystem::add_lactate(double amount_mmol) {
  lactate += amount_mmol;
  double hco3_consumed = std::min(amount_mmol, electrolytes.bicarbonate - 5.0);
  electrolytes.bicarbonate -= hco3_consumed;
}

inline void BloodChemistrySystem::clear_lactate(double dt) {
  double clearance = lactate * lactate_clearance_rate_ * dt / 60.0;
  lactate = std::max(0.5, lactate - clearance);
  electrolytes.bicarbonate =
      std::min(30.0, electrolytes.bicarbonate + clearance * 0.5);
}

} // namespace biology
} // namespace isolated
