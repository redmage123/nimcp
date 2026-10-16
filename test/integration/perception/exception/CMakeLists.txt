# =============================================================================
# Perception Exception Integration Tests
# =============================================================================
#
# WHAT: Integration tests for perception module exception handling
# WHY:  Verify complete exception flow from perception modules to immune system
# HOW:  Test full pipeline: perception error -> exception -> handler -> immune -> recovery
#
# INTEGRATION SCENARIOS:
# - Visual cortex exception -> handler chain -> immune presentation
# - Audio cortex exception -> async queue -> immune response
# - Speech cortex exception -> chained exceptions -> aggregate recovery
# - Multi-sensory exception handling coordination
# - Recovery callback execution for perception failures
#
# =============================================================================

# Perception Exception Integration Tests
add_executable(integration_perception_exception_test_perception_exception_integration
    test_perception_exception_integration.cpp
)

target_link_libraries(integration_perception_exception_test_perception_exception_integration
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        pthread
        m
)

target_compile_features(integration_perception_exception_test_perception_exception_integration PRIVATE cxx_std_17)

# Add test to CTest
add_test(
    NAME integration_perception_exception_test_perception_exception_integration
    COMMAND integration_perception_exception_test_perception_exception_integration
)

set_tests_properties(integration_perception_exception_test_perception_exception_integration
    PROPERTIES
    LABELS "integration;perception;exception;immune"
    TIMEOUT 120
)
