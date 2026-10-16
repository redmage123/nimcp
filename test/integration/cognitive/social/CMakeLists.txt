# ============================================================================
# Social Cognition FEP Bridge Integration Tests
# ============================================================================
# WHAT: Integration tests for Social Cognition + FEP Orchestrator
# WHY:  Verify FEP correctly coordinates social cognition updates and free energy
# HOW:  Test real integration between social bond system and FEP orchestrator
#
# TEST COVERAGE:
# - Social predictions minimize free energy
# - Relationship models reduce uncertainty
# - Norm violations increase prediction error
# - Trust changes affect free energy
# - Group dynamics prediction
# - Social learning integration
# - Emotional contagion effects
# - Reputation tracking reduces uncertainty
# - FEP update cycle integration (50ms)
# - Statistics accumulation across cycles

# ----------------------------------------------------------------------------
# Social FEP Integration Test
# ----------------------------------------------------------------------------
add_executable(integration_social_fep
    test_social_fep_integration.cpp
)

target_link_libraries(integration_social_fep
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_social_fep
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(integration_social_fep PRIVATE cxx_std_17)

gtest_discover_tests(integration_social_fep
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;social;fep;orchestrator"
        TIMEOUT 180
)
