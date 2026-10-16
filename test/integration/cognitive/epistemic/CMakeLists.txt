# ============================================================================
# Epistemic Integration Tests
# ============================================================================
# WHAT: Integration tests for Epistemic SNN-Plasticity bidirectional system
# WHY:  Verify SNN and Plasticity bridges work together correctly
# HOW:  Test closed-loop learning, source reliability tracking, belief revision

add_executable(integration_cognitive_epistemic_snn_plasticity
    test_epistemic_snn_plasticity_integration.cpp
)

target_link_libraries(integration_cognitive_epistemic_snn_plasticity
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_epistemic_snn_plasticity
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(integration_cognitive_epistemic_snn_plasticity PRIVATE cxx_std_17)

gtest_discover_tests(integration_cognitive_epistemic_snn_plasticity
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;epistemic;snn;plasticity"
        TIMEOUT 180
)
