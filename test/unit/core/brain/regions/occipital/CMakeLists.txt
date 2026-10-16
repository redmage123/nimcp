# Occipital Cortex Unit Tests

macro(add_occipital_test TEST_NAME)
    add_executable(unit_core_brain_regions_occipital_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_core_brain_regions_occipital_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    add_test(NAME unit_core_brain_regions_occipital_${TEST_NAME}
             COMMAND unit_core_brain_regions_occipital_${TEST_NAME})
endmacro()

# Occipital adapter tests
add_occipital_test(test_nimcp_occipital_adapter)
add_occipital_test(test_nimcp_occipital_quantum_bridge)

# Occipital bridge tests
add_occipital_test(test_nimcp_occipital_substrate_bridge)
add_occipital_test(test_nimcp_occipital_thalamic_bridge)

# New integration bridge tests
add_occipital_test(test_nimcp_occipital_audiovisual_bridge)
add_occipital_test(test_nimcp_occipital_cortical_bridge)
add_occipital_test(test_nimcp_occipital_logic_bridge)
add_occipital_test(test_nimcp_occipital_cognitive_bridge)
add_occipital_test(test_nimcp_occipital_collective_bridge)
