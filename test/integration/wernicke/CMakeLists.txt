# ============================================================================
# Wernicke Integration Tests
# ============================================================================
# WHAT: Integration tests for Wernicke's area with other brain systems
# WHY:  Verify correct cross-system communication and processing
# HOW:  gtest-based tests for bridge interactions and pipelines
# ============================================================================

# ============================================================================
# Wernicke Full Integration Tests
# ============================================================================
add_executable(integration_wernicke_full
    test_wernicke_integration.cpp
)
target_link_libraries(integration_wernicke_full
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(integration_wernicke_full PRIVATE cxx_std_17)
gtest_discover_tests(integration_wernicke_full
    PROPERTIES LABELS "integration;wernicke;language")
