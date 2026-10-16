# ============================================================================
# Cognitive Integration Regression Tests
# ============================================================================
# WHAT: Regression tests for Cognitive Integration system
# WHY:  Ensure event ordering, no event loss, thread safety, and performance bounds
# HOW:  Test edge cases, stress conditions, and performance thresholds
# ============================================================================

# Hub Regression Tests
add_executable(regression_cognitive_test_hub_regression
    test_hub_regression.cpp
)

target_link_libraries(regression_cognitive_test_hub_regression
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_cognitive_test_hub_regression
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(regression_cognitive_test_hub_regression PRIVATE cxx_std_17)

gtest_discover_tests(regression_cognitive_test_hub_regression
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;integration;hub"
        TIMEOUT 300
)

# Bridge Regression Tests
add_executable(regression_cognitive_test_bridge_regression
    test_bridge_regression.cpp
)

target_link_libraries(regression_cognitive_test_bridge_regression
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_cognitive_test_bridge_regression
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(regression_cognitive_test_bridge_regression PRIVATE cxx_std_17)

gtest_discover_tests(regression_cognitive_test_bridge_regression
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;integration;bridge"
        TIMEOUT 300
)

# Performance Regression Tests
add_executable(regression_cognitive_test_integration_performance_regression
    test_integration_performance_regression.cpp
)

target_link_libraries(regression_cognitive_test_integration_performance_regression
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_cognitive_test_integration_performance_regression
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(regression_cognitive_test_integration_performance_regression PRIVATE cxx_std_17)

gtest_discover_tests(regression_cognitive_test_integration_performance_regression
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;integration;performance"
        TIMEOUT 300
)

# ============================================================================
# Tier 2 Hub Bridges Regression Tests
# ============================================================================
# WHAT: Regression tests for Tier 2 Hub bridges (imagination_reasoning,
#       game_theory_executive, mirror_empathy, salience_attention, predictive_attention)
# WHY:  Ensure event routing stability, performance bounds, memory safety, and
#       concurrent operation correctness for all Tier 2 cognitive bridges
# HOW:  Test event routing, performance regression, memory stability, concurrency,
#       state consistency, and statistics accuracy
# ============================================================================
add_executable(regression_cognitive_test_tier2_hub_bridges_regression
    test_tier2_hub_bridges_regression.cpp
)

target_link_libraries(regression_cognitive_test_tier2_hub_bridges_regression
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_cognitive_test_tier2_hub_bridges_regression
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/cognitive/integration
        ${PROJECT_SOURCE_DIR}/include/cognitive/imagination
        ${PROJECT_SOURCE_DIR}/include/cognitive/game_theory
        ${PROJECT_SOURCE_DIR}/include/cognitive/social
        ${PROJECT_SOURCE_DIR}/include/cognitive/attention
        ${PROJECT_SOURCE_DIR}/include/cognitive/prediction
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(regression_cognitive_test_tier2_hub_bridges_regression PRIVATE cxx_std_17)

gtest_discover_tests(regression_cognitive_test_tier2_hub_bridges_regression
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;integration;tier2;hub;bridges"
        TIMEOUT 300
)

# ============================================================================
# Tier 3 FEP Bridges Regression Tests
# ============================================================================
# WHAT: Regression tests for Tier 3 FEP bridges (FEP bridges for Tier 2 Hub bridges)
# WHY:  Ensure FEP bridge behavior stability, free energy bounds, and state consistency
# HOW:  Test baseline values, state transitions, boundary conditions, reset stability
# ============================================================================
add_executable(regression_cognitive_test_tier3_fep_bridges_regression
    test_tier3_fep_bridges_regression.cpp
)

target_link_libraries(regression_cognitive_test_tier3_fep_bridges_regression
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_cognitive_test_tier3_fep_bridges_regression
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(regression_cognitive_test_tier3_fep_bridges_regression PRIVATE cxx_std_17)

gtest_discover_tests(regression_cognitive_test_tier3_fep_bridges_regression
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;integration;tier3;fep;bridges"
        TIMEOUT 300
)
