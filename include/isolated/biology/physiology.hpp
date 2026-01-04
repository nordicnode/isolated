#pragma once

/**
 * @file physiology.hpp
 * @brief Unified physiology orchestrator integrating all biological systems.
 */

#include <isolated/biology/blood_chemistry.hpp>
#include <isolated/biology/circulation.hpp>
#include <isolated/biology/metabolism.hpp>
#include <isolated/biology/nervous.hpp>
#include <isolated/biology/respiration.hpp>

namespace isolated {
namespace biology {

/**
 * @brief Unified human physiology system.
 */
class UnifiedPhysiologySystem {
public:
  struct Config {
    double body_mass_kg = 70.0;
    double height_m = 1.75;
    double age_years = 30.0;
  };

  struct EnvironmentState {
    double ambient_temp_c = 20.0;
    double ambient_po2_mmhg = 150.0;
    double ambient_pco2_mmhg = 0.3;
    double ambient_pressure_mmhg = 760.0;
    double activity_level = 1.0;
  };

  struct PhysiologySnapshot {
    // Cardiovascular
    double heart_rate;
    double blood_pressure_systolic;
    double blood_pressure_diastolic;
    double cardiac_output;

    // Respiratory
    double respiratory_rate;
    double sao2;
    double pao2;
    double paco2;

    // Metabolic
    double blood_glucose;
    double blood_lactate;
    double metabolic_rate;

    // Blood chemistry
    double blood_ph;
    double bicarbonate;

    // Nervous
    double consciousness;
    double stress_level;
    double fatigue;

    // Status flags
    bool is_alive = true;
    bool is_conscious = true;
    bool is_critical = false;
  };

  explicit UnifiedPhysiologySystem(const Config &config);

  PhysiologySnapshot step(double dt, const EnvironmentState &env);

  // Access subsystems
  WindkesselCirculation &circulation() { return circulation_; }
  RespiratorySystem &respiration() { return respiration_; }
  MetabolismSystem &metabolism() { return metabolism_; }
  BloodChemistrySystem &blood_chemistry() { return blood_chemistry_; }
  AutonomicNervousSystem &nervous() { return nervous_; }

private:
  Config config_;

  // Subsystems
  WindkesselCirculation circulation_;
  RespiratorySystem respiration_;
  MetabolismSystem metabolism_;
  BloodChemistrySystem blood_chemistry_;
  AutonomicNervousSystem nervous_;

  void couple_systems(double dt, const EnvironmentState &env);
  bool check_vitals() const;
};

// ============================================================================
// INLINE IMPLEMENTATIONS
// ============================================================================

inline UnifiedPhysiologySystem::UnifiedPhysiologySystem(const Config &config)
    : config_(config), circulation_(config.body_mass_kg),
      respiration_(RespiratorySystem::Config{}),
      metabolism_(MetabolismSystem::Config{80.0, config.body_mass_kg, 0.4}),
      blood_chemistry_() {}

inline void
UnifiedPhysiologySystem::couple_systems(double dt,
                                        const EnvironmentState &env) {
  // ANS affects heart rate
  auto ans_response = nervous_.compute_response();
  circulation_.heart_rate *= ans_response.heart_rate_modifier;

  // Circulation affects blood chemistry
  blood_chemistry_.electrolytes.potassium = circulation_.potassium;

  // Metabolism affects blood chemistry
  if (metabolism_.get_state().blood_lactate > 2.0) {
    blood_chemistry_.add_lactate(0.1 * dt);
  }

  // Respiration affects pH
  blood_chemistry_.abg.pCO2 = respiration_.get_state().paco2;
}

inline bool UnifiedPhysiologySystem::check_vitals() const {
  const auto &circ = circulation_;
  const auto &resp = respiration_.get_state();

  if (circ.blood_volume < 2.0)
    return false; // Exsanguination
  if (resp.sao2 < 0.50)
    return false; // Severe hypoxia
  if (blood_chemistry_.abg.pH < 6.9 || blood_chemistry_.abg.pH > 7.8)
    return false;

  return true;
}

inline UnifiedPhysiologySystem::PhysiologySnapshot
UnifiedPhysiologySystem::step(double dt, const EnvironmentState &env) {
  PhysiologySnapshot snap;

  // Step all subsystems
  auto circ_result = circulation_.step(dt);
  auto resp_state = respiration_.step(
      dt, env.ambient_po2_mmhg, env.ambient_pco2_mmhg, env.activity_level);
  auto metab_state =
      metabolism_.step(dt, env.activity_level, env.ambient_temp_c);
  auto chem_result = blood_chemistry_.step(dt);

  auto [sbp, dbp] = circulation_.get_blood_pressure();
  double map = dbp + (sbp - dbp) / 3.0;

  auto ans_state =
      nervous_.step(dt, map, resp_state.sao2, 37.0, metab_state.blood_glucose);

  // Couple systems
  couple_systems(dt, env);

  // Populate snapshot
  snap.heart_rate = circulation_.heart_rate;
  snap.blood_pressure_systolic = sbp;
  snap.blood_pressure_diastolic = dbp;
  snap.cardiac_output = circ_result.cardiac_output;

  snap.respiratory_rate =
      12.0 * nervous_.compute_response().respiratory_rate_modifier;
  snap.sao2 = resp_state.sao2;
  snap.pao2 = resp_state.pao2;
  snap.paco2 = resp_state.paco2;

  snap.blood_glucose = metab_state.blood_glucose;
  snap.blood_lactate = metab_state.blood_lactate;
  snap.metabolic_rate = metab_state.metabolic_rate;

  snap.blood_ph = chem_result.pH;
  snap.bicarbonate = chem_result.HCO3;

  snap.consciousness = ans_state.consciousness;
  snap.stress_level = ans_state.stress_level;
  snap.fatigue = ans_state.fatigue;

  // Status
  snap.is_alive = check_vitals();
  snap.is_conscious = ans_state.consciousness > 0.3;
  snap.is_critical = resp_state.sao2 < 0.85 || map < 60 || map > 140;

  return snap;
}

} // namespace biology
} // namespace isolated
