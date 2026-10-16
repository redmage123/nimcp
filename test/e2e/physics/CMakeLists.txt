# ============================================================================
# Physics E2E Tests
# ============================================================================
# WHAT: End-to-end tests for physics modules
# WHY:  Verify complete physics module workflows with full NIMCP integration
# HOW:  Test Graph Theory, Dynamical Systems, and Information Geometry modules
# ============================================================================

# Guard: Check GTest availability
if(NOT GTest_FOUND)
    message(STATUS "GTest not found - skipping Physics E2E tests")
    return()
endif()

# ============================================================================
# Physics Modules E2E Test
# ============================================================================
# WHAT: End-to-end tests for physics modules pipeline
# WHY:  Verify cross-module workflows with KG integration
# HOW:  Test graph analysis, dynamical systems, and info geometry together

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_physics_modules_e2e.cpp")
    message(STATUS "Adding Physics modules E2E tests...")
    add_executable(e2e_test_physics_modules
        ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test_physics_modules_e2e.cpp
    )

    target_link_libraries(e2e_test_physics_modules
        PRIVATE
            nimcp
            GTest::GTest
            GTest::Main
            Threads::Threads
            m
    )

    target_include_directories(e2e_test_physics_modules
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/test/e2e
    )

    target_compile_features(e2e_test_physics_modules PRIVATE cxx_std_17)

    target_compile_definitions(e2e_test_physics_modules PRIVATE
        NIMCP_TESTING
        NIMCP_E2E_TESTING
    )

    add_test(NAME e2e_test_physics_modules COMMAND e2e_test_physics_modules)

    set_tests_properties(e2e_test_physics_modules PROPERTIES
        LABELS "e2e;physics;modules;graphs;dynamics;geometry"
        TIMEOUT 300
        RUN_SERIAL TRUE
    )

    message(STATUS "  Added E2E test: e2e_test_physics_modules")
endif()
