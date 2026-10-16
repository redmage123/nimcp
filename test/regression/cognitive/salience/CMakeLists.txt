# ============================================================================
# Salience Regression Tests
# ============================================================================
# WHAT: Regression tests for Salience SNN-Plasticity bridges
# WHY:  Ensure attention/salience processing maintains performance
# HOW:  Test baselines, boundaries, performance thresholds

add_executable(regression_cognitive_salience_snn_plasticity
    test_salience_snn_plasticity_regression.cpp
)

target_link_libraries(regression_cognitive_salience_snn_plasticity
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_cognitive_salience_snn_plasticity
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_cognitive_salience_snn_plasticity PRIVATE cxx_std_17)

gtest_discover_tests(regression_cognitive_salience_snn_plasticity
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;salience;snn;plasticity"
        TIMEOUT 300
)

# ============================================================================
# Surprise Amplifier Regression Tests (Society of Thought Phase 1)
# ============================================================================

add_executable(regression_cognitive_surprise_amplifier
    test_surprise_amplifier_regression.cpp
)

target_link_libraries(regression_cognitive_surprise_amplifier
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_cognitive_surprise_amplifier
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_cognitive_surprise_amplifier PRIVATE cxx_std_17)

gtest_discover_tests(regression_cognitive_surprise_amplifier
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;salience;surprise;society_of_thought"
        TIMEOUT 300
)

# ============================================================================
# Surprise Bridges Regression Tests (Society of Thought Phase 1)
# ============================================================================

add_executable(regression_cognitive_surprise_bridges
    test_surprise_bridges_regression.cpp
)

target_link_libraries(regression_cognitive_surprise_bridges
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_cognitive_surprise_bridges
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_cognitive_surprise_bridges PRIVATE cxx_std_17)

gtest_discover_tests(regression_cognitive_surprise_bridges
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;salience;surprise;bridges;society_of_thought"
        TIMEOUT 300
)

# ============================================================================
# Surprise Bridges Phase 1b Regression Tests (Society of Thought)
# ============================================================================

add_executable(regression_cognitive_surprise_bridges_phase1b
    test_surprise_bridges_phase1b_regression.cpp
)

target_link_libraries(regression_cognitive_surprise_bridges_phase1b
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_cognitive_surprise_bridges_phase1b
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_cognitive_surprise_bridges_phase1b PRIVATE cxx_std_17)

gtest_discover_tests(regression_cognitive_surprise_bridges_phase1b
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;salience;surprise;bridges;phase1b;society_of_thought"
        TIMEOUT 300
)
