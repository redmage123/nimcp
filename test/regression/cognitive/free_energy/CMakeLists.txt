# ============================================================================
# Free Energy Principle Regression Tests
# ============================================================================
# WHAT: Build configuration for FEP module regression tests
# WHY:  Verify stability, performance, and numerical accuracy of FEP modules
# HOW:  Single executable with all regression tests for long-running validation

# FEP Bridge Regression Tests
add_executable(regression_fep_bridges
    test_fep_bridges_stability_regression.cpp
    test_fep_bridges_performance_regression.cpp
    test_fep_bridges_state_regression.cpp
    test_fep_orchestrator_regression.cpp
)
target_link_libraries(regression_fep_bridges PRIVATE
    nimcp
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
)
target_include_directories(regression_fep_bridges PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(regression_fep_bridges PRIVATE cxx_std_17)
target_compile_definitions(regression_fep_bridges PRIVATE NIMCP_TESTING)
gtest_discover_tests(regression_fep_bridges
    PROPERTIES LABELS "regression;cognitive;free_energy;bridges"
    TIMEOUT 600
)
