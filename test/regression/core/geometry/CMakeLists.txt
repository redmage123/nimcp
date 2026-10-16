# ============================================================================
# Surface Geometry Regression Tests
# ============================================================================
# WHAT: Regression tests for surface geometry paper predictions
# WHY:  Ensure Meng et al. Nature 2026 predictions remain stable
# HOW:  GTest-based tests with numerical assertions
#
# PAPER PREDICTIONS:
# - Chi threshold at 0.83 for trifurcation transition
# - Rho threshold at ~0.6 for sprouting/branching transition
# - 90° orthogonal sprouts, Steiner angles at rho=1
# - ~98% sprout-synapse ratio
# - ~25% longer than Steiner minimum
#
# NIMCP STANDARDS:
# - Regression tests run after unit and integration tests
# - Verify numerical predictions remain consistent
# ============================================================================

cmake_minimum_required(VERSION 3.14)

# ============================================================================
# Surface Geometry Regression Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_surface_regression.cpp")
    message(STATUS "Adding surface geometry regression tests...")
    add_executable(regression_core_geometry_surface
        test_surface_regression.cpp
    )
    target_link_libraries(regression_core_geometry_surface
        PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
    )
    target_compile_features(regression_core_geometry_surface PRIVATE cxx_std_17)
    target_include_directories(regression_core_geometry_surface PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    gtest_discover_tests(regression_core_geometry_surface
        PROPERTIES LABELS "regression;core;geometry;surface"
        DISCOVERY_TIMEOUT 120)  # Optimization tests may take longer
endif()

# ============================================================================
# Summary
# ============================================================================
message(STATUS "Surface geometry regression tests configured:")
message(STATUS "  - Chi threshold: 0.83 ± 0.05")
message(STATUS "  - Rho threshold: 0.6 ± 0.05")
message(STATUS "  - Angle predictions: 90° sprouts, 60° Steiner")
message(STATUS "  - Sprout-synapse ratio: ~98%")
message(STATUS "  - Steiner overhead: ~25%")
