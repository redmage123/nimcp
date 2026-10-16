# ============================================================================
# Surface Geometry Unit Tests
# ============================================================================
# WHAT: Unit tests for surface geometry optimization module
# WHY:  Verify paper predictions (Meng et al. Nature 2026) and integration
# HOW:  GTest-based tests for core, manifold, optimization, and bridges
#
# NIMCP STANDARDS:
# - Test categories: core, manifold, optimization, bridges
# - All tests linked against nimcp library
# - Coverage targets for regression detection
# ============================================================================

cmake_minimum_required(VERSION 3.14)

# ============================================================================
# Core Surface Geometry Tests
# ============================================================================
# WHAT: Tests for parameter computation, regime determination, validation
# WHY:  Verify chi/rho predictions, angle computation (Fig. 4g)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_surface_geometry_core.cpp")
    message(STATUS "Adding surface geometry core unit tests...")
    add_executable(unit_core_geometry_surface_core
        test_surface_geometry_core.cpp
    )
    target_link_libraries(unit_core_geometry_surface_core
        PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
    )
    target_compile_features(unit_core_geometry_surface_core PRIVATE cxx_std_17)
    target_include_directories(unit_core_geometry_surface_core PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    gtest_discover_tests(unit_core_geometry_surface_core
        PROPERTIES LABELS "unit;core;geometry;surface")
endif()

# ============================================================================
# Surface Manifold Tests
# ============================================================================
# WHAT: Tests for manifold construction, metric tensor, Nambu-Goto action
# WHY:  Surface area is computed as integral sqrt(det(gamma)) d^2 sigma

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_surface_manifold.cpp")
    message(STATUS "Adding surface manifold unit tests...")
    add_executable(unit_core_geometry_surface_manifold
        test_surface_manifold.cpp
    )
    target_link_libraries(unit_core_geometry_surface_manifold
        PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
    )
    target_compile_features(unit_core_geometry_surface_manifold PRIVATE cxx_std_17)
    target_include_directories(unit_core_geometry_surface_manifold PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    gtest_discover_tests(unit_core_geometry_surface_manifold
        PROPERTIES LABELS "unit;core;geometry;manifold")
endif()

# ============================================================================
# Surface Optimization Tests
# ============================================================================
# WHAT: Tests for surface area optimization, tetrahedral configuration
# WHY:  Verify bifurcation/trifurcation transition at chi ~= 0.83 (Fig. 3)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_surface_optimization.cpp")
    message(STATUS "Adding surface optimization unit tests...")
    add_executable(unit_core_geometry_surface_optimization
        test_surface_optimization.cpp
    )
    target_link_libraries(unit_core_geometry_surface_optimization
        PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
    )
    target_compile_features(unit_core_geometry_surface_optimization PRIVATE cxx_std_17)
    target_include_directories(unit_core_geometry_surface_optimization PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    gtest_discover_tests(unit_core_geometry_surface_optimization
        PROPERTIES LABELS "unit;core;geometry;optimization"
        DISCOVERY_TIMEOUT 60)
endif()

# ============================================================================
# Surface Bridges Tests
# ============================================================================
# WHAT: Tests for brain, bio-async, quantum, and immune bridges
# WHY:  Surface geometry integrates with multiple NIMCP subsystems

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_surface_bridges.cpp")
    message(STATUS "Adding surface bridges unit tests...")
    add_executable(unit_core_geometry_surface_bridges
        test_surface_bridges.cpp
    )
    target_link_libraries(unit_core_geometry_surface_bridges
        PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
    )
    target_compile_features(unit_core_geometry_surface_bridges PRIVATE cxx_std_17)
    target_include_directories(unit_core_geometry_surface_bridges PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    gtest_discover_tests(unit_core_geometry_surface_bridges
        PROPERTIES LABELS "unit;core;geometry;bridges")
endif()

# ============================================================================
# Summary
# ============================================================================
message(STATUS "Surface geometry unit tests configured:")
message(STATUS "  - Core: chi/rho computation, regime determination")
message(STATUS "  - Manifold: metric tensor, Nambu-Goto action")
message(STATUS "  - Optimization: network optimization, tetrahedral")
message(STATUS "  - Bridges: brain, bio-async, quantum, immune")
