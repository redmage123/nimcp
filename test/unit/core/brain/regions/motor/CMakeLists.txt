# Motor Cortex Unit Tests

macro(add_motor_test TEST_NAME)
    add_executable(unit_core_brain_regions_motor_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_core_brain_regions_motor_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    add_test(NAME unit_core_brain_regions_motor_${TEST_NAME}
             COMMAND unit_core_brain_regions_motor_${TEST_NAME})
endmacro()

# Motor adapter tests
add_motor_test(test_nimcp_motor_adapter)
add_motor_test(test_nimcp_motor_quantum_bridge)
