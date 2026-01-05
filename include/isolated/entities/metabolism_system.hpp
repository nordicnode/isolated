#pragma once

#include "entt/entt.hpp"
#include <isolated/entities/components.hpp>
#include <isolated/thermal/heat_engine.hpp>

namespace isolated {
namespace entities {

/**
 * @brief Handles metabolic processes: "Life is Slow Combustion".
 * 
 * Consumes calories/energy and outputs heat into the thermal simulation.
 */
class MetabolismSystem {
public:
    /**
     * @brief Update metabolic state for all entities.
     * @param dt Delta time in seconds
     * @param registry ECS registry
     * @param thermal Thermal engine for heat injection
     */
    static void update(double dt, entt::registry& registry, thermal::ThermalEngine& thermal);

private:
    // Constants
    static constexpr float WALKING_METABOLIC_RATE = 150.0f; // Watts
    static constexpr float RESTING_METABOLIC_RATE = 80.0f;  // Watts
    static constexpr float CALORIES_PER_JOULE = 0.000239006f;
};

} // namespace entities
} // namespace isolated
