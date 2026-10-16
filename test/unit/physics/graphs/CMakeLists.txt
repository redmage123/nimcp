# Graph Theory Bridge Unit Tests
# Test coverage for nimcp_graph_theory_bridge.h

# Helper macro to add graph theory test
macro(add_graph_theory_test TEST_NAME)
    add_executable(unit_physics_graphs_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_physics_graphs_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m  # Math library
            pthread
    )
    add_test(NAME unit_physics_graphs_${TEST_NAME}
             COMMAND unit_physics_graphs_${TEST_NAME})
    set_tests_properties(unit_physics_graphs_${TEST_NAME}
        PROPERTIES
        LABELS "unit;physics;graphs"
        TIMEOUT 120
    )
endmacro()

# Core graph theory bridge tests
add_graph_theory_test(test_graph_theory_bridge)
