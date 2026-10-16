# Thermodynamics Unit Tests
# Test coverage for nimcp_thermodynamics.h (AC-9)

# Helper macro to add thermodynamics test
macro(add_thermodynamics_test TEST_NAME)
    add_executable(unit_physics_thermodynamics_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_physics_thermodynamics_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m  # Math library
            pthread
    )
    add_test(NAME unit_physics_thermodynamics_${TEST_NAME}
             COMMAND unit_physics_thermodynamics_${TEST_NAME})
endmacro()

# Core thermodynamics tests
add_thermodynamics_test(test_thermodynamics)

# TLS-to-shared-state migration tests (Bug H7)
add_thermodynamics_test(test_thermodynamics_tls_fix)
