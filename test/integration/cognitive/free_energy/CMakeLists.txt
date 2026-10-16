# ============================================================================
# Free Energy Principle Integration Tests
# ============================================================================
# WHAT: Build configuration for FEP module integration tests
# WHY:  Test interactions between FEP enhancements and other NIMCP modules
# HOW:  Single executable with all integration tests for parallel execution

add_executable(integration_fep_enhanced
    test_fep_learning_integration.cpp
    test_fep_curiosity_integration.cpp
    test_fep_neuromod_integration.cpp
    test_fep_immune_integration.cpp
    test_fep_consciousness_integration.cpp
    test_fep_planning_integration.cpp
    test_fep_evidence_integration.cpp
    test_fep_context_integration.cpp
    test_fep_sleep_integration.cpp
    test_fep_full_integration.cpp
    test_fep_cognitive_bridges_integration.cpp
    test_fep_perception_bridges_integration.cpp
    test_fep_plasticity_bridges_integration.cpp
    test_fep_bridges_full_pipeline_integration.cpp
    test_fep_orchestrator_integration.cpp
)
target_link_libraries(integration_fep_enhanced PRIVATE
    nimcp
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
)
target_include_directories(integration_fep_enhanced PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(integration_fep_enhanced PRIVATE cxx_std_17)
target_compile_definitions(integration_fep_enhanced PRIVATE NIMCP_TESTING)
gtest_discover_tests(integration_fep_enhanced
    PROPERTIES LABELS "integration;cognitive;free_energy"
    TIMEOUT 180
)
