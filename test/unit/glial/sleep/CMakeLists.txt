# ============================================================================
# Glial Sleep Bridge Unit Tests
# ============================================================================
# WHAT: Unit tests for glial-sleep integration bridges
# WHY:  Verify sleep modulation of glial cell functions
# HOW:  Test lifecycle, state effects, bio-async integration
# ============================================================================

# Microglia Sleep Bridge Tests
add_executable(unit_glial_sleep_test_microglia_sleep_bridge
    test_microglia_sleep_bridge.cpp
)
target_link_libraries(unit_glial_sleep_test_microglia_sleep_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(unit_glial_sleep_test_microglia_sleep_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_glial_sleep_test_microglia_sleep_bridge
    PROPERTIES
        TIMEOUT 120
        LABELS "unit;glial;microglia;sleep"
)
