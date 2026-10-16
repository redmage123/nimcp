# ============================================================================
# Motor Cortex Integration Tests
# ============================================================================
# WHAT: Build configuration for Motor Cortex module integration tests
# WHY:  Test interactions between motor cortex components including
#       brain init, KG wiring, immune, bio-async, SNN/plasticity,
#       security, cognitive, hypothalamus, perception, and swarm
# HOW:  Individual executables for each integration test suite

# Helper macro for motor integration tests
macro(add_motor_integration_test TEST_NAME)
    add_executable(integration_core_brain_regions_motor_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(integration_core_brain_regions_motor_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    target_include_directories(integration_core_brain_regions_motor_${TEST_NAME}
        PRIVATE
            ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(integration_core_brain_regions_motor_${TEST_NAME}
        PRIVATE cxx_std_17
    )
    target_compile_definitions(integration_core_brain_regions_motor_${TEST_NAME}
        PRIVATE NIMCP_TESTING
    )
    add_test(NAME integration_core_brain_regions_motor_${TEST_NAME}
             COMMAND integration_core_brain_regions_motor_${TEST_NAME})
    set_tests_properties(integration_core_brain_regions_motor_${TEST_NAME}
        PROPERTIES
            LABELS "integration;core;brain;motor"
            TIMEOUT 180
    )
endmacro()

# Brain init integration tests - Tests motor cortex creation via brain factory
add_motor_integration_test(test_motor_brain_init_integration)

# KG wiring integration tests - Tests knowledge graph integration
add_motor_integration_test(test_motor_kg_wiring_integration)

# Immune system integration tests - Tests brain immune system interaction
add_motor_integration_test(test_motor_immune_integration)

# Bio-async integration tests - Tests bio-async messaging system
add_motor_integration_test(test_motor_bio_async_integration)

# SNN/Plasticity/STDP integration tests - Tests neural network plasticity
add_motor_integration_test(test_motor_snn_plasticity_integration)

# Security integration tests - Tests blood-brain barrier security
add_motor_integration_test(test_motor_security_integration)

# Cognitive/Training layer integration tests - Tests training hub integration
add_motor_integration_test(test_motor_cognitive_integration)

# Hypothalamus integration tests - Tests homeostatic and circadian effects
add_motor_integration_test(test_motor_hypothalamus_integration)

# Perception layer integration tests - Tests visuomotor coordination
add_motor_integration_test(test_motor_perception_integration)

# Swarm/Portia/Dragonfly integration tests - Tests collective motor coordination
add_motor_integration_test(test_motor_swarm_integration)
