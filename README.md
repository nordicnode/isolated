# Isolated (C++)

High-performance habitat simulation framework written in modern C++20.

## Features

### 🌪️ Fluid Dynamics (LBM)
- D3Q19 lattice with MRT collision
- Large Eddy Simulation (LES) turbulence
- Multi-species gas tracking (O₂, N₂, CO₂, H₂O)

### 🔥 Multiphase Physics
- Magnus-Tetens condensation/evaporation
- Lagrangian aerosol transport
- Procedural combustion model

### 🌡️ Thermal Systems
- 3D finite difference conduction
- Stefan-Boltzmann radiation
- Enthalpy-based phase changes

### 🧬 Human Physiology
- **Cardiovascular**: Windkessel model, Frank-Starling, HRV
- **Respiratory**: Hill equation, alveolar gas exchange
- **Metabolic**: ATP/glycogen dynamics, substrate utilization
- **Blood Chemistry**: Henderson-Hasselbalch, electrolytes
- **Immune**: WBC dynamics, infection response
- **Muscular**: Fatigue, lactate production
- **Thermoregulation**: Heat balance, sweating, shivering
- **Integumentary**: Wounds, burns, healing

### 🌍 World Generation
- Perlin/Simplex noise
- Geology layers with ore deposits
- Cellular automata caverns

## Build

```bash
# Dependencies (Ubuntu)
sudo apt install libeigen3-dev libfmt-dev libomp-dev cmake

# Build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# Run
./isolated
```

## Performance

| System | Time/Step |
|--------|-----------|
| LBM 100×100 | ~2ms |
| Full simulation | ~2.3ms |

**~200× faster than Python equivalent**

## License

MIT
