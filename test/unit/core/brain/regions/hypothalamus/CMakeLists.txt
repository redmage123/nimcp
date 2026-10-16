# Hypothalamus Region Unit Tests
# Includes homeostatic regulation, circadian rhythms, stress response, autonomic control

# Helper macro to add Hypothalamus test
macro(add_hypothalamus_test TEST_NAME)
    add_executable(unit_core_brain_regions_hypothalamus_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_core_brain_regions_hypothalamus_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m  # Math library
            pthread
    )
    add_test(NAME unit_core_brain_regions_hypothalamus_${TEST_NAME}
             COMMAND unit_core_brain_regions_hypothalamus_${TEST_NAME})
endmacro()

# Core Hypothalamus adapter tests (Phase H1)
add_hypothalamus_test(test_nimcp_hypothalamus_adapter)

# Alignment hardening tests (Phase 19)
add_hypothalamus_test(test_nimcp_hypothalamus_alignment)

# Orchestrator tests (bridge coordination, event routing, drive management)
add_hypothalamus_test(test_hypothalamus_orchestrator)

# Training bridge tests (drive-modulated learning, homeostatic regulation)
add_hypothalamus_test(test_hypothalamus_training_bridge)

# Perception bridge tests (sensory modulation, interoception, chemosensory,
# predictive coding, pain modulation, sleep gating, thermal salience)
add_hypothalamus_test(test_hypothalamus_perception_bridge)

# Logic bridge tests (motivated reasoning, wishful thinking, drive-logic integration,
# FEP prediction, predicate-drive mapping, conclusion processing)
add_hypothalamus_test(test_hypothalamus_logic_bridge)

# Internal bus tests (pub-sub, modulation, thread safety)
add_hypothalamus_test(test_hypothalamus_internal_bus)

# Exception handling tests (NIMCP_THROW_TO_IMMUNE integration)
# Tests NULL params, invalid configs, and boundary conditions for:
# orchestrator, drives, homeostasis, perception_bridge, alignment, internal_bus
add_hypothalamus_test(test_hypothalamus_exception_handling)

# Add FEP bridge tests subdirectory
add_subdirectory(fep)
