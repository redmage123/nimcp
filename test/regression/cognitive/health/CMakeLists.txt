# ============================================================================
# Phase 8 Health Cognitive Regression Tests
# ============================================================================

# Health Cognitive Regression Test
add_executable(regression_cognitive_health_cognitive_regression
    test_health_cognitive_regression.cpp
)

target_link_libraries(regression_cognitive_health_cognitive_regression
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_cognitive_health_cognitive_regression
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_cognitive_health_cognitive_regression PRIVATE cxx_std_17)

gtest_discover_tests(regression_cognitive_health_cognitive_regression
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;health;phase8"
        TIMEOUT 120
)
