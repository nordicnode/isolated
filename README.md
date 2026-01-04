# Isolated (C++)

High-performance habitat simulation framework written in modern C++20 with **CUDA GPU acceleration**.

## Features

### 🚀 GPU-Accelerated Fluid Dynamics (LBM)
- D3Q19 lattice with MRT collision
- **CUDA kernels** for 20× speedup over CPU
- **Sparse readback** for efficient agent queries (~10µs)
- Large Eddy Simulation (LES) turbulence
- Multi-species gas tracking (O₂, N₂, CO₂, H₂O, CO)

### 🔥 Multiphase Physics
- Magnus-Tetens condensation/evaporation
- Lagrangian aerosol transport
- Procedural combustion model with smoke

### 🌡️ Thermal Systems
- 3D finite difference conduction
- Stefan-Boltzmann radiation
- Enthalpy-based phase changes
- Optimized temp buffers (zero heap allocation in hot loops)

### 🧬 Human Physiology

**Cardiovascular**
- Windkessel circulation, Frank-Starling, HRV
- Coagulation cascade (intrinsic/extrinsic pathways)
- Anticoagulant effects (Heparin, Warfarin, DOACs)

**Respiratory**
- Hill equation hemoglobin, alveolar gas exchange
- V/Q mismatch, dead space, work of breathing
- Positive pressure ventilation (CPAP, BiPAP, HFNC)

**Gas Transport Pathologies**
- CO poisoning (Haldane equation, COHb kinetics)
- Nitrogen narcosis (Martini's law, EAD)
- Oxygen toxicity (CNS clock, UPTD)
- Decompression sickness (Bühlmann model)

**Neurological**
- Cerebral autoregulation (Lassen curve)
- Intracranial pressure (ICP) dynamics
- Herniation risk assessment
- Glasgow Coma Scale (GCS)

**Other Systems**
- Metabolic: ATP/glycogen, substrate utilization
- Blood Chemistry: Henderson-Hasselbalch, electrolytes
- Immune: WBC dynamics, infection, sepsis
- Muscular: Fatigue, lactate production
- Thermoregulation: Heat balance, sweating, shivering
- Integumentary: Wounds, burns, healing

### 🌍 World Generation
- Perlin/Simplex noise
- Geology layers with ore deposits
- Cellular automata caverns

## Build

```bash
# Dependencies (Ubuntu)
sudo apt install libeigen3-dev libfmt-dev libomp-dev cmake nvidia-cuda-toolkit

# Build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# Run benchmark
./tests/benchmark

# Run simulation
./isolated
```

## Performance

| System | CPU | GPU | Speedup |
|--------|-----|-----|---------|
| LBM 100×100 | 1.6 ms | 0.11 ms | **15×** |
| LBM 500×500 | ~40 ms | 1.8 ms | **22×** |
| Sparse Readback (100 agents) | — | 10 µs | — |
| Thermal 100×100 | 60 µs | — | — |
| Biology (all systems) | <1 µs | — | — |

**Total simulation: ~9 ms/step @ 60 FPS = 52% budget used**

## Architecture

```
┌─────────────────┐     ┌─────────────────┐
│   GPU (CUDA)    │     │   CPU (OpenMP)  │
├─────────────────┤     ├─────────────────┤
│ LBM Fluids      │◄───►│ Thermal Engine  │
│ Sparse Readback │     │ Biology Systems │
└─────────────────┘     │ World Gen       │
                        └─────────────────┘
```

## License

MIT
