# Cognitive Bio-Async Regression Tests
# Regression tests for cognitive module bio-async stability and performance

add_executable(regression_cognitive_bio_async_test
    test_cognitive_bio_async_regression.cpp
)

target_link_libraries(regression_cognitive_bio_async_test
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)

target_include_directories(regression_cognitive_bio_async_test
    PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_cognitive_bio_async_test PRIVATE cxx_std_17)

gtest_discover_tests(regression_cognitive_bio_async_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;bio-async;performance;stress"
        TIMEOUT 300
)

# Cognitive Modules API Correctness Tests
# Tests for correct API usage of cognitive modules
add_executable(regression_cognitive_modules_api_test
    test_cognitive_modules_api.cpp
)

target_link_libraries(regression_cognitive_modules_api_test
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)

target_include_directories(regression_cognitive_modules_api_test
    PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_cognitive_modules_api_test PRIVATE cxx_std_17)

gtest_discover_tests(regression_cognitive_modules_api_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DISCOVERY_MODE PRE_TEST
    PROPERTIES
        LABELS "regression;cognitive;api;correctness"
        TIMEOUT 60
)
