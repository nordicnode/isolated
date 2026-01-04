/**
 * @file test_basic.cpp
 * @brief Basic unit tests for core systems.
 */

#include <cassert>
#include <cmath>
#include <iostream>

#include <isolated/biology/blood_chemistry.hpp>
#include <isolated/core/constants.hpp>
#include <isolated/fluids/lattice.hpp>

using namespace isolated;

void test_constants() {
  std::cout << "Testing constants..." << std::endl;
  assert(constants::STEFAN_BOLTZMANN > 5.6e-8 &&
         constants::STEFAN_BOLTZMANN < 5.7e-8);
  assert(constants::WATER_FREEZE == 273.15);
  assert(constants::MW_O2 == 32.0);
  std::cout << "  Constants: PASS" << std::endl;
}

void test_lattice() {
  std::cout << "Testing lattice..." << std::endl;

  // D2Q9 weights should sum to 1
  double sum = 0;
  for (int i = 0; i < 9; ++i)
    sum += fluids::D2Q9::w[i];
  assert(std::abs(sum - 1.0) < 1e-10);

  // D3Q19 weights should sum to 1
  sum = 0;
  for (int i = 0; i < 19; ++i)
    sum += fluids::D3Q19::w[i];
  assert(std::abs(sum - 1.0) < 1e-10);

  std::cout << "  Lattice: PASS" << std::endl;
}

void test_blood_chemistry() {
  std::cout << "Testing blood chemistry..." << std::endl;

  biology::BloodChemistrySystem chem;

  // Normal pH
  double ph = chem.compute_ph();
  assert(ph > 7.35 && ph < 7.45);

  // Anion gap
  double ag = chem.electrolytes.anion_gap();
  assert(ag > 8 && ag < 16);

  // Lactate buffering
  chem.add_lactate(5.0);
  assert(chem.lactate > 5.0);
  assert(chem.electrolytes.bicarbonate < 24.0);

  std::cout << "  Blood Chemistry: PASS" << std::endl;
}

int main() {
  std::cout << "=== Running Unit Tests ===" << std::endl;

  test_constants();
  test_lattice();
  test_blood_chemistry();

  std::cout << std::endl;
  std::cout << "All tests PASSED!" << std::endl;
  return 0;
}
