/**
 * @file blood_chemistry.cpp
 * @brief Implementation of blood chemistry system.
 */

#include <algorithm>
#include <isolated/biology/blood_chemistry.hpp>

namespace isolated {
namespace biology {

AcidBaseDisorder BloodChemistrySystem::classify_disorder() const {
  double ph = abg.pH;
  double pco2 = abg.pCO2;
  double hco3 = electrolytes.bicarbonate;

  if (ph >= 7.35 && ph <= 7.45) {
    return AcidBaseDisorder::NORMAL;
  }

  if (ph < 7.35) {
    if (pco2 > 45)
      return AcidBaseDisorder::RESPIRATORY_ACIDOSIS;
    if (hco3 < 22)
      return AcidBaseDisorder::METABOLIC_ACIDOSIS;
    return AcidBaseDisorder::MIXED;
  } else {
    if (pco2 < 35)
      return AcidBaseDisorder::RESPIRATORY_ALKALOSIS;
    if (hco3 > 28)
      return AcidBaseDisorder::METABOLIC_ALKALOSIS;
    return AcidBaseDisorder::MIXED;
  }
}

std::unordered_map<std::string, double>
BloodChemistrySystem::get_cardiac_risk_factors() const {
  std::unordered_map<std::string, double> risks;

  double K = electrolytes.potassium;
  if (K < 3.0)
    risks["hypokalemia_risk"] = 0.8;
  else if (K < 3.5)
    risks["hypokalemia_risk"] = 0.3;
  else if (K > 6.0)
    risks["hyperkalemia_risk"] = 0.9;
  else if (K > 5.5)
    risks["hyperkalemia_risk"] = 0.4;

  double Ca = electrolytes.calcium;
  if (Ca < 7.0)
    risks["hypocalcemia_risk"] = 0.5;
  else if (Ca > 12.0)
    risks["hypercalcemia_risk"] = 0.4;

  if (abg.pH < 7.2)
    risks["acidosis_cardiac_risk"] = 0.6;
  else if (abg.pH > 7.55)
    risks["alkalosis_cardiac_risk"] = 0.4;

  return risks;
}

BloodChemistrySystem::StepResult BloodChemistrySystem::step(double dt) {
  clear_lactate(dt);

  // Respiratory compensation
  if (electrolytes.bicarbonate < 20) {
    double target_pco2 = 40 - (24 - electrolytes.bicarbonate) * 1.2;
    abg.pCO2 += (target_pco2 - abg.pCO2) * 0.1 * dt;
  }

  // Renal compensation (slow)
  if (abg.pCO2 > 45) {
    electrolytes.bicarbonate += 0.001 * dt;
  } else if (abg.pCO2 < 35) {
    electrolytes.bicarbonate -= 0.001 * dt;
  }

  // Clamp values
  electrolytes.bicarbonate = std::clamp(electrolytes.bicarbonate, 5.0, 35.0);
  abg.pCO2 = std::clamp(abg.pCO2, 15.0, 80.0);
  lactate = std::clamp(lactate, 0.5, 20.0);

  compute_ph();
  compute_base_excess();

  abg.lactate = lactate;
  abg.HCO3 = electrolytes.bicarbonate;

  auto disorder = classify_disorder();
  std::string disorder_name;
  switch (disorder) {
  case AcidBaseDisorder::NORMAL:
    disorder_name = "NORMAL";
    break;
  case AcidBaseDisorder::METABOLIC_ACIDOSIS:
    disorder_name = "METABOLIC_ACIDOSIS";
    break;
  case AcidBaseDisorder::METABOLIC_ALKALOSIS:
    disorder_name = "METABOLIC_ALKALOSIS";
    break;
  case AcidBaseDisorder::RESPIRATORY_ACIDOSIS:
    disorder_name = "RESPIRATORY_ACIDOSIS";
    break;
  case AcidBaseDisorder::RESPIRATORY_ALKALOSIS:
    disorder_name = "RESPIRATORY_ALKALOSIS";
    break;
  case AcidBaseDisorder::MIXED:
    disorder_name = "MIXED";
    break;
  }

  return {abg.pH,
          abg.pCO2,
          electrolytes.bicarbonate,
          abg.base_excess,
          lactate,
          electrolytes.anion_gap(),
          electrolytes.potassium,
          disorder_name};
}

} // namespace biology
} // namespace isolated
