#pragma once

#include "raylib.h"
#include <string>
#include <vector>

namespace isolated {
namespace entities {

/**
 * @brief Position in the 3D grid.
 */
struct Position {
  float x;
  float y;
  int z;
};

/**
 * @brief Velocity vector.
 */
struct Velocity {
  float dx;
  float dy;
};

/**
 * @brief Visual representation.
 */
struct Renderable {
  char glyph;
  Color color;
};

/**
 * @brief Tag component for astronauts.
 */
struct Astronaut {
  std::string name;
  // TODO: Add unique ID or DNA
};

/**
 * @brief Movement path.
 */
struct Path {
  std::vector<Vector2> waypoints;
  int current_index;
  bool active;
};

/**
 * @brief Hypoxia stages for oxygen deprivation.
 */
enum class HypoxiaState {
  NORMAL,     // O2 saturation >= 0.5
  CONFUSED,   // O2 saturation 0.2 - 0.5
  COLLAPSED,  // O2 saturation 0.05 - 0.2
  DEAD        // O2 saturation < 0.05
};

/**
 * @brief Physiological needs for survival.
 */
struct Needs {
  float oxygen  = 1.0f;   // 0-1 saturation (blood O2 level)
  float hunger  = 1.0f;   // 0-1 fullness
  float thirst  = 1.0f;   // 0-1 hydration
  float fatigue = 0.0f;   // 0-1 exhaustion
  
  HypoxiaState hypoxia_state = HypoxiaState::NORMAL;
};

/**
 * @brief Metabolic engine for "Life is Slow Combustion".
 */
struct Metabolism {
  float caloric_balance = 2000.0f; // kcal available (Energy buffer)
  float metabolic_rate_watts = 80.0f; // Base heat output (BMR + Activity)
  float core_temperature = 310.15f; // 37.0 C
  float insulation = 0.5f; // Clo units (0.5 = light clothing)
};

} // namespace entities
} // namespace isolated
