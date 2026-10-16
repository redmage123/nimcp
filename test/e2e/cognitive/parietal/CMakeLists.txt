# =============================================================================
# End-to-End Tests for Parietal Module Exception Handling
# =============================================================================
# WHAT: CMakeLists.txt for parietal module exception e2e tests
# WHY:  Test full parietal pipeline with exception recovery
# HOW:  Build test executable and register with CTest
#
# E2E SCENARIOS:
# - Full mathematical reasoning with transient failures
# - Spatial reasoning under exception stress
# - Long-running simulation with random exception injection
# - Multi-threaded parietal with concurrent exceptions
# - System recovery and state preservation under stress
#
# @author NIMCP Development Team
# @date 2026-01-25
# =============================================================================

# Parietal Module Exception E2E Tests
add_executable(e2e_parietal_exception
    test_parietal_exception_e2e.cpp
)

target_include_directories(e2e_parietal_exception
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(e2e_parietal_exception
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(e2e_parietal_exception PRIVATE cxx_std_17)

# Register with CTest using gtest_discover_tests for automatic test discovery
gtest_discover_tests(e2e_parietal_exception
    PROPERTIES
        TIMEOUT 600
        LABELS "e2e;cognitive;parietal;exception;immune"
)

# =============================================================================
# Financial Investment Module End-to-End Tests
# =============================================================================
# WHAT: Full system pipeline tests for all 5 financial sub-modules
# WHY:  Verify complete workflows from market data through decision execution
# HOW:  Create all engines, wire together, run multi-step pipelines
#
# @author NIMCP Development Team
# @date 2026-01-29
# =============================================================================

add_executable(e2e_parietal_financial_investment
    test_financial_e2e.cpp
)

target_include_directories(e2e_parietal_financial_investment
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(e2e_parietal_financial_investment
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(e2e_parietal_financial_investment PRIVATE cxx_std_17)

gtest_discover_tests(e2e_parietal_financial_investment
    PROPERTIES
        TIMEOUT 300
        LABELS "e2e;cognitive;parietal;financial"
)

# Financial Cognitive E2E Tests (Check Framework)
add_subdirectory(financial)
