# Middleware Pipeline Regression Tests
# Test Middleware Context Regression
add_executable(regression_middleware_pipeline_middleware_context
    test_middleware_context_regression.cpp
)
target_link_libraries(regression_middleware_pipeline_middleware_context
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME regression_middleware_pipeline_middleware_context
         COMMAND regression_middleware_pipeline_middleware_context)
# Test Middleware Pipeline Regression
add_executable(regression_middleware_pipeline_middleware_pipeline
    test_middleware_pipeline_regression.cpp
)
target_link_libraries(regression_middleware_pipeline_middleware_pipeline
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME regression_middleware_pipeline_middleware_pipeline
         COMMAND regression_middleware_pipeline_middleware_pipeline)
