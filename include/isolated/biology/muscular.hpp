#pragma once

/**
 * @file muscular.hpp
 * @brief Muscular system with ATP dynamics and fatigue modeling.
 */

#include <algorithm>
#include <cmath>

namespace isolated {
namespace biology {

/**
 * @brief Muscle fiber type.
 */
enum class FiberType { SLOW_TWITCH, FAST_TWITCH_A, FAST_TWITCH_B };

/**
 * @brief Muscular system with energy metabolism.
 */
class MuscularSystem {
public:
  struct State {
    double atp_mmol = 8.0;           // Immediate energy
    double phosphocreatine = 25.0;   // PCr reserve (mmol/kg)
    double glycogen_mmol = 80.0;     // Muscle glycogen
    double fatigue_level = 0.0;      // 0-1
    double lactate_production = 0.0; // mmol/min
    double force_capacity = 1.0;     // 0-1
    double oxygen_debt = 0.0;        // mL O2
  };

  MuscularSystem() = default;

  State step(double dt, double intensity = 1.0, double o2_supply = 1.0);

  double compute_force_output(double activation, double fatigue) const;
  double compute_lactate_production(double intensity, double o2) const;

  void recover(double dt, double o2_availability);

  const State &get_state() const { return state_; }

private:
  State state_;

  void consume_atp(double amount, double intensity, double o2);
  void regenerate_atp(double dt, double o2);
  void update_fatigue(double dt, double intensity);
};

// ============================================================================
// INLINE IMPLEMENTATIONS
// ============================================================================

inline double MuscularSystem::compute_force_output(double activation,
                                                   double fatigue) const {
  return activation * (1.0 - fatigue * 0.7) * state_.force_capacity;
}

inline double MuscularSystem::compute_lactate_production(double intensity,
                                                         double o2) const {
  // High intensity + low O2 = more lactate
  if (intensity > 0.6) {
    double anaerobic_fraction = std::max(0.0, 1.0 - o2);
    return (intensity - 0.6) * 5.0 * (1.0 + anaerobic_fraction);
  }
  return 0.0;
}

inline void MuscularSystem::consume_atp(double amount, double intensity,
                                        double o2) {
  state_.atp_mmol -= amount;

  // Replenish from PCr first (fast)
  if (state_.atp_mmol < 6.0 && state_.phosphocreatine > 0) {
    double pcr_use = std::min(state_.phosphocreatine, 6.0 - state_.atp_mmol);
    state_.phosphocreatine -= pcr_use;
    state_.atp_mmol += pcr_use;
  }

  // Then glycolysis (slower, produces lactate)
  if (state_.atp_mmol < 5.0 && state_.glycogen_mmol > 0) {
    double glyc_use =
        std::min(state_.glycogen_mmol * 0.1, 5.0 - state_.atp_mmol);
    state_.glycogen_mmol -= glyc_use;
    state_.atp_mmol += glyc_use * 0.5;
    state_.lactate_production = glyc_use * (1.0 - o2);
  }
}

inline void MuscularSystem::regenerate_atp(double dt, double o2) {
  // Aerobic regeneration
  double regen_rate = 2.0 * o2;
  state_.atp_mmol = std::min(8.0, state_.atp_mmol + regen_rate * dt);

  // PCr regeneration (slow)
  if (state_.atp_mmol > 7.0) {
    state_.phosphocreatine = std::min(25.0, state_.phosphocreatine + 0.5 * dt);
  }
}

inline void MuscularSystem::update_fatigue(double dt, double intensity) {
  // Fatigue accumulates with intensity
  if (intensity > 0.5) {
    state_.fatigue_level += (intensity - 0.5) * 0.02 * dt;
  }

  // Natural recovery
  state_.fatigue_level -= 0.01 * dt;
  state_.fatigue_level = std::clamp(state_.fatigue_level, 0.0, 1.0);

  // Force capacity affected by fatigue and ATP
  state_.force_capacity =
      (1.0 - state_.fatigue_level * 0.5) * std::min(1.0, state_.atp_mmol / 6.0);
}

inline void MuscularSystem::recover(double dt, double o2) {
  regenerate_atp(dt, o2);
  state_.fatigue_level = std::max(0.0, state_.fatigue_level - 0.02 * dt);
  state_.oxygen_debt = std::max(0.0, state_.oxygen_debt - o2 * 10.0 * dt);
  state_.lactate_production =
      std::max(0.0, state_.lactate_production - 0.5 * dt);
}

inline MuscularSystem::State MuscularSystem::step(double dt, double intensity,
                                                  double o2) {
  double atp_demand = intensity * 0.5 * dt;
  consume_atp(atp_demand, intensity, o2);
  regenerate_atp(dt, o2);
  update_fatigue(dt, intensity);

  state_.lactate_production = compute_lactate_production(intensity, o2);

  if (o2 < intensity) {
    state_.oxygen_debt += (intensity - o2) * 10.0 * dt;
  }

  return state_;
}

} // namespace biology
} // namespace isolated
