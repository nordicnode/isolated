/**
 * @file debug_ui.cpp
 * @brief Dwarf Fortress-style unified sidebar UI implementation.
 */

#include <isolated/renderer/debug_ui.hpp>
#include <isolated/fluids/lbm_engine.hpp>
#include <isolated/thermal/heat_engine.hpp>

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
                           const Camera2D &camera, int tile_size,
                           bool &paused, float &time_scale,
                           double sim_step_time_ms, double sim_time) {
  // Fixed left sidebar
  float sidebar_width = 220.0f;
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

      if (cell_x >= 0 && cell_x < 200 && cell_y >= 0 && cell_y < 200 &&
          mouse_screen.x > sidebar_width) {
        inspected_x_ = cell_x;
        inspected_y_ = cell_y;

        ImGui::Text("Pos: (%d, %d)", cell_x, cell_y);

        // Fluids
        double density = fluids.get_density(cell_x, cell_y, 0);
        auto velocity = fluids.get_velocity(cell_x, cell_y, 0);
        ImGui::Text("Pressure: %.3f", density);
        ImGui::Text("Flow: (%.2f, %.2f)", velocity[0], velocity[1]);

        // Gas composition
        double o2 = fluids.get_species_density("O2", cell_x, cell_y, 0);
        double n2 = fluids.get_species_density("N2", cell_x, cell_y, 0);
        double co2 = fluids.get_species_density("CO2", cell_x, cell_y, 0);
        double total = o2 + n2 + co2 + 1e-10;

        ImGui::Text("O2: %.0f%% N2: %.0f%%", (o2/total)*100, (n2/total)*100);
        ImGui::Text("CO2: %.2f%%", (co2/total)*100);

        // Temperature
        double temp = thermal.get_temperature(cell_x, cell_y, 0);
        ImGui::PushStyleColor(ImGuiCol_Text,
            temp > 350 ? ImVec4(1.0f, 0.5f, 0.2f, 1.0f) :
            temp < 270 ? ImVec4(0.4f, 0.7f, 1.0f, 1.0f) :
                         ImVec4(0.85f, 0.75f, 0.45f, 1.0f));
        ImGui::Text("Temp: %.0f K (%.0f C)", temp, temp - 273.15);
        ImGui::PopStyleColor();
      } else {
        ImGui::TextDisabled("Hover over map...");
        inspected_x_ = -1;
        inspected_y_ = -1;
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

bool DebugUI::is_capturing_mouse() const {
  return ImGui::GetIO().WantCaptureMouse;
}

} // namespace renderer
} // namespace isolated
