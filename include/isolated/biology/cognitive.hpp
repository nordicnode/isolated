#pragma once

/**
 * @file cognitive.hpp
 * @brief Cognitive function modeling.
 *
 * Features:
 * - Confusion states (hypoxia, glucose, drugs)
 * - Reaction time degradation
 * - Memory impairment (concussion, stress)
 * - Attention and focus
 */

#include <algorithm>
#include <cmath>

namespace isolated {
namespace biology {

// ============================================================================
// COGNITIVE STATE
// ============================================================================

struct CognitiveState {
  // Core metrics (0-1, where 1 = normal)
  double clarity = 1.0;          // Mental clarity
  double focus = 1.0;            // Attention/concentration
  double processing_speed = 1.0; // Information processing
  double memory_encoding = 1.0;  // New memory formation
  double memory_recall = 1.0;    // Retrieving memories

  // Reaction time multiplier (1.0 = normal, 2.0 = twice as slow)
  double reaction_multiplier = 1.0;

  // States
  double confusion = 0.0; // 0-1 confusion level
  double fatigue = 0.0;   // 0-1 mental fatigue
  double stress = 0.0;    // 0-1 acute stress
  double sedation = 0.0;  // 0-1 drug sedation

  // Injury
  double concussion_severity = 0.0;  // 0-1 TBI severity
  double post_concussion_days = 0.0; // Days since injury

  double effective_cognition() const {
    double base = clarity * focus * processing_speed;
    base *= (1.0 - confusion * 0.8);
    base *= (1.0 - sedation * 0.9);
    base *= (1.0 - fatigue * 0.5);
    return std::clamp(base, 0.0, 1.0);
  }

  double effective_reaction_time_ms() const {
    // Normal human reaction time ~250ms
    return 250.0 * reaction_multiplier;
  }
};

// ============================================================================
// COGNITIVE SYSTEM
// ============================================================================

class CognitiveSystem {
public:
  /**
   * @brief Update cognitive state.
   * @param dt Time step (seconds)
   * @param pao2 Arterial oxygen (mmHg)
   * @param paco2 Arterial CO2 (mmHg)
   * @param glucose Blood glucose (mg/dL)
   * @param core_temp Core temperature (°C)
   * @param sleep_debt Hours of sleep debt
   */
  void step(double dt, double pao2, double paco2, double glucose,
            double core_temp, double sleep_debt = 0.0) {
    update_hypoxic_effects(pao2);
    update_hypercapnic_effects(paco2);
    update_glucose_effects(glucose);
    update_temperature_effects(core_temp);
    update_fatigue(dt, sleep_debt);
    update_concussion_recovery(dt);
    compute_reaction_time();
  }

  // === Trauma ===

  void apply_concussion(double severity) {
    state_.concussion_severity =
        std::min(1.0, state_.concussion_severity + severity);
    state_.post_concussion_days = 0.0;

    // Immediate effects
    state_.confusion += severity * 0.6;
    state_.memory_encoding *= (1.0 - severity * 0.5);
    state_.processing_speed *= (1.0 - severity * 0.4);
    state_.clarity *= (1.0 - severity * 0.5);
  }

  void apply_psychological_stress(double severity) {
    state_.stress = std::min(1.0, state_.stress + severity);

    // Acute stress can impair or enhance depending on level
    if (severity < 0.3) {
      // Mild stress enhances focus (Yerkes-Dodson)
      state_.focus = std::min(1.2, state_.focus + severity * 0.3);
    } else {
      // High stress impairs cognition
      state_.focus *= (1.0 - (severity - 0.3) * 0.5);
      state_.memory_encoding *= (1.0 - (severity - 0.3) * 0.3);
    }
  }

  // === Drugs ===

  void apply_sedative(double dose) {
    state_.sedation = std::min(1.0, state_.sedation + dose);
    state_.reaction_multiplier += dose * 2.0;
    state_.clarity *= (1.0 - dose * 0.6);
  }

  void apply_stimulant(double dose) {
    state_.sedation = std::max(0.0, state_.sedation - dose * 0.5);
    state_.fatigue = std::max(0.0, state_.fatigue - dose * 0.4);
    state_.focus = std::min(1.5, state_.focus + dose * 0.3);
    state_.reaction_multiplier =
        std::max(0.8, state_.reaction_multiplier - dose * 0.3);

    // Too much causes jitteriness
    if (dose > 0.5) {
      state_.focus *= 0.8;
    }
  }

  void apply_analgesic(double dose) {
    // Pain relief can improve cognition if pain was present
    // But opioids cause sedation
    state_.sedation += dose * 0.3;
    state_.clarity *= (1.0 - dose * 0.2);
  }

  // === Accessors ===

  const CognitiveState &state() const { return state_; }
  CognitiveState &state() { return state_; }

private:
  CognitiveState state_;

  void update_hypoxic_effects(double pao2) {
    // Cognitive impairment begins below PaO2 60 mmHg
    if (pao2 < 60.0) {
      double deficit = (60.0 - pao2) / 60.0;
      state_.confusion += deficit * 0.3;
      state_.clarity *= (1.0 - deficit * 0.5);
      state_.processing_speed *= (1.0 - deficit * 0.4);

      // Severe hypoxia (<40 mmHg) = loss of consciousness
      if (pao2 < 40.0) {
        state_.clarity *= 0.1;
        state_.confusion = 1.0;
      }
    } else {
      // Recovery when oxygen restored
      state_.confusion *= 0.95;
    }
  }

  void update_hypercapnic_effects(double paco2) {
    // CO2 narcosis above 50 mmHg
    if (paco2 > 50.0) {
      double excess = (paco2 - 50.0) / 50.0;
      state_.sedation += excess * 0.4;
      state_.confusion += excess * 0.3;
      state_.clarity *= (1.0 - excess * 0.4);
    }

    // Hyperventilation (low CO2) causes confusion too
    if (paco2 < 30.0) {
      double deficit = (30.0 - paco2) / 30.0;
      state_.confusion += deficit * 0.2;
      state_.focus *= (1.0 - deficit * 0.3);
    }
  }

  void update_glucose_effects(double glucose) {
    // Hypoglycemia (<70 mg/dL) impairs cognition
    if (glucose < 70.0) {
      double deficit = (70.0 - glucose) / 70.0;
      state_.confusion += deficit * 0.5;
      state_.processing_speed *= (1.0 - deficit * 0.4);
      state_.memory_encoding *= (1.0 - deficit * 0.3);

      // Severe hypoglycemia (<40) = seizures, unconsciousness
      if (glucose < 40.0) {
        state_.clarity *= 0.1;
        state_.confusion = 1.0;
      }
    }

    // Hyperglycemia (>200 mg/dL) causes mild cognitive issues
    if (glucose > 200.0) {
      double excess = (glucose - 200.0) / 300.0;
      state_.clarity *= (1.0 - excess * 0.2);
      state_.focus *= (1.0 - excess * 0.2);
    }
  }

  void update_temperature_effects(double temp_c) {
    // Hypothermia (<35°C) causes confusion
    if (temp_c < 35.0) {
      double deficit = (35.0 - temp_c) / 10.0;
      state_.confusion += deficit * 0.4;
      state_.processing_speed *= (1.0 - deficit * 0.5);
      state_.reaction_multiplier += deficit * 1.0;
    }

    // Hyperthermia (>39°C) causes delirium
    if (temp_c > 39.0) {
      double excess = (temp_c - 39.0) / 3.0;
      state_.confusion += excess * 0.5;
      state_.clarity *= (1.0 - excess * 0.4);
    }
  }

  void update_fatigue(double dt, double sleep_debt) {
    // Sleep debt accumulates fatigue
    if (sleep_debt > 0.0) {
      state_.fatigue += sleep_debt * 0.01 * dt / 3600.0;
    } else {
      // Recovery during rest
      state_.fatigue *= std::exp(-dt / 7200.0); // 2 hour recovery constant
    }

    state_.fatigue = std::clamp(state_.fatigue, 0.0, 1.0);

    // Fatigue effects
    state_.focus *= (1.0 - state_.fatigue * 0.4);
    state_.processing_speed *= (1.0 - state_.fatigue * 0.3);
    state_.reaction_multiplier += state_.fatigue * 0.5;
  }

  void update_concussion_recovery(double dt) {
    if (state_.concussion_severity > 0.0) {
      state_.post_concussion_days += dt / 86400.0;

      // Recovery curve - most recovery in first 7-10 days
      double recovery_rate = 0.1; // 10% per day initially
      if (state_.post_concussion_days > 7.0) {
        recovery_rate = 0.05; // Slower after first week
      }

      state_.concussion_severity *= std::exp(-recovery_rate * dt / 86400.0);

      // Persistent post-concussion symptoms
      if (state_.concussion_severity > 0.1) {
        state_.memory_encoding *= (1.0 - state_.concussion_severity * 0.2);
        state_.processing_speed *= (1.0 - state_.concussion_severity * 0.3);
        state_.focus *= (1.0 - state_.concussion_severity * 0.2);
      }
    }
  }

  void compute_reaction_time() {
    // Base reaction time affected by various factors
    double multiplier = 1.0;

    multiplier += state_.confusion * 1.5;
    multiplier += state_.sedation * 2.0;
    multiplier += state_.fatigue * 0.8;
    multiplier += state_.concussion_severity * 1.0;
    multiplier -= (state_.focus - 1.0) * 0.2; // Better focus = faster

    state_.reaction_multiplier = std::clamp(multiplier, 0.7, 5.0);
  }
};

} // namespace biology
} // namespace isolated
