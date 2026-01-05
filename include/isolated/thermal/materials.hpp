#pragma once

/**
 * @file materials.hpp
 * @brief Thermal material properties database.
 */

#include <string>
#include <unordered_map>

namespace isolated {
namespace thermal {

/**
 * @brief Thermal properties of a material.
 */
struct MaterialProperties {
  std::string name;
  double thermal_conductivity;     // W/(m·K)
  double specific_heat;            // J/(kg·K)
  double density;                  // kg/m³
  double emissivity;               // 0-1
  double melting_point;            // K
  double boiling_point;            // K
  double latent_heat_fusion;       // J/kg
  double latent_heat_vaporization; // J/kg
};

/**
 * @brief Standard material database.
 */
inline const std::unordered_map<std::string, MaterialProperties> MATERIALS = {
    // Gases
    {"air", {"air", 0.026, 1005, 1.225, 0.0, 0, 0, 0, 0}},
    
    // Liquids
    {"water", {"water", 0.6, 4186, 1000, 0.95, 273.15, 373.15, 334000, 2260000}},
    {"magma", {"magma", 1.5, 1500, 2700, 0.95, 1073, 1573, 400000, 0}},
    
    // Solids - Ice
    {"ice", {"ice", 2.2, 2090, 917, 0.97, 273.15, 373.15, 334000, 2260000}},
    {"co2_ice", {"co2_ice", 0.5, 850, 1560, 0.9, 194.65, 194.65, 571000, 571000}},
    
    // Solids - Rock
    {"granite", {"granite", 2.5, 790, 2700, 0.9, 1500, 3000, 0, 0}},
    {"basalt", {"basalt", 1.7, 840, 2900, 0.9, 1473, 1573, 400000, 0}},
    {"limestone", {"limestone", 1.3, 840, 2600, 0.9, 1600, 2850, 0, 0}},
    {"sandstone", {"sandstone", 1.7, 920, 2300, 0.9, 1500, 2800, 0, 0}},
    {"shale", {"shale", 1.5, 800, 2400, 0.9, 1400, 2700, 0, 0}},
    {"marble", {"marble", 2.8, 880, 2700, 0.9, 1500, 2950, 0, 0}},
    {"regolith", {"regolith", 0.02, 800, 1500, 0.92, 1473, 3000, 300000, 0}},
    {"soil", {"soil", 0.5, 800, 1500, 0.9, 1200, 2500, 0, 0}},
    
    // Manufactured
    {"steel", {"steel", 50.0, 500, 7850, 0.3, 1800, 3000, 270000, 0}},
    
    // Biological
    {"flesh", {"flesh", 0.5, 3500, 1050, 0.98, 0, 0, 0, 0}},
    
    // Ores
    {"iron_ore", {"iron_ore", 2.0, 700, 4500, 0.85, 1538, 3000, 0, 0}},
    {"copper_ore", {"copper_ore", 2.5, 500, 5000, 0.85, 1085, 2562, 0, 0}},
    {"gold_ore", {"gold_ore", 3.0, 500, 5500, 0.85, 1064, 2856, 0, 0}},
    {"coal", {"coal", 0.2, 1300, 1400, 0.9, 0, 0, 0, 0}}, // Burns, doesn't melt
    {"uranium_ore", {"uranium_ore", 2.5, 700, 3000, 0.85, 1408, 4404, 0, 0}},
};

} // namespace thermal
} // namespace isolated
