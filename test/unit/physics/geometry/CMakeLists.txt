# Information Geometry Unit Tests
# Test coverage for nimcp_information_geometry.h

# Helper macro to add geometry test
macro(add_geometry_test TEST_NAME)
    add_executable(unit_physics_geometry_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_physics_geometry_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m  # Math library
            pthread
    )
    add_test(NAME unit_physics_geometry_${TEST_NAME}
             COMMAND unit_physics_geometry_${TEST_NAME})
endmacro()

# Core information geometry tests
add_geometry_test(test_information_geometry)

# Information geometry bridge tests
add_geometry_test(test_information_geometry_bridge)
