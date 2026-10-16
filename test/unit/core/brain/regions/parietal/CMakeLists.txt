# Parietal Cortex Unit Tests
# Tests for spatial processing, somatosensory, and sensorimotor integration

# Helper macro to add Parietal test
macro(add_parietal_test TEST_NAME)
    add_executable(unit_core_brain_regions_parietal_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_core_brain_regions_parietal_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m  # Math library
            pthread
    )
    add_test(NAME unit_core_brain_regions_parietal_${TEST_NAME}
             COMMAND unit_core_brain_regions_parietal_${TEST_NAME})
endmacro()

# Core Parietal Cortex tests
add_parietal_test(test_nimcp_parietal_adapter)
