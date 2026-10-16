# Temporal Cortex Unit Tests
# Includes auditory processing, object recognition, semantic memory tests

# Helper macro to add Temporal test
macro(add_temporal_test TEST_NAME)
    add_executable(unit_core_brain_regions_temporal_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_core_brain_regions_temporal_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m  # Math library
            pthread
    )
    add_test(NAME unit_core_brain_regions_temporal_${TEST_NAME}
             COMMAND unit_core_brain_regions_temporal_${TEST_NAME})
endmacro()

# Core Temporal adapter tests
add_temporal_test(test_nimcp_temporal_adapter)
