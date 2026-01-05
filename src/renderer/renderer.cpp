/**
 * @file renderer.cpp
 * @brief Implementation of the Raylib-based simulation renderer.
 */

#include <isolated/renderer/renderer.hpp>
#include <isolated/renderer/color_maps.hpp>
#include <isolated/entities/components.hpp>
#include "entt/entt.hpp"

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

  // Initialize grid texture for optimized rendering
  int tex_width = static_cast<int>(grid_nx_ * config_.tile_size);
  int tex_height = static_cast<int>(grid_ny_ * config_.tile_size);
  grid_image_ = GenImageColor(tex_width, tex_height, BLACK);
  grid_texture_ = LoadTextureFromImage(grid_image_);
  grid_texture_initialized_ = true;
}

void Renderer::shutdown() {
  if (grid_texture_initialized_) {
    UnloadTexture(grid_texture_);
    UnloadImage(grid_image_);
    grid_texture_initialized_ = false;
  }
  CloseWindow();
}

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

  size_t nx = grid_nx_;
  size_t ny = grid_ny_;
  int img_width = static_cast<int>(nx * tile);

  // OPTIMIZED: Direct memory access to pixel buffer (no function call overhead)
  // This replaces millions of ImageDrawPixel calls with direct pointer writes
  Color *pixels = reinterpret_cast<Color *>(grid_image_.data);

  // Parallelize outer loop with OpenMP for large grids
  #pragma omp parallel for schedule(dynamic, 16)
  for (int y = 0; y < static_cast<int>(ny); ++y) {
    for (int x = 0; x < static_cast<int>(nx); ++x) {
      // Procedural variation based on position (deterministic noise)
      unsigned int hash = static_cast<unsigned int>(x * 374761393 + y * 668265263);
      hash = (hash ^ (hash >> 13)) * 1274126177;

      // Get cell color based on overlay
      Color cell_color = get_cell_color(fluids, thermal, x, y, z, hash);

      // Pre-calculate darkened border color
      Color border_color = {
          static_cast<unsigned char>(cell_color.r * 0.7),
          static_cast<unsigned char>(cell_color.g * 0.7),
          static_cast<unsigned char>(cell_color.b * 0.7),
          255};

      // Fill tile pixels directly in memory
      int px_base = x * tile;
      int py_base = y * tile;

      for (int ty = 0; ty < tile; ++ty) {
        int row_offset = (py_base + ty) * img_width + px_base;
        bool is_border_row = (ty == tile - 1);

        for (int tx = 0; tx < tile; ++tx) {
          // Grid line on right/bottom edge
          bool is_border = is_border_row || (tx == tile - 1);
          pixels[row_offset + tx] = is_border ? border_color : cell_color;
        }
      }
    }
  }

  // Upload updated image to GPU texture (single operation)
  UpdateTexture(grid_texture_, grid_image_.data);

  // Draw the entire grid with one draw call
  DrawTexture(grid_texture_, 0, 0, WHITE);
}

// Helper: Get color for a cell based on current overlay
Color Renderer::get_cell_color(const fluids::LBMEngine &fluids,
                               const thermal::ThermalEngine &thermal,
                               size_t x, size_t y, int z, unsigned int hash) const {
  int variant = hash % 100;

  double temp = thermal.get_temperature(x, y, z);
  double density = fluids.get_density(x, y, z);

  if (active_overlay_ == OverlayType::NONE) {
    // Classic DF floor look
    if (variant < 5) {
      return {35, 30, 25, 255}; // Occasional rock
    } else {
      return {static_cast<unsigned char>(25 + (variant % 15)),
              static_cast<unsigned char>(22 + (variant % 12)),
              static_cast<unsigned char>(18 + (variant % 10)), 255};
    }
  } else if (active_overlay_ == OverlayType::TEMPERATURE) {
    return temperature_to_color(temp, 200.0, 500.0);
  } else if (active_overlay_ == OverlayType::PRESSURE) {
    return pressure_to_color(density, 0.95, 1.05);
  } else if (active_overlay_ == OverlayType::OXYGEN) {
    double o2 = fluids.get_species_density("O2", x, y, z);
    double total = fluids.get_density(x, y, z);
    double fraction = (total > 0) ? o2 / total : 0.0;
    return oxygen_to_color(fraction, 0.16, 0.21);
  }

  return {25, 22, 18, 255};
}


void Renderer::draw_entities(const void* registry_ptr) {
  if (!registry_ptr) return;

  const entt::registry& registry = *static_cast<const entt::registry*>(registry_ptr);

  int tile = config_.tile_size;
  int z = current_z_;
  
  // Use the default font as a texture atlas for DF-style rendering
  Font font = GetFontDefault();
  // Ensure sharp pixel-art scaling
  SetTextureFilter(font.texture, TEXTURE_FILTER_POINT);

  auto view = registry.view<const isolated::entities::Position, const isolated::entities::Renderable>();
  
  // Pre-calculate glyph info for efficiency (assuming mostly '@')
  // We can cache this if needed, but for now looking it up is fast enough
  
  for (auto [entity, pos, render] : view.each()) {
    if (pos.z != z) continue;
    if (pos.x < 0 || pos.x >= grid_nx_ || pos.y < 0 || pos.y >= grid_ny_) continue;
    
    float px = static_cast<float>(pos.x * tile);
    float py = static_cast<float>(pos.y * tile);
    
    // 1. Get Glyph Source Rectangle from Font Atlas
    int glyph_index = GetGlyphIndex(font, render.glyph);
    Rectangle src_rec = font.recs[glyph_index];
    
    // 2. Calculate Aspect-Correct Fit within the Tile
    // We want the glyph to fit inside 'tile' size, centered.
    // Default font glyphs usually have some padding, but we'll try to fit the bounding box.
    
    float aspect = src_rec.width / src_rec.height;
    float target_h = static_cast<float>(tile);
    float target_w = target_h * aspect;
    
    // If width exceeds tile (rare for vertical tiles), constrain width
    if (target_w > tile) {
        target_w = static_cast<float>(tile);
        target_h = target_w / aspect;
    }
    
    // Center it
    float off_x = (tile - target_w) / 2.0f;
    float off_y = (tile - target_h) / 2.0f;
    
    Rectangle dest_rec = { px + off_x, py + off_y, target_w, target_h };
    Vector2 origin = { 0, 0 };
    
    // 3. Draw using Texture (Sprites)
    // This scales perfectly with camera zoom because it's just a textured quad in world space.
    DrawTexturePro(font.texture, src_rec, dest_rec, origin, 0.0f, render.color);

    // Selection Highlight
    if (entity == selected_entity_) {
      DrawRectangleLines(px, py, tile, tile, YELLOW);
      // Make selection independent of tile size visual
      float thickness = 1.0f / camera_.zoom; // Keep 1px screen thickness? Or just let it scale.
      // Let's just draw a second box inside if we are zoomed in enough
      if (tile * camera_.zoom > 10) {
           DrawRectangleLines(px + 1, py + 1, tile - 2, tile - 2, YELLOW);
      }
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
