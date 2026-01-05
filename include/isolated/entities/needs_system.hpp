#pragma once

#include "entt/entt.hpp"
#include <isolated/entities/components.hpp>
#include <isolated/fluids/lbm_engine.hpp>

namespace isolated {
namespace entities {

/**
 * @brief Updates astronaut needs based on environment.
 * 
 * This system handles:
 * - O2 consumption from ambient air
 * - CO2 exhale into LBM simulation
 * - Hypoxia state transitions
 */
class NeedsSystem {
public:
    /**
     * @brief Update all entities with Needs component.
     * @param dt Delta time in seconds
     * @param registry The ECS registry
     * @param fluids The LBM engine for gas exchange
     */
    static void update(double dt, entt::registry& registry, fluids::LBMEngine& fluids);

private:
    // O2 consumption rate (fraction per second at rest)
    // Real: ~0.25 L/min, but we use normalized values
    static constexpr float O2_CONSUMPTION_RATE = 0.05f; // 5% O2 drain/sec at low ambient
    
    // CO2 production rate (fraction per second)
    static constexpr float CO2_PRODUCTION_RATE = 0.04f;
    
    // Hypoxia thresholds (blood O2 saturation levels)
    static constexpr float HYPOXIA_CONFUSED_THRESHOLD = 0.5f;
    static constexpr float HYPOXIA_COLLAPSED_THRESHOLD = 0.2f;
    static constexpr float HYPOXIA_DEATH_THRESHOLD = 0.05f;
    
    // Minimum ambient O2 for normal breathing (fraction)
    static constexpr float MIN_AMBIENT_O2 = 0.16f; // 16% O2 minimum
};

} // namespace entities
} // namespace isolated
