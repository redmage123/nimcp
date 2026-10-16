# Ephaptic Coupling Unit Tests
# Test coverage for nimcp_ephaptic.h (AC-1)

# Helper macro to add ephaptic test
macro(add_ephaptic_test TEST_NAME)
    add_executable(unit_physics_ephaptic_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_physics_ephaptic_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m  # Math library
            pthread
    )
    add_test(NAME unit_physics_ephaptic_${TEST_NAME}
             COMMAND unit_physics_ephaptic_${TEST_NAME})
    set_tests_properties(unit_physics_ephaptic_${TEST_NAME}
        PROPERTIES LABELS "unit;physics;ephaptic")
endmacro()

# Core ephaptic coupling tests
add_ephaptic_test(test_ephaptic)
