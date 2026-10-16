# Regression tests for middleware controller (Phase 1.5.5)

add_executable(regression_middleware_controller
    test_middleware_controller_regression.cpp
)

target_link_libraries(regression_middleware_controller
    nimcp
    GTest::gtest
    GTest::gtest_main
    pthread
    m
)

target_include_directories(regression_middleware_controller PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

add_test(NAME regression_middleware_controller
    COMMAND regression_middleware_controller)

# Bio-async middleware integration regression tests
add_executable(regression_middleware_bio_async
    test_middleware_bio_async_regression.cpp
)

target_link_libraries(regression_middleware_bio_async
    nimcp
    GTest::gtest
    GTest::gtest_main
    pthread
    m
)

target_include_directories(regression_middleware_bio_async PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

add_test(NAME regression_middleware_bio_async
    COMMAND regression_middleware_bio_async)

# FLAKY: MiddlewareBioAsyncRegressionTest - timeout-prone under load,
#        bio-async timing-sensitive operations can exceed timeout limits
set_tests_properties(regression_middleware_bio_async PROPERTIES
    TIMEOUT 120
    LABELS "regression;middleware;bio_async;performance;flaky"
)
