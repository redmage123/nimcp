# ============================================================================
# Physics Bridges Integration Tests CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for physics bridges integration tests
# WHY:  Verify physics bridges integrate correctly with external systems
# HOW:  Use GoogleTest framework with linked physics modules
# ============================================================================

# Find GoogleTest
find_package(GTest QUIET)

if(NOT GTest_FOUND)
    message(STATUS "GoogleTest not found - skipping physics bridges integration tests")
    return()
endif()

# Add physics bridges integration test executable
add_executable(integration_physics_bridges_test
    test_physics_bridges_integration.cpp
)

target_include_directories(integration_physics_bridges_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(integration_physics_bridges_test PRIVATE
    nimcp
    GTest::gtest
    GTest::gtest_main
    m
)

# Add test to CTest
add_test(
    NAME integration_physics_bridges_test
    COMMAND integration_physics_bridges_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Set test labels
set_tests_properties(integration_physics_bridges_test PROPERTIES
    LABELS "integration;physics;bridges"
    TIMEOUT 60
)

message(STATUS "Added physics bridges integration test")
