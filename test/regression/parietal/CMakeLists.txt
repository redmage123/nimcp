# ============================================================================
# NIMCP Parietal Lobe Regression Tests
# ============================================================================
# WHAT: Regression tests for parietal lobe and FEP integration
# WHY:  Ensure parameter stability, numerical correctness, and performance
# HOW:  Test fixed values, boundary conditions, and performance baselines
#
# TEST COVERAGE:
# - FEP parameter stability (precision weights, learning rates)
# - Numerical stability of belief updates
# - Free energy computation consistency
# - Performance overhead baselines
# - Backward compatibility
# ============================================================================

# Guard: Check GTest availability
if(NOT GTest_FOUND)
    message(STATUS "GTest not found - skipping parietal regression tests")
    return()
endif()

# ============================================================================
# Parietal FEP Regression Tests
# ============================================================================

add_executable(regression_parietal_fep
    test_parietal_fep_regression.cpp
)

target_include_directories(regression_parietal_fep PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${GTEST_INCLUDE_DIRS}
)

target_link_libraries(regression_parietal_fep PRIVATE
    nimcp
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
)

target_compile_features(regression_parietal_fep PRIVATE cxx_std_17)

# Enable test-only APIs
target_compile_definitions(regression_parietal_fep PRIVATE NIMCP_TESTING)

# Register with CTest
add_test(NAME regression_parietal_fep COMMAND regression_parietal_fep)

# Set test properties
set_tests_properties(regression_parietal_fep PROPERTIES
    LABELS "regression;cognitive;parietal;fep"
    TIMEOUT 120
)

message(STATUS "Added parietal FEP regression test: regression_parietal_fep")
