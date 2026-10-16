# Insula Region Unit Tests
# Includes interoception, emotional awareness, social emotions, and quantum bridge tests

# Helper macro to add Insula test
macro(add_insula_test TEST_NAME)
    add_executable(unit_core_brain_regions_insula_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_core_brain_regions_insula_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m  # Math library
            pthread
    )
    add_test(NAME unit_core_brain_regions_insula_${TEST_NAME}
             COMMAND unit_core_brain_regions_insula_${TEST_NAME})
endmacro()

# Insula adapter tests
add_insula_test(test_nimcp_insula_adapter)

# Insula quantum bridge tests
add_insula_test(test_nimcp_insula_quantum_bridge)
