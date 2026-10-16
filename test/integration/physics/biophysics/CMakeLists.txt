# ============================================================================
# Biophysics Integration Tests CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for Hodgkin-Huxley integration tests
# WHY:  Verify HH integration with SNN, temperature, synaptic input, etc.
# HOW:  Build GoogleTest executable linking all required modules
# ============================================================================

# Guard: Check GTest availability
if(NOT GTest_FOUND)
    message(STATUS "GTest not found - skipping biophysics integration tests")
    return()
endif()

# Common link libraries for HH integration tests
set(HH_INTEGRATION_TEST_LIBS
    nimcp
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
)

# Common include directories
set(HH_INTEGRATION_TEST_INCLUDES
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/physics
    ${CMAKE_SOURCE_DIR}/include/physics/biophysics
    ${CMAKE_SOURCE_DIR}/include/snn
)

# ============================================================================
# HH Integration Test
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_hh_integration.cpp)
    add_executable(integration_physics_hh_integration
        test_hh_integration.cpp
    )

    target_link_libraries(integration_physics_hh_integration
        PRIVATE ${HH_INTEGRATION_TEST_LIBS}
    )

    target_include_directories(integration_physics_hh_integration
        PRIVATE ${HH_INTEGRATION_TEST_INCLUDES}
    )

    target_compile_features(integration_physics_hh_integration PRIVATE cxx_std_17)

    gtest_discover_tests(integration_physics_hh_integration
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "integration;physics;biophysics;hodgkin-huxley"
            TIMEOUT 180
    )

    message(STATUS "  Added HH integration test: test_hh_integration")
endif()
