# ============================================================================
# Surface Geometry End-to-End Tests
# ============================================================================
# WHAT: E2E tests for surface geometry complete system workflows
# WHY:  Verify full integration from initialization to optimization
# HOW:  GTest-based tests with all subsystems connected
#
# SCENARIOS:
# - Spine formation workflow
# - Axon branching workflow
# - Trifurcation detection
# - Network optimization
# - Anomaly detection and immune response
# - Multi-system statistics
#
# NIMCP STANDARDS:
# - E2E tests run after unit, integration, and regression tests
# - Tests create full system instances
# ============================================================================

cmake_minimum_required(VERSION 3.14)

# ============================================================================
# Surface Geometry E2E Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_surface_e2e.cpp")
    message(STATUS "Adding surface geometry E2E tests...")
    add_executable(e2e_core_geometry_surface
        test_surface_e2e.cpp
    )
    target_link_libraries(e2e_core_geometry_surface
        PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
    )
    target_compile_features(e2e_core_geometry_surface PRIVATE cxx_std_17)
    target_include_directories(e2e_core_geometry_surface PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    gtest_discover_tests(e2e_core_geometry_surface
        PROPERTIES LABELS "e2e;core;geometry;surface"
        DISCOVERY_TIMEOUT 180)  # E2E tests may take longer
endif()

# ============================================================================
# Summary
# ============================================================================
message(STATUS "Surface geometry E2E tests configured:")
message(STATUS "  - Spine formation workflow")
message(STATUS "  - Axon branching workflow")
message(STATUS "  - Trifurcation detection")
message(STATUS "  - Network optimization")
message(STATUS "  - Anomaly detection and response")
message(STATUS "  - Multi-system statistics")
message(STATUS "  - Stress tests (spines, branches)")
