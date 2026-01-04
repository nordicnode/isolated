#pragma once

/**
 * @file breach.hpp
 * @brief Multi-level breach and atmospheric propagation system.
 *
 * Features:
 * - Compartment pressure/gas modeling
 * - Breach flow with orifice equations
 * - Choked flow at sonic conditions
 * - Multi-compartment propagation network
 */

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

namespace isolated {
namespace fluids {

// ============================================================================
// CONSTANTS
// ============================================================================

namespace breach_constants {
constexpr double R_AIR = 287.05;          // J/(kg·K) specific gas constant
constexpr double GAMMA = 1.4;             // Ratio of specific heats (air)
constexpr double CRITICAL_RATIO = 0.528;  // (2/(γ+1))^(γ/(γ-1)) for γ=1.4
constexpr double ATM_PRESSURE = 101325.0; // Pa
constexpr double SPACE_PRESSURE = 0.0;    // Pa (vacuum)
} // namespace breach_constants

// ============================================================================
// COMPARTMENT
// ============================================================================

struct Compartment {
  std::string id;
  double volume = 100.0;      // m³
  double pressure = 101325.0; // Pa
  double temperature = 293.0; // K

  // Gas composition (mass fractions)
  double o2_fraction = 0.21;
  double n2_fraction = 0.78;
  double co2_fraction = 0.0004;
  double h2o_fraction = 0.01;

  // Computed
  double density() const {
    return pressure / (breach_constants::R_AIR * temperature);
  }

  double mass() const { return density() * volume; }

  double o2_partial_pressure() const { return pressure * o2_fraction; }
};

// ============================================================================
// BREACH / OPENING
// ============================================================================

enum class BreachType {
  DOOR,        // Standard doorway
  VENT,        // HVAC vent
  HULL_BREACH, // Puncture to vacuum
  WINDOW,      // Broken window
  CRACK        // Small crack/leak
};

struct Breach {
  std::string id;
  BreachType type = BreachType::CRACK;

  // Connected compartments (empty = vacuum)
  std::string compartment_a;
  std::string compartment_b; // Empty string = vacuum/outside

  double area = 0.01;            // m² effective flow area
  double discharge_coeff = 0.65; // Cd for orifice

  bool is_open = true;
  bool is_growing = false;  // For propagating cracks
  double growth_rate = 0.0; // m²/s

  // Flow state (computed)
  double mass_flow_rate = 0.0; // kg/s (positive = A→B)
  double velocity = 0.0;       // m/s
  bool is_choked = false;      // Sonic flow?
};

// ============================================================================
// BREACH FLOW CALCULATOR
// ============================================================================

class BreachFlowCalculator {
public:
  /**
   * @brief Calculate mass flow through an orifice.
   * Uses isentropic flow equations with choking.
   */
  static double calculate_mass_flow(double p_upstream, double p_downstream,
                                    double T_upstream, double area, double Cd) {
    using namespace breach_constants;

    if (p_upstream <= p_downstream)
      return 0.0;
    if (area <= 0.0)
      return 0.0;

    double pressure_ratio = p_downstream / p_upstream;
    double rho_upstream = p_upstream / (R_AIR * T_upstream);

    double mass_flow;

    if (pressure_ratio <= CRITICAL_RATIO) {
      // Choked flow (sonic at throat)
      // m_dot = Cd * A * p * sqrt(γ/RT) * (2/(γ+1))^((γ+1)/(2(γ-1)))
      double choke_factor =
          std::pow(2.0 / (GAMMA + 1.0), (GAMMA + 1.0) / (2.0 * (GAMMA - 1.0)));
      mass_flow = Cd * area * p_upstream *
                  std::sqrt(GAMMA / (R_AIR * T_upstream)) * choke_factor;
    } else {
      // Subsonic flow
      // m_dot = Cd * A * sqrt(2 * rho * p_up * (γ/(γ-1)) *
      //         [(p_down/p_up)^(2/γ) - (p_down/p_up)^((γ+1)/γ)])
      double term1 = std::pow(pressure_ratio, 2.0 / GAMMA);
      double term2 = std::pow(pressure_ratio, (GAMMA + 1.0) / GAMMA);
      double flow_term =
          std::sqrt(2.0 * GAMMA / (GAMMA - 1.0) * (term1 - term2));
      mass_flow = Cd * area * rho_upstream *
                  std::sqrt(2.0 * p_upstream / rho_upstream) * flow_term;
    }

    return mass_flow;
  }

  static double calculate_velocity(double mass_flow, double density,
                                   double area) {
    if (density <= 0.0 || area <= 0.0)
      return 0.0;
    return mass_flow / (density * area);
  }

  static double sonic_velocity(double temperature) {
    return std::sqrt(breach_constants::GAMMA * breach_constants::R_AIR *
                     temperature);
  }
};

// ============================================================================
// BREACH PROPAGATION SYSTEM
// ============================================================================

class BreachPropagationSystem {
public:
  void add_compartment(const Compartment &comp) {
    compartments_[comp.id] = comp;
  }

  void add_breach(const Breach &breach) { breaches_.push_back(breach); }

  void create_hull_breach(const std::string &compartment_id, double area) {
    Breach breach;
    breach.id = compartment_id + "_hull";
    breach.type = BreachType::HULL_BREACH;
    breach.compartment_a = compartment_id;
    breach.compartment_b = ""; // Vacuum
    breach.area = area;
    breach.discharge_coeff = 0.8; // Sharp-edged orifice
    breach.is_open = true;
    breaches_.push_back(breach);
  }

  void open_door(const std::string &breach_id) {
    for (auto &b : breaches_) {
      if (b.id == breach_id)
        b.is_open = true;
    }
  }

  void close_door(const std::string &breach_id) {
    for (auto &b : breaches_) {
      if (b.id == breach_id)
        b.is_open = false;
    }
  }

  /**
   * @brief Update all breaches and compartments.
   * @param dt Time step (seconds)
   */
  void step(double dt) {
    // Update growing breaches
    for (auto &breach : breaches_) {
      if (breach.is_growing && breach.growth_rate > 0.0) {
        breach.area += breach.growth_rate * dt;
      }
    }

    // Calculate flow through each breach
    for (auto &breach : breaches_) {
      if (!breach.is_open) {
        breach.mass_flow_rate = 0.0;
        breach.velocity = 0.0;
        breach.is_choked = false;
        continue;
      }

      auto *comp_a = get_compartment(breach.compartment_a);
      auto *comp_b = get_compartment(breach.compartment_b);

      double p_a = comp_a ? comp_a->pressure : breach_constants::SPACE_PRESSURE;
      double p_b = comp_b ? comp_b->pressure : breach_constants::SPACE_PRESSURE;
      double T_a = comp_a ? comp_a->temperature : 293.0;
      double T_b = comp_b ? comp_b->temperature : 293.0;

      // Determine flow direction
      double p_up, p_down, T_up;
      int direction;
      if (p_a >= p_b) {
        p_up = p_a;
        p_down = p_b;
        T_up = T_a;
        direction = 1;
      } else {
        p_up = p_b;
        p_down = p_a;
        T_up = T_b;
        direction = -1;
      }

      // Calculate mass flow
      double mass_flow = BreachFlowCalculator::calculate_mass_flow(
          p_up, p_down, T_up, breach.area, breach.discharge_coeff);

      breach.mass_flow_rate = mass_flow * direction;

      // Check if choked
      breach.is_choked = (p_down / p_up) <= breach_constants::CRITICAL_RATIO;

      // Calculate velocity
      double rho_up = p_up / (breach_constants::R_AIR * T_up);
      breach.velocity = BreachFlowCalculator::calculate_velocity(
          mass_flow, rho_up, breach.area);
    }

    // Apply mass changes to compartments
    for (auto &breach : breaches_) {
      if (!breach.is_open || breach.mass_flow_rate == 0.0)
        continue;

      auto *comp_a = get_compartment(breach.compartment_a);
      auto *comp_b = get_compartment(breach.compartment_b);

      double dm = breach.mass_flow_rate * dt;

      if (comp_a) {
        update_compartment_mass(*comp_a, -dm);
      }
      if (comp_b) {
        update_compartment_mass(*comp_b, dm);
      }
    }
  }

  // === Accessors ===

  Compartment *get_compartment(const std::string &id) {
    if (id.empty())
      return nullptr;
    auto it = compartments_.find(id);
    return it != compartments_.end() ? &it->second : nullptr;
  }

  const Compartment *get_compartment(const std::string &id) const {
    if (id.empty())
      return nullptr;
    auto it = compartments_.find(id);
    return it != compartments_.end() ? &it->second : nullptr;
  }

  const std::vector<Breach> &breaches() const { return breaches_; }
  std::vector<Breach> &breaches() { return breaches_; }

  double total_leak_rate() const {
    double total = 0.0;
    for (const auto &b : breaches_) {
      if (b.compartment_b.empty()) { // Leak to vacuum
        total += std::abs(b.mass_flow_rate);
      }
    }
    return total;
  }

  double time_to_critical_pressure(const std::string &comp_id,
                                   double critical_pressure = 50000.0) const {
    const auto *comp = get_compartment(comp_id);
    if (!comp)
      return 0.0;

    double current_mass = comp->mass();
    double leak_rate = 0.0;

    for (const auto &b : breaches_) {
      if (b.compartment_a == comp_id && b.compartment_b.empty()) {
        leak_rate += std::abs(b.mass_flow_rate);
      }
    }

    if (leak_rate <= 0.0)
      return std::numeric_limits<double>::infinity();

    // Simplified linear estimate
    double critical_mass = critical_pressure * comp->volume /
                           (breach_constants::R_AIR * comp->temperature);
    double mass_to_lose = current_mass - critical_mass;

    return mass_to_lose / leak_rate;
  }

private:
  std::unordered_map<std::string, Compartment> compartments_;
  std::vector<Breach> breaches_;

  void update_compartment_mass(Compartment &comp, double dm) {
    double old_mass = comp.mass();
    double new_mass = std::max(0.001, old_mass + dm);

    // Update pressure (isothermal assumption for simplicity)
    comp.pressure =
        new_mass * breach_constants::R_AIR * comp.temperature / comp.volume;
    comp.pressure = std::max(0.0, comp.pressure);
  }
};

} // namespace fluids
} // namespace isolated
