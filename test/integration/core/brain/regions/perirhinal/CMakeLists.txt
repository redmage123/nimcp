# ============================================================================
# Perirhinal Cortex Integration Tests
# ============================================================================
# WHAT: Build configuration for Perirhinal Cortex module integration tests
# WHY:  Test interactions between perirhinal cortex and other brain systems
#       including brain init, KG wiring, immune, bio-async, SNN/plasticity,
#       security, and cognitive layers
# HOW:  Individual executables for each integration test suite

# Helper macro for perirhinal integration tests
macro(add_perirhinal_integration_test TEST_NAME)
    add_executable(integration_core_brain_regions_perirhinal_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(integration_core_brain_regions_perirhinal_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    target_include_directories(integration_core_brain_regions_perirhinal_${TEST_NAME}
        PRIVATE
            ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(integration_core_brain_regions_perirhinal_${TEST_NAME}
        PRIVATE cxx_std_17
    )
    target_compile_definitions(integration_core_brain_regions_perirhinal_${TEST_NAME}
        PRIVATE NIMCP_TESTING
    )
    add_test(NAME integration_core_brain_regions_perirhinal_${TEST_NAME}
             COMMAND integration_core_brain_regions_perirhinal_${TEST_NAME})
    set_tests_properties(integration_core_brain_regions_perirhinal_${TEST_NAME}
        PROPERTIES
            LABELS "integration;core;brain;perirhinal"
            TIMEOUT 180
    )
endmacro()

# Brain init integration tests - Tests perirhinal cortex creation via brain factory
add_perirhinal_integration_test(test_perirhinal_brain_init_integration)

# Bio-async integration tests - Tests bio-async messaging system
add_perirhinal_integration_test(test_perirhinal_bio_async_integration)

# SNN/Plasticity/STDP integration tests - Tests neural network plasticity
add_perirhinal_integration_test(test_perirhinal_snn_plasticity_integration)

# Cognitive layer integration tests - Tests working memory and attention
add_perirhinal_integration_test(test_perirhinal_cognitive_integration)

# Immune system integration tests - Tests brain immune system interaction
add_perirhinal_integration_test(test_perirhinal_immune_integration)

# Security integration tests - Tests access control and validation
add_perirhinal_integration_test(test_perirhinal_security_integration)

# KG wiring integration tests - Tests knowledge graph integration
add_perirhinal_integration_test(test_perirhinal_kg_wiring_integration)
