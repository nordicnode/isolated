/**
 * @file renderer.cpp
 * @brief Implementation of the Raylib-based simulation renderer.
 */

#include <isolated/renderer/renderer.hpp>
#include <isolated/renderer/color_maps.hpp>

#include <algorithm>
#include <cstdio>

namespace isolated {
namespace renderer {

void Renderer::init(const RendererConfig &config) {
  config_ = config;

  // Initialize Raylib window
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
  InitWindow(config_.window_width, config_.window_height, config_.title.c_str());
  SetTargetFPS(config_.target_fps);

  // Initialize camera (centered on grid)
  camera_.target = {static_cast<float>(grid_nx_ * config_.tile_size) / 2.0f,
                    static_cast<float>(grid_ny_ * config_.tile_size) / 2.0f};
  camera_.offset = {static_cast<float>(config_.window_width) / 2.0f,
                    static_cast<float>(config_.window_height) / 2.0f};
  camera_.rotation = 0.0f;
  camera_.zoom = 1.0f;
}

void Renderer::shutdown() { CloseWindow(); }

void Renderer::update_input() {
  handle_camera_input();
  handle_overlay_input();
  handle_simulation_input();
}

void Renderer::handle_camera_input() {
  // Pan with WASD or arrow keys
  const float pan_speed = 400.0f / camera_.zoom; // Faster pan when zoomed out
  float dt = GetFrameTime();

  if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))
    camera_.target.y -= pan_speed * dt;
  if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))
    camera_.target.y += pan_speed * dt;
  if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))
    camera_.target.x -= pan_speed * dt;
  if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))
    camera_.target.x += pan_speed * dt;

  // Pan with middle mouse button drag
  Vector2 mouse_pos = GetMousePosition();
  if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
    dragging_ = true;
    last_mouse_pos_ = mouse_pos;
  }
  if (IsMouseButtonReleased(MOUSE_BUTTON_MIDDLE)) {
    dragging_ = false;
  }
  if (dragging_) {
    Vector2 delta = {mouse_pos.x - last_mouse_pos_.x,
                     mouse_pos.y - last_mouse_pos_.y};
    camera_.target.x -= delta.x / camera_.zoom;
    camera_.target.y -= delta.y / camera_.zoom;
    last_mouse_pos_ = mouse_pos;
  }

  // Zoom with mouse wheel
  float wheel = GetMouseWheelMove();
  if (wheel != 0) {
    // Zoom toward mouse position
    Vector2 mouse_world_before = GetScreenToWorld2D(mouse_pos, camera_);
    camera_.zoom += wheel * 0.1f * camera_.zoom;
    camera_.zoom = std::clamp(camera_.zoom, 0.1f, 10.0f);
    Vector2 mouse_world_after = GetScreenToWorld2D(mouse_pos, camera_);
    camera_.target.x += mouse_world_before.x - mouse_world_after.x;
    camera_.target.y += mouse_world_before.y - mouse_world_after.y;
  }

  // Update camera offset if window resized
  camera_.offset = {static_cast<float>(GetScreenWidth()) / 2.0f,
                    static_cast<float>(GetScreenHeight()) / 2.0f};
}

void Renderer::handle_overlay_input() {
  if (IsKeyPressed(KEY_ONE))
    active_overlay_ = OverlayType::PRESSURE;
  if (IsKeyPressed(KEY_TWO))
    active_overlay_ = OverlayType::TEMPERATURE;
  if (IsKeyPressed(KEY_THREE))
    active_overlay_ = OverlayType::OXYGEN;
  if (IsKeyPressed(KEY_ZERO) || IsKeyPressed(KEY_GRAVE))
    active_overlay_ = OverlayType::NONE;

  // Z-level with Q/E
  if (IsKeyPressed(KEY_E))
    current_z_ = std::min(current_z_ + 1, static_cast<int>(grid_nz_) - 1);
  if (IsKeyPressed(KEY_Q))
    current_z_ = std::max(current_z_ - 1, 0);
}

void Renderer::handle_simulation_input() {
  // Pause/resume with Space
  if (IsKeyPressed(KEY_SPACE))
    paused_ = !paused_;

  // Step with N (when paused)
  if (IsKeyPressed(KEY_N) && paused_)
    step_requested_ = true;

  // Time scale with +/-
  if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) {
    if (time_scale_ < 10.0f)
      time_scale_ *= 2.0f;
  }
  if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) {
    if (time_scale_ > 0.125f)
      time_scale_ /= 2.0f;
  }
}

void Renderer::begin_frame() {
  BeginDrawing();
  ClearBackground({15, 12, 8, 255}); // DF-style dark brown/black
  BeginMode2D(camera_);
}

void Renderer::draw_grid(const fluids::LBMEngine &fluids,
                         const thermal::ThermalEngine &thermal) {
  int tile = config_.tile_size;
  int z = current_z_;
  int font_size = tile - 2;

  size_t nx = grid_nx_;
  size_t ny = grid_ny_;

  // DF-style ASCII floor characters
  static const char *floor_chars[] = {".", ".", ".", ",", "`", "'"};
  static const int n_floor = 6;

  // TODO: OPTIMIZATION NEEDED FOR LARGE GRIDS (500x500+)
  // Current approach: 10,000 DrawRectangle + DrawText calls per frame for 100x100
  // This works at 60 FPS for 100x100 but will drop to ~5 FPS at 500x500.
  // Fix: Render to an Image, upload to Texture2D once, draw single texture.
  // Keep current code for debugging, refactor when scaling up grid size.

  for (size_t y = 0; y < ny; ++y) {
    for (size_t x = 0; x < nx; ++x) {
      int px = static_cast<int>(x * tile);
      int py = static_cast<int>(y * tile);

      // Procedural variation based on position (deterministic noise)
      unsigned int hash = static_cast<unsigned int>(x * 374761393 + y * 668265263);
      hash = (hash ^ (hash >> 13)) * 1274126177;
      int variant = hash % 100;

      // DF-style base colors: dark browns and grays
      Color bg_color, fg_color;
      const char *glyph;

      double temp = thermal.get_temperature(x, y, z);
      double density = fluids.get_density(x, y, z);

      if (active_overlay_ == OverlayType::NONE) {
        // Classic DF floor look
        if (variant < 5) {
          // Occasional rock/ore
          bg_color = {35, 30, 25, 255};
          fg_color = {80, 75, 65, 255};
          glyph = "*";
        } else {
          // Normal floor
          bg_color = {25, 22, 18, 255};
          fg_color = {static_cast<unsigned char>(100 + (variant % 30)),
                      static_cast<unsigned char>(90 + (variant % 25)),
                      static_cast<unsigned char>(70 + (variant % 20)), 255};
          glyph = floor_chars[hash % n_floor];
        }
      } else if (active_overlay_ == OverlayType::TEMPERATURE) {
        // Temperature visualization
        Color overlay = temperature_to_color(temp, 200.0, 500.0);
        bg_color = {static_cast<unsigned char>(overlay.r / 4),
                    static_cast<unsigned char>(overlay.g / 4),
                    static_cast<unsigned char>(overlay.b / 4), 255};
        fg_color = overlay;

        // Temperature-appropriate glyph
        if (temp > 400) {
          glyph = "~";  // Hot/fire
        } else if (temp < 250) {
          glyph = "#";  // Frozen
        } else {
          glyph = floor_chars[hash % n_floor];
        }
      } else if (active_overlay_ == OverlayType::PRESSURE) {
        Color overlay = pressure_to_color(density, 0.95, 1.05);
        bg_color = {static_cast<unsigned char>(overlay.r / 4),
                    static_cast<unsigned char>(overlay.g / 4),
                    static_cast<unsigned char>(overlay.b / 4), 255};
        fg_color = overlay;
        glyph = floor_chars[hash % n_floor];
      } else if (active_overlay_ == OverlayType::OXYGEN) {
        double o2 = fluids.get_species_density("O2", x, y, z);
        double total = fluids.get_density(x, y, z);
        double fraction = (total > 0) ? o2 / total : 0.0;
        Color overlay = oxygen_to_color(fraction, 0.16, 0.21);
        bg_color = {static_cast<unsigned char>(overlay.r / 4),
                    static_cast<unsigned char>(overlay.g / 4),
                    static_cast<unsigned char>(overlay.b / 4), 255};
        fg_color = overlay;
        glyph = floor_chars[hash % n_floor];
      } else {
        bg_color = {25, 22, 18, 255};
        fg_color = {80, 75, 65, 255};
        glyph = ".";
      }

      // Draw tile background
      DrawRectangle(px, py, tile, tile, bg_color);

      // Draw ASCII character centered in tile
      int text_x = px + (tile - MeasureText(glyph, font_size)) / 2;
      int text_y = py + (tile - font_size) / 2;
      DrawText(glyph, text_x, text_y, font_size, fg_color);
    }
  }
}

void Renderer::draw_hud() {
  EndMode2D(); // Exit camera mode for HUD

  // Background panel
  DrawRectangle(10, 10, 250, 120, {0, 0, 0, 180});

  // Title
  DrawText("ISOLATED", 20, 15, 20, WHITE);

  // Overlay indicator
  const char *overlay_name = "None";
  switch (active_overlay_) {
  case OverlayType::PRESSURE:
    overlay_name = "Pressure";
    break;
  case OverlayType::TEMPERATURE:
    overlay_name = "Temperature";
    break;
  case OverlayType::OXYGEN:
    overlay_name = "Oxygen";
    break;
  default:
    break;
  }
  DrawText(TextFormat("Overlay: %s", overlay_name), 20, 40, 16, LIGHTGRAY);

  // Z-level
  DrawText(TextFormat("Z-Level: %d", current_z_), 20, 58, 16, LIGHTGRAY);

  // Simulation state
  const char *state = paused_ ? "PAUSED" : "RUNNING";
  Color state_color = paused_ ? YELLOW : GREEN;
  DrawText(TextFormat("Sim: %s (%.1fx)", state, time_scale_), 20, 76, 16,
           state_color);

  // FPS
  DrawText(TextFormat("FPS: %d", GetFPS()), 20, 94, 16, GRAY);

  // Zoom
  DrawText(TextFormat("Zoom: %.1fx", camera_.zoom), 20, 112, 14, GRAY);

  // Controls help (bottom)
  int help_y = GetScreenHeight() - 25;
  DrawText("WASD:Pan  Wheel:Zoom  Q/E:Z-Level  1/2/3/0:Overlay  Space:Pause  "
           "N:Step  +/-:Speed",
           10, help_y, 14, {100, 100, 100, 255});
}

void Renderer::end_frame() { EndDrawing(); }

Color Renderer::get_base_tile_color(bool is_solid) const {
  return is_solid ? Color{80, 70, 60, 255} : Color{40, 40, 45, 255};
}

} // namespace renderer
} // namespace isolated
