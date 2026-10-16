# Cingulate Cortex Unit Tests
# Includes conflict monitoring, error detection, and quantum bridge tests

# Helper macro to add Cingulate test
macro(add_cingulate_test TEST_NAME)
    add_executable(unit_core_brain_regions_cingulate_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_core_brain_regions_cingulate_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m  # Math library
            pthread
    )
    add_test(NAME unit_core_brain_regions_cingulate_${TEST_NAME}
             COMMAND unit_core_brain_regions_cingulate_${TEST_NAME})
endmacro()

# Core Cingulate adapter tests
add_cingulate_test(test_nimcp_cingulate_adapter)

# Cingulate quantum bridge tests
add_cingulate_test(test_nimcp_cingulate_quantum_bridge)
