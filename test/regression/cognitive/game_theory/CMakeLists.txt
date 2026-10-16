# ============================================================================
# Game Theory Regression Tests
# ============================================================================
# WHAT: Build configuration for Game Theory module regression tests
# WHY:  Verify correctness, stability, performance, and numerical accuracy
# HOW:  Single executable with all regression tests for comprehensive validation

# Game Theory Regression Tests
add_executable(regression_game_theory
    test_auction_regression.cpp
    test_bargaining_regression.cpp
    test_credit_assignment_regression.cpp
)
target_link_libraries(regression_game_theory PRIVATE
    nimcp
    nimcp_lnn
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
)
target_include_directories(regression_game_theory PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/test/utils
)
target_compile_features(regression_game_theory PRIVATE cxx_std_17)
target_compile_definitions(regression_game_theory PRIVATE NIMCP_TESTING)
gtest_discover_tests(regression_game_theory
    PROPERTIES LABELS "regression;cognitive;game_theory"
    TIMEOUT 600
)

# ----------------------------------------------------------------------------
# Game Theory FEP Bridge Regression Tests
# ----------------------------------------------------------------------------
# WHAT: Regression tests to prevent degradation in Game Theory FEP Bridge behavior
# WHY:  Ensure strategic stability, free energy bounds, performance, and memory safety
# HOW:  Test known-good behaviors, boundary conditions, and performance baselines
# ----------------------------------------------------------------------------
add_executable(regression_game_theory_fep
    test_game_theory_fep_regression.cpp
)

target_link_libraries(regression_game_theory_fep
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_game_theory_fep
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(regression_game_theory_fep PRIVATE cxx_std_17)
target_compile_definitions(regression_game_theory_fep PRIVATE NIMCP_TESTING)

gtest_discover_tests(regression_game_theory_fep
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;game_theory;fep;free_energy"
        TIMEOUT 300
)
