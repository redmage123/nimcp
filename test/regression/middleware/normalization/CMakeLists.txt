# Middleware Normalization Regression Tests
# Test Adaptive Normalizer Regression
add_executable(regression_middleware_normalization_adaptive_normalizer
    test_adaptive_normalizer_regression.cpp
)
target_link_libraries(regression_middleware_normalization_adaptive_normalizer
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME regression_middleware_normalization_adaptive_normalizer
         COMMAND regression_middleware_normalization_adaptive_normalizer)
# Test Homeostatic Normalizer Regression
add_executable(regression_middleware_normalization_homeostatic_normalizer
    test_homeostatic_normalizer_regression.cpp
)
target_link_libraries(regression_middleware_normalization_homeostatic_normalizer
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME regression_middleware_normalization_homeostatic_normalizer
         COMMAND regression_middleware_normalization_homeostatic_normalizer)
# Test Min Max Normalizer Regression
add_executable(regression_middleware_normalization_min_max_normalizer
    test_min_max_normalizer_regression.cpp
)
target_link_libraries(regression_middleware_normalization_min_max_normalizer
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME regression_middleware_normalization_min_max_normalizer
         COMMAND regression_middleware_normalization_min_max_normalizer)
# Test Zscore Normalizer Regression
add_executable(regression_middleware_normalization_zscore_normalizer
    test_zscore_normalizer_regression.cpp
)
target_link_libraries(regression_middleware_normalization_zscore_normalizer
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME regression_middleware_normalization_zscore_normalizer
         COMMAND regression_middleware_normalization_zscore_normalizer)
