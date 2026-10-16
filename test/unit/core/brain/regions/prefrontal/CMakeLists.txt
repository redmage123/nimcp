# Prefrontal Cortex Unit Tests

macro(add_prefrontal_test TEST_NAME)
    add_executable(unit_core_brain_regions_prefrontal_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_core_brain_regions_prefrontal_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    add_test(NAME unit_core_brain_regions_prefrontal_${TEST_NAME}
             COMMAND unit_core_brain_regions_prefrontal_${TEST_NAME})
endmacro()

# Prefrontal adapter tests
add_prefrontal_test(test_nimcp_prefrontal_adapter)
add_prefrontal_test(test_nimcp_prefrontal_quantum_bridge)
