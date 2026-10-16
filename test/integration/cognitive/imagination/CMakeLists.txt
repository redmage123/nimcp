# ============================================================================
# Imagination FEP Integration Tests
# ============================================================================
# Tests integration of imagination engine with:
# - FEP orchestrator (free energy minimization)
# - Mental simulation and prediction error tracking
# - Counterfactual reasoning and free energy costs
# - Scenario planning and uncertainty bounds
# ============================================================================

# ----------------------------------------------------------------------------
# Test: Imagination + FEP Orchestrator Integration
# ----------------------------------------------------------------------------
add_executable(integration_imagination_fep
    test_imagination_fep_integration.cpp
)

target_link_libraries(integration_imagination_fep
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_imagination_fep
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(integration_imagination_fep PRIVATE cxx_std_17)

gtest_discover_tests(integration_imagination_fep
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;imagination;fep;orchestrator"
        TIMEOUT 180
)
