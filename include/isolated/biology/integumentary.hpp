#pragma once

/**
 * @file integumentary.hpp
 * @brief Skin system with wounds, burns, and healing.
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace isolated {
namespace biology {

/**
 * @brief Wound severity classification.
 */
enum class WoundSeverity : uint8_t {
  NONE = 0,
  SUPERFICIAL = 1,
  PARTIAL = 2,
  FULL_THICKNESS = 3,
  CRITICAL = 4
};

/**
 * @brief Wound type classification.
 */
enum class WoundType : uint8_t {
  LACERATION,
  PUNCTURE,
  ABRASION,
  BURN,
  PRESSURE
};

/**
 * @brief Individual wound tracking.
 */
struct Wound {
  size_t location_id;
  WoundType type;
  WoundSeverity severity;
  double area_cm2 = 1.0;
  double depth_mm = 1.0;
  double healing_progress = 0.0; // 0-1
  double infection_risk = 0.0;
  double bleed_rate_ml_min = 0.0;
  double time_since_injury = 0.0;
  bool is_bandaged = false;
};

/**
 * @brief Burns classification.
 */
struct Burn {
  size_t location_id;
  double tbsa_percent = 0.0; // Total Body Surface Area
  uint8_t degree = 1;        // 1st, 2nd, 3rd
  double healing_progress = 0.0;
  double fluid_loss_ml_hr = 0.0;
};

/**
 * @brief Integumentary (skin) system.
 */
class IntegumentarySystem {
public:
  struct State {
    double skin_integrity = 1.0; // 0-1
    double total_wound_area = 0.0;
    double total_burn_tbsa = 0.0;
    double total_bleed_rate = 0.0;
    double infection_count = 0;
    double healing_rate_modifier = 1.0;
  };

  IntegumentarySystem() = default;

  State step(double dt, double blood_flow = 1.0, double nutrition = 1.0);

  void add_wound(const Wound &wound);
  void add_burn(const Burn &burn);
  void bandage_wound(size_t wound_idx);
  void debride_wound(size_t wound_idx);

  const State &get_state() const { return state_; }
  const std::vector<Wound> &get_wounds() const { return wounds_; }
  const std::vector<Burn> &get_burns() const { return burns_; }

private:
  State state_;
  std::vector<Wound> wounds_;
  std::vector<Burn> burns_;

  void heal_wounds(double dt, double blood_flow, double nutrition);
  void heal_burns(double dt);
  void update_bleeding();
  void check_infections(double dt);
};

// ============================================================================
// INLINE IMPLEMENTATIONS
// ============================================================================

inline void IntegumentarySystem::add_wound(const Wound &w) {
  wounds_.push_back(w);
  state_.total_wound_area += w.area_cm2;
  state_.skin_integrity =
      std::max(0.0, state_.skin_integrity - w.area_cm2 * 0.001);
}

inline void IntegumentarySystem::add_burn(const Burn &b) {
  burns_.push_back(b);
  state_.total_burn_tbsa += b.tbsa_percent;
  state_.skin_integrity =
      std::max(0.0, state_.skin_integrity - b.tbsa_percent * 0.01);
}

inline void IntegumentarySystem::bandage_wound(size_t idx) {
  if (idx < wounds_.size()) {
    wounds_[idx].is_bandaged = true;
    wounds_[idx].bleed_rate_ml_min *= 0.2;
    wounds_[idx].infection_risk *= 0.5;
  }
}

inline void IntegumentarySystem::debride_wound(size_t idx) {
  if (idx < wounds_.size()) {
    wounds_[idx].infection_risk *= 0.5;
    wounds_[idx].healing_progress =
        std::min(1.0, wounds_[idx].healing_progress + 0.1);
  }
}

inline void IntegumentarySystem::heal_wounds(double dt, double blood_flow,
                                             double nutrition) {
  double base_rate =
      0.001 * state_.healing_rate_modifier * blood_flow * nutrition;

  for (auto it = wounds_.begin(); it != wounds_.end();) {
    it->time_since_injury += dt;

    double rate = base_rate;
    if (it->is_bandaged)
      rate *= 1.5;
    if (it->infection_risk > 0.5)
      rate *= 0.3;

    it->healing_progress += rate * dt;
    it->bleed_rate_ml_min *= (1.0 - 0.1 * dt); // Natural clotting

    if (it->healing_progress >= 1.0) {
      state_.total_wound_area -= it->area_cm2;
      it = wounds_.erase(it);
    } else {
      ++it;
    }
  }
}

inline void IntegumentarySystem::heal_burns(double dt) {
  for (auto it = burns_.begin(); it != burns_.end();) {
    double rate = (it->degree == 1) ? 0.01 : (it->degree == 2) ? 0.003 : 0.001;
    it->healing_progress += rate * dt;
    it->fluid_loss_ml_hr = it->tbsa_percent * (1.0 - it->healing_progress) *
                           (it->degree == 3 ? 10.0 : 5.0);

    if (it->healing_progress >= 1.0) {
      state_.total_burn_tbsa -= it->tbsa_percent;
      it = burns_.erase(it);
    } else {
      ++it;
    }
  }
}

inline void IntegumentarySystem::update_bleeding() {
  state_.total_bleed_rate = 0.0;
  for (const auto &w : wounds_) {
    state_.total_bleed_rate += w.bleed_rate_ml_min;
  }
}

inline void IntegumentarySystem::check_infections(double dt) {
  state_.infection_count = 0;
  for (auto &w : wounds_) {
    if (!w.is_bandaged && w.time_since_injury > 3600.0) {
      w.infection_risk = std::min(1.0, w.infection_risk + 0.001 * dt);
    }
    if (w.infection_risk > 0.7) {
      state_.infection_count++;
    }
  }
}

inline IntegumentarySystem::State
IntegumentarySystem::step(double dt, double blood_flow, double nutrition) {
  heal_wounds(dt, blood_flow, nutrition);
  heal_burns(dt);
  update_bleeding();
  check_infections(dt);

  state_.skin_integrity =
      1.0 - (state_.total_wound_area * 0.001 + state_.total_burn_tbsa * 0.01);
  state_.skin_integrity = std::clamp(state_.skin_integrity, 0.0, 1.0);

  return state_;
}

} // namespace biology
} // namespace isolated
