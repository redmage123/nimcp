# Middleware Buffering Regression Tests
# Test Integration Buffer Regression
add_executable(regression_middleware_buffering_integration_buffer
    test_integration_buffer_regression.cpp
)
target_link_libraries(regression_middleware_buffering_integration_buffer
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME regression_middleware_buffering_integration_buffer
         COMMAND regression_middleware_buffering_integration_buffer)
# Test Temporal Accumulator Regression
add_executable(regression_middleware_buffering_temporal_accumulator
    test_temporal_accumulator_regression.cpp
)
target_link_libraries(regression_middleware_buffering_temporal_accumulator
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME regression_middleware_buffering_temporal_accumulator
         COMMAND regression_middleware_buffering_temporal_accumulator)

# Circular Buffer Regression Tests (P1-10, P2-11 fix validation)
add_executable(regression_middleware_buffering_circular_buffer
    test_circular_buffer_regression.cpp
)
target_link_libraries(regression_middleware_buffering_circular_buffer
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME regression_middleware_buffering_circular_buffer
         COMMAND regression_middleware_buffering_circular_buffer)
