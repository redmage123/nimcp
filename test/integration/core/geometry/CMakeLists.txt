# ============================================================================
# Surface Geometry Integration Tests
# ============================================================================
# WHAT: Integration tests for surface geometry optimization module
# WHY:  Verify cross-module integration: bio-async, quantum, immune, brain
# HOW:  GTest-based tests for inter-system communication
#
# NIMCP STANDARDS:
# - Integration tests verify multi-module interactions
# - All tests linked against full nimcp library
# ============================================================================

cmake_minimum_required(VERSION 3.14)

# ============================================================================
# Bio-Async Integration Tests
# ============================================================================
# WHAT: Tests for surface geometry <-> bio-async message routing
# WHY:  Geometry events propagate through bio-async system

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_surface_bio_async_integration.cpp")
    message(STATUS "Adding surface bio-async integration tests...")
    add_executable(integration_core_geometry_surface_bio_async
        test_surface_bio_async_integration.cpp
    )
    target_link_libraries(integration_core_geometry_surface_bio_async
        PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
    )
    target_compile_features(integration_core_geometry_surface_bio_async PRIVATE cxx_std_17)
    target_include_directories(integration_core_geometry_surface_bio_async PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    gtest_discover_tests(integration_core_geometry_surface_bio_async
        PROPERTIES LABELS "integration;core;geometry;bio-async"
        DISCOVERY_TIMEOUT 60)
endif()

# ============================================================================
# Quantum Integration Tests
# ============================================================================
# WHAT: Tests for quantum-enhanced surface optimization
# WHY:  Verify QMC and QMCTS integration

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_surface_quantum_integration.cpp")
    message(STATUS "Adding surface quantum integration tests...")
    add_executable(integration_core_geometry_surface_quantum
        test_surface_quantum_integration.cpp
    )
    target_link_libraries(integration_core_geometry_surface_quantum
        PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
    )
    target_compile_features(integration_core_geometry_surface_quantum PRIVATE cxx_std_17)
    target_include_directories(integration_core_geometry_surface_quantum PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    gtest_discover_tests(integration_core_geometry_surface_quantum
        PROPERTIES LABELS "integration;core;geometry;quantum"
        DISCOVERY_TIMEOUT 120)  # Quantum tests may take longer
endif()

# ============================================================================
# Immune Integration Tests
# ============================================================================
# WHAT: Tests for surface geometry <-> immune system interaction
# WHY:  Geometry anomalies are reported as antigens

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_surface_immune_integration.cpp")
    message(STATUS "Adding surface immune integration tests...")
    add_executable(integration_core_geometry_surface_immune
        test_surface_immune_integration.cpp
    )
    target_link_libraries(integration_core_geometry_surface_immune
        PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
    )
    target_compile_features(integration_core_geometry_surface_immune PRIVATE cxx_std_17)
    target_include_directories(integration_core_geometry_surface_immune PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    gtest_discover_tests(integration_core_geometry_surface_immune
        PROPERTIES LABELS "integration;core;geometry;immune"
        DISCOVERY_TIMEOUT 60)
endif()

# ============================================================================
# Brain Integration Tests
# ============================================================================
# WHAT: Tests for surface geometry brain structure integration
# WHY:  Surface optimization governs dendrite/axon branching

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_surface_brain_integration.cpp")
    message(STATUS "Adding surface brain integration tests...")
    add_executable(integration_core_geometry_surface_brain
        test_surface_brain_integration.cpp
    )
    target_link_libraries(integration_core_geometry_surface_brain
        PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
    )
    target_compile_features(integration_core_geometry_surface_brain PRIVATE cxx_std_17)
    target_include_directories(integration_core_geometry_surface_brain PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    gtest_discover_tests(integration_core_geometry_surface_brain
        PROPERTIES LABELS "integration;core;geometry;brain"
        DISCOVERY_TIMEOUT 60)
endif()

# ============================================================================
# Summary
# ============================================================================
message(STATUS "Surface geometry integration tests configured:")
message(STATUS "  - Bio-Async: message routing, subscriptions")
message(STATUS "  - Quantum: QMC estimation, QMCTS optimization")
message(STATUS "  - Immune: antigen/antibody lifecycle")
message(STATUS "  - Brain: spine/axon geometry, initialization")
