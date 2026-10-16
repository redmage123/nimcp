# ============================================================================
# Neural Substrate Unit Tests
# ============================================================================
# WHAT: Unit tests for neural substrate module
# WHY:  Test comprehensive substrate architecture and immune integration
# HOW:  Test lifecycle, configuration, immune bridge, statistics
# ============================================================================

# Neural Substrate Core Tests
add_executable(unit_core_neural_substrate
    test_neural_substrate.cpp
)
target_link_libraries(unit_core_neural_substrate
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(unit_core_neural_substrate PRIVATE cxx_std_17)
gtest_discover_tests(unit_core_neural_substrate
    PROPERTIES LABELS "unit;core;neural_substrate"
    TIMEOUT 60
)

# Substrate-Immune Bridge Tests
add_executable(unit_core_substrate_immune_bridge
    test_substrate_immune_bridge.cpp
)
target_link_libraries(unit_core_substrate_immune_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(unit_core_substrate_immune_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_core_substrate_immune_bridge
    PROPERTIES LABELS "unit;core;neural_substrate;immune"
    TIMEOUT 60
)
