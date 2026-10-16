# ============================================================================
# Bridges Regression Tests
# ============================================================================
# WHAT: CMakeLists for bridge regression tests
# WHY:  Verify stability and memory safety of cognitive bridges
# HOW:  Test memory leaks, stability under load, regression testing
# ============================================================================

# Bridge Memory Leaks Test
add_test_binary(
    regression_bridges_test_bridge_memory_leaks
    ${CMAKE_CURRENT_SOURCE_DIR}/test_bridge_memory_leaks.cpp
    regression
)

# Quantum Bridge Stability Test
add_test_binary(
    regression_bridges_test_quantum_bridge_stability
    ${CMAKE_CURRENT_SOURCE_DIR}/test_quantum_bridge_stability.cpp
    regression
)

# Substrate Bridge Stability Test
add_test_binary(
    regression_bridges_test_substrate_bridge_stability
    ${CMAKE_CURRENT_SOURCE_DIR}/test_substrate_bridge_stability.cpp
    regression
)

# Thalamic Bridge Stability Test
add_test_binary(
    regression_bridges_test_thalamic_bridge_stability
    ${CMAKE_CURRENT_SOURCE_DIR}/test_thalamic_bridge_stability.cpp
    regression
)
