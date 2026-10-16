# ============================================================================
# Plasticity Substrate Bridge Tests
# ============================================================================
# WHAT: Unit tests for Plasticity-Substrate bidirectional integration
# WHY:  Ensure synaptic plasticity integrates with substrate optimization
# HOW:  Test lifecycle, substrate effects on plasticity, bio-async integration
# ============================================================================

add_executable(unit_plasticity_substrate_bridge
    test_plasticity_substrate_bridge.cpp
)

target_link_libraries(unit_plasticity_substrate_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(unit_plasticity_substrate_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_plasticity_substrate_bridge
    PROPERTIES LABELS "unit;plasticity;substrate;bridge"
    TIMEOUT 60
)
