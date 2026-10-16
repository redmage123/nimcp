# ============================================================================
# Collective Cognition Regression Tests
# ============================================================================
# Performance, memory, numerical stability, and thread safety regression tests
# ============================================================================

# ----------------------------------------------------------------------------
# Regression Test 1: Performance Regression
# ----------------------------------------------------------------------------
add_executable(regression_collective_cognition_performance
    test_collective_cognition_performance_regression.cpp
)

target_link_libraries(regression_collective_cognition_performance
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_collective_cognition_performance
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(regression_collective_cognition_performance PRIVATE cxx_std_17)
target_compile_options(regression_collective_cognition_performance PRIVATE -O2 -march=native)

gtest_discover_tests(regression_collective_cognition_performance
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;collective_cognition;performance"
        TIMEOUT 600
)

# ----------------------------------------------------------------------------
# Regression Test 2: Memory Regression
# ----------------------------------------------------------------------------
add_executable(regression_collective_cognition_memory
    test_collective_cognition_memory_regression.cpp
)

target_link_libraries(regression_collective_cognition_memory
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_collective_cognition_memory
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(regression_collective_cognition_memory PRIVATE cxx_std_17)

gtest_discover_tests(regression_collective_cognition_memory
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;collective_cognition;memory"
        TIMEOUT 300
)

# ----------------------------------------------------------------------------
# Regression Test 3: Thread Safety Regression
# ----------------------------------------------------------------------------
add_executable(regression_collective_cognition_thread_safety
    test_collective_cognition_thread_safety_regression.cpp
)

target_link_libraries(regression_collective_cognition_thread_safety
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_collective_cognition_thread_safety
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(regression_collective_cognition_thread_safety PRIVATE cxx_std_17)

gtest_discover_tests(regression_collective_cognition_thread_safety
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;collective_cognition;thread_safety"
        TIMEOUT 300
)

# ----------------------------------------------------------------------------
# Regression Test 4: Numerical Stability Regression
# ----------------------------------------------------------------------------
add_executable(regression_collective_cognition_numerical
    test_collective_cognition_numerical_regression.cpp
)

target_link_libraries(regression_collective_cognition_numerical
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_collective_cognition_numerical
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(regression_collective_cognition_numerical PRIVATE cxx_std_17)

gtest_discover_tests(regression_collective_cognition_numerical
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;collective_cognition;numerical"
        TIMEOUT 300
)

# ----------------------------------------------------------------------------
# Regression Test 5: Integration Stability Regression
# ----------------------------------------------------------------------------
add_executable(regression_collective_cognition_integration_stability
    test_collective_cognition_integration_stability_regression.cpp
)

target_link_libraries(regression_collective_cognition_integration_stability
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_collective_cognition_integration_stability
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(regression_collective_cognition_integration_stability PRIVATE cxx_std_17)

gtest_discover_tests(regression_collective_cognition_integration_stability
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;collective_cognition;integration"
        TIMEOUT 600
)

# ----------------------------------------------------------------------------
# Regression Test 6: SNN/Plasticity Bridge Integration
# ----------------------------------------------------------------------------
add_executable(regression_collective_bridge
    test_collective_bridge_regression.cpp
)

target_link_libraries(regression_collective_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_collective_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(regression_collective_bridge PRIVATE cxx_std_17)
target_compile_options(regression_collective_bridge PRIVATE -O2)

gtest_discover_tests(regression_collective_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;collective_cognition;bridge;snn;plasticity"
        TIMEOUT 300
)
