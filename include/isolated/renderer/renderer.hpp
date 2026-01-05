#pragma once

/**
 * @file renderer.hpp
 * @brief Raylib-based visual renderer for the Isolated simulation.
 *
 * Features:
 * - Tile-based grid rendering
 * - Camera pan/zoom controls
 * - Pressure/temperature overlays
 * - Simulation control (pause/step/speed)
 */

#include "raylib.h"
#include <string>

#include <isolated/fluids/lbm_engine.hpp>
#include <isolated/thermal/heat_engine.hpp>

namespace isolated {
namespace renderer {

/**
 * @brief Active overlay type for visualization.
 */
enum class OverlayType { NONE, PRESSURE, TEMPERATURE, OXYGEN };

/**
 * @brief Renderer configuration.
 */
struct RendererConfig {
  int window_width = 1280;
  int window_height = 720;
  int tile_size = 8;                        // Pixels per simulation cell
  std::string title = "Isolated";
  int target_fps = 60;
};

/**
 * @brief Simulation visualization renderer.
 */
class Renderer {
public:
  Renderer() = default;
  ~Renderer() = default;

  // Lifecycle
  void init(const RendererConfig &config);
  void shutdown();
  bool should_close() const;

  // Input handling
  void update_input();

  // Rendering
  void begin_frame();
  void draw_grid(const fluids::LBMEngine &fluids,
                 const thermal::ThermalEngine &thermal);
  void draw_hud();
  void end_frame();

  // State accessors
  bool is_paused() const { return paused_; }
  bool should_step() const { return step_requested_; }
  void clear_step_request() { step_requested_ = false; }
  float get_time_scale() const { return time_scale_; }
  int get_z_level() const { return current_z_; }
  const Camera2D &get_camera() const { return camera_; }

private:
  RendererConfig config_;
  Camera2D camera_{};

  // Simulation grid dimensions (set from LBM engine)
  size_t grid_nx_ = 200;
  size_t grid_ny_ = 200;
  size_t grid_nz_ = 1;

  // View state
  int current_z_ = 0;
  OverlayType active_overlay_ = OverlayType::PRESSURE;

  // Simulation control
  bool paused_ = false;
  bool step_requested_ = false;
  float time_scale_ = 1.0f;

  // Mouse state
  Vector2 last_mouse_pos_{};
  bool dragging_ = false;

  // Optimized grid rendering (texture-based for large grids)
  Image grid_image_{};
  Texture2D grid_texture_{};
  bool grid_texture_initialized_ = false;

  // Internal helpers
  void handle_camera_input();
  void handle_overlay_input();
  void handle_simulation_input();
  Color get_base_tile_color(bool is_solid) const;
  Color get_cell_color(const fluids::LBMEngine &fluids,
                       const thermal::ThermalEngine &thermal,
                       size_t x, size_t y, int z, unsigned int hash) const;
};

// === Inline implementations ===

inline bool Renderer::should_close() const { return WindowShouldClose(); }

} // namespace renderer
} // namespace isolated
