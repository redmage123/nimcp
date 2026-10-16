# Dynamical Systems Unit Tests
# Test coverage for nimcp_dynamical_systems.h

# Helper macro to add dynamical systems test
macro(add_dynamics_test TEST_NAME)
    add_executable(unit_physics_dynamics_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_physics_dynamics_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m  # Math library
            pthread
    )
    add_test(NAME unit_physics_dynamics_${TEST_NAME}
             COMMAND unit_physics_dynamics_${TEST_NAME})
    set_tests_properties(unit_physics_dynamics_${TEST_NAME}
        PROPERTIES
        LABELS "unit;physics;dynamics"
        TIMEOUT 180
    )
endmacro()

# Core dynamical systems tests
add_dynamics_test(test_dynamical_systems)
