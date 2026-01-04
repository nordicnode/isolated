#pragma once

/**
 * @file immune.hpp
 * @brief Immune system with WBC dynamics and inflammation response.
 */

#include <algorithm>
#include <cmath>
#include <vector>

namespace isolated {
namespace biology {

/**
 * @brief Infection site tracking.
 */
struct Infection {
  size_t location_id;
  double pathogen_load = 0.0;
  double inflammation = 0.0;
  double time_infected = 0.0;
};

/**
 * @brief Immune system with inflammatory response.
 */
class ImmuneSystem {
public:
  struct State {
    double wbc_count = 7000.0; // cells/µL (normal 4500-11000)
    double neutrophils = 4000.0;
    double lymphocytes = 2000.0;
    double monocytes = 500.0;
    double fever_response = 0.0; // Temperature increase
    double cytokine_level = 1.0; // Inflammatory markers
    bool sepsis = false;
  };

  ImmuneSystem() = default;

  State step(double dt);

  void add_infection(size_t location, double pathogen_load);
  void add_wound(size_t location);
  double get_fever_response() const { return state_.fever_response; }

  const State &get_state() const { return state_; }
  const std::vector<Infection> &get_infections() const { return infections_; }

private:
  State state_;
  std::vector<Infection> infections_;

  void fight_infections(double dt);
  void regulate_wbc(double dt);
  void update_fever(double dt);
};

// ============================================================================
// INLINE IMPLEMENTATIONS
// ============================================================================

inline void ImmuneSystem::add_infection(size_t location, double load) {
  Infection inf;
  inf.location_id = location;
  inf.pathogen_load = load;
  infections_.push_back(inf);
  state_.wbc_count += 500.0; // Initial immune response
}

inline void ImmuneSystem::add_wound(size_t location) {
  state_.cytokine_level += 0.2;
  state_.neutrophils += 200.0;
}

inline void ImmuneSystem::fight_infections(double dt) {
  for (auto it = infections_.begin(); it != infections_.end();) {
    it->time_infected += dt;

    // Immune response reduces pathogen load
    double kill_rate = state_.neutrophils * 0.001 * dt;
    it->pathogen_load -= kill_rate;
    it->inflammation = std::min(1.0, it->pathogen_load / 100.0);

    // Clear resolved infections
    if (it->pathogen_load <= 0) {
      it = infections_.erase(it);
    } else {
      ++it;
    }
  }
}

inline void ImmuneSystem::regulate_wbc(double dt) {
  double target_wbc = 7000.0;
  if (!infections_.empty()) {
    target_wbc = 12000.0 + infections_.size() * 2000.0;
  }

  state_.wbc_count += (target_wbc - state_.wbc_count) * 0.01 * dt;
  state_.wbc_count = std::clamp(state_.wbc_count, 1000.0, 50000.0);

  // Distribute WBC types
  state_.neutrophils = state_.wbc_count * 0.60;
  state_.lymphocytes = state_.wbc_count * 0.30;
  state_.monocytes = state_.wbc_count * 0.08;
}

inline void ImmuneSystem::update_fever(double dt) {
  double total_inflammation = 0.0;
  for (const auto &inf : infections_) {
    total_inflammation += inf.inflammation;
  }

  double target_fever = std::min(3.0, total_inflammation);
  state_.fever_response += (target_fever - state_.fever_response) * 0.05 * dt;

  // Update cytokines
  state_.cytokine_level = 1.0 + total_inflammation * 2.0;

  // Sepsis detection
  state_.sepsis = state_.cytokine_level > 10.0 || infections_.size() > 3;
}

inline ImmuneSystem::State ImmuneSystem::step(double dt) {
  fight_infections(dt);
  regulate_wbc(dt);
  update_fever(dt);
  return state_;
}

} // namespace biology
} // namespace isolated
