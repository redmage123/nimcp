# =============================================================================
# Perception Exception Unit Tests
# =============================================================================
#
# WHAT: Unit tests for perception module exception handling
# WHY:  Verify error code mapping, exception creation, handler dispatch
# HOW:  Test individual exception system components in isolation
#
# MODULES COVERED:
# - Visual Cortex exceptions
# - Audio Cortex exceptions
# - Speech Cortex exceptions
# - Cochlea exceptions
# - Retina exceptions
#
# =============================================================================

# Perception Exception Handling Unit Tests
add_executable(unit_perception_exception_test_perception_exception_handling
    test_perception_exception_handling.cpp
)

target_link_libraries(unit_perception_exception_test_perception_exception_handling
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        pthread
        m
)

target_compile_features(unit_perception_exception_test_perception_exception_handling PRIVATE cxx_std_17)

# Add test to CTest
add_test(
    NAME unit_perception_exception_test_perception_exception_handling
    COMMAND unit_perception_exception_test_perception_exception_handling
)

set_tests_properties(unit_perception_exception_test_perception_exception_handling
    PROPERTIES
    LABELS "unit;perception;exception"
    TIMEOUT 60
)
