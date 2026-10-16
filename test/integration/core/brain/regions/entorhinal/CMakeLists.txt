# ============================================================================
# Entorhinal Cortex Integration Tests
# ============================================================================
# WHAT: Integration tests for entorhinal cortex with all NIMCP subsystems
# WHY:  Verify entorhinal integrates correctly with brain init, KG, bio-async,
#       SNN, plasticity, cognitive, immune, and security modules
# HOW:  Test bidirectional communication, state synchronization, error handling
# ============================================================================

# Macro to add entorhinal integration test
macro(add_entorhinal_integration_test TEST_NAME)
    add_executable(integration_core_brain_regions_entorhinal_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(integration_core_brain_regions_entorhinal_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    add_test(NAME integration_core_brain_regions_entorhinal_${TEST_NAME}
             COMMAND integration_core_brain_regions_entorhinal_${TEST_NAME})
endmacro()

# Existing integration test
add_entorhinal_integration_test(test_entorhinal_integration)

# Brain Init Integration - Tests factory integration and initialization
add_entorhinal_integration_test(test_entorhinal_brain_init_integration)

# Knowledge Graph Wiring - Tests KG node registration and queries
add_entorhinal_integration_test(test_entorhinal_kg_wiring_integration)

# Bio-Async Integration - Tests neuromodulator channels and async messaging
add_entorhinal_integration_test(test_entorhinal_bio_async_integration)

# SNN/Plasticity Integration - Tests spike timing and synaptic plasticity
add_entorhinal_integration_test(test_entorhinal_snn_plasticity_integration)

# Cognitive Integration - Tests working memory and attention integration
add_entorhinal_integration_test(test_entorhinal_cognitive_integration)

# Immune System Integration - Tests anomaly detection and self-healing
add_entorhinal_integration_test(test_entorhinal_immune_integration)

# Security Integration - Tests access control and threat detection
add_entorhinal_integration_test(test_entorhinal_security_integration)
