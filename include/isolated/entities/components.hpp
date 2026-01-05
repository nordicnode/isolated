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

} // namespace entities
} // namespace isolated
