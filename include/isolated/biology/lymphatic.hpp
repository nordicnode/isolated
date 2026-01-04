#pragma once

/**
 * @file lymphatic.hpp
 * @brief Lymphatic system modeling.
 *
 * Features:
 * - Lymph circulation
 * - Edema from capillary leak
 * - Lymph node immune filtering
 * - Drainage impairment
 */

#include <algorithm>
#include <cmath>
#include <string>

namespace isolated {
namespace biology {

// ============================================================================
// LYMPHATIC STATE
// ============================================================================

struct LymphaticState {
  // Fluid balance
  double interstitial_fluid = 2.5;     // L (normal ~3L)
  double lymph_flow_rate = 2.0;        // L/day (normal 2-4)
  double protein_concentration = 20.0; // g/L in lymph

  // Edema
  double edema_volume = 0.0;   // L excess fluid
  double edema_severity = 0.0; // 0-1 (pitting edema grading)

  // Regional (simplified)
  double arm_edema = 0.0;
  double leg_edema = 0.0;
  double pulmonary_edema = 0.0; // Dangerous

  // Lymph node function
  double node_filtering = 1.0;    // 0-1 immune filtering capacity
  double node_inflammation = 0.0; // 0-1 lymphadenopathy

  // Damage
  double lymphatic_damage = 0.0; // 0-1 vessel damage
  bool lymphedema = false;       // Chronic obstruction
};

// ============================================================================
// LYMPHATIC SYSTEM
// ============================================================================

class LymphaticSystem {
public:
  struct Config {
    double normal_flow = 2.5;             // L/day baseline
    double capillary_permeability = 0.02; // Normal filtration
    double oncotic_pressure = 25.0;       // mmHg (albumin)
    double hydrostatic_pressure = 30.0;   // mmHg capillary
  };

  explicit LymphaticSystem(const Config &config = Config{}) : config_(config) {}

  /**
   * @brief Update lymphatic system.
   * @param dt Time step (seconds)
   * @param capillary_pressure Capillary hydrostatic pressure (mmHg)
   * @param plasma_albumin Plasma albumin (g/dL)
   * @param inflammation Systemic inflammation level (0-1)
   */
  void step(double dt, double capillary_pressure, double plasma_albumin,
            double inflammation) {
    update_starling_forces(capillary_pressure, plasma_albumin);
    update_lymph_flow(dt, inflammation);
    update_edema(dt);
    update_node_function(inflammation);
  }

  // === Pathology ===

  void apply_venous_obstruction(double severity) {
    // DVT, heart failure → increased capillary pressure
    state_.leg_edema += severity * 0.5;
    state_.edema_volume += severity * 0.3;
  }

  void apply_lymph_node_removal(const std::string &region) {
    state_.lymphatic_damage += 0.3;
    if (region == "arm") {
      state_.arm_edema += 0.2;
      state_.lymphedema = true;
    }
  }

  void apply_capillary_leak(double severity) {
    // Sepsis, burns, anaphylaxis
    capillary_leak_ += severity;
    state_.edema_volume += severity * 1.0; // Rapid fluid shift
    state_.pulmonary_edema += severity * 0.3;
  }

  void apply_hypoalbuminemia(double albumin_deficit) {
    // Liver failure, malnutrition → reduced oncotic pressure
    oncotic_deficit_ = albumin_deficit;
  }

  // === Accessors ===

  const LymphaticState &state() const { return state_; }
  LymphaticState &state() { return state_; }

  bool has_pulmonary_edema() const { return state_.pulmonary_edema > 0.3; }
  bool has_anasarca() const { return state_.edema_volume > 5.0; }

private:
  Config config_;
  LymphaticState state_;
  double capillary_leak_ = 0.0;
  double oncotic_deficit_ = 0.0;

  void update_starling_forces(double cap_pressure, double albumin) {
    // Starling equation: Jv = Kf[(Pc - Pi) - σ(πc - πi)]
    // Simplified: net filtration when hydrostatic > oncotic
    double oncotic = albumin * 5.0; // Approximate oncotic pressure
    oncotic -= oncotic_deficit_ * 2.0;

    double net_filtration = cap_pressure - 10.0 - oncotic; // Pi ~10 mmHg
    net_filtration *= (1.0 + capillary_leak_ * 2.0);

    if (net_filtration > 0) {
      // Fluid leaves capillaries → interstitium
      filtration_rate_ = net_filtration * config_.capillary_permeability;
    } else {
      filtration_rate_ = net_filtration * config_.capillary_permeability * 0.5;
    }
  }

  void update_lymph_flow(double dt, double inflammation) {
    // Lymph flow increases with exercise, decreases with obstruction
    double flow_capacity =
        config_.normal_flow * (1.0 - state_.lymphatic_damage);

    // Inflammation increases permeability but also lymph flow
    flow_capacity *= (1.0 + inflammation * 0.5);

    state_.lymph_flow_rate = flow_capacity;

    // Remove fluid from interstitium
    double drainage = flow_capacity * dt / 86400.0;
    state_.interstitial_fluid -= drainage;
    state_.interstitial_fluid = std::max(1.0, state_.interstitial_fluid);
  }

  void update_edema(double dt) {
    // Accumulate or resolve edema
    double net_fluid =
        filtration_rate_ * dt / 3600.0 - state_.lymph_flow_rate * dt / 86400.0;

    state_.edema_volume += net_fluid;
    state_.edema_volume = std::max(0.0, state_.edema_volume);

    // Severity based on volume
    state_.edema_severity = std::min(1.0, state_.edema_volume / 5.0);

    // Redistribute to regions
    if (state_.edema_volume > 2.0) {
      state_.leg_edema = std::min(1.0, (state_.edema_volume - 2.0) / 3.0);
    }

    // Pulmonary edema slowly resolves
    state_.pulmonary_edema *= std::exp(-dt / 3600.0);
  }

  void update_node_function(double inflammation) {
    // Inflammation → lymphadenopathy
    if (inflammation > 0.3) {
      state_.node_inflammation += (inflammation - 0.3) * 0.1;
    } else {
      state_.node_inflammation *= 0.99;
    }
    state_.node_inflammation = std::clamp(state_.node_inflammation, 0.0, 1.0);

    // Filtering capacity
    state_.node_filtering =
        1.0 - state_.node_inflammation * 0.3 - state_.lymphatic_damage * 0.5;
    state_.node_filtering = std::max(0.0, state_.node_filtering);
  }

  double filtration_rate_ = 0.0;
};

} // namespace biology
} // namespace isolated
