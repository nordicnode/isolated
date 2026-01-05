/**
 * @file debug_ui.cpp
 * @brief Dwarf Fortress-style unified sidebar UI implementation.
 */

#include <isolated/renderer/debug_ui.hpp>
#include <isolated/fluids/lbm_engine.hpp>
#include <isolated/thermal/heat_engine.hpp>
#include <isolated/entities/components.hpp>
#include <isolated/world/chunk_manager.hpp>

#include "imgui.h"
#include "rlImGui.h"
#include "raylib.h"

namespace isolated {
namespace renderer {

void DebugUI::init() {
  rlImGuiSetup(true);
  initialized_ = true;

  // Configure Dwarf Fortress-style ImGui theme
  ImGuiStyle &style = ImGui::GetStyle();

  // DF-style: No rounding, sharp edges
  style.WindowRounding = 0.0f;
  style.FrameRounding = 0.0f;
  style.ScrollbarRounding = 0.0f;
  style.GrabRounding = 0.0f;
  style.TabRounding = 0.0f;

  // Compact spacing
  style.WindowPadding = ImVec2(8, 8);
  style.FramePadding = ImVec2(4, 2);
  style.ItemSpacing = ImVec2(6, 4);
  style.ItemInnerSpacing = ImVec2(4, 4);

  // Borders
  style.WindowBorderSize = 1.0f;
  style.FrameBorderSize = 1.0f;

  // DF-inspired colors
  ImVec4 *colors = style.Colors;

  // Backgrounds - very dark brown/black
  colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.05f, 0.03f, 0.98f);
  colors[ImGuiCol_ChildBg] = ImVec4(0.04f, 0.03f, 0.02f, 1.00f);
  colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.06f, 0.04f, 0.98f);

  // Borders
  colors[ImGuiCol_Border] = ImVec4(0.40f, 0.35f, 0.25f, 0.60f);

  // Frames
  colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.08f, 0.05f, 1.00f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.14f, 0.08f, 1.00f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.20f, 0.10f, 1.00f);

  // Title bar - cyan/teal
  colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.15f, 0.20f, 1.00f);
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.06f, 0.22f, 0.28f, 1.00f);
  colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.02f, 0.10f, 0.12f, 0.80f);

  // Text - amber/gold
  colors[ImGuiCol_Text] = ImVec4(0.85f, 0.75f, 0.45f, 1.00f);
  colors[ImGuiCol_TextDisabled] = ImVec4(0.45f, 0.40f, 0.28f, 1.00f);

  // Buttons
  colors[ImGuiCol_Button] = ImVec4(0.18f, 0.14f, 0.08f, 1.00f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.22f, 0.12f, 1.00f);
  colors[ImGuiCol_ButtonActive] = ImVec4(0.40f, 0.30f, 0.15f, 1.00f);

  // Headers - cyan
  colors[ImGuiCol_Header] = ImVec4(0.08f, 0.20f, 0.25f, 1.00f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.12f, 0.30f, 0.35f, 1.00f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.16f, 0.38f, 0.45f, 1.00f);

  // Separator
  colors[ImGuiCol_Separator] = ImVec4(0.40f, 0.35f, 0.25f, 0.50f);

  // Slider
  colors[ImGuiCol_SliderGrab] = ImVec4(0.65f, 0.50f, 0.25f, 1.00f);
  colors[ImGuiCol_SliderGrabActive] = ImVec4(0.80f, 0.65f, 0.30f, 1.00f);

  // Checkmark
  colors[ImGuiCol_CheckMark] = ImVec4(0.40f, 0.75f, 0.40f, 1.00f);

  // Scrollbar
  colors[ImGuiCol_ScrollbarBg] = ImVec4(0.04f, 0.03f, 0.02f, 1.00f);
  colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.22f, 0.18f, 0.10f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.32f, 0.26f, 0.14f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.42f, 0.34f, 0.18f, 1.00f);

  // Plot
  colors[ImGuiCol_PlotLines] = ImVec4(0.65f, 0.50f, 0.25f, 1.00f);
  colors[ImGuiCol_PlotHistogram] = ImVec4(0.45f, 0.65f, 0.30f, 1.00f);
}

void DebugUI::shutdown() {
  if (initialized_) {
    rlImGuiShutdown();
    initialized_ = false;
  }
}

void DebugUI::begin_frame() { rlImGuiBegin(); }
void DebugUI::end_frame() { rlImGuiEnd(); }

void DebugUI::draw_sidebar(const fluids::LBMEngine &fluids,
                           const thermal::ThermalEngine &thermal,
                           const Camera2D &camera, int tile_size, int z_level,
                           bool &paused, float &time_scale,
                           double sim_step_time_ms, double sim_time,
                           void* chunk_manager,
                           entt::registry* registry,
                           entt::entity selected_entity) {
  // Fixed left sidebar
  float sidebar_width = 250.0f; // Widened for better fit
  ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(sidebar_width, static_cast<float>(GetScreenHeight())),
                           ImGuiCond_Always);

  ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                           ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

  if (ImGui::Begin("##Sidebar", nullptr, flags)) {
    // === TITLE ===
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 0.9f, 1.0f));
    ImGui::Text("ISOLATED");
    ImGui::PopStyleColor();
    ImGui::SameLine(sidebar_width - 70);
    ImGui::TextDisabled("v0.1.0");
    ImGui::Separator();

    // === SIMULATION STATUS ===
    if (ImGui::CollapsingHeader("Simulation", ImGuiTreeNodeFlags_DefaultOpen)) {
      // Status indicator
      if (paused) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
        ImGui::Text("[PAUSED]");
        ImGui::PopStyleColor();
      } else {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.9f, 0.3f, 1.0f));
        ImGui::Text("[RUNNING]");
        ImGui::PopStyleColor();
      }
      ImGui::SameLine();
      ImGui::Text("%.1fx", time_scale);

      ImGui::Text("Time: %.1f s", sim_time);

      // Controls
      ImGui::Spacing();
      if (ImGui::Button(paused ? "[RESUME]" : "[PAUSE]", ImVec2(-1, 0))) {
        paused = !paused;
      }

      ImGui::Text("Speed:");
      ImGui::SameLine();
      if (ImGui::SmallButton("0.5x")) time_scale = 0.5f;
      ImGui::SameLine();
      if (ImGui::SmallButton("1x")) time_scale = 1.0f;
      ImGui::SameLine();
      if (ImGui::SmallButton("2x")) time_scale = 2.0f;
      ImGui::SameLine();
      if (ImGui::SmallButton("5x")) time_scale = 5.0f;

      ImGui::SliderFloat("##speed", &time_scale, 0.1f, 10.0f, "");
    }

    ImGui::Spacing();

    // === CELL INSPECTOR ===
    if (ImGui::CollapsingHeader("Cell Inspector", ImGuiTreeNodeFlags_DefaultOpen)) {
      Vector2 mouse_screen = GetMousePosition();
      Vector2 mouse_world = GetScreenToWorld2D(mouse_screen, camera);

      int cell_x = static_cast<int>(mouse_world.x / tile_size);
      int cell_y = static_cast<int>(mouse_world.y / tile_size);
      
      bool hovering_valid = (cell_x >= 0 && cell_y >= 0 && mouse_screen.x > sidebar_width);
      
      // Click-to-lock: left click on map locks the inspector to that tile
      if (hovering_valid && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !is_capturing_mouse()) {
        inspected_x_ = cell_x;
        inspected_y_ = cell_y;
        inspected_z_ = z_level;
        tile_locked_ = true;
      }
      
      // Right click or escape unlocks
      if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) || IsKeyPressed(KEY_ESCAPE)) {
        tile_locked_ = false;
      }
      
      // If not locked, update to hover position
      if (!tile_locked_ && hovering_valid) {
        inspected_x_ = cell_x;
        inspected_y_ = cell_y;
        inspected_z_ = z_level;
      }

      if (inspected_x_ >= 0 && inspected_y_ >= 0) {
        // Show lock status
        if (tile_locked_) {
          ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "LOCKED (right-click to unlock)");
        }
        
        ImGui::Text("Pos: (%d, %d, %d)", inspected_x_, inspected_y_, inspected_z_);
        ImGui::Separator();
        
        // === CHUNK MATERIAL INFO ===
        if (chunk_manager) {
          auto* cm = static_cast<world::ChunkManager*>(chunk_manager);
          world::Material mat = cm->get_material(inspected_x_, inspected_y_, inspected_z_);
          const char* mat_name = world::material_to_string(mat);
          
          // Color code by material type
          ImVec4 mat_color;
          if (mat == world::Material::AIR) {
            mat_color = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
          } else if (static_cast<int>(mat) < 10) { // Gases
            mat_color = ImVec4(0.7f, 0.9f, 1.0f, 1.0f);
          } else if (static_cast<int>(mat) < 20) { // Liquids
            mat_color = ImVec4(0.2f, 0.6f, 1.0f, 1.0f);
          } else { // Solids
            mat_color = ImVec4(0.9f, 0.7f, 0.5f, 1.0f);
          }
          
          ImGui::TextColored(mat_color, "Material: %s", mat_name);
          
          // Get chunk-specific temp/density
          double chunk_temp = cm->get_temperature(inspected_x_, inspected_y_, inspected_z_);
          double chunk_dens = cm->get_density(inspected_x_, inspected_y_, inspected_z_);
          
          ImGui::Text("Chunk Temp: %.0f K", chunk_temp);
          ImGui::Text("Chunk Dens: %.1f kg/m³", chunk_dens);
        }
        
        ImGui::Separator();
        
        // === PHYSICS INFO (from flat LBM/Thermal) ===
        if (inspected_x_ < 200 && inspected_y_ < 200) {
          double density = fluids.get_density(inspected_x_, inspected_y_, 0);
          auto velocity = fluids.get_velocity(inspected_x_, inspected_y_, 0);
          ImGui::Text("Pressure: %.3f", density);
          ImGui::Text("Flow: (%.2f, %.2f)", velocity[0], velocity[1]);

          // Gas composition
          double o2 = fluids.get_species_density("O2", inspected_x_, inspected_y_, 0);
          double n2 = fluids.get_species_density("N2", inspected_x_, inspected_y_, 0);
          double co2 = fluids.get_species_density("CO2", inspected_x_, inspected_y_, 0);
          double total = o2 + n2 + co2 + 1e-10;

          ImGui::Text("O2: %.0f%% N2: %.0f%%", (o2/total)*100, (n2/total)*100);
          ImGui::Text("CO2: %.2f%%", (co2/total)*100);

          // Temperature
          double temp = thermal.get_temperature(inspected_x_, inspected_y_, 0);
          ImGui::PushStyleColor(ImGuiCol_Text,
              temp > 350 ? ImVec4(1.0f, 0.5f, 0.2f, 1.0f) :
              temp < 270 ? ImVec4(0.4f, 0.7f, 1.0f, 1.0f) :
                           ImVec4(0.85f, 0.75f, 0.45f, 1.0f));
          ImGui::Text("Temp: %.0f K (%.0f C)", temp, temp - 273.15);
          ImGui::PopStyleColor();
        }
      } else {
        ImGui::TextDisabled("Hover over map (Left-click to lock)");
      }
    }

    ImGui::Spacing();

    // === ENTITY INSPECTOR ===
    if (registry && ImGui::CollapsingHeader("Entity Inspector", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (selected_entity != entt::null && registry->valid(selected_entity)) {
            if (ImGui::BeginTabBar("EntityTabs")) {
                
                // === TAB 1: OVERVIEW ===
                if (ImGui::BeginTabItem("Overview")) {
                    ImGui::Spacing();
                    // ID & Name
                    ImGui::Text("ID: %d", static_cast<int>(selected_entity));
                    if (auto* astronaut = registry->try_get<entities::Astronaut>(selected_entity)) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
                        ImGui::Text("Name: %s", astronaut->name.c_str());
                        ImGui::PopStyleColor();
                    }
                    ImGui::Separator();
                    
                    // Position
                    if (auto* pos = registry->try_get<entities::Position>(selected_entity)) {
                        ImGui::Text("Pos: (%.1f, %.1f, %d)", pos->x, pos->y, pos->z);
                    }

                    // Velocity
                    if (auto* vel = registry->try_get<entities::Velocity>(selected_entity)) {
                        ImGui::Text("Vel: (%.3f, %.3f)", vel->dx, vel->dy);
                    }
                    
                    // Visuals
                    if (auto* render = registry->try_get<entities::Renderable>(selected_entity)) {
                        ImGui::Spacing();
                        ImGui::Text("Appearance:");
                        ImVec4 col(render->color.r/255.f, render->color.g/255.f, render->color.b/255.f, 1.0f);
                        ImGui::ColorButton("##color", col, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(30, 30));
                        ImGui::SameLine();
                        ImGui::SetWindowFontScale(2.0f); // Big glyph
                        ImGui::Text("'%c'", render->glyph);
                        ImGui::SetWindowFontScale(1.0f);
                    }
                    ImGui::EndTabItem();
                }

                // === TAB 2: SURVIVAL ===
                if (ImGui::BeginTabItem("Survival")) {
                     if (auto* needs = registry->try_get<entities::Needs>(selected_entity)) {
                        ImGui::Spacing();
                        
                        // Hypoxia Status
                        ImGui::Text("Status:");
                        ImGui::SameLine();
                        const char* hypoxia_label = "Healthy";
                        ImVec4 state_color = ImVec4(0.2f, 0.8f, 0.2f, 1.0f); // Green

                        switch (needs->hypoxia_state) {
                            case entities::HypoxiaState::CONFUSED:
                                hypoxia_label = "CONFUSED";
                                state_color = ImVec4(0.9f, 0.7f, 0.1f, 1.0f);
                                break;
                            case entities::HypoxiaState::COLLAPSED:
                                hypoxia_label = "COLLAPSED";
                                state_color = ImVec4(0.9f, 0.3f, 0.1f, 1.0f);
                                break;
                            case entities::HypoxiaState::DEAD:
                                hypoxia_label = "DEAD";
                                state_color = ImVec4(0.5f, 0.0f, 0.0f, 1.0f);
                                break;
                            default: break;
                        }
                        ImGui::TextColored(state_color, "%s", hypoxia_label);
                        ImGui::Separator();

                        // Needs Bars
                        char buf[32];
                        
                        // Helper: Label ABOVE bar for readability
                        auto draw_bar = [](const char* label, float val, ImVec4 color) {
                            ImGui::Text("%s: %.0f%%", label, val * 100.0f);
                            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
                            ImGui::ProgressBar(val, ImVec2(-1, 8), ""); // Thin bar, no text
                            ImGui::PopStyleColor();
                        };

                        draw_bar("O2", needs->oxygen, ImVec4(0.0f, 0.7f, 0.8f, 1.0f));
                        draw_bar("Food", needs->hunger, ImVec4(0.8f, 0.5f, 0.0f, 1.0f));
                        draw_bar("H2O", needs->thirst, ImVec4(0.1f, 0.4f, 0.8f, 1.0f));
                        draw_bar("Energy", 1.0f - needs->fatigue, ImVec4(0.8f, 0.8f, 0.1f, 1.0f));
                    } else {
                        ImGui::TextDisabled("No Needs Component");
                    }
                    ImGui::EndTabItem();
                }
                
                // === TAB 3: BIO (Metabolism) ===
                if (ImGui::BeginTabItem("Bio")) {
                    if (auto* metab = registry->try_get<entities::Metabolism>(selected_entity)) {
                        ImGui::Spacing();
                        // Core Temp
                        ImGui::Text("Core Temp: %.1f C", metab->core_temperature - 273.15f);
                        
                        // Metabolic Rate
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "(%.0f W)", metab->metabolic_rate_watts);
                        
                        // Caloric Balance
                        ImGui::Spacing();
                        ImGui::Text("Caloric Reserve:");
                        float kcal = metab->caloric_balance;
                        
                        // Determine color based on reserve
                        ImVec4 kcal_color = ImVec4(0.2f, 0.8f, 0.2f, 1.0f); // Green
                        if (kcal < 500) kcal_color = ImVec4(0.9f, 0.2f, 0.2f, 1.0f); // Red
                        else if (kcal < 1000) kcal_color = ImVec4(0.9f, 0.8f, 0.2f, 1.0f); // Yellow
                        
                        ImGui::TextColored(kcal_color, "%.0f kcal", kcal);
                        ImGui::ProgressBar(std::clamp(kcal / 3000.0f, 0.0f, 1.0f), ImVec2(-1, 6), "");
                        
                        ImGui::Spacing();
                        ImGui::TextDisabled("Insulation: %.1f clo", metab->insulation);
                    } else {
                        ImGui::TextDisabled("No Metabolism Component");
                    }
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }

        } else {
            ImGui::TextDisabled("Select an entity...");
        }
    }

    ImGui::Spacing();

    // === PERFORMANCE ===
    if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Text("FPS: %d", GetFPS());
      ImGui::Text("Frame: %.1f ms", GetFrameTime() * 1000.0f);
      ImGui::Text("Sim:   %.1f ms", sim_step_time_ms);

      // Frame time graph
      static float frame_times[60] = {0};
      static int idx = 0;
      frame_times[idx] = GetFrameTime() * 1000.0f;
      idx = (idx + 1) % 60;
      ImGui::PlotLines("##ft", frame_times, 60, idx, nullptr, 0, 33.3f, ImVec2(-1, 30));
    }

    // === CONTROLS HELP ===
    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Controls")) {
      ImGui::TextDisabled("WASD/Arrows: Pan");
      ImGui::TextDisabled("Mouse Wheel: Zoom");
      ImGui::TextDisabled("Space: Pause/Resume");
      ImGui::TextDisabled("1/2/3/0: Overlays");
      ImGui::TextDisabled("Q/E: Z-Level");
    }

    // === LOG (if enabled) ===
    if (show_log_ && ImGui::CollapsingHeader("Event Log")) {
      ImGui::BeginChild("##log", ImVec2(-1, 100), true);
      for (const auto &entry : log_entries_) {
        ImVec4 col = entry.severity == 2 ? ImVec4(1, 0.3f, 0.3f, 1) :
                     entry.severity == 1 ? ImVec4(1, 0.8f, 0.2f, 1) :
                                           ImVec4(0.7f, 0.7f, 0.7f, 1);
        ImGui::TextColored(col, "[%.0f] %s", entry.sim_time, entry.message.c_str());
      }
      if (auto_scroll_log_) ImGui::SetScrollHereY(1.0f);
      ImGui::EndChild();
    }
  }
  ImGui::End();
}

void DebugUI::add_log(double sim_time, const std::string &message, int severity) {
  log_entries_.push_back({sim_time, message, severity});
  if (log_entries_.size() > MAX_LOG_ENTRIES) {
    log_entries_.pop_front();
  }
}

void DebugUI::clear_log() { log_entries_.clear(); }

void DebugUI::draw_right_sidebar(int z_level, int overlay_type, size_t chunk_count) {
  float sidebar_width = 180.0f;
  float screen_width = static_cast<float>(GetScreenWidth());
  
  ImGui::SetNextWindowPos(ImVec2(screen_width - sidebar_width, 0), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(sidebar_width, 200), ImGuiCond_Always);
  
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                           ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;
  
  if (ImGui::Begin("##RightSidebar", nullptr, flags)) {
    // World Info Header
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 0.9f, 1.0f));
    ImGui::Text("WORLD");
    ImGui::PopStyleColor();
    ImGui::Separator();
    
    // Z-Level with color
    ImGui::Text("Z-Level:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.4f, 1.0f), "%d", z_level);
    
    // Chunks loaded
    ImGui::Text("Chunks:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 0.6f, 1.0f), "%zu", chunk_count);
    
    ImGui::Spacing();
    ImGui::Separator();
    
    // Overlay indicator with colored text
    ImGui::Text("Overlay:");
    ImGui::SameLine();
    
    const char* overlay_names[] = {"None", "Temp", "Pressure", "O2"};
    ImVec4 overlay_colors[] = {
      ImVec4(0.5f, 0.5f, 0.5f, 1.0f),  // None - gray
      ImVec4(1.0f, 0.5f, 0.3f, 1.0f),  // Temp - orange/red
      ImVec4(0.8f, 0.8f, 0.3f, 1.0f),  // Pressure - yellow
      ImVec4(0.3f, 0.9f, 0.5f, 1.0f)   // O2 - green
    };
    
    int idx = std::clamp(overlay_type, 0, 3);
    ImGui::TextColored(overlay_colors[idx], "%s", overlay_names[idx]);
    
    ImGui::Spacing();
    
    // Controls hint
    ImGui::TextDisabled("Q/E: Z-level");
    ImGui::TextDisabled("1/2/3/0: Overlay");
  }
  ImGui::End();
}

bool DebugUI::is_capturing_mouse() const {
  return ImGui::GetIO().WantCaptureMouse;
}

} // namespace renderer
} // namespace isolated
