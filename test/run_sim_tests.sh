#!/bin/bash
# Run all NIMCP simulation engine tests
# Usage: cd build && ../tests/run_all_tests.sh

set -e
PASS=0
FAIL=0
ERRORS=""

echo "============================================"
echo "  NIMCP Simulation Engine Test Suite"
echo "============================================"
echo ""

run_test() {
    local test_name=$1
    local test_exe=$2
    if [ ! -x "$test_exe" ]; then
        echo "  [SKIP] $test_name (not built)"
        return
    fi
    echo "--- $test_name ---"
    if $test_exe; then
        PASS=$((PASS + 1))
    else
        FAIL=$((FAIL + 1))
        ERRORS="$ERRORS\n  - $test_name"
    fi
    echo ""
}

# Physics tests
run_test "Intuitive Physics"    "./tests/test_intuitive_physics"
run_test "Relativistic Physics" "./tests/test_relativistic"
run_test "Electromagnetic"      "./tests/test_electromagnetic"
run_test "QED"                  "./tests/test_qed"
run_test "QFT"                  "./tests/test_qft"
run_test "Astrophysics"         "./tests/test_astrophysics"
run_test "Nuclear Physics"      "./tests/test_nuclear"
run_test "Surface Physics"      "./tests/test_surface_physics"
run_test "World Simulator"      "./tests/test_world_simulator"

# Chemistry tests
run_test "Chemistry Sim"        "./tests/test_chemistry_sim"
run_test "Surface Chemistry"    "./tests/test_surface_chemistry"
run_test "Biochemistry"         "./tests/test_biochemistry"
run_test "Physical Chemistry"   "./tests/test_physical_chemistry"
run_test "Organic Chemistry"    "./tests/test_organic_chemistry"
run_test "Analytical Chemistry" "./tests/test_analytical_chemistry"

# Biology tests
run_test "Biology Sim"          "./tests/test_biology_sim"
run_test "Cell Biology"         "./tests/test_cell_biology"
run_test "Immunology"           "./tests/test_immunology"
run_test "Ecology"              "./tests/test_ecology"
run_test "Physiology"           "./tests/test_physiology"

# Math tests
run_test "Number Theory"        "./tests/test_number_theory"
run_test "Linear Algebra"       "./tests/test_linear_algebra"
run_test "Probability"          "./tests/test_probability"
run_test "Numerical Methods"    "./tests/test_numerical_methods"
run_test "Graph Theory"         "./tests/test_graph_theory"
run_test "Combinatorics"        "./tests/test_combinatorics"
run_test "Topology"             "./tests/test_topology"
run_test "Logic"                "./tests/test_logic"
run_test "Zeta Functions"       "./tests/test_zeta"
run_test "Knot Theory"          "./tests/test_knot_theory"
run_test "Complexity Theory"    "./tests/test_complexity"
run_test "Optimization"         "./tests/test_optimization"
run_test "Information Theory"   "./tests/test_information_theory"

# Integration tests
run_test "Cross-Engine Coupling" "./tests/test_cross_engine_coupling"

echo ""
echo "============================================"
echo "  TOTAL: $((PASS + FAIL)) suites, $PASS passed, $FAIL failed"
echo "============================================"

if [ $FAIL -gt 0 ]; then
    echo -e "\nFailed suites:$ERRORS"
    exit 1
fi
exit 0
