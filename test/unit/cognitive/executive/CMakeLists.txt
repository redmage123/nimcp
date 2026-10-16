# ============================================================================
# Executive Function Unit Tests
# ============================================================================
# WHAT: CMakeLists for executive function tests
# WHY:  Build GTest-based tests, skip libcheck tests
# HOW:  Explicitly list GTest tests, exclude .c files using libcheck
#
# NOTE: test_executive_swarm_integration.c uses libcheck, not GTest
#       Need to either convert to GTest or add libcheck support
# ============================================================================

# Build GTest-based tests (C++ files)
add_test_binary(
    unit_cognitive_executive_test_executive
    ${CMAKE_CURRENT_SOURCE_DIR}/test_executive.cpp
    unit
)

add_test_binary(
    unit_cognitive_executive_test_executive_real
    ${CMAKE_CURRENT_SOURCE_DIR}/test_executive_real.cpp
    unit
)
set_tests_properties(unit_cognitive_executive_test_executive_real PROPERTIES TIMEOUT 180)

add_test_binary(
    unit_cognitive_executive_test_executive_portia
    ${CMAKE_CURRENT_SOURCE_DIR}/test_executive_portia.cpp
    unit
)

# TODO: Convert test_executive_swarm_integration.c from libcheck to GTest
message(STATUS "  [SKIP] test_executive_swarm_integration.c - uses libcheck (not GTest)")

# MCTS planning tests
add_test_binary(
    unit_cognitive_executive_test_executive_mcts
    ${CMAKE_CURRENT_SOURCE_DIR}/test_executive_mcts.cpp
    unit
)

# SNN and Plasticity bridge tests
add_test_binary(
    unit_cognitive_executive_test_executive_snn_bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/test_executive_snn_bridge.cpp
    unit
)

add_test_binary(
    unit_cognitive_executive_test_executive_plasticity_bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/test_executive_plasticity_bridge.cpp
    unit
)

# Substrate bridge tests
add_subdirectory(substrate)
