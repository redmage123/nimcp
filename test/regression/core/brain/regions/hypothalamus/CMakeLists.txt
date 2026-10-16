# Hypothalamus Region Regression Tests
# Tests for alignment behavior consistency

# Helper macro for hypothalamus regression tests
macro(add_hypothalamus_regression_test TEST_NAME)
    add_executable(regression_core_brain_regions_hypothalamus_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(regression_core_brain_regions_hypothalamus_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    add_test(NAME regression_core_brain_regions_hypothalamus_${TEST_NAME}
             COMMAND regression_core_brain_regions_hypothalamus_${TEST_NAME})
endmacro()

# Alignment regression tests (Phase 19)
add_hypothalamus_regression_test(test_hypothalamus_alignment_regression)

# Orchestrator regression tests
add_hypothalamus_regression_test(test_hypothalamus_orchestrator_regression)

# FEP bridges regression tests
add_hypothalamus_regression_test(test_hypo_fep_bridges_regression)
set_tests_properties(regression_core_brain_regions_hypothalamus_test_hypo_fep_bridges_regression PROPERTIES
    TIMEOUT 300
    RESOURCE_LOCK "brain_heavy"
)

# Hypothalamus-Training bridge regression tests
add_hypothalamus_regression_test(test_hypothalamus_training_regression)

# Hypothalamus-Perception bridge regression tests (Phase 17.5)
add_hypothalamus_regression_test(test_hypothalamus_perception_bridge_regression)

# Hypothalamus-Logic bridge regression tests
add_hypothalamus_regression_test(test_hypothalamus_logic_bridge_regression)
set_tests_properties(regression_core_brain_regions_hypothalamus_test_hypothalamus_logic_bridge_regression PROPERTIES
    TIMEOUT 120
    RESOURCE_LOCK "brain_heavy"
)

# Hypothalamus Internal Bus regression tests
add_hypothalamus_regression_test(test_hypothalamus_internal_bus_regression)

# Hypothalamus Exception handling regression tests (Phase 7)
add_hypothalamus_regression_test(test_hypothalamus_exception_regression)
