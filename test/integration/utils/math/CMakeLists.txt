# Utils Math Integration Tests
# Test complex number oscillation system integration

# Complex Oscillation Integration Tests
add_executable(integration_utils_math_test_complex_oscillation_integration
    test_complex_oscillation_integration.cpp
)
target_link_libraries(integration_utils_math_test_complex_oscillation_integration
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        nimcp_middleware
        pthread
        m
)
add_test(NAME integration_utils_math_test_complex_oscillation_integration
         COMMAND integration_utils_math_test_complex_oscillation_integration)
set_tests_properties(integration_utils_math_test_complex_oscillation_integration
                     PROPERTIES TIMEOUT 180)
