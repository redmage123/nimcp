# Middleware Encoding Regression Tests
# Test Rate Coding Regression
add_executable(regression_middleware_encoding_rate_coding
    test_rate_coding_regression.cpp
)
target_link_libraries(regression_middleware_encoding_rate_coding
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME regression_middleware_encoding_rate_coding
         COMMAND regression_middleware_encoding_rate_coding)
# Test Temporal Coding Regression
add_executable(regression_middleware_encoding_temporal_coding
    test_temporal_coding_regression.cpp
)
target_link_libraries(regression_middleware_encoding_temporal_coding
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME regression_middleware_encoding_temporal_coding
         COMMAND regression_middleware_encoding_temporal_coding)
