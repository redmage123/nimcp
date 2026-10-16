# Integration tests for plasticity substrate bridges
cmake_minimum_required(VERSION 3.14)

# Test executable
add_executable(integration_plasticity_substrate_bridges
    test_plasticity_substrate_bridges_integration.cpp
)

# Link libraries
target_link_libraries(integration_plasticity_substrate_bridges
    PRIVATE
    nimcp
    GTest::gtest
    GTest::gtest_main
    pthread
)

# Include directories
target_include_directories(integration_plasticity_substrate_bridges
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# Add test
add_test(
    NAME integration_plasticity_substrate_bridges
    COMMAND integration_plasticity_substrate_bridges --gtest_brief=1
)
