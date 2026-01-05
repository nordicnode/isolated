/**
 * @file renderer.cpp
 * @brief Implementation of the Raylib-based simulation renderer.
 */

#include <isolated/renderer/renderer.hpp>
#include <isolated/renderer/color_maps.hpp>
#include <isolated/entities/components.hpp>
#include <isolated/world/chunk_manager.hpp>
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

  // Z-level with Q/E (allow negative for underground)
  if (IsKeyPressed(KEY_E))
    current_z_ = std::min(current_z_ + 1, 150);  // Max Z = 150 (well above surface)
  if (IsKeyPressed(KEY_Q))
    current_z_ = std::max(current_z_ - 1, -100);  // Min Z = -100 (deep underground)
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

void Renderer::draw_chunks(void* chunk_manager_ptr) {
    if (!chunk_manager_ptr) return;
    auto& chunk_manager = *static_cast<world::ChunkManager*>(chunk_manager_ptr);
    
    int tile = config_.tile_size;
    
    // Get loaded chunks
    auto chunks = chunk_manager.get_loaded_chunks();
    
    // === VIEWPORT CALCULATION (once per frame) ===
    Vector2 top_left = GetScreenToWorld2D({0, 0}, camera_);
    Vector2 bottom_right = GetScreenToWorld2D(
        {(float)GetScreenWidth(), (float)GetScreenHeight()}, camera_);
    
    int view_x_min = (int)floor(top_left.x / tile) - 1;
    int view_y_min = (int)floor(top_left.y / tile) - 1;
    int view_x_max = (int)ceil(bottom_right.x / tile) + 1;
    int view_y_max = (int)ceil(bottom_right.y / tile) + 1;
    
    // DF-style: Draw multiple Z-levels with depth fog
    for (int z_offset = -2; z_offset <= 0; z_offset++) {
        int z_layer = current_z_ + z_offset;
        float alpha_mult = 1.0f - (float)(-z_offset) * 0.35f;
        
        for (const auto* chunk : chunks) {
            if (!chunk || !chunk->generated) continue;
            
            auto [ox, oy, oz] = chunk->world_origin();
            
            // Skip if chunk doesn't contain current Z layer
            if (z_layer < oz || z_layer >= oz + world::CHUNK_SIZE) continue;
            
            // Skip if chunk is entirely outside viewport (CHUNK-LEVEL CULLING)
            if (ox + world::CHUNK_SIZE <= view_x_min || ox > view_x_max ||
                oy + world::CHUNK_SIZE <= view_y_min || oy > view_y_max) continue;
            
            int local_z = z_layer - oz;
            
            // Calculate visible tile range within this chunk
            int tile_x_start = std::max(0, view_x_min - ox);
            int tile_x_end = std::min((int)world::CHUNK_SIZE - 1, view_x_max - ox);
            int tile_y_start = std::max(0, view_y_min - oy);
            int tile_y_end = std::min((int)world::CHUNK_SIZE - 1, view_y_max - oy);
            
            for (int y = tile_y_start; y <= tile_y_end; y++) {
                for (int x = tile_x_start; x <= tile_x_end; x++) {
                    size_t idx = world::Chunk::idx(x, y, local_z);
                    if (idx >= world::CHUNK_CELLS) continue;
                    
                    world::Material mat = chunk->material[idx];
                    if (mat == world::Material::AIR) continue;
                    
                    int mat_val = static_cast<int>(mat);
                    if (mat_val < 0 || mat_val > 110) continue;
                    
                    int world_x = ox + x;
                    int world_y = oy + y;
                    
                    // Material colors
                    Color base_color = {80, 80, 80, 255};
                    switch (mat) {
                        case world::Material::GRANITE:   base_color = {60, 50, 50, 255}; break;
                        case world::Material::BASALT:    base_color = {40, 40, 45, 255}; break;
                        case world::Material::LIMESTONE: base_color = {180, 180, 170, 255}; break;
                        case world::Material::SOIL:      base_color = {101, 67, 33, 255}; break;
                        case world::Material::WATER:     base_color = {30, 100, 200, 255}; break;
                        case world::Material::ICE:       base_color = {200, 220, 255, 255}; break;
                        case world::Material::SANDSTONE: base_color = {194, 178, 128, 255}; break;
                        case world::Material::REGOLITH:  base_color = {160, 150, 140, 255}; break;
                        case world::Material::IRON_ORE:  base_color = {150, 90, 70, 255}; break;
                        case world::Material::COPPER_ORE: base_color = {180, 115, 75, 255}; break;
                        case world::Material::SHALE:     base_color = {70, 70, 80, 255}; break;
                        case world::Material::MARBLE:    base_color = {240, 235, 230, 255}; break;
                        default: break;
                    }
                    
                    // Apply depth fog
                    if (z_offset < 0) {
                        base_color.r = (unsigned char)(base_color.r * alpha_mult * 0.7f);
                        base_color.g = (unsigned char)(base_color.g * alpha_mult * 0.7f);
                        base_color.b = (unsigned char)(base_color.b * alpha_mult * 0.7f);
                    }
                    
                    // Coord variation
                    unsigned int seed = world_x * 73856093 ^ world_y * 19349663 ^ z_layer * 83492791;
                    int var = (seed % 20) - 10;
                    if (mat != world::Material::WATER && mat != world::Material::ICE) {
                        base_color.r = (unsigned char)std::clamp(base_color.r + var, 0, 255);
                        base_color.g = (unsigned char)std::clamp(base_color.g + var, 0, 255);
                        base_color.b = (unsigned char)std::clamp(base_color.b + var, 0, 255);
                    }
                    
                    DrawRectangle(world_x * tile, world_y * tile, tile, tile, base_color);
                    
                    // Overlays only on current Z
                    if (z_offset == 0 && active_overlay_ != OverlayType::NONE) {
                        double temp = chunk->temperature[idx];
                        double dens = chunk->density[idx];
                        Color overlay = {0, 0, 0, 0};
                        
                        switch (active_overlay_) {
                            case OverlayType::TEMPERATURE: {
                                double t = std::clamp((temp - 200.0) / 200.0, 0.0, 1.0);
                                overlay.r = static_cast<unsigned char>(t * 255);
                                overlay.g = static_cast<unsigned char>((1.0 - std::abs(t - 0.5) * 2.0) * 100);
                                overlay.b = static_cast<unsigned char>((1.0 - t) * 255);
                                overlay.a = 100;
                                break;
                            }
                            case OverlayType::PRESSURE: {
                                double d = std::clamp((dens - 1.0) / 3000.0, 0.0, 1.0);
                                overlay.r = static_cast<unsigned char>(d * 255);
                                overlay.g = static_cast<unsigned char>(d * 200);
                                overlay.b = static_cast<unsigned char>((1.0 - d) * 200);
                                overlay.a = 100;
                                break;
                            }
                            case OverlayType::OXYGEN: {
                                double o2 = (chunk->o2_fraction.size() > idx) ? chunk->o2_fraction[idx] : 0.21;
                                double o = std::clamp(o2 / 0.21, 0.0, 1.0);
                                overlay.r = static_cast<unsigned char>((1.0 - o) * 200);
                                overlay.g = static_cast<unsigned char>(o * 200);
                                overlay.b = 50;
                                overlay.a = 100;
                                break;
                            }
                            default: break;
                        }
                        if (overlay.a > 0) {
                            DrawRectangle(world_x * tile, world_y * tile, tile, tile, overlay);
                        }
                    }
                }
            }
        }
    }
    
    // Chunk borders (only for visible chunks)
    for (const auto* chunk : chunks) {
        if (!chunk) continue;
        auto [ox, oy, oz] = chunk->world_origin();
        if (current_z_ < oz || current_z_ >= oz + world::CHUNK_SIZE) continue;
        if (ox + world::CHUNK_SIZE <= view_x_min || ox > view_x_max ||
            oy + world::CHUNK_SIZE <= view_y_min || oy > view_y_max) continue;
        DrawRectangleLines(ox * tile, oy * tile, 
                           world::CHUNK_SIZE * tile, 
                           world::CHUNK_SIZE * tile, 
                           {255, 255, 255, 20});
    }
}

void Renderer::draw_cursor(int x, int y, int z, Color color) {
    if (z != current_z_) return; // Only draw if on current layer
    
    int tile = config_.tile_size;
    
    // Draw thick border
    DrawRectangleLinesEx({
        (float)x * tile, 
        (float)y * tile, 
        (float)tile, 
        (float)tile
    }, 2.0f, color);
    
    // Slight fill
    Color fill = color;
    fill.a = 30;
    DrawRectangle(x * tile, y * tile, tile, tile, fill);
}

Color Renderer::get_cell_color(const fluids::LBMEngine &fluids,
                               const thermal::ThermalEngine &thermal,
                               size_t x, size_t y, int z, unsigned int hash) const {
  // Legacy flat grid renderer - kept for overlay support if needed
  // ... (implementation unchanged)
  int variant = hash % 100;
  // ...
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
