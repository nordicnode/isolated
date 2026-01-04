#pragma once

/**
 * @file sensory.hpp
 * @brief Sensory systems modeling.
 *
 * Features:
 * - Vision impairment (hypoxia, flash blindness, injury)
 * - Hearing damage (blast, noise exposure, tinnitus)
 * - Vestibular system (balance, vertigo, motion sickness)
 * - Proprioception (position sense, nerve damage)
 */

#include <algorithm>
#include <cmath>
#include <string>

namespace isolated {
namespace biology {

// ============================================================================
// VISION SYSTEM
// ============================================================================

struct VisionState {
  double acuity = 1.0;          // 0-1 visual acuity
  double peripheral = 1.0;      // 0-1 peripheral vision
  double dark_adaptation = 1.0; // 0-1 night vision capability
  double flash_blindness = 0.0; // 0-1 temporary blindness
  double blur = 0.0;            // 0-1 image blur

  // Injury states
  double corneal_damage = 0.0; // 0-1 eye surface injury
  double retinal_damage = 0.0; // 0-1 permanent retina damage
  bool eye_closed_left = false;
  bool eye_closed_right = false;

  double effective_vision() const {
    double base = acuity * (1.0 - blur) * (1.0 - flash_blindness);
    base *= (1.0 - corneal_damage * 0.5) * (1.0 - retinal_damage);
    if (eye_closed_left && eye_closed_right)
      return 0.0;
    if (eye_closed_left || eye_closed_right)
      base *= 0.7;
    return std::clamp(base, 0.0, 1.0);
  }
};

class VisionSystem {
public:
  void step(double dt, double pao2, double blood_glucose, double icp) {
    update_hypoxic_effects(pao2);
    update_metabolic_effects(blood_glucose);
    update_icp_effects(icp);
    recover_flash(dt);
    state_.blur = std::clamp(state_.blur, 0.0, 1.0);
  }

  // Flash events
  void apply_flash(double intensity) {
    // Intensity 0-1, where 1.0 = welding arc / nuclear flash
    state_.flash_blindness = std::min(1.0, state_.flash_blindness + intensity);
    if (intensity > 0.8) {
      state_.retinal_damage += (intensity - 0.8) * 0.5; // Permanent damage
    }
  }

  void apply_debris(double severity) {
    state_.corneal_damage += severity * 0.3;
    state_.blur += severity * 0.2;
  }

  void apply_chemical_exposure(double severity) {
    state_.corneal_damage += severity * 0.5;
    state_.blur += severity * 0.4;
    if (severity > 0.5) {
      state_.eye_closed_left = state_.eye_closed_right = true;
    }
  }

  const VisionState &state() const { return state_; }
  VisionState &state() { return state_; }

private:
  VisionState state_;

  void update_hypoxic_effects(double pao2) {
    // Vision degrades below PaO2 of 60 mmHg
    if (pao2 < 60.0) {
      double deficit = (60.0 - pao2) / 60.0;
      state_.peripheral *= (1.0 - deficit * 0.5); // Tunnel vision first
      state_.acuity *= (1.0 - deficit * 0.3);
    }
    // Severe hypoxia causes blackout
    if (pao2 < 30.0) {
      state_.acuity *= 0.5;
      state_.peripheral *= 0.3;
    }
  }

  void update_metabolic_effects(double glucose) {
    // Hypoglycemia (<70 mg/dL) causes blurred vision
    if (glucose < 70.0) {
      state_.blur += (70.0 - glucose) / 140.0;
    }
  }

  void update_icp_effects(double icp) {
    // Elevated ICP causes papilledema, vision loss
    if (icp > 20.0) {
      double excess = (icp - 20.0) / 20.0;
      state_.acuity *= (1.0 - excess * 0.3);
      state_.peripheral *= (1.0 - excess * 0.4);
    }
  }

  void recover_flash(double dt) {
    // Flash blindness recovers over ~30 seconds
    double recovery_rate = 1.0 / 30.0;
    state_.flash_blindness *= std::exp(-recovery_rate * dt);
  }
};

// ============================================================================
// HEARING SYSTEM
// ============================================================================

struct HearingState {
  double threshold_left = 0.0; // dB hearing loss (0 = normal)
  double threshold_right = 0.0;
  double tinnitus = 0.0;        // 0-1 tinnitus severity
  double temporary_shift = 0.0; // Temporary threshold shift (dB)

  // Frequency-specific damage (simplified)
  double high_freq_loss = 0.0; // 4-8 kHz damage (noise-induced)
  double low_freq_loss = 0.0;  // <1 kHz damage (blast)

  bool ruptured_left = false;
  bool ruptured_right = false;

  double effective_hearing() const {
    double avg_loss =
        (threshold_left + threshold_right) / 2.0 + temporary_shift;
    // Normal conversation is ~60 dB, loss >40 dB = significant impairment
    double ability = std::max(0.0, 1.0 - avg_loss / 80.0);
    if (ruptured_left && ruptured_right)
      return ability * 0.2;
    if (ruptured_left || ruptured_right)
      return ability * 0.6;
    return ability;
  }
};

class HearingSystem {
public:
  void step(double dt) {
    recover_temporary_shift(dt);
    update_tinnitus(dt);
  }

  void apply_noise_exposure(double db_level, double duration_hours) {
    // NIOSH criteria: 85 dB for 8 hours
    // Every 3 dB doubles the damage rate
    if (db_level < 85.0)
      return;

    double excess = db_level - 85.0;
    double damage_rate = std::pow(2.0, excess / 3.0);
    double exposure = damage_rate * duration_hours / 8.0;

    // High frequency loss first (notch at 4 kHz)
    state_.high_freq_loss += exposure * 2.0;
    state_.threshold_left += exposure;
    state_.threshold_right += exposure;
    state_.temporary_shift += exposure * 5.0;
    state_.tinnitus = std::min(1.0, state_.tinnitus + exposure * 0.1);
  }

  void apply_blast(double overpressure_psi) {
    // Eardrum rupture threshold ~5 psi
    if (overpressure_psi > 5.0) {
      double rupture_prob = (overpressure_psi - 5.0) / 10.0;
      if (rupture_prob > 0.5) {
        state_.ruptured_left = true;
        state_.ruptured_right = true;
      }
    }

    // Blast causes low-frequency hearing loss
    double damage = overpressure_psi * 3.0;
    state_.low_freq_loss += damage;
    state_.threshold_left += damage;
    state_.threshold_right += damage;
    state_.temporary_shift += damage * 2.0;
    state_.tinnitus = std::min(1.0, state_.tinnitus + overpressure_psi * 0.05);
  }

  const HearingState &state() const { return state_; }
  HearingState &state() { return state_; }

private:
  HearingState state_;

  void recover_temporary_shift(double dt) {
    // TTS recovers over hours
    double recovery_rate = 1.0 / 3600.0; // 1 hour time constant
    state_.temporary_shift *= std::exp(-recovery_rate * dt);
  }

  void update_tinnitus(double dt) {
    // Tinnitus slowly decreases but never fully goes away if severe
    if (state_.tinnitus > 0.3) {
      state_.tinnitus *= std::exp(-dt / 86400.0); // Days to recover
    }
  }
};

// ============================================================================
// VESTIBULAR SYSTEM
// ============================================================================

struct VestibularState {
  double balance = 1.0;             // 0-1 balance ability
  double vertigo = 0.0;             // 0-1 spinning sensation
  double nausea = 0.0;              // 0-1 motion sickness
  double spatial_orientation = 1.0; // 0-1 sense of up/down

  // Damage
  double inner_ear_damage = 0.0;
  bool benign_positional = false; // BPPV

  double effective_balance() const {
    return balance * (1.0 - vertigo * 0.8) * (1.0 - inner_ear_damage * 0.5);
  }
};

class VestibularSystem {
public:
  void step(double dt, double angular_velocity, double linear_accel) {
    update_motion_effects(dt, angular_velocity, linear_accel);
    recover_vertigo(dt);
    recover_nausea(dt);
  }

  void apply_head_trauma(double severity) {
    state_.inner_ear_damage += severity * 0.3;
    state_.vertigo = std::min(1.0, state_.vertigo + severity * 0.5);
    state_.balance *= (1.0 - severity * 0.4);

    // BPPV can develop after head trauma
    if (severity > 0.5) {
      state_.benign_positional = true;
    }
  }

  void apply_blast(double overpressure_psi) {
    if (overpressure_psi > 3.0) {
      state_.inner_ear_damage += overpressure_psi * 0.05;
      state_.vertigo = std::min(1.0, state_.vertigo + overpressure_psi * 0.1);
    }
  }

  void trigger_bppv_episode() {
    if (state_.benign_positional) {
      state_.vertigo = std::min(1.0, state_.vertigo + 0.6);
      state_.nausea = std::min(1.0, state_.nausea + 0.4);
    }
  }

  const VestibularState &state() const { return state_; }
  VestibularState &state() { return state_; }

private:
  VestibularState state_;

  void update_motion_effects(double dt, double angular_vel,
                             double linear_accel) {
    // Motion sickness from sustained unusual motion
    double motion_stress =
        angular_vel * 0.1 + std::abs(linear_accel - 9.81) * 0.05;

    if (motion_stress > 0.1) {
      state_.nausea += motion_stress * dt * 0.01;
      state_.spatial_orientation -= motion_stress * dt * 0.005;
    }

    state_.nausea = std::clamp(state_.nausea, 0.0, 1.0);
    state_.spatial_orientation =
        std::clamp(state_.spatial_orientation, 0.0, 1.0);
  }

  void recover_vertigo(double dt) {
    // Vertigo resolves over minutes
    state_.vertigo *= std::exp(-dt / 60.0);
  }

  void recover_nausea(double dt) {
    // Nausea resolves slowly
    state_.nausea *= std::exp(-dt / 300.0);
  }
};

// ============================================================================
// PROPRIOCEPTION SYSTEM
// ============================================================================

struct ProprioceptionState {
  double position_sense = 1.0; // 0-1 joint position awareness
  double movement_sense = 1.0; // 0-1 kinesthesia
  double force_sense = 1.0;    // 0-1 force/weight perception

  // Regional damage (simplified to limbs)
  double arm_left = 1.0;
  double arm_right = 1.0;
  double leg_left = 1.0;
  double leg_right = 1.0;

  double effective_coordination() const {
    double limb_avg = (arm_left + arm_right + leg_left + leg_right) / 4.0;
    return position_sense * movement_sense * limb_avg;
  }
};

class ProprioceptionSystem {
public:
  void step(double dt, double blood_glucose, double vitamin_b12) {
    update_metabolic_effects(blood_glucose, vitamin_b12);
  }

  void apply_nerve_damage(const std::string &region, double severity) {
    if (region == "arm_left")
      state_.arm_left *= (1.0 - severity);
    else if (region == "arm_right")
      state_.arm_right *= (1.0 - severity);
    else if (region == "leg_left")
      state_.leg_left *= (1.0 - severity);
    else if (region == "leg_right")
      state_.leg_right *= (1.0 - severity);

    state_.position_sense *= (1.0 - severity * 0.2);
  }

  void apply_spinal_injury(double level_fraction, double severity) {
    // level_fraction: 0 = cervical, 1 = lumbar
    // Higher injuries affect more
    double affected = 1.0 - level_fraction;

    if (affected > 0.5) {
      state_.arm_left *= (1.0 - severity);
      state_.arm_right *= (1.0 - severity);
    }
    state_.leg_left *= (1.0 - severity);
    state_.leg_right *= (1.0 - severity);
    state_.position_sense *= (1.0 - severity * affected);
  }

  const ProprioceptionState &state() const { return state_; }
  ProprioceptionState &state() { return state_; }

private:
  ProprioceptionState state_;

  void update_metabolic_effects(double glucose, double b12) {
    // Diabetic neuropathy from chronic hyperglycemia
    if (glucose > 180.0) {
      double excess = (glucose - 180.0) / 200.0;
      state_.position_sense -= excess * 0.001; // Very slow damage
    }

    // B12 deficiency causes peripheral neuropathy
    if (b12 < 200.0) {
      double deficit = (200.0 - b12) / 200.0;
      state_.position_sense -= deficit * 0.001;
      state_.leg_left -= deficit * 0.001;
      state_.leg_right -= deficit * 0.001;
    }
  }
};

// ============================================================================
// UNIFIED SENSORY SYSTEM
// ============================================================================

class SensorySystem {
public:
  void step(double dt, double pao2, double glucose, double icp,
            double angular_vel = 0.0, double linear_accel = 9.81,
            double b12 = 400.0) {
    vision_.step(dt, pao2, glucose, icp);
    hearing_.step(dt);
    vestibular_.step(dt, angular_vel, linear_accel);
    proprioception_.step(dt, glucose, b12);
  }

  // Accessors
  VisionSystem &vision() { return vision_; }
  HearingSystem &hearing() { return hearing_; }
  VestibularSystem &vestibular() { return vestibular_; }
  ProprioceptionSystem &proprioception() { return proprioception_; }

  const VisionSystem &vision() const { return vision_; }
  const HearingSystem &hearing() const { return hearing_; }
  const VestibularSystem &vestibular() const { return vestibular_; }
  const ProprioceptionSystem &proprioception() const { return proprioception_; }

  // Overall sensory function
  double overall_sensory_function() const {
    return vision_.state().effective_vision() * 0.4 +
           hearing_.state().effective_hearing() * 0.2 +
           vestibular_.state().effective_balance() * 0.2 +
           proprioception_.state().effective_coordination() * 0.2;
  }

private:
  VisionSystem vision_;
  HearingSystem hearing_;
  VestibularSystem vestibular_;
  ProprioceptionSystem proprioception_;
};

} // namespace biology
} // namespace isolated
