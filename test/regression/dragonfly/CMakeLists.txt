# ============================================================================
# Dragonfly Regression Tests
# ============================================================================
# WHAT: Regression tests for dragonfly bio-inspired target tracking
# WHY:  Ensure biological parameters and performance remain stable
# HOW:  Test fixed values, performance benchmarks, backward compatibility
# ============================================================================

message(STATUS "  Adding dragonfly regression tests...")

# ============================================================================
# Parameter Regression Test
# Ensures biological tuning parameters remain stable across versions
# ============================================================================
add_executable(regression_dragonfly_parameters
    test_dragonfly_parameters.cpp
)

target_link_libraries(regression_dragonfly_parameters
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_dragonfly_parameters
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(regression_dragonfly_parameters PRIVATE cxx_std_17)

gtest_discover_tests(regression_dragonfly_parameters
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;dragonfly;parameters"
        TIMEOUT 120
)

# ============================================================================
# Performance Regression Test
# Ensures dragonfly maintains 95% interception success rate
# ============================================================================
add_executable(regression_dragonfly_performance
    test_dragonfly_performance.cpp
)

target_link_libraries(regression_dragonfly_performance
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_dragonfly_performance
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(regression_dragonfly_performance PRIVATE cxx_std_17)

gtest_discover_tests(regression_dragonfly_performance
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;dragonfly;performance"
        TIMEOUT 300
)

# ============================================================================
# Numerical Stability Regression Test
# Ensures calculations remain numerically stable
# ============================================================================
add_executable(regression_dragonfly_numerical
    test_dragonfly_numerical.cpp
)

target_link_libraries(regression_dragonfly_numerical
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_dragonfly_numerical
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(regression_dragonfly_numerical PRIVATE cxx_std_17)

gtest_discover_tests(regression_dragonfly_numerical
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;dragonfly;numerical"
        TIMEOUT 120
)
