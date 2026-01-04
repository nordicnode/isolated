#pragma once

/**
 * @file coagulation.hpp
 * @brief Blood coagulation system with clotting cascade and platelet dynamics.
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace isolated {
namespace biology {

/**
 * @brief Coagulation factor state.
 */
struct ClottingFactor {
  double concentration = 1.0; // Relative to normal (0-2+)
  double activity = 1.0;      // Functional activity (0-1)
  bool inhibited = false;
};

/**
 * @brief Clot formation site.
 */
struct Clot {
  size_t location_id;
  double fibrin_mass = 0.0;    // mg
  double platelet_count = 0.0; // Platelets incorporated
  double stability = 0.0;      // 0-1
  double age_seconds = 0.0;
  bool is_stable = false;
};

/**
 * @brief Anticoagulant type.
 */
enum class AnticoagulantType : uint8_t {
  NONE = 0,
  HEPARIN = 1,   // Factor Xa/IIa inhibitor
  WARFARIN = 2,  // Vitamin K antagonist
  ASPIRIN = 3,   // Platelet inhibitor
  DIRECT_Xa = 4, // Direct Factor Xa inhibitor
  DIRECT_IIa = 5 // Direct thrombin inhibitor
};

/**
 * @brief Coagulation cascade system.
 */
class CoagulationSystem {
public:
  struct State {
    // Platelet counts
    double platelet_count = 250000.0; // per µL (normal 150k-400k)
    double activated_platelets = 0.0;

    // Clotting factors (simplified cascade)
    ClottingFactor factor_VII;  // Extrinsic pathway
    ClottingFactor factor_VIII; // Intrinsic pathway
    ClottingFactor factor_IX;   // Intrinsic pathway
    ClottingFactor factor_X;    // Common pathway
    ClottingFactor factor_II;   // Prothrombin
    ClottingFactor fibrinogen;  // Factor I

    // Lab values
    double pt_seconds = 12.0;  // Prothrombin time (normal 11-13.5)
    double ptt_seconds = 30.0; // Partial thromboplastin time (normal 25-35)
    double inr = 1.0;          // International normalized ratio
    double d_dimer = 0.0;      // Fibrin degradation (µg/mL)

    // Status
    double bleeding_tendency = 0.0; // 0-1
    double clotting_tendency = 0.0; // 0-1 (thrombosis risk)
  };

  CoagulationSystem() = default;

  State step(double dt);

  // Cascade activation
  void activate_extrinsic_pathway(double tissue_factor);
  void activate_intrinsic_pathway(double contact_activation);

  // Platelet functions
  void activate_platelets(double stimulus);
  double compute_platelet_aggregation(double shear_rate) const;

  // Clot management
  void form_clot(size_t location, double severity);
  void lyse_clot(size_t clot_idx, double tpa_activity);

  // Anticoagulant effects
  void apply_anticoagulant(AnticoagulantType type, double dose);
  void clear_anticoagulant(double dt);

  // Accessors
  double compute_clot_time(double injury_severity) const;
  const State &get_state() const { return state_; }
  const std::vector<Clot> &get_clots() const { return clots_; }

private:
  State state_;
  std::vector<Clot> clots_;

  AnticoagulantType active_anticoagulant_ = AnticoagulantType::NONE;
  double anticoagulant_level_ = 0.0;

  void cascade_extrinsic(double dt);
  void cascade_intrinsic(double dt);
  void cascade_common(double dt);
  void update_lab_values();
  void process_clots(double dt);
};

// ============================================================================
// INLINE IMPLEMENTATIONS
// ============================================================================

inline void
CoagulationSystem::activate_extrinsic_pathway(double tissue_factor) {
  // Tissue factor activates Factor VII
  state_.factor_VII.activity =
      std::min(2.0, state_.factor_VII.activity + tissue_factor * 0.5);
}

inline void CoagulationSystem::activate_intrinsic_pathway(double contact) {
  // Contact activation (collagen exposure) activates Factor XII → XI → IX →
  // VIII
  state_.factor_VIII.activity =
      std::min(2.0, state_.factor_VIII.activity + contact * 0.3);
  state_.factor_IX.activity =
      std::min(2.0, state_.factor_IX.activity + contact * 0.3);
}

inline void CoagulationSystem::activate_platelets(double stimulus) {
  double activation = stimulus * state_.platelet_count / 250000.0;
  if (active_anticoagulant_ == AnticoagulantType::ASPIRIN) {
    activation *= 0.3; // Aspirin inhibits platelet activation
  }
  state_.activated_platelets += activation * 10000.0;
  state_.activated_platelets =
      std::min(state_.activated_platelets, state_.platelet_count * 0.8);
}

inline double
CoagulationSystem::compute_platelet_aggregation(double shear) const {
  // High shear promotes vWF-mediated aggregation
  double base_rate = state_.activated_platelets / 100000.0;
  if (shear > 1000) {
    return base_rate * (1.0 + shear / 5000.0);
  }
  return base_rate;
}

inline void CoagulationSystem::form_clot(size_t location, double severity) {
  Clot clot;
  clot.location_id = location;
  clot.fibrin_mass = severity * 10.0;
  clot.platelet_count = state_.activated_platelets * severity * 0.1;
  state_.activated_platelets -= clot.platelet_count;
  clots_.push_back(clot);
}

inline void CoagulationSystem::lyse_clot(size_t idx, double tpa) {
  if (idx < clots_.size()) {
    clots_[idx].fibrin_mass -= tpa * 0.5;
    state_.d_dimer += tpa * 0.1; // Fibrin degradation products

    if (clots_[idx].fibrin_mass <= 0) {
      clots_.erase(clots_.begin() + idx);
    }
  }
}

inline void CoagulationSystem::apply_anticoagulant(AnticoagulantType type,
                                                   double dose) {
  active_anticoagulant_ = type;
  anticoagulant_level_ = dose;

  switch (type) {
  case AnticoagulantType::HEPARIN:
    state_.factor_X.activity *= (1.0 - dose * 0.3);
    state_.factor_II.activity *= (1.0 - dose * 0.3);
    break;
  case AnticoagulantType::WARFARIN:
    state_.factor_VII.concentration *= (1.0 - dose * 0.4);
    state_.factor_IX.concentration *= (1.0 - dose * 0.4);
    state_.factor_X.concentration *= (1.0 - dose * 0.4);
    state_.factor_II.concentration *= (1.0 - dose * 0.4);
    break;
  case AnticoagulantType::ASPIRIN:
    // Handled in platelet activation
    break;
  case AnticoagulantType::DIRECT_Xa:
    state_.factor_X.inhibited = true;
    break;
  case AnticoagulantType::DIRECT_IIa:
    state_.factor_II.inhibited = true;
    break;
  default:
    break;
  }
}

inline void CoagulationSystem::clear_anticoagulant(double dt) {
  anticoagulant_level_ = std::max(0.0, anticoagulant_level_ - 0.01 * dt);
  if (anticoagulant_level_ < 0.1) {
    active_anticoagulant_ = AnticoagulantType::NONE;
    state_.factor_X.inhibited = false;
    state_.factor_II.inhibited = false;
  }
}

inline void CoagulationSystem::cascade_extrinsic(double dt) {
  // Factor VII → Factor X activation
  if (state_.factor_VII.activity > 1.0) {
    double activation = (state_.factor_VII.activity - 1.0) * 0.5 * dt;
    state_.factor_X.activity += activation;
    state_.factor_VII.activity -= activation * 0.1;
  }
}

inline void CoagulationSystem::cascade_intrinsic(double dt) {
  // Factor VIII + IX → Factor X activation
  if (state_.factor_VIII.activity > 1.0 && state_.factor_IX.activity > 1.0) {
    double activation =
        std::min(state_.factor_VIII.activity, state_.factor_IX.activity) * 0.3 *
        dt;
    state_.factor_X.activity += activation;
  }
}

inline void CoagulationSystem::cascade_common(double dt) {
  // Factor X → Factor II (prothrombin to thrombin)
  if (state_.factor_X.activity > 1.0 && !state_.factor_X.inhibited) {
    double xa_activity = (state_.factor_X.activity - 1.0) * 0.4 * dt;
    state_.factor_II.activity += xa_activity;
  }

  // Thrombin → Fibrinogen to Fibrin
  if (state_.factor_II.activity > 1.0 && !state_.factor_II.inhibited) {
    // Fibrin formation contributes to clot stability
    for (auto &clot : clots_) {
      if (!clot.is_stable) {
        clot.fibrin_mass += (state_.factor_II.activity - 1.0) * dt;
        clot.stability += 0.01 * dt;
        if (clot.stability >= 1.0) {
          clot.is_stable = true;
        }
      }
    }
  }
}

inline void CoagulationSystem::update_lab_values() {
  // PT affected by extrinsic pathway (VII, X, V, II, I)
  double extrinsic_function =
      (state_.factor_VII.activity * state_.factor_X.activity *
       state_.factor_II.activity) /
      3.0;
  state_.pt_seconds = 12.0 / std::max(0.1, extrinsic_function);

  // PTT affected by intrinsic pathway
  double intrinsic_function =
      (state_.factor_VIII.activity * state_.factor_IX.activity *
       state_.factor_X.activity) /
      3.0;
  state_.ptt_seconds = 30.0 / std::max(0.1, intrinsic_function);

  // INR
  state_.inr = state_.pt_seconds / 12.0;

  // Bleeding/clotting tendency
  state_.bleeding_tendency =
      std::clamp((state_.pt_seconds - 12.0) / 10.0 +
                     (1.0 - state_.platelet_count / 150000.0),
                 0.0, 1.0);

  state_.clotting_tendency = std::clamp(
      state_.factor_X.activity - 1.0 + state_.activated_platelets / 200000.0,
      0.0, 1.0);
}

inline void CoagulationSystem::process_clots(double dt) {
  for (auto &clot : clots_) {
    clot.age_seconds += dt;

    // Natural fibrinolysis (slow)
    if (clot.is_stable && clot.age_seconds > 3600) {
      clot.fibrin_mass -= 0.001 * dt;
      state_.d_dimer += 0.0001 * dt;
    }
  }

  // Remove dissolved clots
  clots_.erase(std::remove_if(clots_.begin(), clots_.end(),
                              [](const Clot &c) { return c.fibrin_mass <= 0; }),
               clots_.end());
}

inline double CoagulationSystem::compute_clot_time(double severity) const {
  // Time to form stable clot (seconds)
  double base_time = 300.0; // ~5 minutes normally

  double factor_modifier =
      (state_.factor_X.activity + state_.factor_II.activity +
       state_.fibrinogen.concentration) /
      3.0;

  double platelet_modifier = state_.platelet_count / 250000.0;

  return base_time / (factor_modifier * platelet_modifier * severity);
}

inline CoagulationSystem::State CoagulationSystem::step(double dt) {
  cascade_extrinsic(dt);
  cascade_intrinsic(dt);
  cascade_common(dt);
  process_clots(dt);
  update_lab_values();
  clear_anticoagulant(dt);

  // Natural factor regeneration
  auto regenerate = [dt](ClottingFactor &f) {
    if (!f.inhibited) {
      f.activity += (1.0 - f.activity) * 0.001 * dt;
      f.concentration += (1.0 - f.concentration) * 0.0001 * dt;
    }
  };

  regenerate(state_.factor_VII);
  regenerate(state_.factor_VIII);
  regenerate(state_.factor_IX);
  regenerate(state_.factor_X);
  regenerate(state_.factor_II);
  regenerate(state_.fibrinogen);

  // Platelet regeneration
  state_.platelet_count += (250000.0 - state_.platelet_count) * 0.0001 * dt;
  state_.activated_platelets *= (1.0 - 0.01 * dt); // Deactivation

  // D-dimer clearance
  state_.d_dimer *= (1.0 - 0.001 * dt);

  return state_;
}

} // namespace biology
} // namespace isolated
