# ============================================================================
# Wernicke Unit Tests
# ============================================================================
# WHAT: Unit tests for Wernicke's area module
# WHY:  Verify language comprehension functionality
# HOW:  gtest-based tests for adapter, bridges, and processing
# ============================================================================

# ============================================================================
# Wernicke Adapter Tests
# ============================================================================
add_executable(unit_core_wernicke_adapter
    test_wernicke_adapter.cpp
)
target_link_libraries(unit_core_wernicke_adapter
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_core_wernicke_adapter PRIVATE cxx_std_17)
gtest_discover_tests(unit_core_wernicke_adapter
    PROPERTIES LABELS "unit;core;wernicke;adapter")

# ============================================================================
# Wernicke Bridges Tests
# ============================================================================
add_executable(unit_core_wernicke_bridges
    test_wernicke_bridges.cpp
)
target_link_libraries(unit_core_wernicke_bridges
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_core_wernicke_bridges PRIVATE cxx_std_17)
gtest_discover_tests(unit_core_wernicke_bridges
    PROPERTIES LABELS "unit;core;wernicke;bridges")
