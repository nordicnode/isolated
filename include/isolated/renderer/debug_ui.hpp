#pragma once

/**
 * @file debug_ui.hpp
 * @brief Dwarf Fortress-style unified sidebar UI.
 *
 * Provides a cohesive left-side panel with:
 * - Simulation status and controls
 * - Cell inspector (hover to see data)
 * - Performance metrics
 * - Event log
 */

#include <string>
#include <deque>

// Forward declarations
namespace isolated {
namespace fluids {
class LBMEngine;
}
namespace thermal {
class ThermalEngine;
}
} // namespace isolated

struct Camera2D;

namespace isolated {
namespace renderer {

/**
 * @brief Log entry for the event log.
 */
struct LogEntry {
  double sim_time;
  std::string message;
  int severity; // 0=info, 1=warning, 2=error
};

/**
 * @brief Dwarf Fortress-style unified sidebar UI.
 */
class DebugUI {
public:
  DebugUI() = default;
  ~DebugUI() = default;

  // Lifecycle
  void init();
  void shutdown();

  // Frame management
  void begin_frame();
  void end_frame();

  // Main unified sidebar (call this instead of individual panels)
  void draw_sidebar(const fluids::LBMEngine &fluids,
                    const thermal::ThermalEngine &thermal,
                    const Camera2D &camera, int tile_size,
                    bool &paused, float &time_scale,
                    double sim_step_time_ms, double sim_time);

  // Logging
  void add_log(double sim_time, const std::string &message, int severity = 0);
  void clear_log();

  bool is_capturing_mouse() const;

  // Section toggles
  void toggle_log() { show_log_ = !show_log_; }

private:
  bool initialized_ = false;

  // Section visibility
  bool show_log_ = false;

  // Log storage
  std::deque<LogEntry> log_entries_;
  static constexpr size_t MAX_LOG_ENTRIES = 200;
  bool auto_scroll_log_ = true;

  // State
  int inspected_x_ = -1;
  int inspected_y_ = -1;
};

} // namespace renderer
} // namespace isolated
