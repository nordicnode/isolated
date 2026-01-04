#pragma once

/**
 * @file nervous.hpp
 * @brief Autonomic nervous system regulation.
 */

#include <algorithm>
#include <cmath>

namespace isolated {
namespace biology {

/**
 * @brief Autonomic nervous system balance.
 */
class AutonomicNervousSystem {
public:
  struct State {
    double sympathetic_tone = 0.3; // 0-1
    double parasympathetic_tone = 0.5;
    double stress_level = 0.2;
    double pain_level = 0.0;
    double consciousness = 1.0; // 0-1
    double fatigue = 0.0;
  };

  struct Response {
    double heart_rate_modifier;
    double respiratory_rate_modifier;
    double vasoconstriction;
    double sweat_rate;
  };

  AutonomicNervousSystem() = default;

  State step(double dt, double blood_pressure_map, double blood_o2_sat,
             double core_temp_c, double blood_glucose);

  Response compute_response() const;

  void add_pain(double amount) {
    state_.pain_level = std::min(1.0, state_.pain_level + amount);
  }
  void add_stress(double amount) {
    state_.stress_level = std::min(1.0, state_.stress_level + amount);
  }
  void add_fatigue(double amount) {
    state_.fatigue = std::min(1.0, state_.fatigue + amount);
  }

  const State &get_state() const { return state_; }

private:
  State state_;

  void baroreceptor_reflex(double map);
  void chemoreceptor_reflex(double sao2);
  void thermoregulation_reflex(double temp);
  void glucoregulation(double glucose);
};

// ============================================================================
// INLINE IMPLEMENTATIONS
// ============================================================================

inline void AutonomicNervousSystem::baroreceptor_reflex(double map) {
  // Low BP increases sympathetic, decreases parasympathetic
  double deviation = (map - 93.0) / 93.0;
  state_.sympathetic_tone -= deviation * 0.1;
  state_.parasympathetic_tone += deviation * 0.1;
}

inline void AutonomicNervousSystem::chemoreceptor_reflex(double sao2) {
  // Low O2 sat increases sympathetic drive
  if (sao2 < 0.90) {
    state_.sympathetic_tone += (0.90 - sao2) * 2.0;
  }
}

inline void AutonomicNervousSystem::thermoregulation_reflex(double temp) {
  if (temp > 38.0) {
    state_.sympathetic_tone += (temp - 38.0) * 0.1;
  } else if (temp < 36.0) {
    state_.sympathetic_tone += (36.0 - temp) * 0.15;
  }
}

inline void AutonomicNervousSystem::glucoregulation(double glucose) {
  if (glucose < 70.0) {
    state_.sympathetic_tone += (70.0 - glucose) * 0.01;
    state_.stress_level += 0.01;
  }
}

inline AutonomicNervousSystem::State
AutonomicNervousSystem::step(double dt, double map, double sao2, double temp,
                             double glucose) {

  baroreceptor_reflex(map);
  chemoreceptor_reflex(sao2);
  thermoregulation_reflex(temp);
  glucoregulation(glucose);

  // Pain and stress increase sympathetic
  state_.sympathetic_tone += state_.pain_level * 0.3;
  state_.sympathetic_tone += state_.stress_level * 0.2;

  // Natural recovery
  state_.pain_level = std::max(0.0, state_.pain_level - 0.01 * dt);
  state_.stress_level = std::max(0.0, state_.stress_level - 0.005 * dt);
  state_.fatigue = std::max(0.0, state_.fatigue - 0.001 * dt);

  // Clamp tones
  state_.sympathetic_tone = std::clamp(state_.sympathetic_tone, 0.0, 1.0);
  state_.parasympathetic_tone =
      std::clamp(state_.parasympathetic_tone, 0.0, 1.0);

  // Consciousness affected by severe hypoxia
  if (sao2 < 0.70) {
    state_.consciousness -= (0.70 - sao2) * dt;
  } else {
    state_.consciousness = std::min(1.0, state_.consciousness + 0.1 * dt);
  }
  state_.consciousness = std::clamp(state_.consciousness, 0.0, 1.0);

  return state_;
}

inline AutonomicNervousSystem::Response
AutonomicNervousSystem::compute_response() const {
  Response r;
  double balance = state_.sympathetic_tone - state_.parasympathetic_tone;

  r.heart_rate_modifier = 1.0 + balance * 0.5;
  r.respiratory_rate_modifier = 1.0 + state_.sympathetic_tone * 0.3;
  r.vasoconstriction = state_.sympathetic_tone;
  r.sweat_rate = std::max(0.0, state_.sympathetic_tone - 0.4);

  return r;
}

} // namespace biology
} // namespace isolated
