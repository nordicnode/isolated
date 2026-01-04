#pragma once

/**
 * @file lattice.hpp
 * @brief D2Q9 and D3Q19 lattice definitions for LBM.
 */

#include <array>
#include <cstdint>

namespace isolated {
namespace fluids {

/**
 * @brief D2Q9 Lattice for 2D LBM simulations.
 */
struct D2Q9 {
    static constexpr int Q = 9;
    static constexpr int D = 2;
    
    // Velocity vectors: {cx, cy}
    static constexpr std::array<std::array<int, 2>, 9> c = {{
        {{ 0,  0}},  // 0: rest
        {{ 1,  0}},  // 1: east
        {{ 0,  1}},  // 2: north
        {{-1,  0}},  // 3: west
        {{ 0, -1}},  // 4: south
        {{ 1,  1}},  // 5: NE
        {{-1,  1}},  // 6: NW
        {{-1, -1}},  // 7: SW
        {{ 1, -1}}   // 8: SE
    }};
    
    // Weights
    static constexpr std::array<double, 9> w = {
        4.0/9.0,                           // rest
        1.0/9.0, 1.0/9.0, 1.0/9.0, 1.0/9.0,  // cardinal
        1.0/36.0, 1.0/36.0, 1.0/36.0, 1.0/36.0  // diagonal
    };
    
    // Opposite directions for bounce-back
    static constexpr std::array<int, 9> opposite = {0, 3, 4, 1, 2, 7, 8, 5, 6};
};

/**
 * @brief D3Q19 Lattice for 3D LBM simulations.
 */
struct D3Q19 {
    static constexpr int Q = 19;
    static constexpr int D = 3;
    
    // Velocity vectors: {cx, cy, cz}
    static constexpr std::array<std::array<int, 3>, 19> c = {{
        {{ 0,  0,  0}},  // 0: rest
        {{ 1,  0,  0}},  // 1: +x
        {{-1,  0,  0}},  // 2: -x
        {{ 0,  1,  0}},  // 3: +y
        {{ 0, -1,  0}},  // 4: -y
        {{ 0,  0,  1}},  // 5: +z
        {{ 0,  0, -1}},  // 6: -z
        {{ 1,  1,  0}},  // 7
        {{-1,  1,  0}},  // 8
        {{ 1, -1,  0}},  // 9
        {{-1, -1,  0}},  // 10
        {{ 1,  0,  1}},  // 11
        {{-1,  0,  1}},  // 12
        {{ 1,  0, -1}},  // 13
        {{-1,  0, -1}},  // 14
        {{ 0,  1,  1}},  // 15
        {{ 0, -1,  1}},  // 16
        {{ 0,  1, -1}},  // 17
        {{ 0, -1, -1}}   // 18
    }};
    
    // Weights
    static constexpr std::array<double, 19> w = {
        1.0/3.0,     // rest
        1.0/18.0, 1.0/18.0, 1.0/18.0, 1.0/18.0, 1.0/18.0, 1.0/18.0,  // face
        1.0/36.0, 1.0/36.0, 1.0/36.0, 1.0/36.0,  // edge xy
        1.0/36.0, 1.0/36.0, 1.0/36.0, 1.0/36.0,  // edge xz
        1.0/36.0, 1.0/36.0, 1.0/36.0, 1.0/36.0   // edge yz
    };
    
    // Opposite directions
    static constexpr std::array<int, 19> opposite = {
        0, 2, 1, 4, 3, 6, 5,
        10, 9, 8, 7,
        14, 13, 12, 11,
        18, 17, 16, 15
    };
};

}  // namespace fluids
}  // namespace isolated
