# Middleware Patterns Regression Tests
# Test Oscillation Detector Regression
add_executable(regression_middleware_patterns_oscillation_detector
    test_oscillation_detector_regression.cpp
)
target_link_libraries(regression_middleware_patterns_oscillation_detector
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME regression_middleware_patterns_oscillation_detector
         COMMAND regression_middleware_patterns_oscillation_detector)
# Test Pattern Library Regression
add_executable(regression_middleware_patterns_pattern_library
    test_pattern_library_regression.cpp
)
target_link_libraries(regression_middleware_patterns_pattern_library
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME regression_middleware_patterns_pattern_library
         COMMAND regression_middleware_patterns_pattern_library)
# Test Sequence Detector Regression
add_executable(regression_middleware_patterns_sequence_detector
    test_sequence_detector_regression.cpp
)
target_link_libraries(regression_middleware_patterns_sequence_detector
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME regression_middleware_patterns_sequence_detector
         COMMAND regression_middleware_patterns_sequence_detector)

# Test PAC Backward Compatibility
add_executable(regression_middleware_patterns_pac_backward_compat
    ../test_pac_backward_compat.cpp
)
target_link_libraries(regression_middleware_patterns_pac_backward_compat
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME regression_middleware_patterns_pac_backward_compat
         COMMAND regression_middleware_patterns_pac_backward_compat)
