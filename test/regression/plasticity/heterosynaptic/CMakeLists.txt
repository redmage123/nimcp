# Regression tests for heterosynaptic plasticity

# Test executable
add_executable(regression_plasticity_heterosynaptic
    test_heterosynaptic_regression.cpp
)

target_link_libraries(regression_plasticity_heterosynaptic
    PRIVATE
    nimcp
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(regression_plasticity_heterosynaptic
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Register test
add_test(
    NAME regression_plasticity_heterosynaptic
    COMMAND regression_plasticity_heterosynaptic --gtest_brief=1
)
