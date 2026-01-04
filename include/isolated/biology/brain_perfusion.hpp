#pragma once

/**
 * @file brain_perfusion.hpp
 * @brief Brain perfusion and neurological modeling.
 *
 * Features:
 * - Cerebral autoregulation (Lassen curve)
 * - Intracranial pressure (ICP) dynamics
 * - Cerebral perfusion pressure (CPP)
 * - Herniation risk assessment
 * - Glasgow Coma Scale computation
 */

#include <algorithm>
#include <cmath>

namespace isolated {
namespace biology {

/**
 * @brief Cerebral autoregulation parameters.
 */
struct CerebralConfig {
  // Autoregulation bounds (Lassen curve)
  double map_lower = 60.0;  // mmHg - below this, CBF drops linearly
  double map_upper = 150.0; // mmHg - above this, CBF rises linearly
  double cbf_normal = 50.0; // mL/100g/min - normal cerebral blood flow

  // ICP dynamics
  double icp_normal = 10.0;     // mmHg - normal ICP
  double icp_critical = 20.0;   // mmHg - elevated ICP threshold
  double icp_herniation = 40.0; // mmHg - herniation risk
  double compliance = 0.5;      // mL/mmHg - brain compliance
  double csf_production = 0.35; // mL/min - CSF production rate
  double csf_absorption = 0.35; // mL/min - CSF absorption at normal ICP

  // Brain metabolism
  double cmro2_normal = 3.5;  // mL O2/100g/min - cerebral metabolic rate
  double brain_mass = 1400.0; // grams
};

/**
 * @brief Glasgow Coma Scale components.
 */
struct GCSComponents {
  int eye_response = 4;    // 1-4 (4 = spontaneous)
  int verbal_response = 5; // 1-5 (5 = oriented)
  int motor_response = 6;  // 1-6 (6 = obeys commands)

  int total() const { return eye_response + verbal_response + motor_response; }

  // GCS severity classification
  bool is_severe() const { return total() <= 8; }
  bool is_moderate() const { return total() >= 9 && total() <= 12; }
  bool is_mild() const { return total() >= 13; }
};

/**
 * @brief Brain perfusion state.
 */
struct BrainState {
  // Pressures
  double icp = 10.0; // mmHg - intracranial pressure
  double cpp = 80.0; // mmHg - cerebral perfusion pressure
  double cbf = 50.0; // mL/100g/min - cerebral blood flow

  // Volumes
  double brain_volume = 1200.0; // mL - brain parenchyma
  double csf_volume = 150.0;    // mL - CSF volume
  double blood_volume = 150.0;  // mL - intracranial blood
  double edema_volume = 0.0;    // mL - swelling/edema

  // Oxygenation
  double brain_pO2 = 25.0; // mmHg - brain tissue oxygen
  double sctO2 = 70.0;     // % - cerebral tissue oxygen saturation

  // Injury
  double contusion_volume = 0.0; // mL - hemorrhage/contusion
  double herniation_risk = 0.0;  // 0-1 probability
  bool is_herniating = false;

  // Consciousness
  GCSComponents gcs;
  double consciousness = 1.0; // 0-1 level
};

/**
 * @brief Brain perfusion simulation system.
 */
class BrainPerfusionSystem {
public:
  using Config = CerebralConfig;

  explicit BrainPerfusionSystem(const Config &config = Config{})
      : config_(config) {}

  /**
   * @brief Update brain perfusion state.
   * @param dt Time step (seconds)
   * @param map Mean arterial pressure (mmHg)
   * @param pao2 Arterial oxygen partial pressure (mmHg)
   * @param paco2 Arterial CO2 partial pressure (mmHg)
   * @param hematocrit Blood hematocrit (fraction)
   */
  void step(double dt, double map, double pao2, double paco2,
            double hematocrit) {
    update_icp(dt);
    update_cpp(map);
    update_cbf(map, paco2);
    update_brain_oxygenation(dt, pao2, hematocrit);
    update_consciousness();
    update_herniation_risk();
  }

  // === Trauma/Injury ===

  void add_contusion(double volume_ml) {
    state_.contusion_volume += volume_ml;
    // Contusion acts as mass effect
    state_.brain_volume += volume_ml * 0.5; // Tissue compression
  }

  void add_edema(double volume_ml) { state_.edema_volume += volume_ml; }

  void apply_head_trauma(double severity) {
    // severity 0-1 maps to injury
    double contusion = severity * 30.0; // Up to 30mL hematoma
    add_contusion(contusion);

    // Immediate GCS depression
    if (severity > 0.3) {
      state_.gcs.eye_response =
          std::max(1, state_.gcs.eye_response - static_cast<int>(severity * 2));
      state_.gcs.motor_response = std::max(
          1, state_.gcs.motor_response - static_cast<int>(severity * 3));
      state_.gcs.verbal_response = std::max(
          1, state_.gcs.verbal_response - static_cast<int>(severity * 2));
    }
  }

  // === Treatment ===

  void perform_craniotomy() {
    // Decompressive craniectomy - removes skull constraint
    config_.compliance *= 3.0;               // Increased compliance
    state_.icp = std::min(state_.icp, 15.0); // Immediate ICP reduction
  }

  void drain_csf(double volume_ml) {
    state_.csf_volume = std::max(50.0, state_.csf_volume - volume_ml);
    // ICP drops with CSF drainage
    state_.icp -= volume_ml / config_.compliance;
    state_.icp = std::max(0.0, state_.icp);
  }

  void administer_mannitol(double dose_g) {
    // Osmotic therapy - reduces brain water
    double water_reduction = dose_g * 2.0; // mL per gram
    state_.edema_volume = std::max(0.0, state_.edema_volume - water_reduction);
    state_.brain_volume =
        std::max(1100.0, state_.brain_volume - water_reduction * 0.5);
  }

  // === Accessors ===

  const BrainState &state() const { return state_; }
  BrainState &state() { return state_; }
  const Config &config() const { return config_; }

private:
  Config config_;
  BrainState state_;

  void update_icp(double dt) {
    // Monro-Kellie doctrine: skull is fixed volume
    // ICP rises when contents exceed capacity
    double total_volume = state_.brain_volume + state_.csf_volume +
                          state_.blood_volume + state_.edema_volume +
                          state_.contusion_volume;

    // Normal intracranial volume ~1500mL
    double excess = total_volume - 1500.0;

    // Pressure-volume relationship (exponential at high volumes)
    if (excess > 0) {
      // Exponential ICP rise as compliance exhausted
      double pressure_rise = excess / config_.compliance;
      if (excess > 50.0) {
        pressure_rise *= std::exp((excess - 50.0) / 30.0);
      }
      state_.icp = config_.icp_normal + pressure_rise;
    } else {
      // Normal ICP with some margin
      state_.icp = config_.icp_normal + excess / (config_.compliance * 2.0);
      state_.icp = std::max(0.0, state_.icp);
    }

    // CSF dynamics
    double csf_absorption_rate =
        config_.csf_absorption * (state_.icp / config_.icp_normal);
    double net_csf = (config_.csf_production - csf_absorption_rate) * dt / 60.0;
    state_.csf_volume += net_csf;
    state_.csf_volume = std::clamp(state_.csf_volume, 50.0, 200.0);
  }

  void update_cpp(double map) {
    // CPP = MAP - ICP
    state_.cpp = map - state_.icp;
    state_.cpp = std::max(0.0, state_.cpp);
  }

  void update_cbf(double map, double paco2) {
    // Lassen autoregulation curve
    double cbf_base = config_.cbf_normal;

    if (map < config_.map_lower) {
      // Below autoregulation: CBF drops linearly
      cbf_base = config_.cbf_normal * (map / config_.map_lower);
    } else if (map > config_.map_upper) {
      // Above autoregulation: CBF rises (breakthrough)
      cbf_base =
          config_.cbf_normal * (1.0 + 0.3 * (map - config_.map_upper) / 50.0);
    }
    // Within autoregulation range: CBF stays constant

    // CO2 reactivity (~3% change per mmHg CO2)
    double co2_factor = 1.0 + 0.03 * (paco2 - 40.0);
    co2_factor = std::clamp(co2_factor, 0.5, 2.0);

    // CPP effect (below 50 mmHg, CBF drops sharply)
    double cpp_factor = 1.0;
    if (state_.cpp < 50.0) {
      cpp_factor = state_.cpp / 50.0;
    }

    state_.cbf = cbf_base * co2_factor * cpp_factor;
    state_.cbf = std::max(0.0, state_.cbf);
  }

  void update_brain_oxygenation(double dt, double pao2, double hematocrit) {
    // Simplified brain O2 delivery
    double o2_delivery = state_.cbf * pao2 * hematocrit * 0.003;
    double o2_consumption = config_.cmro2_normal * (config_.brain_mass / 100.0);

    // Brain tissue pO2 equilibrates toward delivery/consumption balance
    double target_pO2 = 25.0 * (o2_delivery / o2_consumption);
    target_pO2 = std::clamp(target_pO2, 0.0, 50.0);

    // Time constant ~30 seconds
    double tau = 30.0;
    state_.brain_pO2 +=
        (target_pO2 - state_.brain_pO2) * (1.0 - std::exp(-dt / tau));

    // Tissue saturation
    state_.sctO2 = 100.0 * state_.brain_pO2 / (state_.brain_pO2 + 26.0);
  }

  void update_consciousness() {
    // Consciousness depends on CBF, oxygenation, ICP
    double cbf_factor = std::min(1.0, state_.cbf / 20.0); // <20 = unconscious
    double o2_factor = std::min(1.0, state_.brain_pO2 / 15.0); // <15 = coma
    double icp_factor = state_.icp < 30.0
                            ? 1.0
                            : std::max(0.0, 1.0 - (state_.icp - 30.0) / 20.0);

    state_.consciousness = cbf_factor * o2_factor * icp_factor;
    state_.consciousness = std::clamp(state_.consciousness, 0.0, 1.0);

    // Update GCS based on consciousness
    if (state_.consciousness < 0.3) {
      state_.gcs.eye_response = 1;
      state_.gcs.verbal_response = 1;
      state_.gcs.motor_response =
          std::max(1, static_cast<int>(state_.consciousness * 10));
    } else if (state_.consciousness < 0.6) {
      state_.gcs.eye_response = 2;
      state_.gcs.verbal_response = 2;
      state_.gcs.motor_response = 4;
    } else if (state_.consciousness < 0.9) {
      state_.gcs.eye_response = 3;
      state_.gcs.verbal_response = 4;
      state_.gcs.motor_response = 5;
    }
    // else: GCS stays at current (possibly traumatically depressed) level
  }

  void update_herniation_risk() {
    // Herniation risk increases exponentially above critical ICP
    if (state_.icp < config_.icp_critical) {
      state_.herniation_risk = 0.0;
    } else {
      double excess = state_.icp - config_.icp_critical;
      double range = config_.icp_herniation - config_.icp_critical;
      state_.herniation_risk = std::min(1.0, excess / range);

      // Exponential increase near herniation threshold
      if (state_.herniation_risk > 0.5) {
        state_.herniation_risk =
            0.5 + 0.5 * std::pow((state_.herniation_risk - 0.5) * 2.0, 2);
      }
    }

    state_.is_herniating = state_.icp >= config_.icp_herniation;
  }
};

} // namespace biology
} // namespace isolated
