# ============================================================================
# Hypothalamus Integration Tests
# ============================================================================
# WHAT: Build configuration for hypothalamus module integration tests
# WHY:  Test interactions between hypothalamus components including
#       orchestrator, FEP bridges, and bio-async integration
# HOW:  Individual executables for each integration test suite

# Helper macro for hypothalamus integration tests
macro(add_hypothalamus_integration_test TEST_NAME)
    add_executable(integration_core_brain_regions_hypothalamus_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(integration_core_brain_regions_hypothalamus_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    target_include_directories(integration_core_brain_regions_hypothalamus_${TEST_NAME}
        PRIVATE
            ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(integration_core_brain_regions_hypothalamus_${TEST_NAME}
        PRIVATE cxx_std_17
    )
    target_compile_definitions(integration_core_brain_regions_hypothalamus_${TEST_NAME}
        PRIVATE NIMCP_TESTING
    )
    add_test(NAME integration_core_brain_regions_hypothalamus_${TEST_NAME}
             COMMAND integration_core_brain_regions_hypothalamus_${TEST_NAME})
    set_tests_properties(integration_core_brain_regions_hypothalamus_${TEST_NAME}
        PROPERTIES
            LABELS "integration;core;brain;hypothalamus"
            TIMEOUT 180
    )
endmacro()

# Alignment-Drive integration tests (Phase 19)
add_hypothalamus_integration_test(test_hypothalamus_alignment_integration)

# Orchestrator integration tests - Tests orchestrator with multiple bridges
add_hypothalamus_integration_test(test_hypothalamus_orchestrator_integration)

# FEP bridges integration tests - Tests FEP bridges working together
add_hypothalamus_integration_test(test_hypo_fep_integration)

# Bio-async integration tests - Tests bio-async router integration
add_hypothalamus_integration_test(test_hypo_bio_async_integration)

# Hypothalamus-Training bridge integration tests - Tests training system integration
add_hypothalamus_integration_test(test_hypothalamus_training_integration)

# Hypothalamus-Perception bridge integration tests - Tests perception modulation integration
add_hypothalamus_integration_test(test_hypothalamus_perception_bridge_integration)

# Hypothalamus-Logic bridge integration tests - Tests motivated reasoning and logic-driven drive updates
add_hypothalamus_integration_test(test_hypothalamus_logic_bridge_integration)

# Internal bus integration tests - Tests bidirectional communication between hypothalamus modules
add_hypothalamus_integration_test(test_hypothalamus_internal_bus_integration)

# Exception handling integration tests - Tests exception-to-immune pipeline for hypothalamus modules
add_hypothalamus_integration_test(test_hypothalamus_exception_integration)
