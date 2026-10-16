# Neuron Types Regression Tests
# Test neuron type operations at scale

# Neuron Types Regression Test
add_executable(regression_core_neuron_types_regression
    test_neuron_types_regression.cpp
)
target_link_libraries(regression_core_neuron_types_regression
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        m
        Threads::Threads
)
target_include_directories(regression_core_neuron_types_regression
    PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src
)

# Use gtest_discover_tests with timeout property
gtest_discover_tests(regression_core_neuron_types_regression
    PROPERTIES TIMEOUT 300
)
