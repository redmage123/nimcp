# Brainstem Region Unit Tests
# Tests for midbrain, pons, medulla, reticular formation integration

# Helper macro to add Brainstem test
macro(add_brainstem_test TEST_NAME)
    add_executable(unit_core_brain_regions_brainstem_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_core_brain_regions_brainstem_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m  # Math library
            pthread
    )
    add_test(NAME unit_core_brain_regions_brainstem_${TEST_NAME}
             COMMAND unit_core_brain_regions_brainstem_${TEST_NAME})
endmacro()

# Core Brainstem adapter tests
add_brainstem_test(test_nimcp_brainstem_adapter)

# Quantum bridge tests
add_brainstem_test(test_nimcp_brainstem_quantum_bridge)

# Factory initialization tests
add_brainstem_test(test_nimcp_brain_init_brainstem)

# Vestibular system tests
add_brainstem_test(test_vestibular)
add_brainstem_test(test_vestibular_cerebellum_bridge)
