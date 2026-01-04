#pragma once

/**
 * @file constants.hpp
 * @brief Physical constants and simulation parameters for Isolated.
 */

namespace isolated {
namespace constants {

// === Physical Constants ===
constexpr double STEFAN_BOLTZMANN = 5.670374419e-8;  // W/(m²·K⁴)
constexpr double BOLTZMANN_K = 1.380649e-23;         // J/K
constexpr double AVOGADRO = 6.02214076e23;           // mol⁻¹
constexpr double GAS_CONSTANT = 8.314462618;         // J/(mol·K)
constexpr double GRAVITY = 9.81;                     // m/s²

// === Temperature References ===
constexpr double ABSOLUTE_ZERO = 0.0;                // K
constexpr double WATER_FREEZE = 273.15;              // K
constexpr double WATER_BOIL = 373.15;                // K
constexpr double ROOM_TEMP = 293.15;                 // K (20°C)

// === Pressure ===
constexpr double ATM_PRESSURE = 101325.0;            // Pa
constexpr double ATM_PRESSURE_MMHG = 760.0;          // mmHg

// === Gas Molecular Weights (g/mol) ===
constexpr double MW_O2 = 32.0;
constexpr double MW_N2 = 28.0;
constexpr double MW_CO2 = 44.0;
constexpr double MW_H2O = 18.0;
constexpr double MW_AIR = 28.97;

// === Biology Constants ===
constexpr double HEMOGLOBIN_NORMAL = 14.0;           // g/dL
constexpr double P50_BASE_MMHG = 26.6;               // mmHg (O2 half-saturation)
constexpr double BODY_TEMP_NORMAL = 310.15;          // K (37°C)
constexpr double HILL_COEFFICIENT = 2.7;             // Hemoglobin cooperativity

// === LBM Parameters ===
constexpr double CS2 = 1.0 / 3.0;                    // Speed of sound squared
constexpr double CS = 0.5773502691896257;            // sqrt(1/3)

}  // namespace constants
}  // namespace isolated
