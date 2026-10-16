# Integration tests for heterosynaptic plasticity

# Test executable
add_executable(integration_plasticity_heterosynaptic
    test_heterosynaptic_integration.cpp
)

target_link_libraries(integration_plasticity_heterosynaptic
    PRIVATE
    nimcp
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(integration_plasticity_heterosynaptic
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Register test
add_test(
    NAME integration_plasticity_heterosynaptic
    COMMAND integration_plasticity_heterosynaptic --gtest_brief=1
)
